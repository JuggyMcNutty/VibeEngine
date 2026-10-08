//=============================================================================
// AugVisionConsole: whether an NPC the vision augmentation draws counts as
// drawn -- its LastRendered() -- logged with "DXAUGVIS:" and the console's
// clock (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island the player is given AugVision at level 3, its range
// logged. Of the level's first 60 path nodes, the first the player can
// stand at with another 200 to 0.8 x that range from its eye, the line to
// it blocked at three heights, takes the player, and the other a
// UNATCOTroop, standing; every other pawn and watcher goes, as SoundConsole
// hushes a level. The player faces the troop through the wall. Then four 5 s
// phases -- the augmentation off; on; on with the player turned about; off
// -- with every half second the troop's LastRendered(), state, bStasis,
// physics and animation frame, and the augmentation's display. The
// augmentation draws a heat source within its range through walls
// (GC.DrawActor), which the original stamps as drawn when it falls in the
// view (dx-reverse-info render-dll.md, render time).
//=============================================================================
class AugVisionConsole extends Console;

var float CapTime, MapTime, StepTime, LogTime;
var string CurrentMap;
var int Phase;
var AugVision Aug;
var ScriptedPawn Troop;
var vector Eye;
var rotator Facing;
var string PhaseName;

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

// Every other pawn and everything that watches for the player goes
// (destroyed, not killed, so no mission script hears of a death); no
// datalink or conversation speaks.
function Hush(DeusExPlayer DXP, Actor Keep)
{
	local Actor A;

	foreach DXP.AllActors(class'Actor', A)
	{
		if (A == Keep || A == DXP)
			continue;
		if (A.IsA('Pawn') || A.IsA('SecurityCamera') || A.IsA('AutoTurret') || A.IsA('AlarmUnit') || A.IsA('LaserTrigger') || A.IsA('BeamTrigger'))
			A.Destroy();
	}
}

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None)
		DXP.conPlay.TerminateConversation();
}

// Whether the line from the eye to a spot is blocked at three heights.
function bool Blocked(PlayerPawn P, vector From, vector Spot)
{
	return !P.FastTrace(Spot + vect(0,0,-30), From) && !P.FastTrace(Spot, From) && !P.FastTrace(Spot + vect(0,0,40), From);
}

// Of the first 60 path nodes, the first the player stands at with another
// 200 to MaxDist from its eye there, the line to it blocked at three
// heights: the player moved there, the other returned.
function PathNode PickSpots(PlayerPawn P, float MaxDist)
{
	local PathNode V, N;
	local vector E;
	local float D;
	local int Tried;

	foreach P.AllActors(class'PathNode', V)
	{
		if (++Tried > 60)
			break;
		E = V.Location;
		E.Z += P.BaseEyeHeight;
		foreach P.AllActors(class'PathNode', N)
		{
			D = VSize(N.Location - E);
			if (D < 200 || D > MaxDist || !Blocked(P, E, N.Location))
				continue;
			if (!P.SetLocation(V.Location) && !P.SetLocation(V.Location + vect(0,0,20)))
				break;
			Log("DXAUGVIS: the player stands at " $ V.Name $ " " $ P.Location);
			return N;
		}
	}
	return None;
}

function LogTroop(DeusExPlayer DXP)
{
	local AugmentationDisplayWindow display;

	display = DeusExRootWindow(DXP.rootWindow).hud.augDisplay;
	Log("DXAUGVIS: " $ PhaseName $ " at " $ CapTime $ ": drawn " $ Troop.LastRendered()
	    $ " state " $ Troop.GetStateName() $ " stasis " $ Troop.bStasis $ " physics " $ Troop.Physics
	    $ " anim " $ Troop.AnimSequence $ " " $ Troop.AnimFrame
	    $ "; aug active " $ Aug.bIsActive $ " vision " $ display.bVisionActive $ " level " $ display.visionLevel
	    $ " range " $ display.visionLevelValue);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local PathNode N;
	local rotator R;
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
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None)
	{
		DXP.Energy = DXP.EnergyMax;
		Silence(DXP);
	}

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 6.0)
		{
			if (DXP == None)
			{
				Log("DXAUGVIS: the player is no DeusExPlayer, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.AugmentationSystem.GivePlayerAugmentation(class'AugVision');
			Aug = AugVision(DXP.AugmentationSystem.FindAugmentation(class'AugVision'));
			if (Aug == None || !Aug.bHasIt)
			{
				Log("DXAUGVIS: no AugVision given, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Aug.CurrentLevel = 3;
			Log("DXAUGVIS: AugVision at level " $ Aug.CurrentLevel $ ", ranges " $ Aug.LevelValues[0] $ " " $ Aug.LevelValues[1]
			    $ " " $ Aug.LevelValues[2] $ " " $ Aug.LevelValues[3]);
			N = PickSpots(P, 0.8 * Aug.LevelValues[3]);
			Eye = P.Location;
			Eye.Z += P.BaseEyeHeight;
			if (N == None)
			{
				Log("DXAUGVIS: no path node behind a wall in range, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Troop = P.Spawn(class'UNATCOTroop',,, N.Location, rotator(Eye - N.Location));
			if (Troop == None)
				Troop = P.Spawn(class'UNATCOTroop',,, N.Location + vect(0,0,20), rotator(Eye - N.Location));
			if (Troop == None)
			{
				Log("DXAUGVIS: no room for the troop at " $ N.Name $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Troop.SetOrders('Standing', '', True);
			Hush(DXP, Troop);
			Log("DXAUGVIS: " $ Troop.Name $ " at " $ N.Name $ " " $ Troop.Location $ ", " $ VSize(Troop.Location - Eye)
			    $ " from the eye at " $ Eye $ ", blocked " $ Blocked(P, Eye, Troop.Location));
			Facing = rotator(Troop.Location - Eye);
			P.SetRotation(Facing);
			P.ViewRotation = Facing;
			PhaseName = "off";
			StepTime = 0;
			LogTime = 0;
			Phase = 2;
		}
	}
	else if (Phase >= 2 && Phase <= 5)
	{
		LogTime += Delta;
		if (LogTime >= 0.5)
		{
			LogTime = 0;
			if (Troop != None)
				LogTroop(DXP);
		}
		if (StepTime >= 5.0)
		{
			StepTime = 0;
			LogTime = 0;
			Phase++;
			if (Phase == 3)
			{
				Aug.Activate();
				PhaseName = "on";
			}
			else if (Phase == 4)
			{
				R = Facing;
				R.Yaw += 32768;
				P.SetRotation(R);
				P.ViewRotation = R;
				PhaseName = "on, turned about";
			}
			else if (Phase == 5)
			{
				Aug.Deactivate();
				P.SetRotation(Facing);
				P.ViewRotation = Facing;
				PhaseName = "off again";
			}
			else
			{
				Log("DXAUGVIS: done at " $ CapTime $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
			}
		}
	}
}
