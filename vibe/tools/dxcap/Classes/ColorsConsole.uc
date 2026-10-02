//=============================================================================
// ColorsConsole: the Colors settings screen (MenuScreenRGB), whose two
// example panes draw short frames with GC.DrawBorders -- a list focus border
// 11 and 12 pixels tall, shorter than its two margins, so they shrink
// (vibe/docs/DEVELOPMENT.md, scripted runs). From the menu map: the main menu
// shown, the screen pushed, a marked shot, an exit.
//=============================================================================
class ColorsConsole extends Console;

var float RunTime;
var int Phase;
var int ShotCount;

var int MarkShot;
var bool bMarking;
var float MarkTime;
var bool bShotPending;

function Shot(PlayerPawn P, string Label)
{
	Log("DXCOLORS: shot " $ ShotCount $ " = " $ Label);
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

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExRootWindow root;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	TickShot(P, Delta);
	if (bShotPending)
		return;

	if (Phase == 0 && RunTime > 3.0)
	{
		DeusExPlayer(P).ShowMainMenu();
		Phase = 1;
	}
	else if (Phase == 1 && RunTime > 4.0)
	{
		root = DeusExRootWindow(DeusExPlayer(P).rootWindow);
		root.PushWindow(class'MenuScreenRGB');
		Log("DXCOLORS: top window " $ root.GetTopWindow().Class);
		Phase = 2;
	}
	else if (Phase == 2 && RunTime > 6.0)
	{
		Shot(P, "the Colors screen");
		Phase = 3;
	}
	else if (Phase == 3 && RunTime > 8.0)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 4;
	}
}
