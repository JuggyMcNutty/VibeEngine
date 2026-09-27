#pragma once

#include "NetConnection.h"
#include <initializer_list>
#include <memory>
#include <string>

class NetChannel;
class UPlayerPawn;

// A message from a package's .int file ([section] key, else the fallback
// given), its %s, %i and %f filled in turn (Core's Localize and appSprintf).
struct LocalizeArg
{
	LocalizeArg(const std::string& text) : Text(text), IsText(true) {}
	LocalizeArg(const char* text) : Text(text), IsText(true) {}
	LocalizeArg(int number) : Number(number) {}
	LocalizeArg(double number) : Number(number) {}
	std::string Text;
	double Number = 0.0;
	bool IsText = false;
};
std::string LocalizeMessage(const char* package, const char* section, const char* key, const char* fallback, std::initializer_list<LocalizeArg> args = {});

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
	// Whether the other side may have the package it asks for by its GUID:
	// a server's clients may, a server may not.
	virtual bool NotifySendingFile(NetConnection* connection, const std::string& guid) { return false; }
	// A client's: a package's download ended, with its error or none.
	virtual void NotifyReceivedFile(NetConnection* connection, int packageIndex, const std::string& error) {}
	// Two lines for the player, shown for the seconds given (the engine's
	// SetProgress).
	virtual void NotifyProgress(const std::string& line1, const std::string& line2, float seconds) {}
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
	float ServerTravelPause = 4.0f;
	int MaxClientRate = 20000;
	int DynamicUpdateRate = 40;
	int StaticUpdateRate = 12;
	// A dedicated server's ticks a second, on the internet and on a LAN.
	int NetServerMaxTickRate = 20;
	int LanServerMaxTickRate = 35;
	// A client's: whether it fetches a package it lacks from the server.
	bool AllowDownloads = true;

private:
	void LoadSettings();
	bool OpenSocket(int port, std::string& error);

	intptr_t Socket = -1;
};
