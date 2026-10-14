#pragma once

struct GameLaunchInfo
{
	static constexpr int ue1Version = 500;	// Epic UE1 engine version game was forked from: Deus Ex's, an unknown version newer than UT but before UE2
	int gameVersion = 0;					// Engine version as named by the game (e.g. 226, 227, 436...)
	int gameSubVersion = 0;				// Engine sub version displayed as a letter (Note: Isn't always consistent)
	bool noEntryMap = false;
	std::string gameName = "";				// Name of the game (e.g. "Unreal Tournament")
	std::string gameRootFolder = "";		// Path to the folder that contains all the subfolders and files
	std::string gameExecutableName = "";	// Name of the game executable (e.g. "UnrealTournament")
	std::string gameVersionString = "";		// Version (+ sub version) info as a string (e.g. "469d")
	std::string url = "";					// The UnrealURL to launch upon startup
	std::string systemIniPath = "";			// --ini: the system ini to read and write, as the original's INI=
	std::string userIniPath = "";			// --userini: the user ini, as the original's USERINI=
	bool dedicatedServer = false;			// --server: no client, the map served, as the original's -SERVER
	bool lanPlay = false;					// --lanplay: a dedicated server's LAN tick rate, as the original's -LANPLAY
	std::string timelinePath = "";			// --timeline: keys and commands at set times, the harness's live mode (Timeline.h)

	// The engine knows no game but Deus Ex (UE1GameDatabase.h).
	bool IsUnrealTournament() const { return false; }
	bool IsUnrealTournament_469() const { return false; }
	bool IsDeusEx() const { return true; }
	bool IsCliveBarkersUndying() const { return false; }
	bool IsKlingonHonorGuard() const { return false; }
	bool IsRune() const { return false; }
	bool IsHarryPotter1() const { return false; }
	bool IsHarryPotter2() const { return false; }
	bool IsNerfArenaBlast() const { return false; }
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
