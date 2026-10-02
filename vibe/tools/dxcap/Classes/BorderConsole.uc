//=============================================================================
// BorderConsole: frames drawn with GC.DrawBorders, shot in both engines
// (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island the player is given a spread of items as BeltConsole
// gives them, then the inventory screen is opened and shot; then an item is
// selected there -- the lockpick, one cell, then the assault gun, a block of
// them -- and shot each time: a selected item's button draws its selection
// frame with DrawBorders. Three marked shots, then an exit. "DXBORDER:"
// lines say what was opened and selected.
//=============================================================================
class BorderConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int ShotCount;
var int Given;
var Inventory Pick, Gun;

// A shot, marked as BeltConsole marks its shots: the mark goes up first and
// stays up for the better part of a second, then the engine's own shot is
// taken under it.
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
	Log("DXBORDER: shot " $ ShotCount $ " = " $ Label);
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
		Log("DXBORDER: could not spawn " $ ItemClass);
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
	Log("DXBORDER: gave " $ item.Name);
	return item;
}

// Selects the item on the open inventory screen, as a click on its button does.
function SelectOnScreen(PlayerPawn P, Inventory item)
{
	local DeusExRootWindow root;
	local PersonaScreenInventory screen;

	root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
	if (root != None)
		screen = PersonaScreenInventory(root.GetTopWindow());
	if (screen == None)
	{
		Log("DXBORDER: no inventory screen to select " $ item.Name $ " on");
		return;
	}
	screen.SelectInventoryItem(item);
	Log("DXBORDER: selected " $ item.Name);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow root;
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
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// The items, a moment apart, so each lands as a pickup lands.
		if (StepTime > 0.3)
		{
			if (Given == 0)
				Gun = GiveItem(P, class'WeaponAssaultGun', 1);
			else if (Given == 1)
				GiveItem(P, class'Multitool', 2);
			else if (Given == 2)
				Pick = GiveItem(P, class'Lockpick', 3);
			else if (Given == 3)
				GiveItem(P, class'BioelectricCell', 1);
			else if (Given == 4)
				GiveItem(P, class'MedKit', 2);
			Given++;
			StepTime = 0;
			if (Given > 4)
				Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (StepTime > 1.0)
		{
			Log("DXBORDER: ShowInventoryWindow at " $ CapTime);
			DXP.ShowInventoryWindow();
			root = DeusExRootWindow(DXP.rootWindow);
			if (root != None && root.GetTopWindow() != None)
				Log("DXBORDER: top window " $ root.GetTopWindow().Class);
			else
				Log("DXBORDER: no top window");
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		if (StepTime > 1.5)
		{
			Shot(P, "the inventory screen");
			StepTime = 0;
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		if (StepTime > 0.5)
		{
			SelectOnScreen(P, Pick);
			StepTime = 0;
			Phase = 6;
		}
	}
	else if (Phase == 6)
	{
		if (StepTime > 1.0)
		{
			Shot(P, "the lockpick selected");
			StepTime = 0;
			Phase = 7;
		}
	}
	else if (Phase == 7)
	{
		if (StepTime > 0.5)
		{
			SelectOnScreen(P, Gun);
			StepTime = 0;
			Phase = 8;
		}
	}
	else if (Phase == 8)
	{
		if (StepTime > 1.0)
		{
			Shot(P, "the assault gun selected");
			StepTime = 0;
			Phase = 9;
		}
	}
	else if (Phase == 9)
	{
		if (StepTime > 1.0)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 10;
		}
	}
}
