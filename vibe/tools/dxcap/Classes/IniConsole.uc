//=============================================================================
// IniConsole: what each engine reads of a key given twice in User.ini, logged
// with "DXINI:" from the menu map, then an exit (vibe/docs/DEVELOPMENT.md,
// scripted runs). Run it with DXCAP_USERINI naming the game's User.ini with
// each of these given a second time, another value, after the first:
// [DefaultPlayer]'s Name, Class and Team, and [Engine.PlayerPawn]'s
// MouseSensitivity. Logged: GetDefaultURL's Name, Class and Team; the
// player's MouseSensitivity, which the fork reads again by hand as it spawns
// a player, and the class default's; the player's name and the level's URL;
// then Team written with UpdateURL and read back with GetDefaultURL, a write
// meeting the line a read takes.
//=============================================================================
class IniConsole extends Console;

var float RunTime;
var bool bDone;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	P = Viewport.Actor;
	Log("DXINI: GetDefaultURL Name=" $ P.GetDefaultURL("Name") $ " Class=" $ P.GetDefaultURL("Class") $ " Team=" $ P.GetDefaultURL("Team"));
	Log("DXINI: MouseSensitivity player=" $ P.MouseSensitivity $ " default=" $ class'PlayerPawn'.default.MouseSensitivity);
	if (P.PlayerReplicationInfo != None)
		Log("DXINI: PlayerName=" $ P.PlayerReplicationInfo.PlayerName);
	Log("DXINI: local URL " $ P.Level.GetLocalURL());
	P.UpdateURL("Team", "7", true);
	Log("DXINI: after UpdateURL Team 7, GetDefaultURL Team=" $ P.GetDefaultURL("Team"));
	Log("DXINI: done");
	bDone = true;
	P.ConsoleCommand("exit");
}
