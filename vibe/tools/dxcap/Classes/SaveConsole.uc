//=============================================================================
// SaveConsole: a save for the other engine to load -- Liberty Island opened
// from wherever the run starts; 8 s in, a LoadMarker is spawned and what the
// level holds is logged (the player, its place and inventory, the game, how
// many actors, pawns and inventory items, the mission script, and the
// marker's PostPostBeginPlay calls), then the game is saved to slot 9 (by a
// SaveHelper, on the next actors' tick) and the run exits 3 s later.
// LoadConsole loads that slot and logs the same: no save holds the mission
// script, and the loaded player's TravelPostAccept spawns it again.
//=============================================================================
class SaveConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	// The options first: the original's level keeps the URL it was saved
	// with, a path for one loaded from a save (..\Save\Current\<map>.dxs).
	U = P.Level.GetLocalURL();
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, "\\");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "\\");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

// What the level holds, logged with "DXSAVE:" in front.
function Census(PlayerPawn P, string Label)
{
	local Actor A;
	local Inventory Inv;
	local LoadMarker M;
	local MissionScript S;
	local int Actors, Pawns, Items;
	local string Carried;

	foreach P.AllActors(class'Actor', A)
	{
		Actors++;
		if (Pawn(A) != None)
			Pawns++;
		if (Inventory(A) != None)
			Items++;
	}
	for (Inv = P.Inventory; Inv != None; Inv = Inv.Inventory)
		Carried = Carried $ " " $ Inv.Class.Name;
	Log("DXSAVE: " $ Label $ " in " $ MapOf(P) $ ": player " $ P.Class.Name $ " at " $ P.Location $ " health " $ P.Health);
	Log("DXSAVE: " $ Label $ " carries" $ Carried);
	Log("DXSAVE: " $ Label $ " game " $ P.Level.Game $ ", " $ Actors $ " actors, " $ Pawns $ " pawns, " $ Items $ " inventory items");
	foreach P.AllActors(class'MissionScript', S)
		Log("DXSAVE: " $ Label $ " mission script " $ S.Name $ " (" $ S.Class.Name $ ")");
	foreach P.AllActors(class'LoadMarker', M)
		Log("DXSAVE: " $ Label $ " " $ M.Name $ " PostPostBeginPlay " $ M.Begun $ " times");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local SaveHelper S;

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
			Log("DXSAVE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 8.0)
	{
		P.Spawn(class'LoadMarker');
		Census(P, "saved");
		S = P.Spawn(class'SaveHelper', P);
		S.Slot = 9;
		S.Description = "DXCapture save";
		Step = 1;
	}
	else if (Step == 1 && MapTime > 11.0)
	{
		Log("DXSAVE: exiting");
		P.ConsoleCommand("exit");
		Step = 2;
	}
}
