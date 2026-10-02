//=============================================================================
// MoveConsole: how the level's animals and bots move -- every in-world
// ScriptedPawn's state, orders, move target and destination logged with
// "DXMOVE:" every two seconds, with the distance it moved since the last
// line, so a pawn that stands still in a moving state (a patrol that never
// leaves its node, a wanderer that never picks a spot) shows as a run of
// moved-0 lines beside the original's moving ones (vibe/docs/DEVELOPMENT.md,
// scripted runs).
//
// On Liberty Island, 120 s of the level's own time, the player left alone
// at its start. What is compared between the engines: which pawns move at
// all, which states they pass through, what they chase (move target,
// destination) and how far they get -- the animals (the rats and the
// gulls) and the UNATCO security bots among them.
//=============================================================================
class MoveConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var string CurrentMap;
var int Phase;

// Each watched pawn's place at the last line, for the distance moved.
var ScriptedPawn Watched[64];
var vector LastPos[64];
var int NumWatched;

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

function Travel(PlayerPawn P, string Map)
{
	Log("DXCAP: opening " $ Map);
	P.ConsoleCommand("open " $ Map);
}

// One line per in-world scripted pawn: what it is doing and where it is
// going, with the distance moved since the last line.
function Census(PlayerPawn P)
{
	local ScriptedPawn S;
	local int i, Count;
	local string MT;

	foreach P.AllActors(class'ScriptedPawn', S)
	{
		// The level keeps pawns out of the world, hidden; they are not
		// moving and would only pad the log.
		if (!S.bInWorld || S.bHidden)
			continue;
		Count++;

		for (i = 0; i < NumWatched; i++)
			if (Watched[i] == S)
				break;
		if (i == NumWatched)
		{
			if (NumWatched < ArrayCount(Watched))
			{
				i = NumWatched++;
				Watched[i] = S;
				LastPos[i] = S.Location;
			}
			else
				i = -1;
		}
		if (i < 0)
			continue;

		MT = "None";
		if (S.MoveTarget != None)
			MT = string(S.MoveTarget.Name);
		Log("DXMOVE: " $ int(P.Level.TimeSeconds) $ " " $ S.Name $ " " $ S.Class.Name
		    $ " state " $ S.GetStateName()
		    $ " orders " $ S.Orders
		    $ " mt " $ MT
		    $ " dest " $ int(S.Destination.X) $ "," $ int(S.Destination.Y) $ "," $ int(S.Destination.Z)
		    $ " vel " $ int(VSize(S.Velocity))
		    $ " at " $ int(S.Location.X) $ "," $ int(S.Location.Y) $ "," $ int(S.Location.Z)
		    $ " moved " $ int(VSize(S.Location - LastPos[i])));
		LastPos[i] = S.Location;
	}
	Log("DXMOVE: " $ int(P.Level.TimeSeconds) $ " " $ Count $ " in-world pawns");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
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
	LogTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Travel(P, "01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		// Two seconds of quiet, then a line every two seconds for two
		// minutes.
		if (StepTime > 2.0)
		{
			Census(P);
			LogTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (LogTime > 2.0)
		{
			Census(P);
			LogTime = 0;
		}
		if (MapTime > 122.0)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
}
