//=============================================================================
// WindowDump: a window and its children logged, one line each, for
// MenuConsole and ScreenConsole: class, place, size, whether hidden,
// insensitive or the focus, text (a text window's, a button's, a title's),
// a toggle's state, the client object, a menu choice's value and the GET of
// its setting, and a list's rows.
//=============================================================================
class WindowDump extends Object;

// A string on one line of the log: line breaks shown as \n, at most 240
// characters.
static function string Clean(string S)
{
	local string Out, Ch;
	local int i;

	for (i = 0; i < Len(S) && Len(Out) < 240; i++)
	{
		Ch = Mid(S, i, 1);
		if (Ch == Chr(10))
			Out = Out $ "\\n";
		else if (Ch != Chr(13))
			Out = Out $ Ch;
	}
	return Out;
}

static function string Indent(int Level)
{
	local string S;
	local int i;

	for (i = 0; i < Level; i++)
		S = S $ "  ";
	return S;
}

static function DumpWindow(PlayerPawn P, Window W, int Level, string Prefix)
{
	local string S, Setting;
	local Window C;
	local ListWindow L;
	local MenuUIChoice Choice;
	local MenuUIChoiceEnum E;
	local int i, j, Rows;
	local string Row;

	S = Prefix $ Indent(Level) $ W.Class.Name $ " " $ W.x $ "," $ W.y $ " " $ W.width $ "x" $ W.height;
	if (!W.bIsVisible)
		S = S $ " hidden";
	if (!W.bIsSensitive)
		S = S $ " insensitive";
	if (W.IsFocusWindow())
		S = S $ " focus";
	if (TextWindow(W) != None && TextWindow(W).GetText() != "")
		S = S $ " '" $ Clean(TextWindow(W).GetText()) $ "'";
	if (MenuUIBorderButtonWindow(W) != None)
		S = S $ " button '" $ Clean(MenuUIBorderButtonWindow(W).buttonText) $ "'";
	if (PersonaBorderButtonWindow(W) != None)
		S = S $ " button '" $ Clean(PersonaBorderButtonWindow(W).buttonText) $ "'";
	if (MenuUITitleWindow(W) != None)
		S = S $ " title '" $ Clean(MenuUITitleWindow(W).titleText) $ "'";
	if (ToggleWindow(W) != None)
		S = S $ " toggle=" $ ToggleWindow(W).GetToggle();
	if (W.GetClientObject() != None)
		S = S $ " object=" $ W.GetClientObject().Name;
	Choice = MenuUIChoice(W);
	if (Choice != None)
	{
		E = MenuUIChoiceEnum(W);
		if (E != None)
			S = S $ " value=" $ E.currentValue $ " '" $ Clean(E.enumText[Clamp(E.currentValue, 0, 39)]) $ "'";
		else if (MenuUIChoiceSlider(W) != None)
			S = S $ " value=" $ MenuUIChoiceSlider(W).GetValue();
		Setting = Choice.configSetting;
		if (Setting != "")
			S = S $ " setting='" $ Setting $ "' get='" $ P.ConsoleCommand("get " $ Setting) $ "'";
	}
	Log(S);

	L = ListWindow(W);
	if (L != None)
	{
		Rows = L.GetNumRows();
		Row = "";
		for (j = 0; j < L.GetNumColumns(); j++)
			Row = Row $ " '" $ Clean(L.GetColumnTitle(j)) $ "' " $ L.GetColumnWidth(j);
		Log(Prefix $ Indent(Level + 1) $ "rows " $ Rows $ ", columns" $ Row);
		for (i = 0; i < Rows && i < 100; i++)
		{
			Row = "";
			for (j = 0; j < L.GetNumColumns(); j++)
			{
				if (j > 0)
					Row = Row $ "|";
				Row = Row $ Clean(L.GetField(L.IndexToRowId(i), j));
			}
			if (L.IsRowSelected(L.IndexToRowId(i)))
				Row = Row $ " selected";
			Log(Prefix $ Indent(Level + 1) $ "row " $ i $ ": " $ Row);
		}
	}

	for (C = W.GetBottomChild(); C != None; C = C.GetHigherSibling())
		DumpWindow(P, C, Level + 1, Prefix);
}
