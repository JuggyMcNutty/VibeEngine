
#include "Precomp.h"
#include "NetClient.h"
#include "NetChannel.h"
#include "Package/PackageManager.h"
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

	// The engine's answer to a server's challenge.
	int ChallengeResponse(int challenge)
	{
		uint32_t c = (uint32_t)challenge;
		return (int)((c * 237u) ^ (uint32_t)(challenge >> 16) ^ (c << 16) ^ 0x93fe92ceu);
	}
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
		Error = "The server needs a newer version of the game";
		connection->State = ConnectionState::Closed;
	}
	else if (NetParseCommand(text, "FAILURE", &rest))
	{
		Error = rest.empty() ? "Rejected by the server" : rest;
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

		// The fork does not download: a package must be here, as the server
		// has it. A map is checked when it loads.
		if (!engine->packages->HasPackage(package.Name))
		{
			Error = "Missing package " + package.Name.ToString();
			connection->State = ConnectionState::Closed;
		}
		else if (!engine->packages->IsMapPackage(package.Name))
		{
			Package* local = engine->packages->GetPackage(package.Name);
			if (!StrTools::equals_ignore_case(local->GetGuidString(), package.Guid))
			{
				Error = "Package " + package.Name.ToString() + " differs from the server's";
				connection->State = ConnectionState::Closed;
			}
		}
	}
	else if (NetParseCommand(text, "WELCOME"))
	{
		if (NetParseValue(text, "LEVEL=", value))
			URL.Map = value;
		if (NetParseValue(text, "LONE=", value))
			LonePlayer = std::atoi(value.c_str()) != 0;
		Success = true;
		LogMessage("Net: welcomed by server: " + text);
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
		connection->SendText("LOGIN RESPONSE=" + std::to_string(ChallengeResponse(connection->Challenge)) + " URL=" + loginURL);
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
