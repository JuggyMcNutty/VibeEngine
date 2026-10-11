//=============================================================================
// MoverConsole: where a line meets Deus Ex's movers -- where the level's BSP
// last filed them (vibe/docs/DEVELOPMENT.md, scripted runs; dx-reverse-info
// engine-dll.md, traces: the movers in the BSP). On TargetMap
// ([DXCapture.MoverConsole], 01_NYC_UNATCOIsland by default), conversations
// stopped, the player out of the world from the first tick there, 3 s in:
// the first door (a DeusExMover with bIsDoor, no glass) and
// the first BreakableGlass in name order, each with a line across its box's
// middle along its thinnest side, 32 units past it each way. At each step,
// for each line, logged with "DXMOVER:": FastTrace's answer, LineOfSightTo's
// from the player, its eye at one end, to a marker at the other, and the
// actor a Trace meets:
//   1. as the map has them;
//   2. the door made not to collide;
//   3. a tick on, the door hidden and moved 100 along its line, off it into
//      the open; a tick on, its line at its old place and at its new;
//   4. after a save to slot 9, both lines again; then the door shown;
//   5. the glass broken (BlowItUp): its line at once, and a tick on;
//   6. after a second save, the glass's line again: what the level holds;
// then an exit.
//=============================================================================
class MoverConsole extends Console;

var config string TargetMap;

var float MapTime, StepTime;
var int Step;
var string CurrentMap;
var Object Stopped;
var DeusExMover Door, Glass;
var vector DoorS, DoorT, GlassS, GlassT, Shift;
var GarbageMarker Marker;
var bool bOut;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

// The line across a mover's box, through its middle along its thinnest side
function bool Across(Mover M, out vector S, out vector T)
{
	local vector Min, Max, C, E, A;

	if (!M.GetBoundingBox(Min, Max))
		return false;
	C = (Min + Max) * 0.5;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y && E.X <= E.Z)
		A = vect(1,0,0) * (E.X + 32);
	else if (E.Y <= E.Z)
		A = vect(0,1,0) * (E.Y + 32);
	else
		A = vect(0,0,1) * (E.Z + 32);
	S = C - A;
	T = C + A;
	return true;
}

function Ask(PlayerPawn P, string Label, vector S, vector T)
{
	local Actor Hit;
	local vector HitLoc, HitNorm;
	local bool bSight;

	if (Marker == None)
		Marker = P.Spawn(class'GarbageMarker',,, T);
	Marker.SetLocation(T);
	P.SetLocation(S - vect(0,0,1) * P.EyeHeight);
	bSight = P.LineOfSightTo(Marker);
	Hit = P.Trace(HitLoc, HitNorm, T, S, true);
	Log("DXMOVER: " $ Label $ " fast " $ P.FastTrace(T, S) $ " sight " $ bSight $ " trace " $ Hit);
}

function Choose(PlayerPawn P)
{
	local DeusExMover M;

	foreach P.AllActors(class'DeusExMover', M)
	{
		if (M.IsA('BreakableGlass'))
		{
			if (Glass == None || Caps(string(M.Name)) < Caps(string(Glass.Name)))
				Glass = M;
		}
		else if (M.bIsDoor && (Door == None || Caps(string(M.Name)) < Caps(string(Door.Name))))
			Door = M;
	}
	if (Door != None && Across(Door, DoorS, DoorT))
		Log("DXMOVER: door " $ Door.Name $ " at " $ Door.Location $ " collides " $ Door.bCollideActors $ " blocks " $ Door.bBlockActors
			$ " line " $ DoorS $ " to " $ DoorT);
	else
		Door = None;
	if (Glass != None && Across(Glass, GlassS, GlassT))
		Log("DXMOVER: glass " $ Glass.Name $ " at " $ Glass.Location $ " collides " $ Glass.bCollideActors $ " blocks " $ Glass.bBlockActors
			$ " line " $ GlassS $ " to " $ GlassT);
	else
		Glass = None;
}

function SaveTo(PlayerPawn P, int Slot)
{
	local SaveHelper S;

	S = P.Spawn(class'SaveHelper', P);
	S.Slot = Slot;
	S.Description = "MoverConsole";
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None && DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}

	if (Step == 0)
	{
		// The original starts in its menu map.
		if (!(CurrentMap ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXMOVER: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Step = -1;
			}
			return;
		}
		// Out of the world at once: a deathmatch map's start is a random
		// one, and a player landing on glass breaks it
		if (!bOut && DXP != None)
		{
			Log("DXMOVER: the player starts at " $ P.Location);
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			P.SetPhysics(PHYS_None);
			bOut = true;
		}
		if (MapTime < 3.0 || DXP == None)
			return;
		Choose(P);
		if (Door != None)
			Ask(P, "1 door", DoorS, DoorT);
		if (Glass != None)
			Ask(P, "1 glass", GlassS, GlassT);
		if (Door != None)
		{
			Door.SetCollision(false, false, false);
			Ask(P, "2 door", DoorS, DoorT);
		}
		Step = 2;
		StepTime = 0;
	}
	else if (Step == -1)
	{
		if (CurrentMap ~= Target())
			Step = 0;
	}
	else if (Step == 2)
	{
		if (Door != None)
		{
			Door.bHidden = true;
			Shift = Normal(DoorT - DoorS) * 100;
			Door.SetLocation(Door.Location + Shift);
		}
		Step = 3;
	}
	else if (Step == 3)
	{
		if (Door != None)
		{
			Log("DXMOVER: door now at " $ Door.Location $ " hidden " $ Door.bHidden $ " collides " $ Door.bCollideActors);
			Ask(P, "3 door old", DoorS, DoorT);
			Ask(P, "3 door new", DoorS + Shift, DoorT + Shift);
		}
		SaveTo(P, 9);
		Step = 4;
		StepTime = 0;
	}
	else if (Step == 4 && StepTime > 1.0)
	{
		if (Door != None)
		{
			Ask(P, "4 door old", DoorS, DoorT);
			Ask(P, "4 door new", DoorS + Shift, DoorT + Shift);
			Door.bHidden = false;
		}
		if (Glass != None)
		{
			Glass.BlowItUp(P);
			Ask(P, "5 glass at once", GlassS, GlassT);
		}
		Step = 5;
	}
	else if (Step == 5)
	{
		if (Glass != None)
		{
			Log("DXMOVER: glass now at " $ Glass.Location $ " hidden " $ Glass.bHidden $ " collides " $ Glass.bCollideActors);
			Ask(P, "5 glass a tick on", GlassS, GlassT);
		}
		SaveTo(P, 9);
		Step = 6;
		StepTime = 0;
	}
	else if (Step == 6 && StepTime > 1.0)
	{
		if (Glass != None)
			Ask(P, "6 glass after a save", GlassS, GlassT);
		Step = 7;
		StepTime = 0;
	}
	else if (Step == 7 && StepTime > 1.0)
	{
		Log("DXMOVER: done, exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
