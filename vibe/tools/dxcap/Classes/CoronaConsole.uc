//=============================================================================
// CoronaConsole: Liberty Island's lamps' coronas, looked at -- the island
// opened from wherever the run starts, then 6 s in the player stood where
// CaptureConsole's shot 7 stands (Light169 from afar, where the original
// shows three glows), the HUD hidden; each corona light within 3000 units logged with
// what its glow is made of (draw scale, hue, saturation, skin); shots 3 s and
// 5 s after standing, marked for the original's grabber. Then a dynamic
// light of the package's own (CoronaLight, Light169's skin and look), 15
// degrees above the view's axis, clear of the lamps' glows, is shot four
// times, each place logged with its BSP leaf and the player's: in the
// player's leaf; some 600 units ahead in another leaf, its LightRadius 40;
// the same at 2; and 55 degrees off the view's axis, at 40. It exits 2 s
// after the last.
//=============================================================================
class CoronaConsole extends Console;

var float MapTime;
var int Step;
var int MarkShot;
var string CurrentMap;
var vector StandLoc;
var rotator StandRot;
var CoronaLight Lamp;
var float AheadDist;

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

// The lamp put Dist from the eye, 15 degrees above the view's axis and Turn
// off it, at LightRadius Radius; its leaf and the player's logged.
function PlaceLamp(PlayerPawn P, float Dist, int Turn, byte Radius, string Label)
{
	local rotator R;
	local vector Eye;

	Eye = P.Location;
	Eye.Z += P.EyeHeight;
	R = StandRot;
	R.Pitch += 2731;
	R.Yaw += Turn;
	Lamp.SetLocation(Eye + vector(R) * Dist);
	Lamp.LightRadius = Radius;
	Log("DXCORONA: lamp for " $ Label $ " at " $ Lamp.Location $ " (" $ Dist $ " from the eye, " $ Turn
	    $ " off the axis), LightRadius " $ Lamp.LightRadius $ ", leaf " $ Lamp.Region.iLeaf $ ", the player's leaf "
	    $ P.Region.iLeaf);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local Actor A;
	local Light L, Model;
	local rotator R;
	local float D;

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
		MarkShot = 1;
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
		MarkShot = 0;
		if (Step == 2)
			MarkShot = 2;
	}
	else if (Step == 3 && MapTime > 12.0)
	{
		foreach P.AllActors(class'Light', L)
			if (L.Name == 'Light169')
				Model = L;
		Lamp = P.Spawn(class'CoronaLight',,, P.Location);
		if (Lamp == None || Model == None)
		{
			Log("DXCORONA: no lamp or no Light169, exiting");
			P.ConsoleCommand("exit");
			Step = 9;
			return;
		}
		Lamp.Skin = Model.Skin;
		Lamp.DrawScale = Model.DrawScale;
		Lamp.LightHue = Model.LightHue;
		Lamp.LightSaturation = Model.LightSaturation;
		// In the player's leaf: the farthest of these that is
		for (D = 150; D >= 30; D -= 30)
		{
			PlaceLamp(P, D, 0, 40, "shot 3");
			if (Lamp.Region.iLeaf == P.Region.iLeaf)
				break;
		}
		MarkShot = 3;
		Step = 4;
	}
	else if (Step == 4 && MapTime > 14.5)
	{
		Log("DXCAP: shot 3 at " $ P.Location $ " facing " $ P.ViewRotation);
		P.ConsoleCommand("shot");
		// Some 600 units ahead, in another leaf
		for (AheadDist = 600; AheadDist <= 1200; AheadDist += 100)
		{
			PlaceLamp(P, AheadDist, 0, 40, "shot 4");
			if (Lamp.Region.iLeaf != P.Region.iLeaf)
				break;
		}
		MarkShot = 4;
		Step = 5;
	}
	else if (Step == 5 && MapTime > 17.0)
	{
		Log("DXCAP: shot 4 at " $ P.Location $ " facing " $ P.ViewRotation);
		P.ConsoleCommand("shot");
		PlaceLamp(P, AheadDist, 0, 2, "shot 5");
		MarkShot = 5;
		Step = 6;
	}
	else if (Step == 6 && MapTime > 19.5)
	{
		Log("DXCAP: shot 5 at " $ P.Location $ " facing " $ P.ViewRotation);
		P.ConsoleCommand("shot");
		PlaceLamp(P, AheadDist, 10012, 40, "shot 6");
		MarkShot = 6;
		Step = 7;
	}
	else if (Step == 7 && MapTime > 22.0)
	{
		Log("DXCAP: shot 6 at " $ P.Location $ " facing " $ P.ViewRotation);
		P.ConsoleCommand("shot");
		MarkShot = 0;
		Step = 8;
	}
	else if (Step == 8 && MapTime > 24.0)
	{
		Log("DXCORONA: exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}

// The original's grabber keeps the frames marked as CaptureConsole marks
// them: a magenta block, then the shot's number in eight blocks.
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (MarkShot == 0)
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
