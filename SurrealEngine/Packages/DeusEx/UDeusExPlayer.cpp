
#include "Precomp.h"
#include "UDeusExPlayer.h"
#include "Engine.h"
#include "Package/PackageManager.h"
#include "Packages/DeusEx/UGameDirectory.h"
#include "Packages/ConSys/History/UConHistory.h"
#include "Packages/Extension/Flags/UFlagBase.h"
#include "Utils/Logger.h"
#include "vibe_version.h"

void UDeusExPlayer::ConBindEvents()
{
	DeusExConBindEvents();
}

UObject* UDeusExPlayer::CreateDataVaultImageNoteObject()
{
	auto cls = engine->packages->FindClass("DeusEx.DataVaultImageNote");
	return package->NewObject("DataVaultImageNote", cls, ObjectFlags::NoFlags);
}

UObject* UDeusExPlayer::CreateDumpLocationObject()
{
	auto cls = engine->packages->FindClass("DeusEx.DumpLocation");
	return package->NewObject("DumpLocation", cls, ObjectFlags::NoFlags);
}

UObject* UDeusExPlayer::CreateGameDirectoryObject()
{
	// A new one each call, in the transient package, both as the original's;
	// the scripts CriticalDelete it after use.
	auto cls = engine->packages->FindClass("DeusEx.GameDirectory");
	return Cast<UDXGameDirectory>(engine->packages->GetTransientPackage()->NewObject("GameDirectory", cls, ObjectFlags::Transient));
}

UObject* UDeusExPlayer::CreateHistoryEvent()
{
	auto cls = engine->packages->FindClass("ConSys.ConHistoryEvent");
	return package->NewObject("ConHistoryEvent", cls, ObjectFlags::NoFlags);
}

UObject* UDeusExPlayer::CreateHistoryObject()
{
	auto cls = engine->packages->FindClass("ConSys.ConHistory");
	return Cast<UConHistory>(package->NewObject("ConHistory", cls, ObjectFlags::NoFlags));
}

UObject* UDeusExPlayer::CreateLogObject()
{
	auto cls = engine->packages->FindClass("DeusEx.DeusExLog");
	return package->NewObject("DeusExLog", cls, ObjectFlags::NoFlags);
}

void UDeusExPlayer::DeleteSaveGameFiles(std::optional<std::string> saveDirectory)
{
	engine->DeleteSaveGameFiles(saveDirectory.value_or(""));
}

std::string UDeusExPlayer::GetDeusExVersion()
{
	// The game's version and the engine's commit, which every build stamps
	// (vibe/cmake/version.cmake): the main menu shows it at its bottom right.
	return "1.112fm VibeEngine " VIBE_VERSION;
}

void UDeusExPlayer::SaveGame(int saveIndex, std::optional<std::string> saveDesc)
{
	engine->SaveGameInfo.SaveGameSlot = saveIndex;
	engine->SaveGameInfo.SaveGameDescription = saveDesc.value_or("");
	engine->TakeSaveSnapshot();
}

// The string made a name, which is set as a bool flag in the player's flag
// base -- added, its expiration the flag base's default (-1) -- and returned
// (DeusEx.dll 0x100144e0; deusex-dll.md, the player).
NameString UDeusExPlayer::SetBoolFlagFromString(const std::string& flagNameString, bool bValue)
{
	NameString flagName = flagNameString;
	if (UFlagBase* flags = FlagBase())
		flags->SetBool(flagName, bValue, true, -1);
	return flagName;
}

void UDeusExPlayer::UnloadTexture(UObject* Texture)
{
	// Nothing going on here because SE never unloads textures atm. This is just here so it doesn't throw LogUnimplemented.
}
