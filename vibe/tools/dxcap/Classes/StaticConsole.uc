//=============================================================================
// StaticConsole: what a level's static actors do over time. The map is the
// ini's TargetMap ([DXCapture.StaticConsole]), Liberty Island by default.
// At 5 s and again at 15 s, every bStatic actor that animates or has a
// timer, a life span, physics or a state of its own is logged with
// "DXSTATIC:" -- its animation and frame, timer and count, life span,
// physics, state, place and rotation -- then a count of the static actors
// by class. An engine that ticks static actors shows their frames, timers
// and turns moving on between the two passes; one that never ticks them
// (the original: its level ticks from its first dynamic actor) shows them
// as they were.
//=============================================================================
class StaticConsole extends Console;

var config string TargetMap;

var string CurrentMap;
var float MapTime;
var int Phase;

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

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function bool Busy(Actor A)
{
	return A.IsAnimating() || A.TimerRate > 0 || A.LifeSpan > 0 || A.Physics != PHYS_None
	    || (A.GetStateName() != 'None' && A.GetStateName() != A.Class.Name);
}

function Census(PlayerPawn P, int Pass)
{
	local Actor A;
	local name Classes[64];
	local int Counts[64];
	local int i, NumClasses, NumStatic, NumBusy;
	local string Line;

	foreach P.AllActors(class'Actor', A)
	{
		if (!A.bStatic)
			continue;
		NumStatic++;
		for (i = 0; i < NumClasses; i++)
			if (Classes[i] == A.Class.Name)
				break;
		if (i == NumClasses && NumClasses < ArrayCount(Classes))
		{
			Classes[i] = A.Class.Name;
			NumClasses++;
		}
		if (i < ArrayCount(Classes))
			Counts[i]++;
		if (!Busy(A))
			continue;
		NumBusy++;
		Log("DXSTATIC: pass " $ Pass $ " " $ A.Name $ " " $ A.Class.Name
		    $ " anim " $ A.AnimSequence $ " frame " $ A.AnimFrame $ " rate " $ A.AnimRate
		    $ " timer " $ A.TimerRate $ " count " $ A.TimerCounter
		    $ " life " $ A.LifeSpan $ " physics " $ A.Physics $ " state " $ A.GetStateName()
		    $ " at " $ A.Location $ " rot " $ A.Rotation);
	}
	Line = "DXSTATIC: pass " $ Pass $ " at " $ P.Level.TimeSeconds $ ": " $ NumStatic $ " static, " $ NumBusy $ " busy;";
	for (i = 0; i < NumClasses; i++)
		Line = Line $ " " $ Classes[i] $ " " $ Counts[i];
	Log(Line);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXSTATIC: now in " $ M);
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXSTATIC: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == -1 && (M ~= Target()))
		Phase = 1;
	else if (Phase == 1 && MapTime > 5.0)
	{
		Census(P, 1);
		Phase = 2;
	}
	else if (Phase == 2 && MapTime > 15.0)
	{
		Census(P, 2);
		Log("DXSTATIC: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 3;
	}
}
