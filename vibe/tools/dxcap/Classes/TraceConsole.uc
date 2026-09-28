//=============================================================================
// TraceConsole: what TraceTexture gives -- Liberty Island opened from
// wherever the run starts, the player left at its start; 2 s into the
// level's time, for each NPC in the world, the hits of a line from the
// player's eye to it, and the first hit of a line from it straight down 200
// units (its floor, as footsteps read it): each hit's actor, texture, group,
// flags and distance, one line each; then an exit.
//=============================================================================
class TraceConsole extends Console;

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

function string Hits(PlayerPawn P, vector Start, vector End, int Max)
{
	local Actor Hit;
	local name TexName, TexGroup;
	local vector HitLoc, HitNorm;
	local int Flags, Count;
	local string S;

	foreach P.TraceTexture(class'Actor', Hit, TexName, TexGroup, Flags, HitLoc, HitNorm, End, Start)
	{
		S = S $ " [" $ Hit.Name $ " " $ TexName $ " " $ TexGroup $ " " $ Flags $ " at " $ int(VSize(HitLoc - Start)) $ "]";
		if (++Count >= Max)
			break;
	}
	return S;
}

function Traces(PlayerPawn P)
{
	local ScriptedPawn S;
	local vector Eye;

	Eye = P.Location + vect(0,0,1) * P.EyeHeight;
	foreach P.AllActors(class'ScriptedPawn', S)
	{
		if (!S.bInWorld)
			continue;
		Log("DXTRACE: " $ S.Name $ " sight" $ Hits(P, Eye, S.Location, 3));
		Log("DXTRACE: " $ S.Name $ " floor" $ Hits(P, S.Location, S.Location - vect(0,0,200), 1));
	}
	Log("DXTRACE: player floor" $ Hits(P, P.Location, P.Location - vect(0,0,200), 1));
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
			Log("DXTRACE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Traces(P);
		Log("DXTRACE: exiting");
		P.ConsoleCommand("exit");
		Step = 1;
	}
}
