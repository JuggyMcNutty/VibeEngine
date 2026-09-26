
#include "Precomp.h"
#include "NetDriver.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
#include "Utils/Logger.h"
#include "Engine.h"

#ifdef WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace
{
#ifdef WIN32
	constexpr intptr_t NoSocket = (intptr_t)INVALID_SOCKET;
	bool WouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
	bool ConnectionReset() { return WSAGetLastError() == WSAECONNRESET; }
	void CloseSocket(intptr_t s) { closesocket((SOCKET)s); }
	void SetNonBlocking(intptr_t s) { u_long nonblocking = 1; ioctlsocket((SOCKET)s, FIONBIO, &nonblocking); }
#else
	constexpr intptr_t NoSocket = -1;
	bool WouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
	bool ConnectionReset() { return errno == ECONNREFUSED || errno == ECONNRESET; }
	void CloseSocket(intptr_t s) { close((int)s); }
	void SetNonBlocking(intptr_t s) { fcntl((int)s, F_SETFL, fcntl((int)s, F_GETFL, 0) | O_NONBLOCK); }
#endif

	float IniFloat(const char* key, float defaultValue)
	{
		std::string value = engine->packages->GetIniValue("system", "IpDrv.TcpNetDriver", key);
		return value.empty() ? defaultValue : (float)std::atof(value.c_str());
	}

	int IniInt(const char* section, const char* key, int defaultValue)
	{
		std::string value = engine->packages->GetIniValue("system", section, key);
		return value.empty() ? defaultValue : std::atoi(value.c_str());
	}
}

NetDriver::NetDriver()
{
	LoadSettings();
}

NetDriver::~NetDriver()
{
	ServerConnection.reset();
	if (Socket != NoSocket)
		CloseSocket(Socket);
}

void NetDriver::LoadSettings()
{
	ConnectionTimeout = IniFloat("ConnectionTimeout", ConnectionTimeout);
	InitialConnectTimeout = IniFloat("InitialConnectTimeout", InitialConnectTimeout);
	AckTimeout = IniFloat("AckTimeout", AckTimeout);
	KeepAliveTime = IniFloat("KeepAliveTime", KeepAliveTime);
	MaxClientRate = IniInt("IpDrv.TcpNetDriver", "MaxClientRate", MaxClientRate);
}

bool NetDriver::InitConnect(NetNotify* notify, const std::string& host, int port, std::string& error)
{
	Notify = notify;

	// The server's address: a dotted number as it is, a name looked up.
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	addrinfo* info = nullptr;
	if (getaddrinfo(host.c_str(), nullptr, &hints, &info) != 0 || !info)
	{
		error = "Could not resolve " + host;
		return false;
	}
	uint32_t addr = ntohl(((sockaddr_in*)info->ai_addr)->sin_addr.s_addr);
	freeaddrinfo(info);

	Socket = (intptr_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (Socket == NoSocket)
	{
		error = "Could not create a socket";
		return false;
	}
	SetNonBlocking(Socket);

	sockaddr_in local = {};
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = 0;
	if (bind((decltype(socket(0, 0, 0)))Socket, (sockaddr*)&local, sizeof(local)) != 0)
	{
		error = "Could not bind a socket";
		return false;
	}

	// The speed the client asks for: [Engine.Player]'s for the internet,
	// its LAN one with ?LAN in the URL (the caller's to choose).
	int netSpeed = IniInt("Engine.Player", "ConfiguredInternetSpeed", 2600);
	ServerConnection = std::make_unique<NetConnection>(this, addr, port, netSpeed);
	ServerConnection->CreateChannel(ChannelType::Control, true, 0);
	return true;
}

void NetDriver::TickDispatch(float deltaTime)
{
	Time += deltaTime;

	if (Socket == NoSocket)
		return;

	uint8_t data[4096];
	for (;;)
	{
		sockaddr_in from = {};
		socklen_t fromSize = sizeof(from);
		int size = (int)recvfrom((decltype(socket(0, 0, 0)))Socket, (char*)data, sizeof(data), 0, (sockaddr*)&from, &fromSize);
		if (size < 0)
		{
			if (WouldBlock())
				break;
			if (ConnectionReset())
				continue;
			LogMessage("Net: receive failed");
			break;
		}

		NetConnection* connection = ServerConnection.get();
		if (connection && ntohl(from.sin_addr.s_addr) == connection->RemoteAddr && ntohs(from.sin_port) == connection->RemotePort)
			connection->ReceivedRawPacket(data, size);
	}
}

void NetDriver::TickFlush()
{
	if (ServerConnection)
		ServerConnection->Tick();
}

void NetDriver::LowLevelSend(NetConnection* connection, const uint8_t* data, int count)
{
	if (Socket == NoSocket)
		return;
	sockaddr_in to = {};
	to.sin_family = AF_INET;
	to.sin_addr.s_addr = htonl(connection->RemoteAddr);
	to.sin_port = htons((uint16_t)connection->RemotePort);
	sendto((decltype(socket(0, 0, 0)))Socket, (const char*)data, count, 0, (sockaddr*)&to, sizeof(to));
}
