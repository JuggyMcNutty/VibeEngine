
#include "Precomp.h"
#include "NPlayerPawn.h"
#include "VM/NativeFunc.h"
#include "Packages/Engine/Resources/Level/ULevel.h"
#include "Packages/Engine/Actors/Pawn/UPlayerPawn.h"
#include "Packages/Engine/UPlayer.h"
#include "Packages/Engine/Actors/Info/ULevelInfo.h"
#include "Engine.h"
#include "Network/NetServer.h"
#include "Package/PackageManager.h"
#include "VM/ScriptCall.h"

void NPlayerPawn::RegisterFunctions()
{
	RegisterVMNativeFunc_3("PlayerPawn", "ClientTravel", &NPlayerPawn::ClientTravel, 0);
	if (engine->LaunchInfo.ue1Version > 219)
	{
		RegisterVMNativeFunc_2("PlayerPawn", "ConsoleCommand", &NPlayerPawn::ConsoleCommand, 0);
	}
	else
	{
		RegisterVMNativeFunc_1("PlayerPawn", "ClientMessage", &NPlayerPawn::ClientMessage_219, 0);
		RegisterVMNativeFunc_1("PlayerPawn", "ConsoleCommand", &NPlayerPawn::ConsoleCommand_219, 537);
		RegisterVMNativeFunc_2("PlayerPawn", "ConsoleCommandResult", &NPlayerPawn::ConsoleCommandResult_219, 542);
	}
	RegisterVMNativeFunc_1("PlayerPawn", "CopyToClipboard", &NPlayerPawn::CopyToClipboard, 0);
	RegisterVMNativeFunc_2("PlayerPawn", "GetDefaultURL", &NPlayerPawn::GetDefaultURL, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "GetEntryLevel", &NPlayerPawn::GetEntryLevel, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "GetPlayerNetworkAddress", &NPlayerPawn::GetPlayerNetworkAddress, 0);
	RegisterVMNativeFunc_1("PlayerPawn", "PasteFromClipboard", &NPlayerPawn::PasteFromClipboard, 0);
	RegisterVMNativeFunc_0("PlayerPawn", "ResetKeyboard", &NPlayerPawn::ResetKeyboard, 544);
	if (engine->LaunchInfo.ue1Version > 219)
		RegisterVMNativeFunc_3("PlayerPawn", "UpdateURL", &NPlayerPawn::UpdateURL, 546);
	else
		RegisterVMNativeFunc_1("PlayerPawn", "UpdateURL", &NPlayerPawn::UpdateURL_219, 546);
	if (engine->LaunchInfo.IsUnreal1_227())
		RegisterVMNativeFunc_2("PlayerPawn", "IsPressing", &NPlayerPawn::IsPressing_U227, 549);
}

void NPlayerPawn::ClientTravel(UObject* Self, const std::string& URL, uint8_t TravelType, bool bItems)
{
	if (engine->LaunchInfo.IsDeusEx())
		CallEvent(Self, "PreTravel", {});
	engine->ClientTravel(URL, static_cast<ETravelType>(TravelType), bItems);
}

void NPlayerPawn::ConsoleCommand(UObject* Self, const std::string& Command, std::string& ReturnValue)
{
	// "Execute a console command in the context of this player, then forward to Actor.ConsoleCommand"

	ExpressionValue found = ExpressionValue::BoolValue(false);
	ReturnValue = engine->ConsoleCommand(Self, Command, found.ToType<BitfieldBool&>());
}

void NPlayerPawn::ClientMessage_219(UObject* Self, const std::string& S)
{
	LogUnimplemented("Playerawn.ClientMessage");
}

void NPlayerPawn::ConsoleCommand_219(UObject* Self, const std::string& Command)
{
	std::string result;
	ConsoleCommand(Self, Command, result);
}

void NPlayerPawn::ConsoleCommandResult_219(UObject* Self, const std::string& Command, std::string& ReturnValue)
{
	ConsoleCommand(Self, Command, ReturnValue);
}

void NPlayerPawn::CopyToClipboard(UObject* Self, const std::string& Text)
{
	engine->window->SetClipboardText(Text);
}

void NPlayerPawn::GetDefaultURL(UObject* Self, const std::string& Option, std::string& ReturnValue)
{
	ReturnValue = engine->packages->GetIniValue("user", "DefaultPlayer", Option);
}

void NPlayerPawn::GetEntryLevel(UObject* Self, UObject*& ReturnValue)
{
	ReturnValue = engine->EntryLevelInfo;
}

// The original's (Engine.dll 0x103df310): a pawn whose player is a net
// connection -- a remote player's, on the server -- answers its address,
// a.b.c.d:port; any other "".
void NPlayerPawn::GetPlayerNetworkAddress(UObject* Self, std::string& ReturnValue)
{
	NetConnection* connection = NetConnectionOfPlayer(UObject::Cast<UPlayerPawn>(Self)->Player());
	ReturnValue = connection ? connection->RemoteAddressString() : "";
}

void NPlayerPawn::IsPressing_U227(UObject* Self, uint8_t& KeyNum, BitfieldBool& ReturnValue)
{
	ReturnValue = UObject::Cast<UPlayerPawn>(Self)->IsPressing(KeyNum);
}

void NPlayerPawn::PasteFromClipboard(UObject* Self, std::string& ReturnValue)
{
	ReturnValue = engine->window->GetClipboardText();
}

void NPlayerPawn::ResetKeyboard(UObject* Self)
{
	// The original's ResetConfig of the input's class copies nothing in
	// Deus Ex and has the input read the player's bindings from User.ini
	// again (core-dll.md, configuration); the fork's binding store reads
	// from the same ini. DeusExPlayer.TravelPostAccept calls this on every
	// level change.
	engine->LoadKeybindings();
}

// The option into the game engine's last URL (Engine.dll
// APlayerPawn::execUpdateURL 0x103b87c0), which travel goes by.
void NPlayerPawn::UpdateURL(UObject* Self, const std::string& NewOption, const std::string& NewValue, bool bSaveDefault)
{
	engine->LastURL.AddOrReplaceOption(NewOption + "=" + NewValue);
	if (bSaveDefault)
	{
		// Save the setting to DefaultUser section in User.ini
		engine->packages->SetIniValue("User", "DefaultPlayer", NewOption, NewValue);
	}
}

void NPlayerPawn::UpdateURL_219(UObject* Self, const std::string& NewOption)
{
	engine->LastURL.AddOrReplaceOption(NewOption);
}
