//=============================================================================
// VisibleConsole: what FastTrace and the visible-actor iterators give --
// Liberty Island opened from wherever the run starts, the player left where
// the map starts it; 2 s into the level's time, for each mover a line
// across its box's thinnest side through the middle, 32 units past it each
// way: FastTrace's answer and the actor a Trace meets; then each actor that
// VisibleCollidingActors lists within 1000 units of the player, with and
// without bIgnoreHidden, and that VisibleActors lists within 1000, one line
// each; then an exit.
//=============================================================================
class VisibleConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;

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

function Movers(PlayerPawn P)
{
	local Mover M;
	local Actor Hit;
	local vector Min, Max, C, E, A, S, T, HitLoc, HitNorm;

	foreach P.AllActors(class'Mover', M)
	{
		if (!M.GetBoundingBox(Min, Max))
		{
			Log("DXVIS: mover " $ M.Name $ " no box");
			continue;
		}
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
		Hit = P.Trace(HitLoc, HitNorm, T, S, true);
		Log("DXVIS: mover " $ M.Name $ " fast " $ P.FastTrace(T, S) $ " trace " $ Hit);
	}
}

function Iterators(PlayerPawn P)
{
	local Actor A;

	foreach P.VisibleCollidingActors(class'Actor', A, 1000, P.Location)
		Log("DXVIS: colliding " $ A.Name);
	foreach P.VisibleCollidingActors(class'Actor', A, 1000, P.Location, true)
		Log("DXVIS: colliding unhidden " $ A.Name);
	foreach P.VisibleActors(class'Actor', A, 1000, P.Location)
		Log("DXVIS: visible " $ A.Name);
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXVIS: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Movers(P);
		Iterators(P);
		Log("DXVIS: exiting");
		P.ConsoleCommand("exit");
		Step = 1;
	}
}
