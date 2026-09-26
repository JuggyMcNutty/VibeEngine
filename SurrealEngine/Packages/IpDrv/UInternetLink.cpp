
#include "Precomp.h"
#include "UInternetLink.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UFunction.h"
#include "UnrealURL.h"
#include "Engine.h"
#include <thread>

#ifdef WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
typedef unsigned long in_addr_t;
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#endif

UInternetLink::~UInternetLink()
{
}

void UInternetLink::InitNativeDefaults()
{
	// The original's constructor: text, events, no sockets.
	LinkMode() = MODE_Text;
	ReceiveMode() = RMODE_Event;
	DataPending() = 0;
	Port() = 0;
	Socket() = -1;
	RemoteSocket() = -1;
}

void UInternetLink::Tick(float elapsed)
{
	UInternetInfo::Tick(elapsed);

	if (!Resolving)
		return;

	std::unique_lock<std::mutex> lock(Resolving->Mutex);
	int status = Resolving->Status;
	IpAddr addr = Resolving->Addr;
	lock.unlock();

	if (status == 2)
	{
		Resolving.reset();
		UStructProperty prop({}, nullptr, ObjectFlags::NoFlags);
		CallEvent(this, EventName::Resolved, { IpAddrArg("Resolved", addr, prop) });
	}
	else if (status == 3)
	{
		Resolving.reset();
		CallEvent(this, EventName::ResolveFailed);
	}
}

ExpressionValue UInternetLink::IpAddrArg(const NameString& eventName, IpAddr& addr, UStructProperty& prop)
{
	UFunction* func = FindEventFunction(this, eventName);
	if (func && !func->Properties.empty())
		prop.Struct = UObject::Cast<UStructProperty>(func->Properties[0])->Struct;
	return ExpressionValue::Variable(&addr, &prop);
}

int UInternetLink::GetLastError()
{
	return LastError;
}

IpAddr UInternetLink::GetLocalIP()
{
	// The address this machine reaches the network from: a UDP socket
	// "connected" outward sends nothing and names its local end. The
	// original asks the resolver for its own host name, which on Linux often
	// gives a loopback address.
	IpAddr result = { 0x7f000001, 0 };
	socket_t s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == invalid_socket_value)
		return result;
	sockaddr_in remote = {};
	remote.sin_family = AF_INET;
	remote.sin_port = htons(53);
	remote.sin_addr.s_addr = htonl(0x08080808);
	if (connect(s, (const sockaddr*)&remote, sizeof(remote)) == 0)
	{
		sockaddr_in local = {};
#ifdef WIN32
		int size = sizeof(local);
#else
		socklen_t size = sizeof(local);
#endif
		if (getsockname(s, (sockaddr*)&local, &size) == 0 && local.sin_addr.s_addr != 0)
			result.Addr = (int32_t)ntohl(local.sin_addr.s_addr);
	}
	CloseSocket(s);
	return result;
}

bool UInternetLink::IsDataPending()
{
	return DataPending() != 0;
}

void UInternetLink::Resolve(const std::string& Domain)
{
	auto state = std::make_shared<ResolveState>();
	state->Status = 1;
	Resolving = state;

	std::thread([state, Domain]()
	{
		IpAddr addr = { 0, 0 };
		bool found = false;
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		addrinfo* info = nullptr;
		if (getaddrinfo(Domain.c_str(), nullptr, &hints, &info) == 0 && info)
		{
			addr.Addr = (int32_t)ntohl(((sockaddr_in*)info->ai_addr)->sin_addr.s_addr);
			found = true;
			freeaddrinfo(info);
		}
		std::unique_lock<std::mutex> lock(state->Mutex);
		state->Addr = addr;
		state->Status = found ? 2 : 3;
	}).detach();
}

bool UInternetLink::ParseURL(const std::string& URL, std::string& Addr, int& Port, std::string& LevelName, std::string& EntryName)
{
	UnrealURL url(URL);
	Addr = url.Host;
	Port = url.Port;
	LevelName = url.Map;
	EntryName = url.Portal;
	return true;
}

std::string UInternetLink::IpAddrToString(const IpAddr& Arg)
{
	uint32_t a = (uint32_t)Arg.Addr;
	return std::to_string((a >> 24) & 0xff) + "." + std::to_string((a >> 16) & 0xff) + "." +
		std::to_string((a >> 8) & 0xff) + "." + std::to_string(a & 0xff) + ":" + std::to_string(Arg.Port);
}

bool UInternetLink::StringToIpAddr(const std::string& Str, IpAddr& Addr)
{
	// The address alone: "a.b.c.d:port" is not one.
	in_addr_t a = inet_addr(Str.c_str());
	if (a == INADDR_NONE)
		return false;
	Addr.Addr = (int32_t)ntohl(a);
	Addr.Port = 0;
	return true;
}

std::string UInternetLink::Validate(const std::string& ValidationString, const std::string& GameName)
{
	// GameSpy's answer to a master server's challenge: the challenge's six
	// characters through RC4 -- GameSpy's variant, whose first index moves on
	// by each byte encrypted -- keyed by the game's secret, then in eight
	// characters of base 64. Only three games have a key; any other's is six
	// spaces.
	std::string key;
	if (GameName == "unreal")
		key = "DAncRK";
	else if (GameName == "ut")
		key = "Z5Nfb0";
	else if (GameName == "oldver")
		key = "EBodSL";
	else
		key = "      ";

	uint8_t state[256];
	for (int i = 0; i < 256; i++)
		state[i] = (uint8_t)i;
	uint8_t j = 0;
	for (int i = 0; i < 256; i++)
	{
		j = (uint8_t)(j + (uint8_t)key[i % key.size()] + state[i]);
		std::swap(state[i], state[j]);
	}

	uint8_t buf[6] = {};
	for (size_t i = 0; i < 6 && i < ValidationString.size(); i++)
		buf[i] = (uint8_t)ValidationString[i];

	uint8_t x = 0, y = 0;
	for (int i = 0; i < 6; i++)
	{
		x = (uint8_t)(x + buf[i] + 1);
		y = (uint8_t)(y + state[x]);
		std::swap(state[x], state[y]);
		buf[i] ^= state[(uint8_t)(state[x] + state[y])];
	}

	static const char* base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string result;
	for (int i = 0; i < 6; i += 3)
	{
		uint8_t a = buf[i], b = buf[i + 1], c = buf[i + 2];
		result += base64[a >> 2];
		result += base64[((a & 3) << 4) | (b >> 4)];
		result += base64[((b & 15) << 2) | (c >> 6)];
		result += base64[c & 63];
	}
	return result;
}

void UInternetLink::SetNonBlocking(socket_t s)
{
#ifdef WIN32
	u_long nonblocking = 1;
	ioctlsocket(s, FIONBIO, &nonblocking);
#else
	int nonblocking = 1;
	ioctl(s, FIONBIO, &nonblocking);
#endif
}

void UInternetLink::CloseSocket(socket_t s)
{
#ifdef WIN32
	closesocket(s);
#else
	close(s);
#endif
}

int UInternetLink::LastSocketError()
{
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

int UInternetLink::BindSocket(socket_t s, int port, bool bUseNextAvailable)
{
	int tries = bUseNextAvailable ? 20 : 1;
	for (int i = 0; i < tries; i++)
	{
		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons((uint16_t)(port + i));
		if (bind(s, (const sockaddr*)&addr, sizeof(addr)) == 0)
		{
#ifdef WIN32
			int size = sizeof(addr);
#else
			socklen_t size = sizeof(addr);
#endif
			if (getsockname(s, (sockaddr*)&addr, &size) != 0)
				return 0;
			return ntohs(addr.sin_port);
		}
		if (port == 0)
			break;
	}
	return 0;
}
