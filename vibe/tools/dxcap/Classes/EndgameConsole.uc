//=============================================================================
// EndgameConsole: an ending's way to the credits (vibe/docs/DEVELOPMENT.md,
// scripted runs). On TargetMap ([DXCapture.EndgameConsole], 99_Endgame2 by
// default) the ending's conversation is stopped as it starts, as a player
// skipping it would; then every second for 30 s, logged with "DXEND:": the
// level's time, the conversation's Played flag, the mission script's timer,
// whether its quote is printed, and whether the credits are up; then exit.
//=============================================================================
class EndgameConsole extends Console;

var config string TargetMap;

var float MapTime, LogTime;
var int Phase;
var Object Stopped;
var name PlayedFlag;

function string Target()
{
	if (TargetMap == "")
		return "99_Endgame2";
	return TargetMap;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local MissionEndgame M;
	local DeusExRootWindow Root;
	local bool bCredits;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	if (DXP == None)
		return;

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), Caps(Target())) < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXEND: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = 1;
			}
		}
		else
			Phase = 1;
		return;
	}
	if (InStr(Caps(P.Level.GetLocalURL()), Caps(Target())) < 0)
		return;
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		PlayedFlag = DXP.rootWindow.StringToName(DXP.conPlay.con.conName $ "_Played");
		Log("DXEND: conversation " $ DXP.conPlay.con.conName $ " stopped at " $ P.Level.TimeSeconds);
		DXP.conPlay.TerminateConversation();
	}
	LogTime += Delta;
	if (Phase == 1 && LogTime >= 1.0)
	{
		LogTime = 0;
		foreach P.AllActors(class'MissionEndgame', M)
			break;
		Root = DeusExRootWindow(DXP.rootWindow);
		bCredits = Root != None && CreditsWindow(Root.GetTopWindow()) != None;
		if (M != None)
			Log("DXEND: at " $ P.Level.TimeSeconds $ " flag " $ PlayedFlag $ " " $ DXP.flagBase.GetBool(PlayedFlag)
				$ " timer " $ M.endgameTimer $ " quote " $ M.bQuotePrinted $ " check " $ M.checkTime $ " credits " $ bCredits);
		else
			Log("DXEND: at " $ P.Level.TimeSeconds $ " no mission script, credits " $ bCredits);
		if (P.Level.TimeSeconds > 30.0)
		{
			Log("DXEND: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
