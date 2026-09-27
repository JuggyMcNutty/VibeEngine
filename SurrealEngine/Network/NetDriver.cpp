
#include "Precomp.h"
#include "NetDriver.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
#include "Packages/Engine/Actors/UActor.h"
#include "Utils/Logger.h"
#include "Engine.h"
#include <algorithm>

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
	ClientConnections.clear();
	if (Socket != NoSocket)
		CloseSocket(Socket);
}

void NetDriver::LoadSettings()
{
	ConnectionTimeout = IniFloat("ConnectionTimeout", ConnectionTimeout);
	InitialConnectTimeout = IniFloat("InitialConnectTimeout", InitialConnectTimeout);
	AckTimeout = IniFloat("AckTimeout", AckTimeout);
	KeepAliveTime = IniFloat("KeepAliveTime", KeepAliveTime);
	RelevantTimeout = IniFloat("RelevantTimeout", RelevantTimeout);
	SpawnPrioritySeconds = IniFloat("SpawnPrioritySeconds", SpawnPrioritySeconds);
	ServerTravelPause = IniFloat("ServerTravelPause", ServerTravelPause);
	MaxClientRate = IniInt("IpDrv.TcpNetDriver", "MaxClientRate", MaxClientRate);
	DynamicUpdateRate = IniInt("IpDrv.TcpNetDriver", "DynamicUpdateRate", DynamicUpdateRate);
	StaticUpdateRate = IniInt("IpDrv.TcpNetDriver", "StaticUpdateRate", StaticUpdateRate);
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

	if (!OpenSocket(0, error))
		return false;

	// The speed the client asks for: [Engine.Player]'s for the internet,
	// its LAN one with ?LAN in the URL (the caller's to choose).
	int netSpeed = IniInt("Engine.Player", "ConfiguredInternetSpeed", 2600);
	ServerConnection = std::make_unique<NetConnection>(this, addr, port, netSpeed);
	ServerConnection->CreateChannel(ChannelType::Control, true, 0);
	return true;
}

bool NetDriver::InitListen(NetNotify* notify, int port, std::string& error)
{
	Notify = notify;
	return OpenSocket(port, error);
}

bool NetDriver::OpenSocket(int port, std::string& error)
{
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
	local.sin_port = htons((uint16_t)port);
	if (bind((decltype(socket(0, 0, 0)))Socket, (sockaddr*)&local, sizeof(local)) != 0)
	{
		error = "Could not bind port " + std::to_string(port);
		return false;
	}
	return true;
}

void NetDriver::TickDispatch(float deltaTime)
{
	Time += deltaTime;

	// A server lets go of the connections that closed, and their players.
	for (size_t i = ClientConnections.size(); i-- > 0;)
	{
		if (ClientConnections[i]->State == ConnectionState::Closed)
		{
			std::unique_ptr<NetConnection> connection = std::move(ClientConnections[i]);
			ClientConnections.erase(ClientConnections.begin() + i);
			if (Notify)
				Notify->NotifyConnectionClosed(connection.get());
		}
	}

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

		uint32_t addr = ntohl(from.sin_addr.s_addr);
		int port = ntohs(from.sin_port);
		NetConnection* connection = nullptr;
		if (ServerConnection && addr == ServerConnection->RemoteAddr && port == ServerConnection->RemotePort)
			connection = ServerConnection.get();
		for (size_t i = 0; !connection && i < ClientConnections.size(); i++)
		{
			if (addr == ClientConnections[i]->RemoteAddr && port == ClientConnections[i]->RemotePort)
				connection = ClientConnections[i].get();
		}

		// A new client, when the level takes it; its connection is open
		// from the first packet.
		if (!connection && !ServerConnection && Notify && Notify->NotifyAcceptingConnection())
		{
			ClientConnections.push_back(std::make_unique<NetConnection>(this, addr, port, IniInt("Engine.Player", "ConfiguredInternetSpeed", 2600)));
			connection = ClientConnections.back().get();
			connection->State = ConnectionState::Open;
			LogMessage("Net: open " + std::to_string(addr >> 24) + "." + std::to_string((addr >> 16) & 255) + "." + std::to_string((addr >> 8) & 255) + "." + std::to_string(addr & 255) + ":" + std::to_string(port));
		}

		if (connection)
			connection->ReceivedRawPacket(data, size);
	}
}

void NetDriver::TickFlush()
{
	if (ServerConnection)
		ServerConnection->Tick();
	for (auto& connection : ClientConnections)
		connection->Tick();
}

void NetDriver::NotifyActorDestroyed(UActor* actor)
{
	for (size_t i = ClientConnections.size(); i-- > 0;)
	{
		NetConnection* connection = ClientConnections[i].get();
		if (actor->bNetTemporary())
		{
			auto& sent = connection->SentTemporaries;
			sent.erase(std::remove(sent.begin(), sent.end(), actor), sent.end());
		}
		if (NetActorChannel* channel = connection->FindActorChannel(actor))
			channel->Close();
	}
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
