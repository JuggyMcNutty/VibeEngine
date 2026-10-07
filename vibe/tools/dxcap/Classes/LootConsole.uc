//=============================================================================
// LootConsole: carcasses looted and the inventory grid after, logged with
// "DXLOOT:" and the console's clock (vibe/docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island: an isolated ScriptedPawn killed outright and its
// carcass searched (DeusExCarcass.Frob, the player's own frob), then, 30 s
// later, a second one. The grid is logged after each search and every 10 s
// between: the player's invSlots row by row, each item's place and size and
// its belt slot, and whether invSlots holds exactly the cells the items in
// the grid cover. A timeline (DXCAP_TIMELINE) drags items in the inventory
// screen between the searches; the grid must still match after.
//=============================================================================
class LootConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var string CurrentMap;
var int Phase;
var int Searches;
var ScriptedPawn Victim;
var vector DeathSpot;
var bool bTravelled;

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

// The most isolated scripted pawn at least 400 units from the player, as
// DeathConsole picks its victims: a human, so it leaves a carcass.
function ScriptedPawn FindVictim(PlayerPawn P)
{
	local ScriptedPawn A, B, Best;
	local float Near, BestNear;

	foreach P.AllActors(class'ScriptedPawn', A)
	{
		if (A.bImportant || A.Health <= 0 || A.bHidden || A.IsA('Robot') || A.IsA('Animal'))
			continue;
		Near = 100000;
		foreach P.AllActors(class'ScriptedPawn', B)
		{
			if (B == A || B.Health <= 0)
				continue;
			if (VSize(B.Location - A.Location) < Near)
				Near = VSize(B.Location - A.Location);
		}
		if (VSize(A.Location - P.Location) < 400)
			continue;
		if (Near > BestNear)
		{
			BestNear = Near;
			Best = A;
		}
	}
	return Best;
}

// The grid as the items say it is, against invSlots.
function LogGrid(DeusExPlayer P, string Label)
{
	local Inventory item;
	local int Want[30];
	local int x, y, i;
	local string Row;
	local bool bMatch;

	Log("DXLOOT: grid " $ Label $ " at " $ MapTime);
	for (item = P.Inventory; item != None; item = item.Inventory)
	{
		Log("DXLOOT:   " $ item.Name $ " pos " $ item.invPosX $ "," $ item.invPosY
		    $ " size " $ item.invSlotsX $ "x" $ item.invSlotsY
		    $ " belt " $ item.bInObjectBelt $ " " $ item.beltPos);
		if (item.invPosX < 0 || item.invPosY < 0)
			continue;
		for (y = item.invPosY; y < item.invPosY + item.invSlotsY; y++)
			for (x = item.invPosX; x < item.invPosX + item.invSlotsX; x++)
				if (x < P.maxInvCols && y < P.maxInvRows)
					Want[y * P.maxInvCols + x]++;
	}
	bMatch = true;
	for (y = 0; y < P.maxInvRows; y++)
	{
		Row = "";
		for (x = 0; x < P.maxInvCols; x++)
		{
			i = y * P.maxInvCols + x;
			Row = Row $ P.invSlots[i] $ Want[i] $ " ";
			if (P.invSlots[i] != Min(Want[i], 1) || Want[i] > 1)
				bMatch = false;
		}
		Log("DXLOOT:   row " $ y $ ": " $ Row);
	}
	Log("DXLOOT: grid " $ Label $ " matches the items: " $ bMatch);
}

// The victim killed outright; the player stood beside the spot.
function Kill(PlayerPawn P)
{
	local vector Dir;

	Dir = vector(Victim.Rotation);
	DeathSpot = Victim.Location;
	Log("DXLOOT: killing " $ Victim.Name $ " (" $ Victim.Class.Name $ ") at " $ DeathSpot);
	P.SetLocation(DeathSpot + Dir * 100);
	P.SetRotation(rotator(-Dir));
	P.ViewRotation = P.Rotation;
	Victim.TakeDamage(1000, P, DeathSpot + Dir * 10, -Dir * 1000, 'Shot');
}

// The carcass nearest the death spot, searched as the player's frob does.
function Search(DeusExPlayer P)
{
	local DeusExCarcass C, Best;

	foreach P.AllActors(class'DeusExCarcass', C)
		if (VSize(C.Location - DeathSpot) < 300 && (Best == None || VSize(C.Location - DeathSpot) < VSize(Best.Location - DeathSpot)))
			Best = C;
	if (Best == None)
	{
		Log("DXLOOT: no carcass near " $ DeathSpot);
		return;
	}
	Log("DXLOOT: searching " $ Best.Name);
	Best.Frob(P, None);
	Searches++;
	LogGrid(P, "after search " $ Searches);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
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
	// Only the island needs Deus Ex's player; the menu map's travels.
	if (DXP == None && M ~= "01_NYC_UNATCOIsland")
		return;
	P.ReducedDamageType = 'All';

	if (Phase >= 2 && Phase < 9)
	{
		LogTime += Delta;
		if (LogTime >= 10.0)
		{
			LogTime = 0;
			LogGrid(DXP, "now");
		}
	}

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0 && !bTravelled)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				bTravelled = true;
			}
		}
		else if (MapTime > 6.0)
		{
			LogGrid(DXP, "at the start");
			Phase = 1;
		}
	}
	else if (Phase == 1 || Phase == 4)
	{
		if (Phase == 1 || MapTime > 40.0)
		{
			Victim = FindVictim(P);
			if (Victim == None)
			{
				Log("DXLOOT: no victim, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Kill(P);
			StepTime = 0;
			Phase++;
		}
	}
	else if (Phase == 2 || Phase == 5)
	{
		if (StepTime > 4.0)
		{
			Search(DXP);
			StepTime = 0;
			Phase++;
		}
	}
	else if (Phase == 3)
	{
		// The timeline's drags happen here.
		Phase = 4;
	}
	else if (Phase == 6)
	{
		if (StepTime > 20.0)
		{
			LogGrid(DXP, "at the end");
			Log("DXLOOT: " $ Searches $ " searches, done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
