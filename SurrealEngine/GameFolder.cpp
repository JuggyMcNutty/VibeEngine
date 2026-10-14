
#include "Precomp.h"
#include "GameFolder.h"
#include "Utils/File.h"
#include "Utils/UTF16.h"
#include "UE1GameDatabase.h"
#include "Utils/CommandLine.h"
#include "OriginalCommandLine.h"
#include <filesystem>

Array<GameLaunchInfo> GameFolderSelection::Games;

void GameFolderSelection::UpdateList()
{
	Games.clear();

	for (const std::string& folder : commandline->GetItems())
	{
		GameLaunchInfo game = ExamineFolder(folder);
		if (!game.gameName.empty())
			Games.push_back(game);
	}

	if (Games.empty())
	{
		// Check if we're within an UE1-game System folder.
		auto p = std::filesystem::current_path();
		if (p.filename().string() == "System")
		{
			GameLaunchInfo game = ExamineFolder(p.parent_path().string());
			if (!game.gameName.empty())
				Games.push_back(game);
		}
	}

	if (Games.empty())
	{
		for (const std::string& folder : FindGameFolders())
		{
			GameLaunchInfo game = ExamineFolder(folder);
			if (!game.gameName.empty())
				Games.push_back(game);
		}
	}
}

GameLaunchInfo GameFolderSelection::GetLaunchInfo(int selectedGame)
{
	if (selectedGame < 0 || (size_t)selectedGame >= Games.size())
		return {};

	GameLaunchInfo info = Games[selectedGame];
	info.url = commandline->GetArg("-u", "--url", info.url);
	info.systemIniPath = commandline->GetArg("", "--ini", info.systemIniPath);
	info.userIniPath = commandline->GetArg("", "--userini", info.userIniPath);
	info.dedicatedServer = commandline->HasArg("", "--server") || info.dedicatedServer;
	info.lanPlay = commandline->HasArg("", "--lanplay") || info.lanPlay;
	info.timelinePath = commandline->GetArg("", "--timeline", info.timelinePath);

	// The original's command line, when the launcher gave one: its start
	// URL, -server, and the inis its INI= and USERINI= name, over the
	// fork's own options.
	const OriginalCommandLine& original = OriginalCommandLine::Get();
	if (original.Given())
	{
		std::string url = original.StartURL();
		if (!url.empty())
			info.url = url;
		if (original.Param("server"))
			info.dedicatedServer = true;
		std::string ini;
		if (original.Value("INI", ini) && !ini.empty())
			info.systemIniPath = ini;
		if (original.Value("USERINI", ini) && !ini.empty())
			info.userIniPath = ini;
	}
	return info;
}

GameLaunchInfo GameFolderSelection::ExamineFolder(const std::string& path)
{
	GameLaunchInfo info;

	if (path.empty())
		return info;

	auto ue1_game = FindUE1GameInPath(path);

	if (ue1_game.first != KnownUE1Games::UE1_GAME_NOT_FOUND)
	{
		info.gameRootFolder = path;
		info.gameExecutableName = fs::path(ue1_game.second).stem().string();

		switch (ue1_game.first)
		{
			case KnownUE1Games::DEUS_EX_1002f:
			{
				info.gameName = "Deus Ex";
				info.gameVersionString = "1002";
			}
			break;
			case KnownUE1Games::DEUS_EX_1112fm:
			{
				info.gameName = "Deus Ex";
				info.gameVersionString = "1112fm";
			}
			break;
			case KnownUE1Games::UE1_GAME_NOT_FOUND:
				// Do nothing, we want the fields to be empty.
			break;
		}
	}
	
	// If no game is found, this will be an empty GameLaunchInfo
	return info;
}

#ifdef WIN32

static std::string FindEpicRegisteredGame(const std::string& keyname)
{
	Array<wchar_t> buffer(1024);
	HKEY regkey = 0;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, to_utf16("SOFTWARE\\Unreal Technology\\Installed Apps\\" + keyname).c_str(), 0, KEY_READ, &regkey) == ERROR_SUCCESS)
	{
		DWORD type = 0;
		DWORD size = (DWORD)(buffer.size() * sizeof(wchar_t));
		LSTATUS result = RegQueryValueEx(regkey, L"Folder", 0, &type, (LPBYTE)buffer.data(), &size);
		RegCloseKey(regkey);
		if (result == ERROR_SUCCESS && type == REG_SZ)
		{
			buffer.back() = 0;
			return from_utf16(buffer.data());
		}
	}
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, to_utf16("SOFTWARE\\WOW6432Node\\Unreal Technology\\Installed Apps\\" + keyname).c_str(), 0, KEY_READ, &regkey) == ERROR_SUCCESS)
	{
		DWORD type = 0;
		DWORD size = (DWORD)(buffer.size() * sizeof(wchar_t));
		LSTATUS result = RegQueryValueEx(regkey, L"Folder", 0, &type, (LPBYTE)buffer.data(), &size);
		RegCloseKey(regkey);
		if (result == ERROR_SUCCESS && type == REG_SZ)
		{
			buffer.back() = 0;
			return from_utf16(buffer.data());
		}
	}
	return {};
}

Array<std::string> GameFolderSelection::FindGameFolders()
{
	return { FindEpicRegisteredGame("Deus Ex") };
}

#else

Array<std::string> GameFolderSelection::FindGameFolders()
{
	return {};
}

#endif
