//=============================================================================
// CarcassConsole: what a carcass holds and what its search hands over, logged
// with "DXCARC:" and the console's clock (vibe/docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island, 6 s in: every carcass's inventory chain -- the map's own
// carcasses spawn theirs in PostBeginPlay, during the level's start, and the
// start's SetInitialState sends them to Idle2, hidden on the carcass. Then
// UNATCOTroopCarcass0 is searched (DeusExCarcass.Frob, the player's own
// frob), the player's chain and the carcass's are logged, the weapon it held
// is put in hand from the inventory, and 3 s later the hand emptied; the
// hand is logged every half second -- a weapon owned by none never finishes
// going down. Then an isolated ScriptedPawn is killed, as LootConsole kills
// its own, and its carcass logged, searched and its weapon put in hand and
// away the same.
//=============================================================================
class CarcassConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float HandTime;
var string CurrentMap;
var int Phase;
var bool bTravelled;
var ScriptedPawn Victim;
var vector DeathSpot;
var DeusExCarcass Searched;
var Inventory Loot;

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

function string NameOf(Actor A)
{
	if (A == None)
		return "None";
	return string(A.Name);
}

// A holder's inventory chain, each item's state, flags and owner; a chain
// longer than 40 does not end.
function LogChain(Actor Holder, string Label)
{
	local Inventory item;
	local int n;

	Log("DXCARC: " $ Label $ " " $ NameOf(Holder) $ " at " $ MapTime);
	if (Holder == None)
		return;
	for (item = Holder.Inventory; item != None && n < 40; item = item.Inventory)
	{
		Log("DXCARC:   " $ item.Name $ " " $ item.GetStateName()
		    $ " hidden " $ item.bHidden $ " physics " $ item.Physics
		    $ " owner " $ NameOf(item.Owner)
		    $ " off " $ int(VSize(item.Location - Holder.Location))
		    $ " belt " $ item.bInObjectBelt $ " " $ item.beltPos);
		n++;
	}
	if (n >= 40)
		Log("DXCARC:   the chain does not end");
}

function LogHand(DeusExPlayer P)
{
	Log("DXCARC: hand at " $ MapTime $ ": inHand " $ NameOf(P.inHand)
	    $ " (" $ GetStateOf(P.inHand) $ ") pending " $ NameOf(P.inHandPending)
	    $ " weapon " $ NameOf(P.Weapon) $ " transition " $ P.bInHandTransition);
}

function string GetStateOf(Actor A)
{
	if (A == None)
		return "";
	return string(A.GetStateName());
}

// The first weapon in a holder's chain.
function Inventory WeaponIn(Actor Holder)
{
	local Inventory item;
	local int n;

	for (item = Holder.Inventory; item != None && n < 40; item = item.Inventory)
	{
		if (DeusExWeapon(item) != None)
			return item;
		n++;
	}
	return None;
}

// The most isolated scripted pawn at least 400 units from the player, as
// LootConsole picks its victims: a human, so it leaves a carcass.
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

function Kill(PlayerPawn P)
{
	local vector Dir;

	Dir = vector(Victim.Rotation);
	DeathSpot = Victim.Location;
	Log("DXCARC: killing " $ Victim.Name $ " (" $ Victim.Class.Name $ ") at " $ DeathSpot);
	P.SetLocation(DeathSpot + Dir * 100);
	P.SetRotation(rotator(-Dir));
	P.ViewRotation = P.Rotation;
	Victim.TakeDamage(1000, P, DeathSpot + Dir * 10, -Dir * 1000, 'Shot');
}

function DeusExCarcass NearestCarcass(PlayerPawn P)
{
	local DeusExCarcass C, Best;

	foreach P.AllActors(class'DeusExCarcass', C)
		if (VSize(C.Location - DeathSpot) < 300 && (Best == None || VSize(C.Location - DeathSpot) < VSize(Best.Location - DeathSpot)))
			Best = C;
	return Best;
}

// The carcass searched as the player's frob does, the chains after.
function Search(DeusExPlayer P, DeusExCarcass C)
{
	Searched = C;
	Loot = WeaponIn(C);
	Log("DXCARC: searching " $ NameOf(C) $ " for " $ NameOf(Loot));
	C.Frob(P, None);
	LogChain(P, "after the search, the player");
	LogChain(C, "after the search, the carcass");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExCarcass C;
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

	// The hand, every half second while a weapon goes up and down.
	if (Phase == 3 || Phase == 4 || Phase == 8 || Phase == 9)
	{
		HandTime += Delta;
		if (HandTime >= 0.5)
		{
			HandTime = 0;
			LogHand(DXP);
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
			foreach P.AllActors(class'DeusExCarcass', C)
				LogChain(C, "at the start");
			LogChain(P, "at the start, the player");
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		foreach P.AllActors(class'DeusExCarcass', C)
			if (C.Name == 'UNATCOTroopCarcass0')
				Searched = C;
		if (Searched == None)
		{
			Log("DXCARC: no UNATCOTroopCarcass0");
			Phase = 5;
		}
		else
		{
			Search(DXP, Searched);
			Phase = 2;
		}
		StepTime = 0;
	}
	else if (Phase == 2 || Phase == 7)
	{
		if (StepTime > 1.0)
		{
			Log("DXCARC: putting " $ NameOf(Loot) $ " in hand");
			DXP.PutInHand(Loot);
			StepTime = 0;
			HandTime = 0;
			Phase++;
		}
	}
	else if (Phase == 3 || Phase == 8)
	{
		if (StepTime > 3.0)
		{
			Log("DXCARC: emptying the hand");
			DXP.PutInHand(None);
			StepTime = 0;
			Phase++;
		}
	}
	else if (Phase == 4)
	{
		if (StepTime > 5.0)
		{
			StepTime = 0;
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		Victim = FindVictim(P);
		if (Victim == None)
		{
			Log("DXCARC: no victim, exiting");
			P.ConsoleCommand("exit");
			Phase = 11;
			return;
		}
		Kill(P);
		StepTime = 0;
		Phase = 6;
	}
	else if (Phase == 6)
	{
		if (StepTime > 4.0)
		{
			C = NearestCarcass(P);
			LogChain(C, "killed");
			if (C == None)
			{
				Log("DXCARC: no carcass near " $ DeathSpot $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 11;
				return;
			}
			Search(DXP, C);
			StepTime = 0;
			Phase = 7;
		}
	}
	else if (Phase == 9)
	{
		if (StepTime > 5.0)
		{
			LogChain(P, "at the end, the player");
			Log("DXCARC: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 11;
		}
	}
}
