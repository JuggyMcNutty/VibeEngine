
#include "Precomp.h"
#include "UnrealURL.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Utils/StrTools.h"

UnrealURL::UnrealURL(const UnrealURL& baseURL, const UnrealURL& nextURL)
{
	// To do: this also needs to be able to handle fully qualified URLs for network support

	*this = baseURL;

	// Pass options from the nextURL to the base one
	Map = nextURL.Map;
	Portal = nextURL.Portal;

	for (auto& option : nextURL.Options)
		AddOrReplaceOption(option);

	/*
	// Unreal uses relative urls
	if (Map.size() > 8 && Map.substr(0, 8) == "..\\maps\\")
		Map = Map.substr(8);
	*/
}

// The game's [URL] Port: any URL's port unless it names another, as the
// original's FURL defaults it.
static int DefaultPort()
{
	return engine && engine->packages ? std::atoi(engine->packages->GetIniValue("system", "URL", "Port", "7777").c_str()) : 7777;
}

UnrealURL::UnrealURL() : Port(DefaultPort())
{
}

UnrealURL::UnrealURL(std::string urlString)
{
	Port = DefaultPort();

	// Expected url format on a local game:
	// mapname[#teleporttag][?key1=value1[?key2=value2]...]
	// Or in case of Klingon Honor Guard
	// mapname[/teleporttag][?key1=value1[?key2=value2]...]
	// Note that the options and the teleport tag can be in arbitrary places, as long as the map name comes first.

	// Trim the url string of whitespaces
	// Fixes the crash during the transition from Temple of Vandora to The Trench in Unreal
	// Due to the exit teleporter pointing to " trench" (with the whitespace at the beginning)
	urlString.erase(urlString.find_last_not_of(' ') + 1);
	urlString.erase(0, urlString.find_first_not_of(' '));

	ParseAddress(urlString);

	size_t mapNamePos = StrTools::find_first_of_any(urlString, "?/#");

	Map = urlString.substr(0, mapNamePos);

	if (mapNamePos != std::string::npos)
	{
		// We need to parse all options individually
		char paramType = urlString[mapNamePos]; // Can be '/', '#' or '?'
		std::string allParams = urlString.substr(mapNamePos + 1);

		auto nextParamPos = StrTools::find_first_of_any(allParams, "?/#");

		// Every part, the last included.
		for (;;)
		{
			if (paramType == '#' || paramType == '/')
				Portal = allParams.substr(0, nextParamPos);
			else if (paramType == '?')
			{
				auto optionStr = allParams.substr(0, nextParamPos);
				AddOrReplaceOption(optionStr);
			}

			if (nextParamPos == std::string::npos)
				break;

			paramType = allParams[nextParamPos];
			allParams = allParams.substr(nextParamPos + 1);
			nextParamPos = StrTools::find_first_of_any(allParams, "?/#");
		}
	}
}

// A server's address ahead of the map, as the original's URL parser finds
// one (dx-reverse-info/network.md, the address): [protocol:][//]host[:port][/map],
// where the host is the text up to a slash with a dot past its first
// character whose next letters are not the map's or a save's extension. A
// colon past the second character before any dot ends a protocol; a colon as
// the second is a drive letter, a file.
void UnrealURL::ParseAddress(std::string& urlString)
{
	size_t end = StrTools::find_first_of_any(urlString, "?#");
	std::string text = urlString.substr(0, end);
	std::string rest = end != std::string::npos ? urlString.substr(end) : std::string();

	if (text.size() > 2 && text[1] == ':')
		return;

	std::string protocol;
	size_t colon = text.find(':');
	size_t dot = text.find('.');
	if (colon != std::string::npos && colon > 1 && (dot == std::string::npos || colon < dot))
	{
		protocol = text.substr(0, colon);
		text = text.substr(colon + 1);
	}

	if (!text.empty() && text[0] == '/')
	{
		if (text.size() < 2 || text[1] != '/')
			return;
		text = text.substr(2);
	}

	dot = text.find('.');
	if (dot == std::string::npos || dot < 1)
		return;
	auto isExtension = [&](const std::string& ext) {
		if (ext.empty() || !StrTools::equals_ignore_case(text.substr(dot + 1, ext.size()), ext))
			return false;
		size_t after = dot + 1 + ext.size();
		return after >= text.size() || !std::isalnum((unsigned char)text[after]);
	};
	std::string mapExt = engine ? engine->packages->GetMapExtension() : MapExt;
	std::string saveExt = engine ? engine->packages->GetSaveExtension() : SaveExt;
	if (isExtension(mapExt) || isExtension(saveExt))
		return;

	size_t slash = text.find('/');
	std::string host = text.substr(0, slash);
	std::string mapPart = slash != std::string::npos ? text.substr(slash + 1) : std::string();

	if (!protocol.empty())
		Protocol = protocol;
	size_t portPos = host.find(':');
	if (portPos != std::string::npos)
	{
		Port = std::atoi(host.substr(portPos + 1).c_str());
		host = host.substr(0, portPos);
	}
	Host = host;

	if (mapPart.empty() && engine)
		mapPart = engine->packages->GetIniValue("system", "URL", "Map");
	urlString = mapPart + rest;
}

void UnrealURL::AddOrReplaceOption(const std::string& newvalue)
{
	size_t pos = newvalue.find('=');
	if (pos != std::string::npos)
	{
		std::string name = newvalue.substr(0, pos);
		for (char& c : name) c = std::tolower(c);
		for (std::string& option : Options)
		{
			if (option.size() >= name.size() + 1 && option[name.size()] == '=')
			{
				std::string key = option.substr(0, name.size());
				for (char& c : key) c = std::tolower(c);
				if (key == name)
				{
					option = newvalue;
					return;
				}
			}
		}
		Options.push_back(newvalue);
	}
	else
	{
		std::string name = newvalue;
		for (char& c : name) c = std::tolower(c);
		for (std::string& option : Options)
		{
			if (option.size() == name.size())
			{
				std::string key = option;
				for (char& c : key) c = std::tolower(c);
				if (key == name)
				{
					option = newvalue;
					return;
				}
			}
		}
		Options.push_back(newvalue);
	}
}

bool UnrealURL::HasOption(const std::string& name) const
{
	for (const std::string& option : Options)
	{
		if ((option.size() >= name.size() + 1 && option[name.size()] == '=') || option.size() == name.size())
		{
			std::string key = option.substr(0, name.size());
			for (char& c : key) c = std::tolower(c);
			if (key == name)
				return true;
		}
	}
	return false;
}

std::string UnrealURL::GetOption(std::string name) const
{
	for (char& c : name) c = std::tolower(c);

	for (const std::string& option : Options)
	{
		if (option.size() >= name.size() + 1 && option[name.size()] == '=')
		{
			std::string key = option.substr(0, name.size());
			for (char& c : key) c = std::tolower(c);
			if (key == name)
				return option.substr(name.size() + 1);
		}
	}
	return {};
}

std::string UnrealURL::GetAddressURL() const
{
	return Host + ":" + std::to_string(Port);
}

std::string UnrealURL::GetOptions() const
{
	std::string result;
	for (const std::string& option : Options)
	{
		result += "?";
		result += option;
	}
	return result;
}

std::string UnrealURL::GetPortal() const
{
	return Portal;
}

std::string UnrealURL::ToString() const
{
	std::string result;

	if (Protocol != "unreal")
	{
		result += Protocol;
		result += ":";
		if (!Host.empty())
			result += "//";
	}

	if (!Host.empty() || Port != DefaultPort())
	{
		result += Host;
		result += ":";
		result += std::to_string(Port);
		result += "/";
	}

	result += Map;
	result += GetOptions();
	result += GetPortal();

	return result;
}

bool UnrealURL::Empty() const
{
	return Map.empty() && Portal.empty() && Options.empty();
}

void UnrealURL::Clear()
{
	Map.clear();
	Portal.clear();
	Options.clear();
	Host.clear();
}

