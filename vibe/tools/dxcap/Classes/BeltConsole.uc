//=============================================================================
// BeltConsole: the object belt -- its slots, their text, and the use, swap
// and pickup paths through it, logged with "DXBELT:" and the console's
// clock, with shots of the HUD (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island: the belt as the level starts it; items given as the
// game's own pickup gives them (Inventory.GiveTo, a weapon's ammo spawned
// beside it) with the belt logged after each; an item used (ActivateBelt,
// the number keys' path), another selected over it (the swap, with the
// player's inHand states through the transition), a new item picked up
// after that, and the belt logged after every step.
//
// What is compared between the engines: each slot's item, its
// BeltDescription and the slot's itemText (the counts), its Icon, the
// player's inHand and bInHandTransition through the swaps, and which slot
// each item lands in. The belt's own drawing is read from the shots.
//=============================================================================
class BeltConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var string CurrentMap;
var int Phase;
var int ShotCount;
var Inventory Gun, Tool, Pick, Cell;
var int GunPos, ToolPos, PickPos, CellPos;

// A shot: the mark goes up first and stays up for the better part of a
// second -- a grab of the screen from outside needs it on many frames
// (the original's own SHOT reads back nothing under Proton) -- then the
// engine's own shot is taken under it, as CaptureConsole marks its shots.
var int MarkShot;
var bool bMarking;
var float MarkTime;
var string ShotLabel;
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

function Travel(PlayerPawn P, string Map)
{
	Log("DXCAP: opening " $ Map);
	P.ConsoleCommand("open " $ Map);
}

// An item's name for the log, "None" for none.
function string ItemName(Inventory item)
{
	if (item == None)
		return "None";
	return string(item.Name);
}

function Shot(PlayerPawn P, string Label)
{
	Log("DXBELT: shot " $ ShotCount $ " = " $ Label);
	MarkShot = ShotCount;
	bMarking = true;
	MarkTime = 0;
	ShotLabel = Label;
	bShotPending = true;
}

// The pending shot's own tick: 0.8 s of mark, then the shot under it.
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

// The belt's whole state: every slot's item, its description, the slot's
// count text, the item's icon, and the player's in-hand state.
function LogBelt(PlayerPawn P, string Label)
{
	local DeusExPlayer DXP;
	local DeusExRootWindow root;
	local HUDObjectBelt belt;
	local int i;
	local Inventory item;

	DXP = DeusExPlayer(P);
	Log("DXBELT: --- " $ Label $ " at " $ CapTime $ " ---");
	if (DXP == None || DeusExRootWindow(DXP.rootWindow) == None || DeusExRootWindow(DXP.rootWindow).hud == None
	    || DeusExRootWindow(DXP.rootWindow).hud.belt == None)
	{
		Log("DXBELT: no belt");
		return;
	}
	root = DeusExRootWindow(DXP.rootWindow);
	belt = root.hud.belt;
	for (i = 0; i < 10; i++)
	{
		item = belt.objects[i].GetItem();
		if (item == None)
			Log("DXBELT: slot " $ i $ ": empty");
		else
			Log("DXBELT: slot " $ i $ ": " $ item.Name $ " '" $ item.BeltDescription $ "' icon "
			    $ item.Icon $ " text '" $ belt.objects[i].itemText $ "' inBelt " $ item.bInObjectBelt
			    $ " beltPos " $ item.beltPos);
	}
	Log("DXBELT: player inHand " $ ItemName(DXP.inHand)
	    $ " pending " $ ItemName(DXP.inHandPending)
	    $ " transitioning " $ DXP.bInHandTransition
	    $ " weapon " $ ItemName(DXP.Weapon)
	    $ " selected " $ ItemName(DXP.SelectedItem));
}

// An item given as the game's own pickup gives it; a weapon's ammo is
// spawned beside it as Weapon's travel code does.
function GiveItem(PlayerPawn P, Inventory item)
{
	local DeusExPlayer DXP;
	local Ammo ammo;

	DXP = DeusExPlayer(P);
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
}

// Which slot holds the item, -1 for none.
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
		return;    // hold the world still while the mark is up

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Travel(P, "01_NYC_UNATCOIsland");
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
			LogBelt(P, "as the level starts it");
			Shot(P, "the belt as it starts");
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Items given as pickups give them, a moment apart, the belt
		// logged after each.
		if (StepTime > 1.0 && Gun == None)
		{
			Gun = P.Spawn(class'WeaponAssaultGun',,, P.Location + vect(40,0,0));
			GiveItem(P, Gun);
			Log("DXBELT: gave " $ Gun.Name);
			LogBelt(P, "after the assault gun");
		}
		else if (StepTime > 2.0 && Tool == None)
		{
			Tool = P.Spawn(class'Multitool',,, P.Location + vect(40,0,0));
			DeusExPickup(Tool).NumCopies = 2;
			GiveItem(P, Tool);
			Log("DXBELT: gave " $ Tool.Name $ " (2 copies)");
			LogBelt(P, "after the multitool");
		}
		else if (StepTime > 3.0 && Pick == None)
		{
			Pick = P.Spawn(class'Lockpick',,, P.Location + vect(40,0,0));
			DeusExPickup(Pick).NumCopies = 3;
			GiveItem(P, Pick);
			Log("DXBELT: gave " $ Pick.Name $ " (3 copies)");
			LogBelt(P, "after the lockpick");
			Shot(P, "the belt with the new items");
			StepTime = 0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		// Use the gun: the number keys' path.
		if (StepTime > 1.0)
		{
			GunPos = PosOf(P, Gun);
			Log("DXBELT: ActivateBelt " $ GunPos $ " (the gun) at " $ CapTime);
			DXP.ActivateBelt(GunPos);
			LogTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		// The bring-up transition, logged twice while it runs.
		if (StepTime > 0.5 && LogTime == 0)
		{
			Log("DXBELT: 0.5 s in: inHand " $ ItemName(DXP.inHand)
			    $ " pending " $ ItemName(DXP.inHandPending)
			    $ " transitioning " $ DXP.bInHandTransition
			    $ " weapon " $ ItemName(DXP.Weapon)
			    $ " gun state " $ Gun.GetStateName());
			LogTime = 1;
		}
		else if (StepTime > 1.2 && LogTime == 1)
		{
			Log("DXBELT: 1.2 s in: inHand " $ ItemName(DXP.inHand)
			    $ " pending " $ ItemName(DXP.inHandPending)
			    $ " transitioning " $ DXP.bInHandTransition
			    $ " weapon " $ ItemName(DXP.Weapon)
			    $ " gun state " $ Gun.GetStateName());
			LogTime = 2;
		}
		else if (StepTime > 2.0)
		{
			LogBelt(P, "the gun in hand");
			StepTime = 0;
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		// Swap to the multitool with the gun up: the stuck-transition case.
		if (StepTime > 1.0)
		{
			ToolPos = PosOf(P, Tool);
			Log("DXBELT: ActivateBelt " $ ToolPos $ " (the multitool) at " $ CapTime);
			DXP.ActivateBelt(ToolPos);
			StepTime = 0;
			Phase = 6;
		}
	}
	else if (Phase == 6)
	{
		if (StepTime > 2.0)
		{
			LogBelt(P, "after the swap to the multitool");
			Shot(P, "the multitool in hand");
			StepTime = 0;
			Phase = 7;
		}
	}
	else if (Phase == 7)
	{
		// A new item picked up after all that.
		if (StepTime > 1.0)
		{
			Cell = P.Spawn(class'BioelectricCell',,, P.Location + vect(40,0,0));
			GiveItem(P, Cell);
			Log("DXBELT: gave " $ Cell.Name);
			LogBelt(P, "after a new pickup");
			Shot(P, "the belt after a new pickup");
			StepTime = 0;
			Phase = 8;
		}
	}
	else if (Phase == 8)
	{
		// The mouse wheel's path: select the next belt item.
		if (StepTime > 1.0)
		{
			Log("DXBELT: NextBeltItem at " $ CapTime);
			DXP.NextBeltItem();
			StepTime = 0;
			Phase = 9;
		}
	}
	else if (Phase == 9)
	{
		if (StepTime > 1.5)
		{
			LogBelt(P, "after NextBeltItem");
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 10;
		}
	}
}
