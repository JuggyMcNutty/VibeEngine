//=============================================================================
// EditConsole: what a script's text calls do to an edit window, a text window
// and a button. From the menu map, the main menu shown, an EditLogWindow is
// put on it with an edit window in it, and each call below is logged
// ("DXEDIT:") with the edit window's text, insertion point, selection and
// changed flag -- and every TextChanged the edit window announces as it
// comes. Then a text window given a text and the same text in capitals, and
// a button's and a large text window's accelerator (acceleratorKey) as their
// text and EnableTextAsAccelerator change. Then an exit.
//=============================================================================
class EditConsole extends Console;

var float RunTime;
var int Step;
var EditLogWindow Holder;
var EditWindow Edit;

function LogEdit(string Label)
{
	local int Start, Count;

	Edit.GetSelectedArea(Start, Count);
	Log("DXEDIT: " $ Label $ ": text '" $ Edit.GetText() $ "', insertion point " $ Edit.GetInsertionPoint()
		$ ", selected " $ Start $ "+" $ Count $ ", changed " $ Edit.HasTextChanged());
}

function LogKey(string Label, Window W)
{
	Log("DXEDIT: " $ Label $ ": accelerator " $ W.acceleratorKey);
}

function TestEdit()
{
	Edit = EditWindow(Holder.NewChild(class'EditWindow'));
	Edit.SetSize(200, 20);
	LogEdit("made");
	Log("DXEDIT: SetText('Liberty Island')");
	Edit.SetText("Liberty Island");
	LogEdit("after");
	Edit.ClearTextChangedFlag();
	LogEdit("ClearTextChangedFlag");
	Log("DXEDIT: SetText('LIBERTY ISLAND')");
	Edit.SetText("LIBERTY ISLAND");
	LogEdit("after");
	Edit.SetInsertionPoint(0);
	LogEdit("SetInsertionPoint(0)");
	Log("DXEDIT: InsertText('X')");
	Edit.InsertText("X");
	LogEdit("after");
	Log("DXEDIT: InsertText('Y')");
	Edit.InsertText("Y");
	LogEdit("after");
	Edit.SetSelectedArea(2, 3);
	LogEdit("SetSelectedArea(2, 3)");
	Log("DXEDIT: DeleteChar()");
	Edit.DeleteChar();
	LogEdit("after");
	Log("DXEDIT: AppendText('!')");
	Edit.AppendText("!");
	LogEdit("after");
	Edit.MoveInsertionPoint(MOVEINSERT_End);
	LogEdit("MoveInsertionPoint(End)");
	Log("DXEDIT: SetText('')");
	Edit.SetText("");
	LogEdit("after");
	Log("DXEDIT: SetText('') again");
	Edit.SetText("");
	LogEdit("after");
}

function TestText()
{
	local TextWindow T;

	T = TextWindow(Holder.NewChild(class'TextWindow'));
	T.SetText("Some text");
	T.SetText("SOME TEXT");
	Log("DXEDIT: a text window given 'Some text' then 'SOME TEXT': '" $ T.GetText() $ "'");
	T.SetText("Other text");
	Log("DXEDIT: then 'Other text': '" $ T.GetText() $ "'");
}

function TestKeys()
{
	local ButtonWindow B;
	local LargeTextWindow L;

	B = ButtonWindow(Holder.NewChild(class'ButtonWindow'));
	LogKey("a new button", B);
	B.SetText("|&Save Game");
	LogKey("the button given '|&Save Game'", B);
	B.SetText("Ca|&ncel");
	LogKey("then 'Ca|&ncel'", B);
	B.AppendText(" |&x");
	LogKey("then ' |&x' appended", B);
	B.EnableTextAsAccelerator(False);
	LogKey("EnableTextAsAccelerator(False)", B);
	B.SetText("|&Restore");
	LogKey("then '|&Restore'", B);
	B.EnableTextAsAccelerator();
	LogKey("EnableTextAsAccelerator()", B);
	B.SetText("No key");
	LogKey("then 'No key'", B);
	L = LargeTextWindow(Holder.NewChild(class'LargeTextWindow'));
	L.SetText("|&Large");
	LogKey("a large text window given '|&Large'", L);
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
	Root = DeusExRootWindow(DeusExPlayer(P).rootWindow);

	switch (Step)
	{
	case 0:
		if (RunTime < 3.0)
			return;
		DeusExPlayer(P).ShowMainMenu();
		break;
	case 1:
		if (RunTime < 4.0)
			return;
		Holder = EditLogWindow(Root.GetTopWindow().NewChild(class'EditLogWindow'));
		Holder.SetPos(10, 10);
		Holder.SetSize(300, 200);
		TestEdit();
		TestText();
		TestKeys();
		break;
	case 2:
		if (RunTime < 5.0)
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
