//=============================================================================
// CoronaConsole: Liberty Island's lamps' coronas, looked at -- the island
// opened from wherever the run starts, then 6 s in the player stood where
// CaptureConsole's shot 7 stands (Light169 from afar, where the original
// shows three glows), the HUD hidden; each corona light in sight logged with
// what its glow is made of (draw scale, hue, saturation, skin); shots 3 s and
// 5 s after standing, marked for the original's grabber; exits 2 s later.
//=============================================================================
class CoronaConsole extends Console;

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
	local Actor A;
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
			Log("DXCORONA: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
	}
	else if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;
	else if (Step == 0 && MapTime > 6.0)
	{
		StandLoc = vect(-873.028076, 5163.140625, 559.696167);
		R.Pitch = -1846;
		R.Yaw = -28672;
		R.Roll = 0;
		StandRot = R;
		P.SetLocation(StandLoc);
		P.ViewRotation = R;
		P.SetRotation(R);
		P.SetPhysics(PHYS_None);
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		Log("DXCORONA: standing at " $ P.Location $ " facing " $ P.ViewRotation);
		foreach P.AllActors(class'Actor', A)
			if (A.bCorona && A.Skin != None && VSize(A.Location - P.Location) < 3000)
				Log("DXCORONA: " $ string(A) $ " at " $ A.Location $ " scale " $ A.DrawScale $ " hue " $ A.LightHue $ " sat " $ A.LightSaturation $ " bright " $ A.LightBrightness $ " type " $ A.LightType $ " skin " $ string(A.Skin) $ " distance " $ VSize(A.Location - P.Location));
		Step = 1;
	}
	else if (Step >= 1 && Step <= 2 && MapTime < 7.0)
	{
		// Held there a moment, as CaptureConsole holds its player.
		P.SetLocation(StandLoc);
		P.SetRotation(StandRot);
		P.ViewRotation = StandRot;
		P.Velocity = vect(0,0,0);
		P.SetPhysics(PHYS_None);
	}
	else if ((Step == 1 && MapTime > 9.0) || (Step == 2 && MapTime > 11.0))
	{
		P.ViewRotation = StandRot;
		Log("DXCAP: shot " $ Step $ " at " $ P.Location $ " facing " $ P.ViewRotation);
		P.ConsoleCommand("shot");
		Step++;
	}
	else if (Step == 3 && MapTime > 13.0)
	{
		Log("DXCORONA: exiting");
		P.ConsoleCommand("exit");
		Step = 4;
	}
}

// The original's grabber keeps the frames marked as CaptureConsole marks
// them: a magenta block, then the shot's number in eight blocks.
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (Step != 1 && Step != 2)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((Step & (1 << i)) != 0)
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
