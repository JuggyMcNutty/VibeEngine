//=============================================================================
// SlideConsole: a walking player on a slippery floor -- logged with
// "DXSLIDE:" and the console's clock (vibe/docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island, datalinks and conversations silenced every tick. The
// slope is the floor under a navigation point whose normal's Z is 0.75 to
// 0.97, the nearest to the player's start; its texture, from TraceTexture,
// the Texture object of that name and group. Each scenario stands the player
// a second, still, then logs it every tick -- its place, velocity,
// acceleration, rotation, physics and state, its zone's water flag and
// ground friction, its base:
//   a1. on the slope, the texture's Friction 0.5, for 4 s: the slide the
//       ladder check gives a walking player on a texture with a Friction
//       under 1;
//   a2. the same with Friction 0;
//   b.  at the start, the zone's ZoneGroundFriction 0.2 (as the Bar's, the
//       Airfield's, the Underworld's and Area 51's zones have it), running 1
//       s along the longest clear line from there, then let go for 6 s: the
//       glide to a stop;
//   c.  on the slope, its zone's ZoneGroundFriction 0.2, for 4 s: the slide
//       a walk gives on a slope where the friction x the floor's normal's Z
//       is under 3.3.
// Each value is put back after its scenario. The forward axis is set every
// tick to 120000 to run (SwimConsole's). Then an exit.
//=============================================================================
class SlideConsole extends Console;

var float CapTime, MapTime, StepTime;
var string CurrentMap;
var int Phase, Scenario, Ticks;
var vector PlayerStand, SlopeSpot, SlopeNormal, RunDir;
var rotator PlayerRot;
var Texture SlopeTex;
var float SlopeFriction, SlopeZoneFriction, ZoneFriction, RunClear;
var ZoneInfo SlopeZone, StandZone;
var bool bSlope;
var string ScenarioName;
var float ScenarioTime;

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

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None)
		DXP.conPlay.TerminateConversation();
}

// The texture TraceTexture meets first along a line, its object found by
// name and group; None for none, or one not found.
function Texture TextureAt(PlayerPawn P, vector Start, vector End, out vector HitLoc, out vector HitNorm)
{
	local Actor A;
	local name TexName, TexGroup;
	local int Flags;
	local Texture T, Found;

	foreach P.TraceTexture(class'Actor', A, TexName, TexGroup, Flags, HitLoc, HitNorm, End, Start)
	{
		if (A != P.Level || TexName == '')
			return None;
		foreach AllObjects(class'Texture', T)
		{
			if (T.Name == TexName && (Found == None || T.Outer.Name == TexGroup))
				Found = T;
		}
		return Found;
	}
	return None;
}

// The sloped floor under a navigation point nearest the player.
function bool FindSlope(PlayerPawn P)
{
	local NavigationPoint N;
	local vector HitLoc, HitNorm;
	local float Best, D;
	local Texture T;

	Best = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		if (N.Region.Zone.bWaterZone)
			continue;
		T = TextureAt(P, N.Location, N.Location - vect(0,0,200), HitLoc, HitNorm);
		if (T == None || HitNorm.Z < 0.75 || HitNorm.Z > 0.97)
			continue;
		D = VSize(HitLoc - P.Location);
		if (D < Best)
		{
			Best = D;
			SlopeSpot = HitLoc;
			SlopeNormal = HitNorm;
			SlopeTex = T;
			SlopeZone = N.Region.Zone;
		}
	}
	if (SlopeTex == None)
		return false;
	SlopeFriction = SlopeTex.Friction;
	SlopeZoneFriction = SlopeZone.ZoneGroundFriction;
	Log("DXSLIDE: the slope at " $ SlopeSpot $ " normal " $ SlopeNormal $ ", " $ VSize(SlopeSpot - P.Location) $ " from the start, texture "
	    $ SlopeTex $ " friction " $ SlopeFriction $ ", zone " $ SlopeZone.Name $ " ground friction " $ SlopeZone.ZoneGroundFriction);
	return true;
}

// The longest clear line from the start, of 16 directions, level and as
// wide as the player.
function FindRun(PlayerPawn P)
{
	local vector HitLoc, HitNorm, Dir, Extent;
	local rotator R;
	local float D;
	local int i;

	Extent.X = P.CollisionRadius;
	Extent.Y = P.CollisionRadius;
	Extent.Z = P.CollisionHeight * 0.5;
	for (i = 0; i < 16; i++)
	{
		R.Yaw = i * 4096;
		Dir = vector(R);
		if (P.Trace(HitLoc, HitNorm, PlayerStand + Dir * 3000, PlayerStand, false, Extent) == None)
			D = 3000;
		else
			D = VSize(HitLoc - PlayerStand);
		if (D > RunClear)
		{
			RunClear = D;
			RunDir = Dir;
		}
	}
	Log("DXSLIDE: the run along " $ RunDir $ ", clear for " $ RunClear);
}

function LogTick(PlayerPawn P)
{
	Log("DXSLIDE: " $ ScenarioName $ " tick " $ Ticks $ " t " $ StepTime $ " loc " $ P.Location $ " vel " $ P.Velocity $ " accel " $ P.Acceleration
	    $ " rot " $ P.Rotation $ " physics " $ int(P.Physics) $ " state " $ P.GetStateName() $ " water " $ P.Region.Zone.bWaterZone
	    $ " friction " $ P.Region.Zone.ZoneGroundFriction $ " base " $ P.Base);
}

// The player put there, facing R, still, falling to its feet.
function Place(PlayerPawn P, vector Where, rotator R)
{
	P.aBaseY = 0;
	if (!P.SetLocation(Where))
		Log("DXSLIDE: " $ ScenarioName $ ": cannot put the player at " $ Where);
	P.SetRotation(R);
	P.ViewRotation = R;
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.SetPhysics(PHYS_Falling);
}

// Scenario's set-up: false once there are no more.
function bool StartScenario(PlayerPawn P)
{
	local rotator R;

	while (Scenario < 4)
	{
		ScenarioTime = 4.0;
		if (Scenario == 2)
		{
			ScenarioName = "b";
			ScenarioTime = 7.0;
			R = rotator(RunDir);
			Place(P, PlayerStand, R);
			return true;
		}
		if (bSlope)
		{
			if (Scenario == 0)
				ScenarioName = "a1";
			else if (Scenario == 1)
				ScenarioName = "a2";
			else
				ScenarioName = "c";
			R = rotator(-SlopeNormal);
			R.Pitch = 0;
			Place(P, SlopeSpot + vect(0,0,1) * (P.CollisionHeight + 4), R);
			return true;
		}
		Scenario++;
	}
	return false;
}

// The scenario's slippery floor, from its start.
function Slip()
{
	if (Scenario == 0)
		SlopeTex.Friction = 0.5;
	else if (Scenario == 1)
		SlopeTex.Friction = 0.0;
	else if (Scenario == 2)
		StandZone.ZoneGroundFriction = 0.2;
	else
		SlopeZone.ZoneGroundFriction = 0.2;
	Log("DXSLIDE: " $ ScenarioName $ ": slippery from " $ StepTime);
}

// Every value put back.
function Unslip()
{
	if (SlopeTex != None)
		SlopeTex.Friction = SlopeFriction;
	if (SlopeZone != None)
		SlopeZone.ZoneGroundFriction = SlopeZoneFriction;
	if (StandZone != None)
		StandZone.ZoneGroundFriction = ZoneFriction;
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
	if (DXP != None)
		Silence(DXP);

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
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 6.0)
		{
			PlayerRot = P.ViewRotation;
			PlayerRot.Pitch = 0;
			PlayerRot.Roll = 0;
			PlayerStand = P.Location;
			StandZone = P.Region.Zone;
			ZoneFriction = StandZone.ZoneGroundFriction;
			Log("DXSLIDE: the start at " $ PlayerStand $ ", zone " $ StandZone.Name $ " ground friction " $ ZoneFriction
			    $ ", the player's ground speed " $ P.GroundSpeed $ " accel rate " $ P.AccelRate $ " step " $ P.MaxStepHeight);
			bSlope = FindSlope(P);
			if (!bSlope)
				Log("DXSLIDE: no slope found");
			FindRun(P);
			Scenario = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (!StartScenario(P))
		{
			Unslip();
			Log("DXSLIDE: done at " $ CapTime $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3)
	{
		// Stood a second, then the slippery floor
		if (StepTime > 1.0)
		{
			Log("DXSLIDE: " $ ScenarioName $ ": the player at " $ P.Location $ " physics " $ int(P.Physics) $ " base " $ P.Base);
			Ticks = 0;
			StepTime = 0;
			Slip();
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		if (Scenario == 2 && StepTime <= 1.0)
			P.aBaseY = 120000.0;
		else
			P.aBaseY = 0;
		Ticks++;
		LogTick(P);
		if (StepTime > ScenarioTime)
		{
			P.aBaseY = 0;
			Unslip();
			Scenario++;
			Phase = 2;
		}
	}
}
