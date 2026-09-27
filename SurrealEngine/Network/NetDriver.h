#pragma once

#include "NetConnection.h"
#include <memory>
#include <string>

class NetChannel;
class UPlayerPawn;

// What a driver tells its owner: the pending level while joining, the level
// once in (Engine's FNetworkNotify).
class NetNotify
{
public:
	virtual ~NetNotify() = default;
	// A server's: whether a new connection is taken.
	virtual bool NotifyAcceptingConnection() { return false; }
	virtual bool NotifyAcceptingChannel(NetChannel* channel) = 0;
	virtual void NotifyReceivedText(NetConnection* connection, const std::string& text) = 0;
	// The server's pawn for this client has arrived (the original's
	// HandleClientPlayer on the connection).
	virtual void NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn) {}
	// A server's connection closed: its player leaves.
	virtual void NotifyConnectionClosed(NetConnection* connection) {}
};

// The UDP socket and its connections (IpDrv's TcpNetDriver over Engine's
// UNetDriver): packets in at the start of a tick, out at the end. As a
// client, one connection: the server's; as a server, one for each client
// that sends, taken as the level says.
class NetDriver
{
public:
	NetDriver();
	~NetDriver();

	bool InitConnect(NetNotify* notify, const std::string& host, int port, std::string& error);
	bool InitListen(NetNotify* notify, int port, std::string& error);
	void TickDispatch(float deltaTime);
	void TickFlush();

	void LowLevelSend(NetConnection* connection, const uint8_t* data, int count);

	// A server's actor is gone: each client's channel for it closes.
	void NotifyActorDestroyed(UActor* actor);

	NetNotify* Notify = nullptr;
	std::unique_ptr<NetConnection> ServerConnection;
	Array<std::unique_ptr<NetConnection>> ClientConnections;
	double Time = 0.0;

	// [IpDrv.TcpNetDriver]
	float ConnectionTimeout = 15.0f;
	float InitialConnectTimeout = 500.0f;
	float AckTimeout = 1.0f;
	float KeepAliveTime = 1.0f;
	float RelevantTimeout = 5.0f;
	float SpawnPrioritySeconds = 1.0f;
	int MaxClientRate = 20000;
	int DynamicUpdateRate = 40;
	int StaticUpdateRate = 12;

private:
	void LoadSettings();
	bool OpenSocket(int port, std::string& error);

	intptr_t Socket = -1;
};
