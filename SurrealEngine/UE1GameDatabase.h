#pragma once

enum class KnownUE1Games
{
	UE1_GAME_NOT_FOUND,
	DEUS_EX_1002f,
	DEUS_EX_1112fm,
};

static const std::map<std::string, KnownUE1Games> SHA1Database = {
	// Deus Ex, v1002f
	{"9f923d667a396e8243028c14dc3f5e0a6db13d84", KnownUE1Games::DEUS_EX_1002f},

	// Deus Ex, v1112fm
	{"2a933e26aa9cfb33b37f78afe21434caa031f14a", KnownUE1Games::DEUS_EX_1112fm},
	// Deus Ex, v1112fm - Steam Version
	{"af951ddd35b38e8d9cc8501b8a50a02a3ab6cae7", KnownUE1Games::DEUS_EX_1112fm},
};

static const Array<std::string> knownUE1ExecutableNames = {
	"DeusEx.exe",
};

// Returns a pair of UE1-Game type and executable name
std::pair<KnownUE1Games, std::string> FindUE1GameInPath(const std::string& ue1_game_root_folder_path);
