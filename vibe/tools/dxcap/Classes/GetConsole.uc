//=============================================================================
// GetConsole: the console's GET and SET, as the game's menus read and write
// their settings with them -- from the menu map, one class default of each
// kind of property (a string, an empty string, a name, an object, a class, a
// float, an int, a bool, an enum byte, and a GET the multiplayer Host screen
// makes) logged with what GET gives; then SETs of a bare class name's
// value, a value with spaces and a value the player has too, each read
// back; then the audio device's EffectsChannels, as the Sound menu reads
// and writes it, set within its range, above it and below it, each read
// back, and put back; then GetPropertyText of a public property, a public
// array, a private property, a missing one and a const one, and
// SetPropertyText of a const, a missing and a public property, each read
// back; then the main menu opened and shot 2 s later, its pointer drawn,
// and an exit.
//=============================================================================
class GetConsole extends Console;

var float MenuTime, ShotTime;
var bool bDone;

function LogGet(PlayerPawn P, string Setting)
{
	Log("DXGET: " $ Setting $ " = '" $ P.ConsoleCommand("get " $ Setting) $ "'");
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	P = Viewport.Actor;
	MenuTime += Delta;
	if (MenuTime < 3.0)
		return;
	if (ShotTime > 0)
	{
		if (MenuTime > ShotTime)
		{
			P.ConsoleCommand("shot");
			Log("DXGET: exiting");
			bDone = true;
			P.ConsoleCommand("exit");
		}
		return;
	}

	LogGet(P, "DeusExMPGame VictoryCondition");
	Log("DXGET: the default itself '" $ class'DeusExMPGame'.default.VictoryCondition $ "', a deathmatch's '" $ class'DeathMatchGame'.default.VictoryCondition $ "'");
	LogGet(P, "Engine.GameReplicationInfo ServerName");
	LogGet(P, "Engine.GameInfo DefaultPlayerName");
	LogGet(P, "Engine.PlayerPawn WeaponPriority");
	LogGet(P, "Engine.Actor Texture");
	LogGet(P, "Engine.GameInfo DefaultPlayerClass");
	LogGet(P, "Engine.PlayerPawn MouseSensitivity");
	LogGet(P, "Engine.GameInfo MaxPlayers");
	LogGet(P, "Engine.PlayerPawn bAlwaysMouseLook");
	LogGet(P, "Engine.Actor Physics");
	P.ConsoleCommand("set DeusExMPGame ScoreToWin 17");
	LogGet(P, "DeusExMPGame ScoreToWin");
	P.ConsoleCommand("set GameReplicationInfo ServerName My Test Server");
	LogGet(P, "GameReplicationInfo ServerName");
	P.ConsoleCommand("set PlayerPawn MouseSensitivity 5.5");
	Log("DXGET: the player's MouseSensitivity after a set " $ P.MouseSensitivity);
	LogGet(P, "ini:Engine.Engine.AudioDevice EffectsChannels");
	P.ConsoleCommand("set ini:Engine.Engine.AudioDevice EffectsChannels 8");
	LogGet(P, "ini:Engine.Engine.AudioDevice EffectsChannels");
	P.ConsoleCommand("set ini:Engine.Engine.AudioDevice EffectsChannels 64");
	LogGet(P, "ini:Engine.Engine.AudioDevice EffectsChannels");
	P.ConsoleCommand("set ini:Engine.Engine.AudioDevice EffectsChannels -5");
	LogGet(P, "ini:Engine.Engine.AudioDevice EffectsChannels");
	P.ConsoleCommand("set ini:Engine.Engine.AudioDevice EffectsChannels 16");
	Log("DXGET: GetPropertyText MouseSensitivity '" $ P.GetPropertyText("MouseSensitivity") $ "'");
	Log("DXGET: GetPropertyText WeaponPriority '" $ P.GetPropertyText("WeaponPriority") $ "'");
	if (DeusExPlayer(P) != None && DeusExPlayer(P).rootWindow != None)
		Log("DXGET: GetPropertyText the root's private hMultiplier '" $ DeusExPlayer(P).rootWindow.GetPropertyText("hMultiplier") $ "'");
	Log("DXGET: GetPropertyText NoSuchProperty '" $ P.GetPropertyText("NoSuchProperty") $ "'");
	Log("DXGET: GetPropertyText the const Level '" $ P.GetPropertyText("Level") $ "'");
	P.SetPropertyText("Level", "None");
	Log("DXGET: SetPropertyText the const Level None, Level " $ P.Level);
	P.SetPropertyText("NoSuchProperty", "1");
	Log("DXGET: SetPropertyText NoSuchProperty 1, still running");
	P.SetPropertyText("MouseSensitivity", "4.25");
	Log("DXGET: SetPropertyText MouseSensitivity 4.25, MouseSensitivity " $ P.MouseSensitivity);
	if (DeusExPlayer(P) != None)
		DeusExPlayer(P).ShowMainMenu();
	ShotTime = MenuTime + 2.0;
}
