//=============================================================================
// FlashConsole: the screen flash -- Liberty Island opened from wherever the
// run starts, then 6 s in the player stood where CoronaConsole stands it,
// the HUD hidden; a shot 3 s later with no flash, then a steady glow set
// (ConstantGlowScale -0.5 and a dark red ConstantGlowFog, which the game's
// ViewFlash turns into a FlashScale of 0.5 and that fog) and a second shot
// 3 s after it, each marked for the original's grabber and its flash logged;
// exits 2 s later.
//=============================================================================
class FlashConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var vector StandLoc;
var rotator StandRot;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXFLASH: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 6.0)
	{
		StandLoc = vect(-873.028076, 5163.140625, 559.696167);
		R.Pitch = -1846;
		R.Yaw = -28672;
		R.Roll = 0;
		StandRot = R;
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		Log("DXFLASH: standing at " $ StandLoc $ " facing " $ StandRot);
		Step = 1;
	}
	if (Step >= 1)
	{
		// Held there throughout, as CoronaConsole holds its player.
		P.SetLocation(StandLoc);
		P.SetRotation(StandRot);
		P.ViewRotation = StandRot;
		P.Velocity = vect(0,0,0);
		P.SetPhysics(PHYS_None);
	}
	if (Step == 1 && MapTime > 9.0)
	{
		Log("DXCAP: shot 1 flash scale " $ P.FlashScale $ " fog " $ P.FlashFog);
		P.ConsoleCommand("shot");
		P.ConstantGlowScale = -0.5;
		P.ConstantGlowFog = vect(0.2, 0.05, 0.0);
		Step = 2;
	}
	else if (Step == 2 && MapTime > 12.0)
	{
		Log("DXCAP: shot 2 flash scale " $ P.FlashScale $ " fog " $ P.FlashFog);
		P.ConsoleCommand("shot");
		Step = 3;
	}
	else if (Step == 3 && MapTime > 14.0)
	{
		Log("DXFLASH: exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}

// The original's grabber keeps the frames marked as CaptureConsole marks
// them: a magenta block, then the shot's number in eight blocks. The mark
// is drawn after the flash, so the flash leaves it alone.
event PostRender(canvas C)
{
	local int i, Shot;

	Super.PostRender(C);
	if (Step == 1)
		Shot = 1;
	else if (Step == 2 && MapTime > 11.0)
		Shot = 2;
	else
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((Shot & (1 << i)) != 0)
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
