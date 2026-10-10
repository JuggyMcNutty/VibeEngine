//=============================================================================
// ClickConsole: buttons, a choice and a checkbox clicked with the mouse, the
// mouse pressed by the fork's timeline (vibe/tools/dxcap/timelines/
// clicks.txt, on the same clock: seconds from the start). From the menu
// map, on this schedule the UI cursor is put on a window and the timeline
// clicks there; what the click did is logged ("DXCLICK:").
//
// 5 s   cursor on the main menu's Settings button     timeline: click at 6
// 8     the top window logged; Game Options opened, cursor on its first
//       choice's button                               timeline: click at 11
// 13    the choice's value logged                     timeline: right click at 14
// 16    the value logged; the LAN join screen opened, cursor on its first
//       checkbox                                      timeline: click at 20
// 22    the checkbox logged                           timeline: click at 23
// 25    the checkbox logged; 27 exit
//=============================================================================
class ClickConsole extends Console;

var float RunTime;
var int Step;
var DeusExRootWindow Root;
var MenuUIChoiceEnum Choice;
var MenuUICheckboxWindow Checkbox;

function Window FindClass(Window W, class<Window> Wanted)
{
	local Window C, Found;

	if (ClassIsChildOf(W.Class, Wanted))
		return W;
	for (C = W.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Found = FindClass(C, Wanted);
		if (Found != None)
			return Found;
	}
	return None;
}

function Window FindButton(Window W, string Text)
{
	local Window C, Found;

	if (MenuUIBorderButtonWindow(W) != None && InStr(Caps(MenuUIBorderButtonWindow(W).buttonText), Caps(Text)) >= 0)
		return W;
	for (C = W.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Found = FindButton(C, Text);
		if (Found != None)
			return Found;
	}
	return None;
}

function PointAt(Window W, string What)
{
	if (W == None)
	{
		Log("DXCLICK: no " $ What);
		return;
	}
	W.SetCursorPos(W.width / 2, W.height / 2);
	Log("DXCLICK: cursor on " $ What);
}

function bool Due(float Time)
{
	return RunTime >= Time;
}

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Step)
	{
	case 0:
		if (!Due(3.0))
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (!Due(5.0))
			return;
		PointAt(FindButton(Root.GetTopWindow(), "Settings"), "the Settings button");
		break;
	case 2:
		if (!Due(8.0))
			return;
		Log("DXCLICK: after the click the top window is " $ Root.GetTopWindow().Class.Name);
		Root.InvokeMenuScreen(class'MenuScreenOptions');
		break;
	case 3:
		if (!Due(9.5))
			return;
		Choice = MenuUIChoiceEnum(FindClass(Root.GetTopWindow(), class'MenuUIChoiceEnum'));
		if (Choice != None)
		{
			Log("DXCLICK: " $ Choice.Class.Name $ " value " $ Choice.currentValue);
			PointAt(Choice.btnAction, "its button");
		}
		else
			Log("DXCLICK: no choice");
		break;
	case 4:
		if (!Due(13.0))
			return;
		if (Choice != None)
			Log("DXCLICK: after a click the value is " $ Choice.currentValue);
		break;
	case 5:
		if (!Due(16.0))
			return;
		if (Choice != None)
			Log("DXCLICK: after a right click the value is " $ Choice.currentValue);
		Root.PopWindow();
		Root.InvokeMenuScreen(class'MenuScreenJoinLan');
		break;
	case 6:
		if (!Due(18.0))
			return;
		Checkbox = MenuUICheckboxWindow(FindClass(Root.GetTopWindow(), class'MenuUICheckboxWindow'));
		if (Checkbox != None)
		{
			Log("DXCLICK: checkbox '" $ Checkbox.GetText() $ "' toggle " $ Checkbox.GetToggle());
			PointAt(Checkbox, "the checkbox");
		}
		else
			Log("DXCLICK: no checkbox");
		break;
	case 7:
		if (!Due(22.0))
			return;
		if (Checkbox != None)
			Log("DXCLICK: after a click the toggle is " $ Checkbox.GetToggle());
		break;
	case 8:
		if (!Due(25.0))
			return;
		if (Checkbox != None)
			Log("DXCLICK: after another click the toggle is " $ Checkbox.GetToggle());
		break;
	case 9:
		if (!Due(27.0))
			return;
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Step = 99;
		return;
	default:
		return;
	}
	Step++;
}
