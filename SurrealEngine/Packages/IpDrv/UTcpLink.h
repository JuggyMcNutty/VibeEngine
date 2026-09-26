#pragma once

#include "UInternetLink.h"

enum ETcpLinkState
{
	STATE_Initialized,
	STATE_Ready,
	STATE_Listening,
	STATE_Connecting,
	STATE_Connected,
	STATE_ListenClosePending,
	STATE_ConnectClosePending,
	STATE_ListenClosing,
	STATE_ConnectClosing
};

class UTcpLink : public UInternetLink
{
public:
	using UInternetLink::UInternetLink;
	~UTcpLink();

	void Tick(float elapsed) override;

	int BindPort(int Port, bool bUseNextAvailable);
	bool Listen();

	bool Open(const IpAddr& Addr);
	bool Close();

	bool IsConnected();

	int ReadBinary(int Count, uint8_t* B);
	int SendBinary(int Count, const uint8_t* B);

	int ReadText(std::string& Str);
	int SendText(const std::string& Str);

	UClass*& AcceptClass() { return Value<UClass*>(PropOffsets_TcpLink.AcceptClass); }
	uint8_t& LinkState() { return Value<uint8_t>(PropOffsets_TcpLink.LinkState); }
	IpAddr& RemoteAddr() { return Value<IpAddr>(PropOffsets_TcpLink.RemoteAddr); }
	TypedScriptArray<void*> SendFIFO() { return DynamicArray<void*>(PropOffsets_TcpLink.SendFIFO); }

private:
	bool EnsureSocket();
	// A link's data goes over the connection it accepted into itself, else
	// its own socket.
	socket_t DataSocket() const { return remote != invalid_socket_value ? remote : handle; }

	void CheckConnectionQueue();
	void CheckConnectionAttempt();
	void PollConnections();
	int FlushSendBuffer(); // what is left to send
	void ShutdownConnection();
	void CheckClosed(socket_t& s);

	socket_t handle = invalid_socket_value; // Socket
	socket_t remote = invalid_socket_value; // RemoteSocket
	std::vector<uint8_t> sendBuffer;
};
