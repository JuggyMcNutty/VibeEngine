//=============================================================================
// ReturnConsole: a return to a map within its mission -- Liberty Island,
// UNATCO HQ, then Liberty Island again, which comes back from Current as
// the player left it. 6 s into each map it logs, with "DXRETURN:", the game
// (Level.Game), its BaseMutator, and whether the player holds its
// augmentation and skill systems and its key ring; it exits 6 s into the
// third. The original keeps a returned level's game (dx-reverse-info
// engine-dll.md, a level's tick), so all are there each time. A LoadMarker
// spawned on the first visit to Liberty Island logs its PostPostBeginPlay
// calls each time there: one, then two on the return.
//=============================================================================
class ReturnConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var bool bLogged;

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

function Report(DeusExPlayer P, string Map)
{
	local LoadMarker M;

	Log("DXRETURN: in " $ Map $ " game " $ P.Level.Game $ " BaseMutator " $ P.Level.Game.BaseMutator
		$ " augs " $ (P.AugmentationSystem != None) $ " skills " $ (P.SkillSystem != None) $ " keys " $ (P.KeyRing != None));
	foreach P.AllActors(class'LoadMarker', M)
		Log("DXRETURN: in " $ Map $ " " $ M.Name $ " PostPostBeginPlay " $ M.Begun $ " times");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Map, Next;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	Map = MapOf(P);
	if (Map != CurrentMap)
	{
		CurrentMap = Map;
		MapTime = 0;
		bLogged = false;
	}
	MapTime += Delta;
	if (MapTime < 6.0 || bLogged)
		return;
	bLogged = true;
	if (Step == 0 && Map != "01_NYC_UNATCOIsland")
		Next = "01_NYC_UNATCOIsland";
	else
	{
		if (Step == 0)
			P.Spawn(class'LoadMarker');
		if (DeusExPlayer(P) != None)
			Report(DeusExPlayer(P), Map);
		Step++;
		if (Step == 1)
			Next = "01_NYC_UNATCOHQ";
		else if (Step == 2)
			Next = "01_NYC_UNATCOIsland";
	}
	if (Next != "")
	{
		Log("DXRETURN: opening " $ Next);
		P.ConsoleCommand("open " $ Next);
	}
	else
	{
		Log("DXRETURN: exiting");
		P.ConsoleCommand("exit");
	}
}
