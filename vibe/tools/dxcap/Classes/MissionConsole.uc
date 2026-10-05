//=============================================================================
// MissionConsole: a mission map's script -- does the mission state machine
// come up, and do its first flags land? The map is whatever the run's URL
// names (vibe/docs/DEVELOPMENT.md, scripted runs); the console waits for
// the level's DeusExLevelInfo and its MissionScript actor, then for the
// script to hold the player and its flag base (InitStateMachine), the two
// flags a travelled-in start sets (MissionScript.FirstFrame:
// M<map>_StartupText and M<n>MissionStart) logged but not required -- only
// a real travel sets them --, logging what it finds with "DXMISSION:" --
// success, or what is missing after a timeout -- and exits. A sweep runs it
// map by map over every mission map, both engines for the failures and a
// sample; no driver for that is committed.
//=============================================================================
class MissionConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var DeusExLevelInfo dxInfo;
// The map this run is for, from the run's ini ([DXCapture.MissionConsole]);
// the fork's runs land on the map by their URL, the original's start at the
// menu map and travel here.
var globalconfig string TargetMap;
var name StartFlag;
var name MissionFlag;
var bool bStartFlagSet;
var bool bMissionFlagSet;
var bool bScriptSeen;

function string MapOfName(string URL)
{
	local int i;
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local MissionScript script;
	local Actor A;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
		dxInfo = None;
		StartFlag = '';
		MissionFlag = '';
		bStartFlagSet = false;
		bMissionFlagSet = false;
		bScriptSeen = false;
		Phase = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		// The original's runs start at the menu map: travel to the target.
		if (TargetMap != "" && !(M ~= MapOfName(TargetMap)))
		{
			if (MapTime > 2.0)
			{
				Log("DXCAP: opening " $ TargetMap);
				P.ConsoleCommand("open " $ TargetMap);
				Phase = 4;
			}
			return;
		}
		// The level's own info and its mission script.
		foreach P.AllActors(class'DeusExLevelInfo', dxInfo)
			break;
		if (dxInfo != None && MapTime > 1.0)
		{
			Log("DXMISSION: " $ M $ " mission " $ dxInfo.MissionNumber
			    $ " startup message " $ (dxInfo.startupMessage[0] != ""));
			foreach P.AllActors(class'MissionScript', script)
			{
				bScriptSeen = true;
				Log("DXMISSION: mission script " $ script.Class.Name
				    $ " checkTime " $ script.checkTime);
			}
			if (!bScriptSeen)
				Log("DXMISSION: no MissionScript actor");
			if (DXP == None || DXP.rootWindow == None)
			{
				Log("DXMISSION: no player root window");
				Phase = 2;
				return;
			}
			StartFlag = DXP.rootWindow.StringToName("M" $ Caps(M) $ "_StartupText");
			MissionFlag = DXP.rootWindow.StringToName("M" $ dxInfo.MissionNumber $ "MissionStart");
			Phase = 1;
		}
		else if (MapTime > 8.0)
		{
			Log("DXMISSION: no DeusExLevelInfo in " $ M);
			Phase = 2;
		}
	}
	else if (Phase == 1)
	{
		// The machine initialized: the script found the player and the flag
		// base (InitStateMachine). FirstFrame's own flags need
		// PlayerTraveling, which only a real travel sets -- a direct map
		// load has it unset in both engines -- so they are reported, not
		// required.
		if (bScriptSeen)
		{
			foreach P.AllActors(class'MissionScript', script)
				if (script.Player != None && script.flags != None)
				{
					Log("DXMISSION: state machine initialized at " $ MapTime $ " s");
					Phase = 3;
					break;
				}
		}
		if (Phase == 3)
		{
			if (DXP != None && DXP.flagBase != None)
			{
				Log("DXMISSION: startup flag " $ DXP.flagBase.GetBool(StartFlag)
				    $ " mission-start flag " $ DXP.flagBase.GetBool(MissionFlag)
				    $ " (need travel; informational)");
			}
			Log("DXMISSION: " $ M $ " OK");
			P.ConsoleCommand("exit");
			return;
		}
		if (DXP != None && DXP.flagBase != None)
		{
			if (!bStartFlagSet && DXP.flagBase.GetBool(StartFlag))
			{
				bStartFlagSet = true;
				Log("DXMISSION: startup flag set at " $ MapTime $ " s");
			}
			if (!bMissionFlagSet && DXP.flagBase.GetBool(MissionFlag))
			{
				bMissionFlagSet = true;
				Log("DXMISSION: mission-start flag set at " $ MapTime $ " s");
			}
		}
		if (MapTime > 12.0)
		{
			Log("DXMISSION: " $ M $ " INCOMPLETE after 12 s: script " $ bScriptSeen
			    $ " startup " $ bStartFlagSet $ " missionstart " $ bMissionFlagSet);
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
	else if (Phase == 2)
	{
		if (MapTime > 12.0)
		{
			Log("DXMISSION: " $ M $ " FAILED");
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
}
