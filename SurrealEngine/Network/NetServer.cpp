
#include "Precomp.h"
#include "NetServer.h"
#include "NetChannel.h"
#include "NetClient.h"
#include "Package/Package.h"
#include "Package/PackageManager.h"
#include "Packages/Core/UClass.h"
#include "Packages/Core/Properties/UProperty.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Packages/Engine/Actors/Info/UGameInfo.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "Engine.h"
#include <algorithm>
#include <chrono>

namespace
{
	std::map<UObject*, NetConnection*> PlayerConnections;

	std::string AddressString(const NetConnection* connection)
	{
		uint32_t a = connection->RemoteAddr;
		return std::to_string(a >> 24) + "." + std::to_string((a >> 16) & 255) + "." + std::to_string((a >> 8) & 255) + "." + std::to_string(a & 255) + ":" + std::to_string(connection->RemotePort);
	}

	void Refuse(NetConnection* connection, const std::string& text)
	{
		connection->SendText(text);
		connection->FlushNet();
		connection->State = ConnectionState::Closed;
	}
}

NetConnection* NetConnectionOfPlayer(UObject* player)
{
	auto it = PlayerConnections.find(player);
	return it != PlayerConnections.end() ? it->second : nullptr;
}

NetServerLevel::NetServerLevel(Package* level) : Level(level)
{
	AddPackage(level);
}

NetServerLevel::~NetServerLevel()
{
	PlayerConnections.clear();
}

void NetServerLevel::AddPackage(Package* package)
{
	for (const NetPackageMap::PackageInfo& info : Packages)
	{
		if (info.Pkg == package)
			return;
	}
	if (((uint32_t)package->GetFlags() & (uint32_t)PackageFlags::ServerSideOnly) != 0)
		return;

	NetPackageMap::PackageInfo info;
	info.Pkg = package;
	info.Name = package->GetPackageName();
	info.Guid = package->GetGuidString();
	info.Flags = (uint32_t)package->GetFlags();
	std::error_code ec;
	info.FileSize = (int)fs::file_size(package->GetPackageFilePath(), ec);
	info.LocalGeneration = (int)package->GetGenerations().size();
	Packages.push_back(info);

	for (const NameString& name : package->GetImportedPackages())
	{
		if (engine->packages->HasPackage(name))
			AddPackage(engine->packages->GetPackage(name));
	}
}

bool NetServerLevel::NotifyAcceptingConnection()
{
	return true;
}

bool NetServerLevel::NotifyAcceptingChannel(NetChannel* channel)
{
	// A client opens the control channel; downloads the fork does not serve.
	return channel->ChType == ChannelType::Control;
}

void NetServerLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	LogMessage("Net: level server received: " + text);

	std::string rest, value;
	if (NetParseCommand(text, "HELLO"))
	{
		// A version missing counts as 219's, the first networked engine's.
		int remoteMinVer = 219, remoteVer = 219;
		if (NetParseValue(text, "MINVER=", value))
			remoteMinVer = std::atoi(value.c_str());
		if (NetParseValue(text, "VER=", value))
			remoteVer = std::atoi(value.c_str());
		if (remoteVer < 1100 || remoteMinVer > 1100)
		{
			Refuse(connection, "UPGRADE MINVER=1100 VER=1100");
			return;
		}
		connection->NegotiatedVer = std::min(remoteVer, 1100);
		connection->Challenge = (int)(uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
		bool stats = false;
		if (UObject* game = engine->LevelInfo->Game())
		{
			for (UProperty* prop : game->PropertyData.Class->Properties)
			{
				if (prop->Name == "bWorldLog")
					stats = game->GetBool(prop->Name);
			}
		}
		connection->SendText("CHALLENGE VER=" + std::to_string(connection->NegotiatedVer) + " CHALLENGE=" + std::to_string(connection->Challenge) + " STATS=" + std::to_string(stats ? 1 : 0));
		connection->FlushNet();
	}
	else if (NetParseCommand(text, "NETSPEED", &rest))
	{
		int rate = std::atoi(rest.c_str());
		if (rate >= 500)
			connection->CurrentNetSpeed = std::clamp(rate, 500, connection->Driver->MaxClientRate);
		LogMessage("Net: client netspeed is " + std::to_string(connection->CurrentNetSpeed));
	}
	else if (NetParseCommand(text, "HAVE"))
	{
		// The client's generation of a package: both sides count only what
		// it has.
		std::string guid;
		if (NetParseValue(text, "GUID=", guid) && NetParseValue(text, "GEN=", value))
		{
			for (NetPackageMap::PackageInfo& info : connection->PackageMap.List)
			{
				if (StrTools::equals_ignore_case(info.Guid, guid))
					info.RemoteGeneration = std::clamp(std::atoi(value.c_str()), 1, std::max(info.LocalGeneration, 1));
			}
		}
	}
	else if (NetParseCommand(text, "LOGIN"))
	{
		std::string response, url;
		NetParseValue(text, "RESPONSE=", response);
		NetParseValue(text, "URL=", url);
		if (std::atoi(response.c_str()) != NetChallengeResponse(connection->Challenge))
		{
			Refuse(connection, "FAILURE CHALLENGE");
			return;
		}
		LogMessage("Net: login request: " + url);
		RequestURLs[connection] = UnrealURL(url);

		// The game may refuse: its message, and a code the client's menu
		// acts on.
		std::string error, failcode;
		if (!engine->PreLogin(RequestURLs[connection].GetOptions(), AddressString(connection), error, failcode))
		{
			connection->SendText("FAILURE " + error);
			if (!failcode.empty())
				connection->SendText("FAILCODE " + failcode);
			connection->FlushNet();
			connection->State = ConnectionState::Closed;
			return;
		}
		Welcome(connection);
	}
	else if (NetParseCommand(text, "JOIN"))
	{
		if (!connection->Actor)
			Join(connection);
	}
	else if (NetParseCommand(text, "USERFLAG", &rest))
	{
		connection->UserFlags = std::atoi(rest.c_str());
	}
}

void NetServerLevel::Welcome(NetConnection* connection)
{
	connection->PackageMap.List = Packages;
	for (NetPackageMap::PackageInfo& info : connection->PackageMap.List)
	{
		info.RemoteGeneration = info.LocalGeneration;
		connection->SendText("USES GUID=" + info.Guid + " PKG=" + info.Name.ToString() + " FLAGS=" + std::to_string(info.Flags) + " SIZE=" + std::to_string(info.FileSize) + " GEN=" + std::to_string(info.LocalGeneration));
	}
	connection->SendText("WELCOME LEVEL=" + Level->GetPackageName().ToString() + " LONE=" + std::to_string(engine->LevelInfo->bLonePlayer() ? 1 : 0));
	connection->SendText("DYNAMICRATE " + std::to_string(connection->Driver->DynamicUpdateRate));
	connection->SendText("STATICRATE " + std::to_string(connection->Driver->StaticUpdateRate));
	connection->FlushNet();
}

void NetServerLevel::Join(NetConnection* connection)
{
	connection->PackageMap.Compute();

	LogMessage("Net: join request: " + RequestURLs[connection].ToString());
	std::string error;
	UObject* player = engine->packages->GetTransientPackage()->NewObject("NetConnection" + std::to_string(PlayerConnections.size()), engine->packages->FindClass("Engine.NetConnection"), ObjectFlags::Transient);
	PlayerConnections[player] = connection;
	connection->PlayerObject = player;

	UPlayerPawn* pawn = engine->SpawnPlayActor(player, ROLE_AutonomousProxy, RequestURLs[connection], error);
	if (!pawn)
	{
		PlayerConnections.erase(player);
		connection->PlayerObject = nullptr;
		Refuse(connection, "FAILURE " + (error.empty() ? std::string("Could not spawn the player") : error));
		return;
	}
	connection->Actor = pawn;
	LogMessage("Net: join succeeded: " + RequestURLs[connection].GetOption("Name"));
}

void NetServerLevel::NotifyConnectionClosed(NetConnection* connection)
{
	LogMessage("Net: close " + AddressString(connection));
	RequestURLs.erase(connection);
	if (connection->PlayerObject)
		PlayerConnections.erase(connection->PlayerObject);

	// Its player goes with it.
	if (UPlayerPawn* pawn = connection->Actor)
	{
		connection->Actor = nullptr;
		pawn->Player() = nullptr;
		pawn->Destroy();
	}
}
