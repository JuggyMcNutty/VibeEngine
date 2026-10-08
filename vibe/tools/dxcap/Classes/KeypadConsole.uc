//=============================================================================
// KeypadConsole: the root window's focus with no modal up and around the
// keypad, and the keys a modal is handed -- logged with "DXKEYPAD:" and the
// console's clock (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, datalinks aborted as they come, the root's focus is
// logged on two ticks with no modal up. Keypad0 (UN_Keypad1) is then opened
// as the player opens it (DeusExPlayer.ActivateKeypadWindow, not hacked),
// and the focus, the keypad's focusMode and its inputCode logged in that
// tick, the next five and a second later; it is popped (PopWindow) and the
// focus logged over five ticks, a second and three seconds later. Then, for
// a timeline's walk, the player faces ground clear and floored for 600
// units, and its place and velocity are logged every quarter second for
// 25 s; then the package's KeyLogWindow is up for 45 s, logging every
// key the root hands it. vibe/tools/dxcap/timelines/stray-release.txt drives
// both on the fork with a release of E that no press came before.
//=============================================================================
class KeypadConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var string CurrentMap;
var int Phase;
var int Ticks;
var bool bLoggedSecond;
var Keypad Pad;
var KeyLogWindow LogWin;

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

// A window's name for the log, "None" for none.
function string WinName(Window w)
{
	if (w == None)
		return "None";
	return string(w.Name) $ " (" $ string(w.Class.Name) $ ")";
}

function LogFocus(DeusExPlayer DXP, string Label)
{
	local DeusExRootWindow root;

	root = DeusExRootWindow(DXP.rootWindow);
	if (root == None)
	{
		Log("DXKEYPAD: " $ Label $ ": no root window");
		return;
	}
	Log("DXKEYPAD: " $ Label $ " at " $ CapTime $ ": focus " $ WinName(root.GetFocusWindow())
	    $ ", top window " $ WinName(root.GetTopWindow()));
	if (Pad != None && Pad.keypadwindow != None)
		Log("DXKEYPAD: " $ Label $ ": keypad focusMode " $ int(Pad.keypadwindow.focusMode)
		    $ " inputCode '" $ Pad.keypadwindow.inputCode $ "'");
}

// The player faced along ground clear of walls and floored for 600 units,
// the first of 16 headings that is, for the timeline's walk.
function FaceOpenGround(PlayerPawn P)
{
	local int i, k;
	local vector Dir, HitLoc, HitNorm, Ahead, Extent;
	local rotator R;
	local bool bClear;

	Extent.X = P.CollisionRadius;
	Extent.Y = P.CollisionRadius;
	Extent.Z = P.CollisionHeight * 0.5;
	for (i = 0; i < 16; i++)
	{
		R.Pitch = 0;
		R.Yaw = i * 4096;
		R.Roll = 0;
		Dir = vector(R);
		if (P.Trace(HitLoc, HitNorm, P.Location + Dir * 600, P.Location, False, Extent) != None)
			continue;
		bClear = True;
		for (k = 1; k <= 3; k++)
		{
			Ahead = P.Location + Dir * (200 * k);
			if (P.Trace(HitLoc, HitNorm, Ahead - vect(0,0,1) * (P.CollisionHeight + 40), Ahead, False) == None)
				bClear = False;
		}
		if (!bClear)
			continue;
		P.SetRotation(R);
		P.ViewRotation = R;
		Log("DXKEYPAD: facing yaw " $ R.Yaw $ " for the walk");
		return;
	}
	Log("DXKEYPAD: no clear, floored heading; facing as before");
}

function Keypad FindPad(PlayerPawn P)
{
	local Keypad K;

	foreach P.AllActors(class'Keypad', K)
	{
		if (K.Name == 'Keypad0')
			return K;
	}
	return None;
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
	if (DXP != None && DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();

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
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 8.0)
		{
			if (DXP == None)
			{
				Log("DXKEYPAD: the player is no DeusExPlayer, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Ticks = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		LogFocus(DXP, "no modal, tick " $ Ticks);
		Ticks++;
		if (Ticks == 2)
			Phase = 3;
	}
	else if (Phase == 3)
	{
		Pad = FindPad(P);
		if (Pad == None)
		{
			Log("DXKEYPAD: no Keypad0, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Log("DXKEYPAD: opening " $ Pad.Name $ " (" $ Pad.Class.Name $ ", tag " $ Pad.Tag $ ")");
		DXP.ActivateKeypadWindow(Pad, False);
		LogFocus(DXP, "keypad opened, same tick");
		Ticks = 0;
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4)
	{
		Ticks++;
		if (Ticks <= 5)
			LogFocus(DXP, "keypad up, tick " $ Ticks);
		if (StepTime >= 1.0)
		{
			LogFocus(DXP, "keypad up, +1 s");
			root = DeusExRootWindow(DXP.rootWindow);
			root.PopWindow();
			LogFocus(DXP, "keypad popped, same tick");
			Ticks = 0;
			StepTime = 0;
			bLoggedSecond = false;
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		Ticks++;
		if (Ticks <= 5)
			LogFocus(DXP, "popped, tick " $ Ticks);
		if (StepTime >= 1.0 && !bLoggedSecond)
		{
			LogFocus(DXP, "popped, +1 s");
			bLoggedSecond = true;
		}
		if (StepTime >= 3.0)
		{
			LogFocus(DXP, "popped, +3 s");
			FaceOpenGround(P);
			StepTime = 0;
			LogTime = 0;
			Phase = 6;
		}
	}
	else if (Phase == 6)
	{
		LogTime += Delta;
		if (LogTime >= 0.25)
		{
			LogTime = 0;
			Log("DXKEYPAD: walk at " $ CapTime $ ": place " $ P.Location $ " velocity " $ P.Velocity);
		}
		if (StepTime >= 25.0)
		{
			root = DeusExRootWindow(DXP.rootWindow);
			LogWin = KeyLogWindow(root.NewChild(class'KeyLogWindow'));
			Log("DXKEYPAD: logging window up at " $ CapTime $ ": " $ WinName(LogWin));
			LogFocus(DXP, "logging window up");
			StepTime = 0;
			LogTime = 0;
			Phase = 7;
		}
	}
	else if (Phase == 7)
	{
		LogTime += Delta;
		if (LogTime >= 5.0)
		{
			LogTime = 0;
			LogFocus(DXP, "logging window still up");
		}
		if (StepTime >= 45.0)
		{
			if (LogWin != None)
				LogWin.Destroy();
			Log("DXKEYPAD: done at " $ CapTime $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
