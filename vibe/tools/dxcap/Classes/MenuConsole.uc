//=============================================================================
// MenuConsole: every menu screen, as the player reaches it. From the menu
// map, the main menu shown, then depth first: each of a menu's buttons that
// opens a menu or a screen pressed in turn, and each choice on a screen that
// opens another (and a Join screen's Host button); each window opened logged
// -- its title, action buttons and tree of windows (WindowDump) -- and shot
// with a mark; then closed as its own Cancel or Previous closes it. A message box is logged, shot and answered No. Buttons that
// leave the map (Training, Play Intro, Disconnect) and Full-Screen Mode are
// logged, not pressed. Then an exit.
//=============================================================================
class MenuConsole extends Console;

var float RunTime;
var float Wait;
var int Step;
var int ShotCount;

var DeusExBaseWindow FrameWin[8];
var int FrameNext[8];
var int Depth;

var int MarkShot;
var bool bMarking;
var float MarkTime;
var bool bShotPending;

function Shot(string Label)
{
	Log("DXMENU: shot " $ ShotCount $ " = " $ Label);
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

// The frames follow the root's window stack: those still on it kept, a
// window pushed over them made a frame of its own and logged.
function bool SyncFrames(PlayerPawn P, DeusExRootWindow Root)
{
	local int i;
	local bool bNew;

	for (i = 0; i < Depth; i++)
	{
		if (i >= Root.winCount || FrameWin[i] != Root.winStack[i])
		{
			Depth = i;
			break;
		}
	}
	for (i = Depth; i < Root.winCount && i < ArrayCount(FrameWin); i++)
	{
		FrameWin[i] = Root.winStack[i];
		FrameNext[i] = 0;
		Depth = i + 1;
		bNew = true;
	}
	if (bNew)
	{
		Log("DXMENU: window " $ FrameWin[Depth - 1].Class.Name $ ", depth " $ Depth);
		LogActionButtons(MenuUIWindow(FrameWin[Depth - 1]));
		class'WindowDump'.static.DumpWindow(P, FrameWin[Depth - 1], 0, "DXMENU: ");
		Shot(string(FrameWin[Depth - 1].Class.Name));
	}
	return bNew;
}

function LogActionButtons(MenuUIWindow W)
{
	local int i;

	if (W == None)
		return;
	Log("DXMENU: title '" $ W.title $ "'");
	for (i = 0; i < ArrayCount(W.actionButtons); i++)
	{
		if (W.actionButtons[i].btn != None)
			Log("DXMENU: action button " $ i $ " '" $ W.actionButtons[i].text $ "' action " $ W.actionButtons[i].action $ " " $ W.actionButtons[i].key);
	}
}

// The A-th choice of a screen that opens another window, or None.
function MenuUIChoiceAction ActionChoice(MenuUIScreenWindow S, int A)
{
	local Window C;
	local int n;

	for (C = S.winClient.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		if (MenuUIChoiceAction(C) != None)
		{
			if (n == A)
				return MenuUIChoiceAction(C);
			n++;
		}
	}
	return None;
}

// The top frame's next action: 1 when one was taken, 0 when the frame has
// none left.
function int NextAction(PlayerPawn P, DeusExRootWindow Root)
{
	local DeusExBaseWindow W;
	local MenuUIMenuWindow M;
	local MenuUIScreenWindow S;
	local MenuUIMessageBoxWindow B;
	local MenuUIChoiceAction A;
	local int k, Action;
	local string Key, Label;

	W = FrameWin[Depth - 1];
	B = MenuUIMessageBoxWindow(W);
	M = MenuUIMenuWindow(W);
	S = MenuUIScreenWindow(W);
	while (true)
	{
		k = FrameNext[Depth - 1]++;
		if (B != None)
		{
			if (k > 0)
				return 0;
			if (B.btnNo != None)
			{
				Log("DXMENU: answer No");
				B.ButtonActivated(B.btnNo);
			}
			else
			{
				Log("DXMENU: answer OK");
				B.ButtonActivated(B.btnOK);
			}
			return 1;
		}
		else if (M != None)
		{
			if (k >= ArrayCount(M.winButtons) || M.winButtons[k] == None)
				return 0;
			Action = M.buttonDefaults[k].action;
			Key = M.buttonDefaults[k].key;
			Label = M.ButtonNames[k];
			if (!M.winButtons[k].bIsSensitive)
				Log("DXMENU: button " $ k $ " '" $ Label $ "' insensitive, not pressed");
			// MA_Previous, MA_Training, MA_Intro; Disconnect
			else if (Action == 2 || Action == 4 || Action == 5 || Key == "DISCONNECT")
				Log("DXMENU: button " $ k $ " '" $ Label $ "' action " $ Action $ " " $ Key $ ", not pressed");
			else
			{
				Log("DXMENU: button " $ k $ " '" $ Label $ "' action " $ Action $ " " $ M.buttonDefaults[k].invoke $ " " $ Key $ ", pressed");
				M.ButtonActivated(M.winButtons[k]);
				return 1;
			}
		}
		else if (S != None)
		{
			A = ActionChoice(S, k);
			if (MenuChoice_FullScreen(A) != None)
				Log("DXMENU: choice " $ A.Class.Name $ " '" $ A.actionText $ "', not pressed");
			else if (A != None)
			{
				Log("DXMENU: choice " $ A.Class.Name $ " '" $ A.actionText $ "' " $ A.invoke $ ", pressed");
				A.ButtonActivated(A.btnAction);
				return 1;
			}
			else if (MenuScreenJoinGame(S) != None && k == ActionChoiceCount(S))
			{
				Log("DXMENU: Host button '" $ MenuScreenJoinGame(S).HostButton.buttonText $ "', pressed");
				S.ButtonActivated(MenuScreenJoinGame(S).HostButton);
				return 1;
			}
			else
				return 0;
		}
		else
			return 0;
	}
}

function int ActionChoiceCount(MenuUIScreenWindow S)
{
	local int n;

	while (ActionChoice(S, n) != None)
		n++;
	return n;
}

// The top frame's window closed as its own buttons close it.
function CloseTop(DeusExRootWindow Root)
{
	local DeusExBaseWindow W;

	W = FrameWin[Depth - 1];
	Log("DXMENU: close " $ W.Class.Name);
	if (MenuUIScreenWindow(W) != None)
		MenuUIScreenWindow(W).CancelScreen();
	else
		Root.PopWindow();
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	TickShot(P, Delta);
	if (bShotPending)
		return;
	if (Wait > 0)
	{
		Wait -= Delta;
		return;
	}
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	if (Step == 0)
	{
		if (RunTime < 3.0)
			return;
		Log("DXMENU: the main menu");
		DeusExPlayer(P).ShowMainMenu();
		Step = 1;
		Wait = 1.5;
	}
	else if (Step == 1)
	{
		if (SyncFrames(P, Root))
			return;
		if (Depth == 0)
		{
			Log("DXMENU: done, exiting");
			P.ConsoleCommand("exit");
			Step = 2;
			return;
		}
		if (NextAction(P, Root) == 0)
			CloseTop(Root);
		Wait = 1.2;
	}
}
