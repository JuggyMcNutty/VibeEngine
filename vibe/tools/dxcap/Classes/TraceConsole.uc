//=============================================================================
// TraceConsole: what TraceTexture gives -- Liberty Island opened from
// wherever the run starts, the player left at its start; 2 s into the
// level's time, for each NPC in the world, the hits of a line from the
// player's eye to it, and the first hit of a line from it straight down 200
// units (its floor, as footsteps read it): each hit's actor, texture, group,
// flags and distance, one line each. Then, toward each decoration within
// 3000 units and on past it as far again, from and to whole units the same
// in both engines: TraceActors' hits of Actor and of ScriptedPawn (each
// hit's actor, distance and normal, in the order given), TraceTexture's,
// Trace's first hit of the level and FastTrace's answer. Then an exit.
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

function string ActorHits(PlayerPawn P, class<Actor> C, vector Start, vector End, int Max)
{
	local Actor Hit;
	local vector HitLoc, HitNorm;
	local int Count;
	local string S;

	foreach P.TraceActors(C, Hit, HitLoc, HitNorm, End, Start)
	{
		S = S $ " [" $ Hit.Name $ " at " $ int(VSize(HitLoc - Start)) $ " n " $ int(HitNorm.X * 100) $ "," $ int(HitNorm.Y * 100) $ "," $ int(HitNorm.Z * 100) $ "]";
		if (++Count >= Max)
			break;
	}
	return S;
}

// The level's first hit as Trace gives it (with nothing hit, the location
// and normal it leaves), and whether FastTrace is clear.
function string Single(PlayerPawn P, vector Start, vector End)
{
	local Actor Hit;
	local vector HitLoc, HitNorm;

	HitLoc = vect(1,2,3);
	HitNorm = vect(4,5,6);
	Hit = P.Trace(HitLoc, HitNorm, End, Start, false);
	if (Hit == None)
		return " [None, location " $ HitLoc $ " normal " $ HitNorm $ "] fast " $ P.FastTrace(End, Start);
	return " [" $ Hit $ " at " $ int(VSize(HitLoc - Start)) $ " z " $ int(HitLoc.Z) $ "] fast " $ P.FastTrace(End, Start);
}

function Traces(PlayerPawn P)
{
	local ScriptedPawn S;
	local DeusExDecoration D;
	local vector Eye, Start, Aim, End;

	Eye = P.Location + vect(0,0,1) * P.EyeHeight;
	foreach P.AllActors(class'ScriptedPawn', S)
	{
		if (!S.bInWorld)
			continue;
		Log("DXTRACE: " $ S.Name $ " sight" $ Hits(P, Eye, S.Location, 3));
		Log("DXTRACE: " $ S.Name $ " floor" $ Hits(P, S.Location, S.Location - vect(0,0,200), 1));
	}
	Log("DXTRACE: player floor" $ Hits(P, P.Location, P.Location - vect(0,0,200), 1));
	Log("DXTRACE: player eye " $ Eye);
	// The lines to the decorations start and end at whole units, the same
	// in both engines whatever height the player or a decoration came to
	// rest at: the eye's place truncated, aimed 50 units under it at the
	// decoration's X and Y truncated.
	Start.X = int(Eye.X);
	Start.Y = int(Eye.Y);
	Start.Z = int(Eye.Z);
	foreach P.AllActors(class'DeusExDecoration', D)
	{
		if (VSize(D.Location - Eye) > 3000)
			continue;
		Aim.X = int(D.Location.X);
		Aim.Y = int(D.Location.Y);
		Aim.Z = Start.Z - 50;
		End = Start + 2 * (Aim - Start);
		Log("DXTRACE: " $ D.Name $ " actors" $ ActorHits(P, class'Actor', Start, End, 8));
		Log("DXTRACE: " $ D.Name $ " pawns" $ ActorHits(P, class'ScriptedPawn', Start, End, 8));
		Log("DXTRACE: " $ D.Name $ " at " $ D.Location $ " surfaces" $ Hits(P, Start, End, 8));
		Log("DXTRACE: " $ D.Name $ " single" $ Single(P, Start, End));
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
