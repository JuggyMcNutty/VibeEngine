//=============================================================================
// HeldConsole: what a pawn holds, drawn on its mesh -- the player seen from
// behind (bBehindView) on Liberty Island, holding a multitool (no weapon:
// the selected item is drawn at the weapon triangle) and then the assault
// gun, a marked shot of each (vibe/docs/DEVELOPMENT.md, scripted runs).
// "DXHELD:" lines log the player's Weapon, SelectedItem and inHand at each.
//=============================================================================
class HeldConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int ShotCount;
var Inventory Tool, Gun;

var int MarkShot;
var bool bMarking;
var float MarkTime;
var bool bShotPending;

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

function Shot(PlayerPawn P, string Label)
{
	Log("DXHELD: shot " $ ShotCount $ " = " $ Label);
	MarkShot = ShotCount;
	bMarking = true;
	MarkTime = 0;
	bShotPending = true;
}

function TickShot(PlayerPawn P, float Delta)
{
	if (!bShotPending)
		return;
	MarkTime += Delta;
	if (MarkTime > 0.8)
	{
		P.ConsoleCommand("shot");
		bMarking = false;
		bShotPending = false;
		ShotCount++;
	}
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

// An item given as the game's own pickup gives it; a weapon's ammo is
// spawned beside it as Weapon's travel code does (BeltConsole).
function Inventory GiveItem(PlayerPawn P, class<Inventory> ItemClass, int Copies)
{
	local DeusExPlayer DXP;
	local Inventory item;
	local Ammo ammo;

	DXP = DeusExPlayer(P);
	item = P.Spawn(ItemClass,,, P.Location + vect(40,0,0));
	if (item == None)
	{
		Log("DXHELD: could not spawn " $ ItemClass);
		return None;
	}
	if (DeusExPickup(item) != None && Copies > 1)
		DeusExPickup(item).NumCopies = Copies;
	if (DeusExWeapon(item) != None && DeusExWeapon(item).AmmoType == None && DeusExWeapon(item).AmmoName != None)
	{
		ammo = P.Spawn(DeusExWeapon(item).AmmoName);
		if (ammo != None)
		{
			DXP.AddInventory(ammo);
			ammo.BecomeItem();
			ammo.AmmoAmount = DeusExWeapon(item).PickUpAmmoCount;
			ammo.GotoState('Idle2');
		}
	}
	item.GiveTo(DXP);
	Log("DXHELD: gave " $ item.Name);
	return item;
}

// Which belt slot holds the item, -1 for none (BeltConsole).
function int PosOf(PlayerPawn P, Inventory item)
{
	local DeusExPlayer DXP;
	local int i;

	DXP = DeusExPlayer(P);
	for (i = 0; i < 10; i++)
		if (DeusExRootWindow(DXP.rootWindow).hud.belt.objects[i].GetItem() == item)
			return i;
	return -1;
}

function string ItemName(Inventory item)
{
	if (item == None)
		return "None";
	return string(item.Name);
}

function LogHeld(PlayerPawn P, string Label)
{
	local DeusExPlayer DXP;
	DXP = DeusExPlayer(P);
	Log("DXHELD: " $ Label $ ": weapon " $ ItemName(DXP.Weapon) $ " selected " $ ItemName(DXP.SelectedItem)
	    $ " inHand " $ ItemName(DXP.inHand) $ " transitioning " $ DXP.bInHandTransition
	    $ " behind " $ DXP.bBehindView);
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
	P.ReducedDamageType = 'All';
	TickShot(P, Delta);
	if (bShotPending)
		return;

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 6.0)
		{
			P.SetPhysics(PHYS_Walking);
			Tool = GiveItem(P, class'Multitool', 1);
			Gun = GiveItem(P, class'WeaponAssaultGun', 1);
			P.bBehindView = true;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (StepTime > 1.0)
		{
			Log("DXHELD: ActivateBelt " $ PosOf(P, Tool) $ " (the multitool)");
			DXP.ActivateBelt(PosOf(P, Tool));
			StepTime = 0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (StepTime > 2.5)
		{
			LogHeld(P, "the multitool in hand");
			Shot(P, "the multitool in hand, from behind");
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		if (StepTime > 1.0)
		{
			Log("DXHELD: ActivateBelt " $ PosOf(P, Gun) $ " (the assault gun)");
			DXP.ActivateBelt(PosOf(P, Gun));
			StepTime = 0;
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		if (StepTime > 2.5)
		{
			LogHeld(P, "the assault gun in hand");
			Shot(P, "the assault gun in hand, from behind");
			StepTime = 0;
			Phase = 6;
		}
	}
	else if (Phase == 6)
	{
		if (StepTime > 1.0)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 7;
		}
	}
}
