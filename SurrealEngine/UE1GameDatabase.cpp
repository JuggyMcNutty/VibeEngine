
#include "Precomp.h"
#include "UE1GameDatabase.h"
#include "Utils/SHA1Sum.h"
#include <iostream>

std::pair<KnownUE1Games, std::string> FindUE1GameInPath(const std::string& ue1_game_root_folder_path)
{
	if (ue1_game_root_folder_path.empty())
		return std::make_pair(KnownUE1Games::UE1_GAME_NOT_FOUND, "");

	if (!fs::exists(ue1_game_root_folder_path) || !fs::is_directory(ue1_game_root_folder_path))
		return std::make_pair(KnownUE1Games::UE1_GAME_NOT_FOUND, "");

	const auto UE1GameSystemPath = fs::path(ue1_game_root_folder_path) / "System";

	for (auto& executable_name : knownUE1ExecutableNames)
	{
		auto executablePath = UE1GameSystemPath / executable_name;

		std::string sha1sum = SHA1Sum::of_file(executablePath);

		// If sha1sum is empty this means that the file doesn't exist
		if (sha1sum.empty())
			continue;

		// Now check whether there is a match within the database or not
		const auto it = SHA1Database.find(sha1sum);
		
		if (it == SHA1Database.end())
			return std::make_pair(KnownUE1Games::UE1_GAME_NOT_FOUND, "");

		return std::make_pair(it->second, executable_name);
	}

	// We got nothing here
	return std::make_pair(KnownUE1Games::UE1_GAME_NOT_FOUND, "");
}