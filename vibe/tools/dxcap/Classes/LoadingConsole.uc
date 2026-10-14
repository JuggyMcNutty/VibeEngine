//=============================================================================
// LoadingConsole: the loading screen, the same in both engines
// (dx-reverse-info engine-dll.md, the loading screen). From the menu map,
// each step a second after a beep:
// 1. 3 s in, 01_NYC_UNATCOIsland opened: LOADING over the menu map.
// 2. 8 s into the island, 01_NYC_UNATCOHQ opened, within mission 1: SAVING
//    (the island into Current), then LOADING.
// 3. 8 s into UNATCO HQ, the game saved to slot 9 by a SaveHelper (never
//    from this console's tick, which the original runs inside a draw):
//    SAVING over the HUD.
// 4. 4 s later, slot 9 loaded: LOADING, the same map again; 8 s into it, an
//    exit.
// Its PostRender, after Super's, logs each frame drawn with a level action --
// the action, the map, the pauser, bShowMenu, the player's flash and fog,
// the console's TextLines, the wall clock -- and marks it for grab.py as
// MeshConsole marks its shots; the next tick takes a fork shot of it, the
// last frame drawn, and logs the clock: the frame was up until then.
// Two frames drawn before that tick (a travel's SAVING, then LOADING) share
// the mark's number, and the shot is the last. Nothing advances while a
// level action is up: the original ticks the console inside the paint's draw
// too, the fork does not. It uses Save\Save0009 and Save\Current as
// SaveConsole does. Each line is logged with "DXLOADING:" in front, a beep
// with the console's clock.
//=============================================================================
class LoadingConsole extends SaveConsole;

var float RunTime;
var bool bNewLevel;
var bool bBeeped;
var int Frames;
var int ShotsTaken;
var bool bShotDue;

event NotifyLevelChange()
{
	Super.NotifyLevelChange();
	bNewLevel = true;
}

function string ActionName(byte Action)
{
	switch (Action)
	{
		case 0: return "none";
		case 1: return "LOADING";
		case 2: return "SAVING";
		case 3: return "CONNECTING";
		case 4: return "PRECACHING";
	}
	return "action " $ Action;
}

// The mark for the original's screen grabber, as MeshConsole's.
function DrawMark(canvas C, int Number)
{
	local int i;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((Number & (1 << i)) != 0)
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

event PostRender(canvas C)
{
	local PlayerPawn P;
	local string Word;
	local float XL, YL;

	Super.PostRender(C);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.LevelAction == LEVACT_None)
		return;
	Frames++;
	Log("DXLOADING: frame " $ Frames $ " drawn with " $ ActionName(P.Level.LevelAction) $ " in " $ MapOf(P)
		$ ": pauser '" $ P.Level.Pauser $ "', bShowMenu " $ P.bShowMenu $ ", flash " $ P.FlashScale
		$ " fog " $ P.FlashFog $ ", " $ TextLines $ " text lines; shot " $ ShotsTaken $ "; clock " $ WallClock(P));
	// Where PrintActionMessage put the word: the frame's middle less half
	// its size in the large font.
	if (P.Level.LevelAction == LEVACT_Loading)
		Word = LoadingMessage;
	else if (P.Level.LevelAction == LEVACT_Saving)
		Word = SavingMessage;
	else if (P.Level.LevelAction == LEVACT_Connecting)
		Word = ConnectingMessage;
	else
		Word = PrecachingMessage;
	C.Font = C.LargeFont;
	C.StrLen(Word, XL, YL);
	Log("DXLOADING: " $ Word $ " is " $ XL $ "x" $ YL $ " at " $ (FrameX / 2 - XL / 2) $ "," $ (FrameY / 2 - YL / 2)
		$ " in a frame of " $ FrameX $ "x" $ FrameY);
	DrawMark(C, ShotsTaken);
	bShotDue = true;
}

// The wall clock in milliseconds of the day, the level's as its last tick
// set it.
function int WallClock(PlayerPawn P)
{
	return ((P.Level.Hour * 60 + P.Level.Minute) * 60 + P.Level.Second) * 1000 + P.Level.Millisecond;
}

// True once MapTime reaches At, the beep a second before.
function bool Due(PlayerPawn P, float At)
{
	if (!bBeeped && MapTime >= At - 1.0)
	{
		Log("DXLOADING: beep at " $ RunTime);
		P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 4000, 1.0);
		bBeeped = true;
	}
	if (MapTime < At)
		return false;
	bBeeped = false;
	return true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local SaveHelper S;

	Super(Console).Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (P.Level.LevelAction != LEVACT_None)
		return;
	RunTime += Delta;
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (bShotDue)
	{
		// The fork's shot is of the last frame drawn: the one marked.
		Log("DXLOADING: shot " $ ShotsTaken $ " at clock " $ WallClock(P));
		P.ConsoleCommand("shot");
		ShotsTaken++;
		bShotDue = false;
	}
	if (bNewLevel)
	{
		bNewLevel = false;
		CurrentMap = MapOf(P);
		MapTime = 0;
		Log("DXLOADING: now in " $ CurrentMap $ " at " $ RunTime);
	}

	if (Step == 0 && Due(P, 3.0))
	{
		Log("DXLOADING: opening 01_NYC_UNATCOIsland at " $ RunTime);
		P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		Step = 1;
	}
	else if (Step == 1 && MapOf(P) ~= "01_NYC_UNATCOIsland" && Due(P, 8.0))
	{
		Log("DXLOADING: opening 01_NYC_UNATCOHQ at " $ RunTime);
		P.ConsoleCommand("open 01_NYC_UNATCOHQ");
		Step = 2;
	}
	else if (Step == 2 && MapOf(P) ~= "01_NYC_UNATCOHQ" && Due(P, 8.0))
	{
		Log("DXLOADING: saving to slot 9 at " $ RunTime);
		S = P.Spawn(class'SaveHelper', P);
		S.Slot = 9;
		S.Description = "DXCapture loading";
		Step = 3;
	}
	else if (Step == 3 && Due(P, 12.0))
	{
		Log("DXLOADING: loading slot 9 at " $ RunTime);
		DeusExPlayer(P).LoadGame(9);
		Step = 4;
		MapTime = -1000;
	}
	else if (Step == 4 && MapOf(P) ~= "01_NYC_UNATCOHQ" && Due(P, 8.0))
	{
		Log("DXLOADING: done, " $ Frames $ " frames drawn with a level action, " $ ShotsTaken $ " shots; exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
