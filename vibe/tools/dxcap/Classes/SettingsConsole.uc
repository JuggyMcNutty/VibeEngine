//=============================================================================
// SettingsConsole: every setting the menus change, changed through its menu
// and read back. From the menu map, the main menu shown, each settings
// screen in turn (Controls, Game Options, Display, Sound, Colors, the
// multiplayer Player Setup) is opened, each of its choices logged -- its
// value and the GET of its setting -- then stepped one value on and the
// screen closed with OK, as the player saves it; a box asking to restart is
// answered OK. The screen is opened again and logged, so what was saved
// shows; then each choice is stepped back and saved, and the screen logged
// once more. Screen Resolution is left as it is (a new size resizes the
// window). Then an exit.
//=============================================================================
class SettingsConsole extends Console;

var float RunTime;
var float Wait;
var int Phase;
var int ScreenIndex;
var int Pass;

function class<MenuUIScreenWindow> ScreenClass(int i)
{
	switch (i)
	{
	case 0: return class'MenuScreenControls';
	case 1: return class'MenuScreenOptions';
	case 2: return class'MenuScreenDisplay';
	case 3: return class'MenuScreenSound';
	case 4: return class'MenuScreenAdjustColors';
	case 5: return class'MenuScreenPlayerSetup';
	}
	return None;
}

function LogChoices(PlayerPawn P, MenuUIScreenWindow S, string Label)
{
	local Window C;
	local MenuUIChoice Choice;
	local MenuUIChoiceEnum E;
	local string Line;

	Log("DXSET: " $ S.Class.Name $ " " $ Label);
	for (C = S.winClient.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Choice = MenuUIChoice(C);
		if (Choice == None || MenuUIChoiceAction(Choice) != None)
			continue;
		Line = "DXSET:   " $ Choice.Class.Name;
		E = MenuUIChoiceEnum(Choice);
		if (E != None)
			Line = Line $ " value=" $ E.currentValue $ " '" $ E.enumText[Clamp(E.currentValue, 0, 39)] $ "'";
		else if (MenuUIChoiceSlider(Choice) != None)
			Line = Line $ " value=" $ MenuUIChoiceSlider(Choice).GetValue();
		if (Choice.configSetting != "")
			Line = Line $ " get='" $ P.ConsoleCommand("get " $ Choice.configSetting) $ "'";
		Log(Line);
	}
	Log("DXSET:   player MouseSensitivity=" $ P.MouseSensitivity $ " bAlwaysRun=" $ DeusExPlayer(P).bAlwaysRun $ " bToggleCrouch=" $ DeusExPlayer(P).bToggleCrouch $ " bInvertMouse=" $ P.bInvertMouse $ " bSubtitles=" $ DeusExPlayer(P).bSubtitles $ " MenuThemeName=" $ DeusExPlayer(P).MenuThemeName $ " HUDThemeName=" $ DeusExPlayer(P).HUDThemeName $ " UIBackground=" $ DeusExPlayer(P).UIBackground);
}

// Every choice but Screen Resolution stepped on (bForward) or back.
function StepChoices(MenuUIScreenWindow S, bool bForward)
{
	local Window C;
	local MenuUIChoice Choice;

	for (C = S.winClient.GetBottomChild(); C != None; C = C.GetHigherSibling())
	{
		Choice = MenuUIChoice(C);
		if (Choice == None || MenuUIChoiceAction(Choice) != None || MenuChoice_Resolution(Choice) != None)
			continue;
		if (bForward)
			Choice.CycleNextValue();
		else
			Choice.CyclePreviousValue();
	}
}

// OK, as the screen's button does it; then any box it raised answered OK.
function SaveAndClose(DeusExRootWindow Root, MenuUIScreenWindow S)
{
	Log("DXSET: OK on " $ S.Class.Name);
	S.SaveSettings();
	Root.PopWindow();
}

function AnswerBoxes(DeusExRootWindow Root)
{
	local MenuUIMessageBoxWindow B;

	B = MenuUIMessageBoxWindow(Root.GetTopWindow());
	while (B != None)
	{
		Log("DXSET: box '" $ B.winText.GetText() $ "' answered");
		if (B.btnOK != None)
			B.ButtonActivated(B.btnOK);
		else if (B.btnYes != None)
			B.ButtonActivated(B.btnYes);
		else
			Root.PopWindow();
		B = MenuUIMessageBoxWindow(Root.GetTopWindow());
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow Root;
	local MenuUIScreenWindow S;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || DeusExPlayer(Viewport.Actor) == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Wait > 0)
	{
		Wait -= Delta;
		return;
	}
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	if (Phase == 0)
	{
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		Phase = 1;
		Wait = 1.0;
	}
	else if (Phase == 1)
	{
		if (ScreenClass(ScreenIndex) == None)
		{
			Log("DXSET: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Root.InvokeMenuScreen(ScreenClass(ScreenIndex));
		Phase = 2;
		Wait = 1.0;
	}
	else if (Phase == 2)
	{
		S = MenuUIScreenWindow(Root.GetTopWindow());
		if (S == None)
		{
			Log("DXSET: no screen " $ ScreenClass(ScreenIndex));
			ScreenIndex++;
			Phase = 1;
			return;
		}
		if (Pass == 0)
		{
			LogChoices(P, S, "as opened");
			StepChoices(S, true);
			LogChoices(P, S, "stepped on");
		}
		else if (Pass == 1)
		{
			LogChoices(P, S, "reopened after OK");
			StepChoices(S, false);
		}
		else
		{
			LogChoices(P, S, "reopened after stepping back");
			Root.PopWindow();
			Pass = 0;
			ScreenIndex++;
			Phase = 1;
			Wait = 1.0;
			return;
		}
		SaveAndClose(Root, S);
		Phase = 3;
		Wait = 1.0;
	}
	else if (Phase == 3)
	{
		AnswerBoxes(Root);
		Pass++;
		Phase = 1;
		Wait = 1.0;
	}
}
