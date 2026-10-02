//=============================================================================
// ChoiceConsole: a conversation's choices -- the buttons, where the focus
// sits and moves, and the pick, logged with "DXCHOICE:" and the console's
// clock, with marked shots (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island: Tech Sergeant Kaplan's MeetKaplan (third-person, with
// speech), its lines run through quickly to the choices, which are then
// left to sit: the focus window logged, shots of the choices as displayed,
// the root window's own VirtualKeyPressed driving Down, Down and Up between
// them (the keys' own path -- ConWindowActive passes them on and
// RootWindow moves the focus), the focus logged after each with a shot,
// then the focused choice picked through ConPlay.PlayChoice, the path a
// click ends in. What is compared between the engines: whether the choice
// buttons exist and are focusable, whether the focus moves between them,
// and what the shots show of the highlighted choice.
//=============================================================================
class ChoiceConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float EventTime;
var string CurrentMap;
var int Phase;
var int ShotCount;
var Actor Kaplan;
var ConEvent LastEvent;
var bool bSkipping;
var int ChoiceVisits;
var int KeySteps;

// A mark in the view's corner while a shot is due, as CaptureConsole marks
// its shots.
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

function Travel(PlayerPawn P, string Map)
{
	Log("DXCAP: opening " $ Map);
	P.ConsoleCommand("open " $ Map);
}

function Shot(PlayerPawn P, string Label)
{
	Log("DXCHOICE: shot " $ ShotCount $ " = " $ Label);
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

// A window's name for the log, "None" for none.
function string WinName(Window w)
{
	if (w == None)
		return "None";
	return string(w.Name) $ " (" $ string(w.Class.Name) $ ")";
}

function Actor FindKaplan(PlayerPawn P)
{
	local Actor A;
	local ConListItem Item;

	foreach P.AllActors(class'Actor', A)
	{
		for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
		{
			if (Item.con != None && Item.con.conName == 'MeetKaplan')
				return A;
		}
	}
	return None;
}

function bool StandBy(PlayerPawn P, Actor Other)
{
	local int i, j;
	local vector Dir, Cand;

	for (j = 0; j < 3; j++)
	{
		for (i = 0; i < 16; i++)
		{
			Dir.X = cos(i * 0.392699);
			Dir.Y = sin(i * 0.392699);
			Dir.Z = 0.15;
			Dir = Normal(Dir);
			Cand = Other.Location + Dir * 90 * (1.0 - 0.3 * j);
			if (!P.FastTrace(Other.Location, Cand) || !P.FastTrace(Cand + Dir * 40, Cand))
				continue;
			P.SetLocation(Cand);
			P.SetRotation(rotator(Other.Location - Cand));
			P.ViewRotation = rotator(Other.Location - Cand);
			return true;
		}
	}
	return false;
}

// The conversation window's state: the choices and where the focus is.
function LogChoices(DeusExPlayer DXP, string Label)
{
	local ConWindowActive win;
	local DeusExRootWindow root;
	local Window focus;
	local int i;

	root = DeusExRootWindow(DXP.rootWindow);
	if (root == None)
	{
		Log("DXCHOICE: " $ Label $ ": no root window");
		return;
	}
	focus = root.GetFocusWindow();
	Log("DXCHOICE: " $ Label $ ": focus " $ WinName(focus));
	win = DXP.conPlay.conWinThird;
	if (win == None)
	{
		Log("DXCHOICE: " $ Label $ ": no conversation window");
		return;
	}
	for (i = 0; i < win.numChoices; i++)
	{
		if (win.conChoices[i] != None)
			Log("DXCHOICE: " $ Label $ ": choice " $ i $ " " $ win.conChoices[i].Name
			    $ " selectable " $ win.conChoices[i].bIsSelectable
			    $ " sensitive " $ win.conChoices[i].bIsSensitive
			    $ " visible " $ win.conChoices[i].bIsVisible
			    $ " focused " $ (focus == win.conChoices[i]));
	}
	if (win.numChoices == 0)
		Log("DXCHOICE: " $ Label $ ": no choices up");
}

// One key through the root window's own handler -- the path a real key
// takes once ConWindowActive has passed it on.
function KeyDown(DeusExPlayer DXP, string Label)
{
	DeusExRootWindow(DXP.rootWindow).VirtualKeyPressed(IK_Down, False);
	Log("DXCHOICE: " $ Label $ " (Down) sent");
	LogChoices(DXP, "after " $ Label);
}

function KeyUp(DeusExPlayer DXP, string Label)
{
	DeusExRootWindow(DXP.rootWindow).VirtualKeyPressed(IK_Up, False);
	Log("DXCHOICE: " $ Label $ " (Up) sent");
	LogChoices(DXP, "after " $ Label);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local DeusExRootWindow root;
	local ConChoiceWindow focusChoice;
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
	EventTime += Delta;
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
			Kaplan = FindKaplan(P);
			if (Kaplan == None || !StandBy(P, Kaplan))
			{
				Log("DXCHOICE: no Kaplan or nowhere to stand, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
			}
			else
				Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Kaplan's own radius conversation starts on its own; adopt it or
		// force it by name, as SkipConsole does.
		if (DXP != None && DXP.conPlay != None)
		{
			Log("DXCHOICE: conversation " $ DXP.conPlay.con.conName $ " playing at " $ CapTime);
			LastEvent = None;
			bSkipping = true;
			EventTime = 0;
			Phase = 3;
		}
		else if (StepTime > 3.0)
		{
			if (DXP != None && DXP.dataLinkPlay != None)
				DXP.dataLinkPlay.AbortDataLink();
			if (DXP == None || !DXP.StartConversationByName('MeetKaplan', Kaplan, False, True))
			{
				Log("DXCHOICE: did not start, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
			}
		}
	}
	else if (Phase == 3)
	{
		if (DXP == None || DXP.conPlay == None || EventTime > 240.0)
		{
			Log("DXCHOICE: ended at " $ CapTime $ " after " $ ChoiceVisits $ " choice events, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
		else if (DXP.conPlay.currentEvent != LastEvent)
		{
			LastEvent = DXP.conPlay.currentEvent;
			EventTime = 0;
			if (ConEventChoice(LastEvent) != None && ConEventChoice(LastEvent).ChoiceList != None)
			{
				// A choice event: stop skipping, let them sit.
				bSkipping = false;
				ChoiceVisits++;
				LogChoices(DXP, "choice event " $ ChoiceVisits);
				KeySteps = 0;
				StepTime = 0;
				Phase = 4;
			}
			else if (bSkipping)
			{
				DXP.conPlay.PlayNextEvent();
			}
		}
		else if (bSkipping && EventTime > 0.3)
		{
			// A line run through quickly to reach the choices sooner.
			DXP.conPlay.PlayNextEvent();
			EventTime = 0;
		}
	}
	else if (Phase == 4)
	{
		// The choices sit: shot, then Down, Down, Up through the root
		// window's own key handler, a shot after each.
		if (StepTime > 1.0 && KeySteps == 0)
		{
			Shot(P, "the choices as they come up");
			KeySteps = 1;
		}
		else if (StepTime > 2.0 && KeySteps == 1)
		{
			KeyDown(DXP, "Down");
			Shot(P, "after Down");
			KeySteps = 2;
		}
		else if (StepTime > 3.0 && KeySteps == 2)
		{
			KeyDown(DXP, "Down again");
			Shot(P, "after Down again");
			KeySteps = 3;
		}
		else if (StepTime > 4.0 && KeySteps == 3)
		{
			KeyUp(DXP, "Up");
			Shot(P, "after Up");
			KeySteps = 4;
		}
		else if (StepTime > 5.0 && KeySteps == 4)
		{
			// Pick through the focused button's own object, the path a
			// click ends in.
			root = DeusExRootWindow(DXP.rootWindow);
			focusChoice = ConChoiceWindow(root.GetFocusWindow());
			if (focusChoice != None && focusChoice.GetUserObject() != None)
			{
				Log("DXCHOICE: picking the focused choice's object");
				DXP.conPlay.PlayChoice(ConChoice(focusChoice.GetUserObject()));
			}
			else
			{
				Log("DXCHOICE: focus is not a choice button, playing the first");
				DXP.conPlay.PlayChoice(ConEventChoice(LastEvent).ChoiceList);
			}
			bSkipping = true;
			Phase = 3;
		}
	}
}
