#pragma once

#include "Packages/Engine/Actors/Info/UInternetInfo.h"
#include "VM/ExpressionValue.h"

#include <memory>
#include <mutex>

#ifdef WIN32
typedef SOCKET socket_t;
#define invalid_socket_value INVALID_SOCKET
#else
typedef int socket_t;
#define invalid_socket_value -1
#endif

enum ELinkMode
{
	MODE_Text,
	MODE_Line,
	MODE_Binary
};

enum EReceiveMode
{
	RMODE_Manual,
	RMODE_Event
};

// The script's sockets, as IpDrv.dll's (docs/re/ipdrv-dll.md). An IpAddr is
// in host byte order, its port a plain number.
class UInternetLink : public UInternetInfo
{
public:
	using UInternetInfo::UInternetInfo;
	~UInternetLink();

	void InitNativeDefaults() override;
	void Tick(float elapsed) override;

	int GetLastError();
	IpAddr GetLocalIP();
	bool IsDataPending();
	void Resolve(const std::string& Domain);
	bool ParseURL(const std::string& URL, std::string& Addr, int& Port, std::string& LevelName, std::string& EntryName);

	std::string IpAddrToString(const IpAddr& Arg);
	bool StringToIpAddr(const std::string& Str, IpAddr& Addr);
	std::string Validate(const std::string& ValidationString, const std::string& GameName);

	int& DataPending() { return Value<int>(PropOffsets_InternetLink.DataPending); }
	uint8_t& LinkMode() { return Value<uint8_t>(PropOffsets_InternetLink.LinkMode); }
	int& Port() { return Value<int>(PropOffsets_InternetLink.Port); }
	int& PrivateResolveInfo() { return Value<int>(PropOffsets_InternetLink.PrivateResolveInfo); } // native
	uint8_t& ReceiveMode() { return Value<uint8_t>(PropOffsets_InternetLink.ReceiveMode); }
	int& RemoteSocket() { return Value<int>(PropOffsets_InternetLink.RemoteSocket); }
	int& Socket() { return Value<int>(PropOffsets_InternetLink.Socket); }

protected:
	static void SetNonBlocking(socket_t s);
	static void CloseSocket(socket_t s);
	static int LastSocketError();
	// Binds INADDR_ANY at the port, or with bUseNextAvailable the first free
	// of the 20 from it; the port bound, or 0.
	static int BindSocket(socket_t s, int port, bool bUseNextAvailable);
	// The event's IpAddr parameter, for a call from native code.
	ExpressionValue IpAddrArg(const NameString& eventName, IpAddr& addr, UStructProperty& prop);

	int LastError = 0;

private:
	// A lookup runs on a thread of its own; the link may be gone before it
	// ends, so the thread holds its own share of the result.
	struct ResolveState
	{
		std::mutex Mutex;
		int Status = 0; // 1 looking up, 2 found, 3 failed
		IpAddr Addr = { 0, 0 };
	};
	std::shared_ptr<ResolveState> Resolving;
};
