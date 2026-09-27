
#include "Precomp.h"
#include "NetClient.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
#include "Package/PackageFlags.h"
#include "Utils/Logger.h"
#include "Utils/StrTools.h"
#include "Packages/Engine/UConsole.h"
#include "VM/ScriptCall.h"
#include "Engine.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace
{
	bool EqualsIgnoreCase(const std::string& a, size_t pos, const char* b)
	{
		for (size_t i = 0; b[i]; i++)
		{
			if (pos + i >= a.size() || std::toupper((unsigned char)a[pos + i]) != std::toupper((unsigned char)b[i]))
				return false;
		}
		return true;
	}

	// A rate a second as the time between updates, 0.01 s to 1 s; none for 0.
	float RateInterval(int rate)
	{
		return rate != 0 ? std::clamp(1.0f / (float)rate, 0.01f, 1.0f) : 0.0f;
	}

}

int NetChallengeResponse(int challenge)
{
	uint32_t c = (uint32_t)challenge;
	return (int)((c * 237u) ^ (uint32_t)(challenge >> 16) ^ (c << 16) ^ 0x93fe92ceu);
}

bool NetParseCommand(const std::string& text, const char* command, std::string* rest)
{
	size_t length = strlen(command);
	if (!EqualsIgnoreCase(text, 0, command) || (text.size() > length && text[length] != ' '))
		return false;
	if (rest)
		*rest = text.size() > length ? text.substr(length + 1) : std::string();
	return true;
}

bool NetParseValue(const std::string& text, const char* key, std::string& value)
{
	for (size_t pos = 0; pos < text.size(); pos++)
	{
		if (!EqualsIgnoreCase(text, pos, key))
			continue;
		size_t start = pos + strlen(key);
		if (start < text.size() && text[start] == '"')
		{
			size_t end = text.find('"', start + 1);
			value = text.substr(start + 1, end == std::string::npos ? std::string::npos : end - start - 1);
		}
		else
		{
			size_t end = text.find_first_of(" ,)\r\n", start);
			value = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
		}
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

NetPendingLevel::NetPendingLevel(const UnrealURL& url) : URL(url)
{
	Driver = std::make_unique<NetDriver>();
	if (!Driver->InitConnect(this, URL.Host, URL.Port, Error))
		return;

	NetConnection* connection = Driver->ServerConnection.get();
	if (URL.HasOption("LAN"))
	{
		std::string lanSpeed = engine->packages->GetIniValue("system", "Engine.Player", "ConfiguredLanSpeed");
		if (!lanSpeed.empty())
			connection->CurrentNetSpeed = std::atoi(lanSpeed.c_str());
	}

	connection->SendText("HELLO REVISION=0 MINVER=1100 VER=1100");
	connection->FlushNet();
}

NetPendingLevel::~NetPendingLevel()
{
	// Its connection goes with it, telling it nothing more.
	if (Driver)
		Driver->Notify = nullptr;
	Driver.reset();
}

void NetPendingLevel::Close()
{
	NetConnection* connection = Driver ? Driver->ServerConnection.get() : nullptr;
	if (!connection)
		return;
	if (NetChannel* control = connection->Channels[0])
		control->Close();
	connection->FlushNet();
}

bool NetPendingLevel::VerifyPackages(std::string& error)
{
	for (const UsedPackage& package : Uses)
	{
		std::string guid;
		if (engine->packages->IsPackageLoaded(package.Name))
			guid = engine->packages->GetPackage(package.Name)->GetGuidString();
		else
			guid = PackageManager::ReadPackageGuid(engine->packages->FindPackageFile(package.Name, package.Guid));
		if (!StrTools::equals_ignore_case(guid, package.Guid))
		{
			error = LocalizeMessage("Core", "Errors", "PackageVersion", "Package '%s' version mismatch", { package.Name.ToString() });
			return false;
		}
	}
	return true;
}

void NetPendingLevel::Tick(float deltaTime)
{
	if (!Driver || !Error.empty())
		return;

	NetConnection* connection = Driver->ServerConnection.get();
	if (connection->State == ConnectionState::Closed)
	{
		Error = "Connection failed";
		return;
	}

	Driver->TickDispatch(deltaTime);
	Driver->TickFlush();
}

bool NetPendingLevel::NotifyAcceptingChannel(NetChannel* channel)
{
	return channel->ChType == ChannelType::Control;
}

void NetPendingLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	LogMessage("Net: pending level received: " + text);

	std::string rest, value;
	if (NetParseCommand(text, "UPGRADE"))
	{
		// A server too old for this side, or this side too old for it (the
		// game's upgrade menu).
		int minVer = 0;
		if (NetParseValue(text, "MINVER=", value))
			minVer = std::atoi(value.c_str());
		if (minVer <= 1100)
			Error = LocalizeMessage("Engine", "Errors", "ServerOutdated", "Server's version is outdated");
		else
			engine->SetProgress("", "", -1.0f);
		connection->State = ConnectionState::Closed;
	}
	else if (NetParseCommand(text, "FAILURE", &rest))
	{
		engine->SetProgress("Rejected By Server", rest, 10.0f);
		Error = "Rejected By Server: " + rest;
		connection->State = ConnectionState::Closed;
	}
	else if (NetParseCommand(text, "FAILCODE", &rest))
	{
		// The console asks again (a password wanted), from the URL without
		// the one tried.
		UnrealURL retry = URL;
		Array<std::string> options;
		for (const std::string& option : retry.Options)
		{
			if (!EqualsIgnoreCase(option, 0, "PASSWORD="))
				options.push_back(option);
		}
		retry.Options = options;
		if (engine->console)
			CallEvent(engine->console, "ConnectFailure", { ExpressionValue::StringValue(rest), ExpressionValue::StringValue(retry.ToString()) });
		Error = "Rejected by the server: " + rest;
		connection->State = ConnectionState::Closed;
	}
	else if (NetParseCommand(text, "USES"))
	{
		UsedPackage package;
		if (NetParseValue(text, "PKG=", value))
			package.Name = value;
		NetParseValue(text, "GUID=", package.Guid);
		if (NetParseValue(text, "FLAGS=", value))
			package.Flags = (uint32_t)std::strtoul(value.c_str(), nullptr, 10);
		if (NetParseValue(text, "SIZE=", value))
			package.Size = std::atoi(value.c_str());
		if (NetParseValue(text, "GEN=", value))
			package.Generation = std::atoi(value.c_str());
		Uses.push_back(package);
	}
	else if (NetParseCommand(text, "WELCOME"))
	{
		LogMessage("Net: welcomed by server: " + text);
		if (NetParseValue(text, "LEVEL=", value))
			URL.Map = value;
		if (NetParseValue(text, "LONE=", value))
			LonePlayer = std::atoi(value.c_str()) != 0 || StrTools::equals_ignore_case(value, "True");

		// Each package is on the paths by its name, or in the cache by its
		// GUID -- then loaded from there under its name --, or it is to be
		// downloaded: if this side allows downloads and the server lets
		// that package go.
		for (UsedPackage& package : Uses)
		{
			bool onPaths = engine->packages->HasPackage(package.Name);
			std::string file = engine->packages->FindPackageFile(package.Name, package.Guid);
			if (!file.empty())
			{
				if (!onPaths)
					engine->packages->UsePackageFile(package.Name, file);
				continue;
			}
			FilesNeeded++;
			package.Flags |= (uint32_t)PackageFlags::Need;
			if (!Driver->AllowDownloads || (package.Flags & (uint32_t)PackageFlags::AllowDownload) == 0)
			{
				Error = "Downloading '" + package.Name.ToString() + "' not allowed";
				connection->State = ConnectionState::Closed;
				return;
			}
		}
		ReceiveNextFile(connection);
		Success = true;
	}
	else if (NetParseCommand(text, "CHALLENGE"))
	{
		if (NetParseValue(text, "VER=", value))
			connection->NegotiatedVer = std::atoi(value.c_str());
		if (NetParseValue(text, "CHALLENGE=", value))
			connection->Challenge = (int)std::strtol(value.c_str(), nullptr, 10);
		bool stats = NetParseValue(text, "STATS=", value) && std::atoi(value.c_str()) == 1;
		if (stats)
		{
			// A player with a world stats password sends a checksum of it;
			// the fork has none to send.
			URL.AddOrReplaceOption("Checksum=NoChecksum");
		}

		// The URL the server logs in: the map and options, without the
		// server's own address or a game type.
		std::string loginURL = URL.Map;
		for (const std::string& option : URL.Options)
		{
			if (!EqualsIgnoreCase(option, 0, "game="))
				loginURL += "?" + option;
		}
		if (!URL.Portal.empty())
			loginURL += "#" + URL.Portal;

		connection->SendText("NETSPEED " + std::to_string(connection->CurrentNetSpeed));
		connection->SendText("LOGIN RESPONSE=" + std::to_string(NetChallengeResponse(connection->Challenge)) + " URL=" + loginURL);
		connection->FlushNet();
	}
	else if (NetParseCommand(text, "DYNAMICRATE", &rest))
	{
		connection->DynamicUpdateInterval = RateInterval(std::atoi(rest.c_str()));
	}
	else if (NetParseCommand(text, "STATICRATE", &rest))
	{
		connection->StaticUpdateInterval = RateInterval(std::atoi(rest.c_str()));
	}
	else if (NetParseCommand(text, "USERFLAG", &rest))
	{
		connection->UserFlags = std::atoi(rest.c_str());
	}
}

void NetPendingLevel::ReceiveNextFile(NetConnection* connection)
{
	for (size_t i = 0; i < Uses.size(); i++)
	{
		const UsedPackage& package = Uses[i];
		if ((package.Flags & (uint32_t)PackageFlags::Need) != 0)
		{
			NetFileChannel::Request(connection, (int)i, package.Name.ToString(), package.Guid, package.Size);
			return;
		}
	}
}

void NetPendingLevel::NotifyReceivedFile(NetConnection* connection, int packageIndex, const std::string& error)
{
	if (packageIndex < 0 || packageIndex >= (int)Uses.size())
		return;
	UsedPackage& package = Uses[packageIndex];
	if (!error.empty())
	{
		if (Error.empty())
			Error = LocalizeMessage("Engine", "Errors", "DownloadFailed", "Downloading package '%s' failed: %s", { package.Name.ToString(), error });
		return;
	}

	// In the cache now, and loaded from there under its name.
	package.Flags &= ~(uint32_t)PackageFlags::Need;
	FilesNeeded--;
	engine->packages->UsePackageFile(package.Name, engine->packages->GetCachedPackagePath(package.Guid));
	ReceiveNextFile(connection);
}

void NetPendingLevel::NotifyProgress(const std::string& line1, const std::string& line2, float seconds)
{
	engine->SetProgress(line1, line2, seconds);
}

/////////////////////////////////////////////////////////////////////////////

bool NetClientLevel::NotifyAcceptingChannel(NetChannel* channel)
{
	return channel->ChType == ChannelType::Actor;
}

void NetClientLevel::NotifyReceivedText(NetConnection* connection, const std::string& text)
{
	LogMessage("Net: level received: " + text);

	std::string rest;
	if (NetParseCommand(text, "FAILURE", &rest))
		engine->NetFailure = rest.empty() ? "Rejected by the server" : rest;
	else if (NetParseCommand(text, "USERFLAG", &rest))
		connection->UserFlags = std::atoi(rest.c_str());
}

void NetClientLevel::NotifyClientPlayer(NetConnection* connection, UPlayerPawn* pawn)
{
	engine->HandleClientPlayer(connection, pawn);
}
