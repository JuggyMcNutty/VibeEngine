#pragma once

struct GameLaunchInfo
{
	bool noEntryMap = false;
	std::string gameName = "";				// Name of the game ("Deus Ex")
	std::string gameRootFolder = "";		// Path to the folder that contains all the subfolders and files
	std::string gameExecutableName = "";	// Name of the game executable ("DeusEx")
	std::string gameVersionString = "";		// The game's version ("1112fm" or "1002")
	std::string url = "";					// The UnrealURL to launch upon startup
	std::string systemIniPath = "";			// --ini: the system ini to read and write, as the original's INI=
	std::string userIniPath = "";			// --userini: the user ini, as the original's USERINI=
	bool dedicatedServer = false;			// --server: no client, the map served, as the original's -SERVER
	bool lanPlay = false;					// --lanplay: a dedicated server's LAN tick rate, as the original's -LANPLAY
	std::string timelinePath = "";			// --timeline: keys and commands at set times, the harness's live mode (Timeline.h)
};

class GameFolderSelection
{
public:
	static void UpdateList();
	static GameLaunchInfo GetLaunchInfo(int selectedGame);

	static Array<GameLaunchInfo> Games;

private:
	static GameLaunchInfo ExamineFolder(const std::string& path);
	static Array<std::string> FindGameFolders();
};
