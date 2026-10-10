//=============================================================================
// ScreenConsole: the screens of a game in progress, on the ini's TargetMap
// ([DXCapture.ScreenConsole]), Liberty Island by default. The player, given
// items as BorderConsole gives them, a note, a goal and an image, opens each
// data vault screen in turn (inventory, health, augmentations, skills,
// goals, conversations, images, logs); the main menu and its Save Game
// screen; a personal, a public and a security computer (the map's first of
// each, or one placed by the player, given a user) and an ATM, each logged
// in to with its first user; a medical bot the player places (Liberty
// Island's own opens no screen in either engine), without an augmentation
// canister and with one, and a repair bot. Each screen's tree of windows is logged
// (WindowDump) and shot with a mark; then it is closed. Then an exit.
//=============================================================================
class ScreenConsole extends Console;

var config string TargetMap;

var float CapTime;
var float MapTime;
var float Wait;
var string CurrentMap;
var int Phase;
var int Step;
var bool bDumpPending;
var string DumpLabel;
var int ShotCount;
var Actor Device;

var int MarkShot;
var bool bMarking;
var float MarkTime;
var bool bShotPending;

function string MapName()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

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

function Shot(string Label)
{
	Log("DXSCREEN: shot " $ ShotCount $ " = " $ Label);
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
function Inventory GiveItem(DeusExPlayer P, class<Inventory> ItemClass, int Copies)
{
	local Inventory item;
	local Ammo ammo;

	item = P.Spawn(ItemClass,,, P.Location + vect(40,0,0));
	if (item == None)
	{
		Log("DXSCREEN: could not spawn " $ ItemClass);
		return None;
	}
	if (DeusExPickup(item) != None && Copies > 1)
		DeusExPickup(item).NumCopies = Copies;
	if (DeusExWeapon(item) != None && DeusExWeapon(item).AmmoType == None && DeusExWeapon(item).AmmoName != None)
	{
		ammo = P.Spawn(DeusExWeapon(item).AmmoName);
		if (ammo != None)
		{
			P.AddInventory(ammo);
			ammo.BecomeItem();
			ammo.AmmoAmount = DeusExWeapon(item).PickUpAmmoCount;
			ammo.GotoState('Idle2');
		}
	}
	item.GiveTo(P);
	Log("DXSCREEN: gave " $ item.Name);
	return item;
}

// The map's first actor of a class, or one placed in front of the player.
function Actor FindOrPlace(DeusExPlayer P, class<Actor> C)
{
	local Actor A;

	foreach P.AllActors(C, A)
	{
		Log("DXSCREEN: found " $ A.Name);
		return A;
	}
	return Place(P, C);
}

// An actor placed in front of the player, or over it when that is blocked.
function Actor Place(DeusExPlayer P, class<Actor> C)
{
	local Actor A;

	A = P.Spawn(C,,, P.Location + 80 * vector(P.Rotation));
	if (A == None)
		A = P.Spawn(C,,, P.Location + vect(0,0,40));
	if (A != None)
		Log("DXSCREEN: placed " $ A.Name);
	else
		Log("DXSCREEN: could not place " $ C);
	return A;
}

function OpenComputer(DeusExPlayer P, class<Computers> C)
{
	local Computers Comp;

	Comp = Computers(FindOrPlace(P, C));
	Device = Comp;
	if (Comp == None)
		return;
	if (Comp.GetUserName(0) == "")
	{
		Comp.userList[0].userName = "JCD";
		Comp.userList[0].password = "BIONICMAN";
	}
	Comp.bLockedOut = false;
	P.InvokeComputerScreen(Comp, 0, P.Level.TimeSeconds);
}

// The open computer's login screen given its first user, and its login.
function LogIn(DeusExPlayer P)
{
	local NetworkTerminal Term;
	local ComputerScreenLogin Login;
	local ComputerScreenATM ATMLogin;

	Term = NetworkTerminal(DeusExRootWindow(P.rootWindow).GetTopWindow());
	if (Term == None)
	{
		Log("DXSCREEN: no terminal to log in to");
		return;
	}
	Login = ComputerScreenLogin(Term.winComputer);
	ATMLogin = ComputerScreenATM(Term.winComputer);
	if (Login != None && Computers(Device) != None)
	{
		Login.editUserName.SetText(Computers(Device).GetUserName(0));
		Login.editPassword.SetText(Computers(Device).GetPassword(0));
		Log("DXSCREEN: logging in as " $ Computers(Device).GetUserName(0));
		Login.ProcessLogin();
	}
	else if (ATMLogin != None && ATM(Device) != None)
	{
		ATMLogin.editAccount.SetText(ATM(Device).GetAccountNumber(0));
		ATMLogin.editPIN.SetText(ATM(Device).GetPIN(0));
		Log("DXSCREEN: logging in to account " $ ATM(Device).GetAccountNumber(0));
		ATMLogin.ProcessLogin();
	}
	else
		Log("DXSCREEN: no login screen: " $ Term.winComputer);
}

function CloseTerminal(DeusExPlayer P)
{
	local NetworkTerminal Term;

	Term = NetworkTerminal(DeusExRootWindow(P.rootWindow).GetTopWindow());
	if (Term != None)
		Term.CloseScreen("EXIT");
	else
		Log("DXSCREEN: no terminal to close");
}

function CloseTop(DeusExPlayer P)
{
	local DeusExRootWindow Root;

	Root = DeusExRootWindow(P.rootWindow);
	if (Root.GetTopWindow() != None)
	{
		Log("DXSCREEN: close " $ Root.GetTopWindow().Class.Name);
		Root.PopWindow();
	}
	else
		Log("DXSCREEN: nothing to close");
}

// Step N of the walk; true when it opened a screen to log.
function bool RunStep(DeusExPlayer P, int N)
{
	local DeusExRootWindow Root;
	local DeusExGoal Goal;
	local DataVaultImage Image;
	local ATM Machine;
	local Inventory Can;

	Root = DeusExRootWindow(P.rootWindow);
	DumpLabel = "";
	switch (N)
	{
	case 0:
		GiveItem(P, class'WeaponAssaultGun', 1);
		GiveItem(P, class'Multitool', 2);
		GiveItem(P, class'Lockpick', 3);
		GiveItem(P, class'BioelectricCell', 1);
		GiveItem(P, class'MedKit', 2);
		P.AddNote("A note the console wrote.", true, true);
		Goal = P.AddGoal('ScreenConsoleGoal', true);
		Goal.SetText("A goal the console set.");
		Image = P.Spawn(class'Image01_LibertyIsland');
		if (Image != None)
			P.AddImage(Image);
		return false;
	case 1: P.ShowInventoryWindow(); DumpLabel = "inventory"; return true;
	case 2: CloseTop(P); return false;
	case 3: P.ShowHealthWindow(); DumpLabel = "health"; return true;
	case 4: CloseTop(P); return false;
	case 5: P.ShowAugmentationsWindow(); DumpLabel = "augmentations"; return true;
	case 6: CloseTop(P); return false;
	case 7: P.ShowSkillsWindow(); DumpLabel = "skills"; return true;
	case 8: CloseTop(P); return false;
	case 9: P.ShowGoalsWindow(); DumpLabel = "goals"; return true;
	case 10: CloseTop(P); return false;
	case 11: P.ShowConversationsWindow(); DumpLabel = "conversations"; return true;
	case 12: CloseTop(P); return false;
	case 13: P.ShowImagesWindow(); DumpLabel = "images"; return true;
	case 14: CloseTop(P); return false;
	case 15: P.ShowLogsWindow(); DumpLabel = "logs"; return true;
	case 16: CloseTop(P); return false;
	case 17: P.ShowMainMenu(); DumpLabel = "the main menu"; return true;
	case 18:
		MenuUIMenuWindow(Root.GetTopWindow()).ButtonActivated(MenuUIMenuWindow(Root.GetTopWindow()).winButtons[1]);
		DumpLabel = "Save Game";
		return true;
	case 19: MenuUIScreenWindow(Root.GetTopWindow()).CancelScreen(); return false;
	case 20: CloseTop(P); return false;
	case 21: OpenComputer(P, class'ComputerPersonal'); DumpLabel = "a personal computer"; return true;
	case 22: LogIn(P); DumpLabel = "a personal computer, logged in"; return true;
	case 23: CloseTerminal(P); return false;
	case 24: OpenComputer(P, class'ComputerPublic'); DumpLabel = "a public computer"; return true;
	case 25: CloseTerminal(P); return false;
	case 26: OpenComputer(P, class'ComputerSecurity'); DumpLabel = "a security computer"; return true;
	case 27: LogIn(P); DumpLabel = "a security computer, logged in"; return true;
	case 28: CloseTerminal(P); return false;
	case 29:
		Machine = ATM(FindOrPlace(P, class'ATM'));
		Device = Machine;
		if (Machine != None)
		{
			if (Machine.GetAccountNumber(0) == "")
			{
				Machine.userList[0].accountNumber = "1234";
				Machine.userList[0].PIN = "5678";
				Machine.userList[0].balance = 500;
			}
			Machine.Frob(P, None);
		}
		DumpLabel = "an ATM";
		return true;
	case 30: LogIn(P); DumpLabel = "an ATM, logged in"; return true;
	case 31: CloseTerminal(P); return false;
	case 32:
		Device = Place(P, class'MedicalBot');
		if (Device != None)
			Device.Frob(P, None);
		DumpLabel = "a medical bot";
		return true;
	case 33: CloseTop(P); Root.MaskBackground(false); return false;
	case 34:
		Can = GiveItem(P, class'AugmentationCannister', 1);
		if (Can != None)
		{
			AugmentationCannister(Can).AddAugs[0] = 'AugSpeed';
			AugmentationCannister(Can).AddAugs[1] = 'AugAqualung';
		}
		if (Device != None)
			Device.Frob(P, None);
		DumpLabel = "a medical bot, a canister held";
		return true;
	case 35: CloseTop(P); Root.MaskBackground(false); return false;
	case 36:
		Device = FindOrPlace(P, class'RepairBot');
		if (Device != None)
			Device.Frob(P, None);
		DumpLabel = "a repair bot";
		return true;
	case 37: CloseTop(P); Root.MaskBackground(false); return false;
	default:
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 4;
		return false;
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow Root;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
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
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';
	TickShot(P, Delta);
	if (bShotPending)
		return;
	if (Wait > 0)
	{
		Wait -= Delta;
		return;
	}

	if (Phase == 0)
	{
		if (!(M ~= MapName()))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening " $ MapName());
				P.ConsoleCommand("open " $ MapName());
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= MapName() && MapTime > 6.0)
		{
			P.SetPhysics(PHYS_Walking);
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		Root = DeusExRootWindow(DXP.rootWindow);
		if (bDumpPending)
		{
			bDumpPending = false;
			if (Root.GetTopWindow() == None)
			{
				Log("DXSCREEN: " $ DumpLabel $ ": no window");
				return;
			}
			Log("DXSCREEN: window " $ Root.GetTopWindow().Class.Name $ " = " $ DumpLabel);
			class'WindowDump'.static.DumpWindow(P, Root.GetTopWindow(), 0, "DXSCREEN: ");
			Shot(DumpLabel);
			return;
		}
		Log("DXSCREEN: step " $ Step);
		bDumpPending = RunStep(DXP, Step);
		Step++;
		Wait = 1.5;
	}
}
