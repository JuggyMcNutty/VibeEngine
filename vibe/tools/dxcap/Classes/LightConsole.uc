//=============================================================================
// LightConsole: a light of one effect on a surface, shot in both engines to
// compare its shape (vibe/docs/DEVELOPMENT.md, scripted runs). On the ini's
// TargetMap ([DXCapture.<console>], Liberty Island by default), the lights
// of LightEffect Effect (12, LE_Spotlight, by default) whose axis meets the
// world within the light's radius, in name order: a spotlight's
// (LE_Spotlight, LE_StaticSpot) along its rotation, any other's straight
// down (Aim 1: along the rotation, 2: down). The Pick-th of them (0 by
// default), or the one named LightName, has the view put View units (150
// by default) off the spot its axis meets, or short of what is in the way,
// facing it along the surface's normal: the player out of the world, with
// no shadow, the HUD hidden, conversations and datalinks stopped as they
// start. Each candidate,
// the spot and the view are logged with "DXLIGHT:", then four shots 2 s
// apart, each marked for the original's grabber; then exit. With ShotAt
// set, a changing light is shot in both engines at that moment of the
// level's time: the level's clock is slowed to a tenth 0.3 s before it and
// to 0.0025 from it, the least the fork's clock takes, and the shots are
// timed by the wall clock (the level's Hour to Millisecond), which the
// slowed clock does not stretch. Every light reaching the spot is logged
// too. Pick -1 is a census of the map instead: every light with an effect,
// its effect and whether its axis meets the world, then an exit.
//=============================================================================
class LightConsole extends Console;

var config string TargetMap;
var config byte Effect;
var config int Pick;
var config int Aim;
var config string LightName;
var config float View;
var config float ShotAt;

var string CurrentMap;
var float MapTime;
var float StepTime;
var float LastReal;
var int Phase;
var int ShotsTaken;
var Actor Chosen;
var Object Stopped;
var vector Spot, SpotNormal, Eye;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

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

// A conversation's camera would take the view, and a datalink's text the
// frame's foot.
function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXLIGHT: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXLIGHT: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

// Seconds of the day by the wall clock.
function float RealTime(LevelInfo L)
{
	return L.Hour * 3600.0 + L.Minute * 60.0 + L.Second + L.Millisecond / 1000.0;
}

function bool AimsAlong()
{
	if (Aim == 1)
		return true;
	if (Aim == 2)
		return false;
	return Effect == 12 || Effect == 8;
}

function float Radius(Actor A)
{
	return 25.0 * (float(A.LightRadius) + 1.0);
}

function vector Axis(Actor A)
{
	if (AimsAlong())
		return vector(A.Rotation);
	return vect(0,0,-1);
}

function Census(PlayerPawn P)
{
	local Actor A;
	local vector HitLocation, HitNormal, Dir;
	local string Meets;
	local int Count;

	foreach P.AllActors(class'Actor', A)
	{
		if (A.LightType != LT_None && A.LightEffect != LE_None)
		{
			Count++;
			if (A.LightEffect == LE_Spotlight || A.LightEffect == LE_StaticSpot)
				Dir = vector(A.Rotation);
			else
				Dir = vect(0,0,-1);
			Meets = "none";
			if (P.Trace(HitLocation, HitNormal, A.Location + Dir * Radius(A), A.Location, false) != None)
				Meets = VSize(HitLocation - A.Location) $ " normal " $ HitNormal;
			Log("DXLIGHT: census " $ int(A.LightEffect) $ " " $ A.Name $ " at " $ A.Location $ " rotation " $ A.Rotation
				$ " cone " $ A.LightCone $ " radius " $ A.LightRadius $ " brightness " $ A.LightBrightness
				$ " static " $ A.bStatic $ " meets " $ Meets);
		}
	}
	Log("DXLIGHT: census of " $ Count $ " lights with an effect");
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

// The candidates in name order; the Pick-th made the one shot.
function Choose(PlayerPawn P)
{
	local Actor A, T, Cands[64];
	local vector HitLocation, HitNormal, Ends[64], Normals[64], V;
	local int Count, Total, i;
	local float Dist;

	foreach P.AllActors(class'Actor', A)
	{
		if (A.LightType != LT_None && A.LightEffect == Effect)
		{
			Total++;
			if (Count < 64 && P.Trace(HitLocation, HitNormal, A.Location + Axis(A) * Radius(A), A.Location, false) != None)
			{
				Cands[Count] = A;
				Ends[Count] = HitLocation;
				Normals[Count] = HitNormal;
				Count++;
				for (i = Count - 1; i > 0 && Caps(string(Cands[i].Name)) < Caps(string(Cands[i - 1].Name)); i--)
				{
					T = Cands[i]; Cands[i] = Cands[i - 1]; Cands[i - 1] = T;
					V = Ends[i]; Ends[i] = Ends[i - 1]; Ends[i - 1] = V;
					V = Normals[i]; Normals[i] = Normals[i - 1]; Normals[i - 1] = V;
				}
			}
		}
	}
	Log("DXLIGHT: " $ Total $ " lights of effect " $ Effect $ ", " $ Count $ " meeting the world along their axis");
	for (i = 0; i < Count; i++)
	{
		A = Cands[i];
		Log("DXLIGHT: candidate " $ i $ " " $ A.Name $ " at " $ A.Location $ " rotation " $ A.Rotation
			$ " type " $ int(A.LightType) $ " cone " $ A.LightCone $ " radius " $ A.LightRadius $ " brightness " $ A.LightBrightness
			$ " hue " $ A.LightHue $ " saturation " $ A.LightSaturation $ " period " $ A.LightPeriod $ " phase " $ A.LightPhase
			$ " static " $ A.bStatic $ " spot " $ Ends[i] $ " distance " $ VSize(Ends[i] - A.Location));
	}
	if (LightName != "")
	{
		for (i = 0; i < Count && !(string(Cands[i].Name) ~= LightName); i++)
			;
		Pick = i;
	}
	if (Pick >= Count)
	{
		Log("DXLIGHT: no candidate " $ Pick $ " " $ LightName);
		return;
	}
	Chosen = Cands[Pick];
	Spot = Ends[Pick];
	SpotNormal = Normals[Pick];
	Dist = View;
	if (Dist <= 0)
		Dist = 150;
	if (P.Trace(HitLocation, HitNormal, Spot + SpotNormal * Dist, Spot + SpotNormal * 2, false) != None)
		Dist = FMax(VSize(HitLocation - Spot) - 8, 16);
	Eye = Spot + SpotNormal * Dist;
	foreach P.AllActors(class'Actor', T)
		if (T.LightType != LT_None && VSize(T.Location - Spot) < Radius(T))
			Log("DXLIGHT: reaching the spot " $ T.Name $ " type " $ int(T.LightType) $ " effect " $ int(T.LightEffect)
				$ " brightness " $ T.LightBrightness $ " radius " $ T.LightRadius $ " hue " $ T.LightHue $ " saturation " $ T.LightSaturation
				$ " period " $ T.LightPeriod $ " phase " $ T.LightPhase $ " static " $ T.bStatic $ " " $ VSize(T.Location - Spot) $ " away");
	Log("DXLIGHT: chosen " $ Chosen.Name $ " spot " $ Spot $ " normal " $ SpotNormal $ " axis " $ Axis(Chosen)
		$ " axis to normal " $ (-Axis(Chosen) dot SpotNormal) $ " view distance " $ Dist);
}

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = rotator(Spot - Eye);
	// Straight down or up the yaw is the rounding's: none.
	if (Abs(SpotNormal.Z) > 0.9999)
		P.ViewRotation.Yaw = 0;
	P.SetRotation(P.ViewRotation);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	M = MapOf(P);
	if (M != CurrentMap)
	{
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	if (ShotAt > 0 && P.Level.TimeDilation < 1.0)
	{
		if (LastReal > 0 && RealTime(P.Level) >= LastReal)
			StepTime += RealTime(P.Level) - LastReal;
		LastReal = RealTime(P.Level);
	}
	else
		StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None && (M ~= Target()))
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXLIGHT: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None && Pick < 0)
		{
			Census(P);
			P.ConsoleCommand("exit");
			Phase = 9;
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			Choose(P);
			if (Chosen == None)
			{
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.KillShadow();
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			HoldView(P);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && (M ~= Target()))
		Phase = 0;
	else if (Phase == 1)
	{
		HoldView(P);
		if (ShotAt > 0 && ShotsTaken < 4)
		{
			if (P.Level.TimeSeconds >= ShotAt)
			{
				if (P.Level.TimeDilation == 1.0)
					Log("DXLIGHT: the level's time " $ P.Level.TimeSeconds $ " passed ShotAt " $ ShotAt $ " before the view settled");
				P.Level.TimeDilation = 0.0025;
			}
			else
			{
				if (StepTime > 3.0 && P.Level.TimeSeconds >= ShotAt - 0.3)
					P.Level.TimeDilation = 0.1;
				return;
			}
		}
		// Settled after 3 s; a shot every 2 s from then, the mark shown for
		// the 0.6 s before each.
		if (StepTime > 3.0 && ShotsTaken < 4)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 2.4;
			}
			else
			{
				Log("DXLIGHT: shot " $ ShotsTaken $ " view at " $ (P.Location + vect(0,0,1) * P.EyeHeight) $ " rotation " $ P.ViewRotation
					$ " FOV " $ P.FOVAngle $ " time " $ P.Level.TimeSeconds);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 1.0;
			}
		}
		else if (ShotsTaken >= 4 && StepTime > 2.0)
		{
			Log("DXLIGHT: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}

defaultproperties
{
	Effect=12
}
