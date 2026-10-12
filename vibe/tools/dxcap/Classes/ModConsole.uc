//=============================================================================
// ModConsole: the natives no stock script calls, which only a mod reaches,
// logged with "DXMOD:" from the menu map, then an exit -- clock, unclock and
// CyclesToSeconds round a loop; DeusExPlayer.SetBoolFlagFromString, the flag
// read back with its expiration under a default expiration of 7, and with an
// empty string; an ExtString's text in parts (GetFirstTextPart, then
// GetNextTextPart until none is left) for texts of 600, 478 and 5 characters
// and none, and GetTextPart at its edges; ResetConfig of a User class
// (DeusExPlayer: a key DefUser.ini has and one it lacks, each SET first) and
// of a System one (GameReplicationInfo's ServerName), each read back with
// GET and from the live object (vibe/docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class ModConsole extends Console;

var float RunTime;
var bool bDone;

function string Ends(string S)
{
	return Len(S) $ " '" $ Left(S, 1) $ "'..'" $ Right(S, 1) $ "'";
}

function string Pattern(int n)
{
	local string S;
	local int i;
	for (i = 0; i < n; i++)
		S = S $ Chr(65 + i % 26);
	return S;
}

function Parts(ExtString E, string Label)
{
	local string S;
	local int n, i;

	S = "unset";
	n = E.GetFirstTextPart(S);
	Log("DXMOD: " $ Label $ ": length " $ E.GetTextLength() $ ", first part " $ n $ " " $ Ends(S));
	for (i = 0; i < 5; i++)
	{
		S = "unset";
		n = E.GetNextTextPart(S);
		Log("DXMOD: " $ Label $ ": next part " $ n $ " " $ Ends(S));
	}
}

function TextPart(ExtString E, int StartPos, int Count)
{
	local string S;
	local int n;

	S = "unset";
	n = E.GetTextPart(StartPos, Count, S);
	Log("DXMOD: GetTextPart(" $ StartPos $ ", " $ Count $ ") " $ n $ " " $ Ends(S));
}

function LogGet(PlayerPawn P, string Setting)
{
	Log("DXMOD: GET " $ Setting $ " = '" $ P.ConsoleCommand("get " $ Setting) $ "'");
}

event Tick(float Delta)
{
	local DeusExPlayer P;
	local ExtString E;
	local int c, i;
	local float f, secs;
	local name N;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	bDone = true;
	P = DeusExPlayer(Viewport.Actor);

	// The timing natives
	clock(c);
	for (i = 0; i < 20000; i++)
		f += Sqrt(i);
	unclock(c);
	secs = CyclesToSeconds(c);
	Log("DXMOD: clock and unclock round a loop, in seconds over 0 and under 1: " $ (secs > 0 && secs < 1) $ "; CyclesToSeconds(0) " $ CyclesToSeconds(0));

	// SetBoolFlagFromString
	if (P != None && P.flagBase != None)
	{
		P.flagBase.SetDefaultExpiration(7);
		N = P.SetBoolFlagFromString("ModConsoleFlag", True);
		Log("DXMOD: SetBoolFlagFromString gives '" $ N $ "', the flag " $ P.flagBase.GetBool('ModConsoleFlag') $ ", expiration " $ P.flagBase.GetExpiration('ModConsoleFlag', FLAG_Bool));
		N = P.SetBoolFlagFromString("ModConsoleFlag", False);
		Log("DXMOD: set again False gives '" $ N $ "', the flag " $ P.flagBase.GetBool('ModConsoleFlag') $ ", expiration " $ P.flagBase.GetExpiration('ModConsoleFlag', FLAG_Bool));
		N = P.SetBoolFlagFromString("", True);
		Log("DXMOD: an empty string gives '" $ N $ "'");
	}
	else
		Log("DXMOD: no player or flag base");

	// ExtString
	E = new(None) class'ExtString';
	E.SetText(Pattern(600));
	Parts(E, "600");
	TextPart(E, 0, 10);
	TextPart(E, -5, 10);
	TextPart(E, 590, 50);
	TextPart(E, 600, 1);
	TextPart(E, 700, 5);
	TextPart(E, 5, 300);
	TextPart(E, 10, -3);
	E.SetText(Pattern(478));
	Parts(E, "478");
	E.SetText("Hello");
	E.AppendText(" world");
	Log("DXMOD: SetText and AppendText give '" $ E.GetText() $ "'");
	E.SetText("Hello");
	Parts(E, "5");
	E.SetText("");
	Parts(E, "empty");

	// ResetConfig
	if (P != None)
	{
		P.ConsoleCommand("set DeusExPlayer logTimeout 9.5");
		P.ConsoleCommand("set DeusExPlayer bShowAmmoDescriptions " $ !class'DeusExPlayer'.default.bShowAmmoDescriptions);
		LogGet(P, "DeusExPlayer logTimeout");
		LogGet(P, "DeusExPlayer bShowAmmoDescriptions");
		Log("DXMOD: the player's logTimeout " $ P.logTimeout);
		class'DeusExPlayer'.static.ResetConfig();
		Log("DXMOD: DeusExPlayer ResetConfig");
		LogGet(P, "DeusExPlayer logTimeout");
		LogGet(P, "DeusExPlayer bShowAmmoDescriptions");
		Log("DXMOD: the player's logTimeout " $ P.logTimeout $ ", DeusExPlayer's default " $ class'DeusExPlayer'.default.logTimeout $ ", JCDentonMale's " $ class'JCDentonMale'.default.logTimeout);

		P.ConsoleCommand("set GameReplicationInfo ServerName Mod Test Server");
		LogGet(P, "GameReplicationInfo ServerName");
		class'GameReplicationInfo'.static.ResetConfig();
		Log("DXMOD: GameReplicationInfo ResetConfig");
		LogGet(P, "GameReplicationInfo ServerName");
		if (P.GameReplicationInfo != None)
			Log("DXMOD: the game's GameReplicationInfo's ServerName '" $ P.GameReplicationInfo.ServerName $ "'");
	}

	Log("DXMOD: done");
	Viewport.Actor.ConsoleCommand("exit");
}
