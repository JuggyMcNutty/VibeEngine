//=============================================================================
// MoverServer: where a line meets a moved door on a dedicated server, which
// draws no frame (vibe/docs/NATIVES.md, implemented, not as the original;
// dx-reverse-info engine-dll.md, traces: the movers in the BSP). One of the
// server's ServerActors (vibe/tools/dxcap.sh dedicated): 3 s into the map,
// the first door (a DeusExMover with bIsDoor, no glass) in name order, with
// a line across its box's middle along its thinnest side, 32 units past it
// each way, as MoverConsole's. Logged with "DXDEDI:", FastTrace's answer and
// the actor a Trace meets:
//   1. as the map has it;
//   2. the door made not to collide and moved 100 along its line, off it
//      into the open, still shown: its line at its old place and at its new;
//   3. 2 s on, both lines again. A dedicated server, with no client, files
//      no mover in the level's BSP at its load, as the original's: FastTrace
//      passes the door at either place, and Trace meets it only as an actor,
//      while it collides;
// then an exit.
//=============================================================================
class MoverServer extends Info;

var float MapTime, StepTime;
var int Step;
var DeusExMover Door;
var vector DoorS, DoorT, Shift;

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

function Ask(string Label, vector S, vector T)
{
	local Actor Hit;
	local vector HitLoc, HitNorm;

	Hit = Trace(HitLoc, HitNorm, T, S, true);
	Log("DXDEDI: " $ Label $ " fast " $ FastTrace(T, S) $ " trace " $ Hit);
}

event Tick(float Delta)
{
	local DeusExMover M;

	MapTime += Delta;
	StepTime += Delta;
	if (Step == 0 && MapTime > 3.0)
	{
		Log("DXDEDI: net mode " $ Level.NetMode $ ", " $ Level.GetLocalURL());
		foreach AllActors(class'DeusExMover', M)
		{
			if (!M.IsA('BreakableGlass') && M.bIsDoor && (Door == None || Caps(string(M.Name)) < Caps(string(Door.Name))))
				Door = M;
		}
		if (Door == None || !Across(Door, DoorS, DoorT))
		{
			Log("DXDEDI: no door, exiting");
			ConsoleCommand("exit");
			Step = 9;
			return;
		}
		Log("DXDEDI: door " $ Door.Name $ " at " $ Door.Location $ " collides " $ Door.bCollideActors $ " line " $ DoorS $ " to " $ DoorT);
		Ask("1 door", DoorS, DoorT);
		Door.SetCollision(false, false, false);
		Shift = Normal(DoorT - DoorS) * 100;
		Door.SetLocation(Door.Location + Shift);
		Log("DXDEDI: door now at " $ Door.Location $ " hidden " $ Door.bHidden $ " collides " $ Door.bCollideActors);
		Ask("2 door old", DoorS, DoorT);
		Ask("2 door new", DoorS + Shift, DoorT + Shift);
		Step = 1;
		StepTime = 0;
	}
	else if (Step == 1 && StepTime > 2.0)
	{
		Log("DXDEDI: door at " $ Door.Location $ " after " $ StepTime $ " s");
		Ask("3 door old", DoorS, DoorT);
		Ask("3 door new", DoorS + Shift, DoorT + Shift);
		Log("DXDEDI: done, exiting");
		ConsoleCommand("exit");
		Step = 9;
	}
}

defaultproperties
{
	RemoteRole=ROLE_None
}
