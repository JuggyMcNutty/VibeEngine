//=============================================================================
// SightConsole: what LineOfSightTo, CanSee and PlayerCanSeeMe give --
// Liberty Island opened from wherever the run starts, the player left where
// the map starts it; 2 s into the level's time, for each NPC in the world and
// each light within 4000 units of the player: the player's LineOfSightTo to
// it, with and without bIgnoreDistance, its CanSee of it, and the actor's
// PlayerCanSeeMe, one line each; then an exit.
//=============================================================================
class SightConsole extends Console;

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

function LogSight(PlayerPawn P, Actor A)
{
	Log("DXSIGHT: " $ A.Name $ " los " $ P.LineOfSightTo(A) $ " far " $ P.LineOfSightTo(A, true)
		$ " cansee " $ P.CanSee(A) $ " seen " $ A.PlayerCanSeeMe() $ " at " $ int(VSize(A.Location - P.Location)));
}

function Sights(PlayerPawn P)
{
	local ScriptedPawn S;
	local Light L;

	foreach P.AllActors(class'ScriptedPawn', S)
	{
		if (S.bInWorld)
			LogSight(P, S);
	}
	foreach P.AllActors(class'Light', L)
	{
		if (VSize(L.Location - P.Location) < 4000)
			LogSight(P, L);
	}
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
			Log("DXSIGHT: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Sights(P);
		Log("DXSIGHT: exiting");
		P.ConsoleCommand("exit");
		Step = 1;
	}
}
