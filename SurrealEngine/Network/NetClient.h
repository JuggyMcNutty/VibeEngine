#pragma once

#include "NetDriver.h"
#include "UnrealURL.h"
#include "Package/NameString.h"
#include <memory>
#include <string>

// A join under way (Engine's UNetPendingLevel, dx-reverse-info/network.md): the
// connection to the server, the handshake up to WELCOME, and the packages
// this side lacks downloaded one after another. The engine then loads the
// map as a client, takes the driver over and sends JOIN.
class NetPendingLevel : public NetNotify
{
public:
	explicit NetPendingLevel(const UnrealURL& url);
	~NetPendingLevel();

	void Tick(float deltaTime);
	// The server told it is left: its control channel closed.
	void Close();
	// Whether what loads under each package's name is the server's: its GUID
	// the same (Core's check as the original's LoadMap verifies them).
	bool VerifyPackages(std::string& error);

	bool NotifyAcceptingChannel(NetChannel* channel) override;
	void NotifyReceivedText(NetConnection* connection, const std::string& text) override;
	void NotifyReceivedFile(NetConnection* connection, int packageIndex, const std::string& error) override;
	void NotifyProgress(const std::string& line1, const std::string& line2, float seconds) override;

	// A USES line: a package the server's level needs, in the server's order.
	struct UsedPackage
	{
		NameString Name;
		std::string Guid;
		uint32_t Flags = 0;
		int Size = 0;
		int Generation = 0;
	};

	UnrealURL URL;
	std::unique_ptr<NetDriver> Driver;
	Array<UsedPackage> Uses;
	std::string Error;
	bool Success = false;
	bool LonePlayer = false;
	// The downloads still to come (the packages flagged Need): the map
	// loads once there are none.
	int FilesNeeded = 0;

private:
	void ReceiveNextFile(NetConnection* connection);
};

// A client's level: what its connection to the server may open, what the
// server says after the join, and the player the server gives it.
class NetClientLevel : public NetNotify
{
public:
	bool NotifyAcceptingChannel(NetChannel* channel) override;
	void NotifyReceivedText(NetConnection* connection, const std::string& text) override;
	void NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn) override;
};

// The engine's answer to a server's challenge.
int NetChallengeResponse(int challenge);

// The handshake's text: a leading command word, a KEY=value anywhere.
bool NetParseCommand(const std::string& text, const char* command, std::string* rest = nullptr);
bool NetParseValue(const std::string& text, const char* key, std::string& value);
