
#include "Precomp.h"
#include "UTcpLink.h"
#include "VM/ScriptCall.h"
#include "VM/Frame.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UFunction.h"
#include "Engine.h"

#ifdef WIN32
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#define SHUT_WR SD_SEND
static bool WouldBlock(int error) { return error == WSAEWOULDBLOCK; }
static bool InProgress(int error) { return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS; }
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
static bool WouldBlock(int error) { return error == EWOULDBLOCK || error == EAGAIN; }
static bool InProgress(int error) { return error == EINPROGRESS || error == EWOULDBLOCK || error == EAGAIN; }
#endif

static bool Readable(socket_t s)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);
	timeval timeout = {};
	return select((int)s + 1, &set, nullptr, nullptr, &timeout) > 0;
}

UTcpLink::~UTcpLink()
{
	if (remote != invalid_socket_value)
		CloseSocket(remote);
	if (handle != invalid_socket_value)
		CloseSocket(handle);
}

bool UTcpLink::EnsureSocket()
{
	if (handle != invalid_socket_value)
		return true;
	handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (handle == invalid_socket_value)
	{
		LastError = LastSocketError();
		return false;
	}
	int on = 1;
	setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
	SetNonBlocking(handle);
	Socket() = (int)handle;
	return true;
}

void UTcpLink::Tick(float elapsed)
{
	UInternetLink::Tick(elapsed);

	if (handle == invalid_socket_value && remote == invalid_socket_value)
		return;

	switch (LinkState())
	{
	case STATE_Listening:
		CheckConnectionQueue();
		PollConnections();
		FlushSendBuffer();
		break;
	case STATE_Connecting:
		CheckConnectionAttempt();
		PollConnections();
		break;
	case STATE_Connected:
		PollConnections();
		FlushSendBuffer();
		break;
	case STATE_ListenClosePending:
	case STATE_ConnectClosePending:
		PollConnections();
		if (FlushSendBuffer() == 0)
			ShutdownConnection();
		break;
	default:
		break;
	}

	// The other end going away.
	switch (LinkState())
	{
	case STATE_Listening:
	case STATE_ListenClosePending:
	case STATE_ListenClosing:
		CheckClosed(remote);
		break;
	case STATE_Connected:
	case STATE_ConnectClosePending:
	case STATE_ConnectClosing:
		CheckClosed(handle);
		break;
	default:
		break;
	}
}

void UTcpLink::CheckClosed(socket_t& s)
{
	if (s == invalid_socket_value || !Readable(s))
		return;
	char peek;
	int result = (int)recv(s, &peek, 1, MSG_PEEK);
	if (result > 0 || (result < 0 && WouldBlock(LastSocketError())))
		return;

	if (LinkState() != STATE_Listening)
		LinkState() = STATE_Initialized;
	CloseSocket(s);
	s = invalid_socket_value;
	Socket() = handle == invalid_socket_value ? -1 : (int)handle;
	RemoteSocket() = remote == invalid_socket_value ? -1 : (int)remote;
	CallEvent(this, "Closed");
}

void UTcpLink::CheckConnectionQueue()
{
	if (!Readable(handle))
		return;
	sockaddr_in peer = {};
#ifdef WIN32
	int size = sizeof(peer);
#else
	socklen_t size = sizeof(peer);
#endif
	socket_t s = accept(handle, (sockaddr*)&peer, &size);
	if (s == invalid_socket_value)
		return;
	SetNonBlocking(s);
	IpAddr addr = { (int32_t)ntohl(peer.sin_addr.s_addr), ntohs(peer.sin_port) };

	if (AcceptClass())
	{
		// A new link of the accept class takes the connection, in the
		// listener's link mode.
		UTcpLink* link = UObject::TryCast<UTcpLink>(Spawn(AcceptClass(), this, {}, Location(), Rotation()));
		if (!link)
		{
			CloseSocket(s);
			return;
		}
		link->handle = s;
		link->Socket() = (int)s;
		link->LinkState() = STATE_Connected;
		link->LinkMode() = LinkMode();
		link->RemoteAddr() = addr;
		CallEvent(link, "Accepted");
	}
	else if (remote == invalid_socket_value)
	{
		remote = s;
		RemoteSocket() = (int)s;
		RemoteAddr() = addr;
		CallEvent(this, "Accepted");
	}
	else
	{
		CloseSocket(s);
	}
}

void UTcpLink::CheckConnectionAttempt()
{
	fd_set writable;
	FD_ZERO(&writable);
	FD_SET(handle, &writable);
	timeval timeout = {};
	if (select((int)handle + 1, nullptr, &writable, nullptr, &timeout) <= 0)
		return;

	// A connection that failed stays waiting, as on the original's Windows,
	// where only a made one turns writable; the script's own timer gives up.
	int error = 0;
#ifdef WIN32
	int size = sizeof(error);
#else
	socklen_t size = sizeof(error);
#endif
	if (getsockopt(handle, SOL_SOCKET, SO_ERROR, (char*)&error, &size) != 0 || error != 0)
	{
		LastError = error;
		return;
	}

	LinkState() = STATE_Connected;
	CallEvent(this, "Opened");
}

void UTcpLink::PollConnections()
{
	socket_t s = DataSocket();
	if (s == invalid_socket_value)
		return;

	if (ReceiveMode() == RMODE_Manual)
	{
		DataPending() = Readable(s) ? 1 : 0;
		return;
	}

	// One read a tick, of up to 999 bytes, as the original's.
	char data[1000];
	int count = (int)recv(s, data, 999, 0);
	if (count < 0)
		return;
	data[count] = 0;

	if (LinkMode() == MODE_Text)
	{
		CallEvent(this, "ReceivedText", { ExpressionValue::StringValue(std::string(data)) });
	}
	else if (LinkMode() == MODE_Line)
	{
		CallEvent(this, "ReceivedLine", { ExpressionValue::StringValue(std::string(data)) });
	}
	else if (LinkMode() == MODE_Binary && count > 0)
	{
		UFunction* func = FindEventFunction(this, "ReceivedBinary");
		if (func && func->Properties.size() >= 2)
		{
			for (int start = 0; start < count; start += 255)
			{
				uint8_t bytes[255] = {};
				int n = std::min(count - start, 255);
				memcpy(bytes, data + start, n);
				CallEvent(this, "ReceivedBinary", { ExpressionValue::IntValue(n), ExpressionValue::Variable(bytes, func->Properties[1]) });
			}
		}
	}
}

int UTcpLink::FlushSendBuffer()
{
	socket_t s = DataSocket();
	if (s == invalid_socket_value || sendBuffer.empty())
		return (int)sendBuffer.size();
	int sent = (int)send(s, (const char*)sendBuffer.data(), (int)sendBuffer.size(), 0);
	if (sent > 0)
		sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + sent);
	else if (sent < 0 && !WouldBlock(LastSocketError()))
		LastError = LastSocketError();
	return (int)sendBuffer.size();
}

void UTcpLink::ShutdownConnection()
{
	socket_t s = DataSocket();
	if (s != invalid_socket_value)
		shutdown(s, SHUT_WR);
	LinkState() = (LinkState() == STATE_ListenClosePending) ? STATE_ListenClosing : STATE_ConnectClosing;
}

int UTcpLink::BindPort(int port, bool bUseNextAvailable)
{
	if (!EnsureSocket())
		return 0;
	int bound = BindSocket(handle, port, bUseNextAvailable);
	if (bound == 0)
	{
		LastError = LastSocketError();
		return 0;
	}
	Port() = bound;
	LinkState() = STATE_Ready;
	return bound;
}

bool UTcpLink::Listen()
{
	if (handle == invalid_socket_value || LinkState() == STATE_Listening)
		return false;
	if (listen(handle, SOMAXCONN) != 0)
	{
		LastError = LastSocketError();
		return false;
	}
	LinkState() = STATE_Listening;
	return true;
}

bool UTcpLink::Open(const IpAddr& Addr)
{
	if (!EnsureSocket())
		return false;
	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl((uint32_t)Addr.Addr);
	addr.sin_port = htons((uint16_t)Addr.Port);
	if (connect(handle, (const sockaddr*)&addr, sizeof(addr)) != 0 && !InProgress(LastSocketError()))
	{
		LastError = LastSocketError();
		return false;
	}
	LinkState() = STATE_Connecting;
	return true;
}

bool UTcpLink::Close()
{
	switch (LinkState())
	{
	case STATE_Listening:
		LinkState() = STATE_ListenClosePending;
		return true;
	case STATE_Connected:
		LinkState() = STATE_ConnectClosePending;
		return true;
	case STATE_Initialized:
	case STATE_Ready:
	case STATE_Connecting:
		if (remote != invalid_socket_value)
			CloseSocket(remote);
		if (handle != invalid_socket_value)
			CloseSocket(handle);
		remote = invalid_socket_value;
		handle = invalid_socket_value;
		Socket() = -1;
		RemoteSocket() = -1;
		sendBuffer.clear();
		LinkState() = STATE_Initialized;
		return true;
	default:
		return false;
	}
}

bool UTcpLink::IsConnected()
{
	return LinkState() == STATE_Connected || (LinkState() == STATE_Listening && remote != invalid_socket_value);
}

int UTcpLink::ReadBinary(int Count, uint8_t* B)
{
	socket_t s = DataSocket();
	if (s == invalid_socket_value)
		return 0;
	int count = (int)recv(s, (char*)B, std::clamp(Count, 0, 255), 0);
	return count > 0 ? count : 0;
}

int UTcpLink::SendBinary(int Count, const uint8_t* B)
{
	Count = std::clamp(Count, 0, 255);
	sendBuffer.insert(sendBuffer.end(), B, B + Count);
	FlushSendBuffer();
	return Count;
}

int UTcpLink::ReadText(std::string& Str)
{
	socket_t s = DataSocket();
	if (s == invalid_socket_value)
		return 0;
	char data[1000];
	int count = (int)recv(s, data, 999, 0);
	if (count <= 0)
		return 0;
	data[count] = 0;
	Str = data;
	return count;
}

int UTcpLink::SendText(const std::string& Str)
{
	// Queued whole and sent as the connection takes it; the script is told
	// all of it went.
	sendBuffer.insert(sendBuffer.end(), Str.begin(), Str.end());
	if (LinkMode() == MODE_Line)
	{
		sendBuffer.push_back('\r');
		sendBuffer.push_back('\n');
	}
	FlushSendBuffer();
	return (int)Str.size();
}
