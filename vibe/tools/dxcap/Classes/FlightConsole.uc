//=============================================================================
// FlightConsole: a flying NPC's flight -- logged with "DXFLIGHT:" and the
// console's clock (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, datalinks and conversations silenced every tick, at the
// pier's edge RestConsole finds, the water line 80 past it (SwimConsole's),
// the player stood on the pier 150 back from the edge. Each scenario spawns
// a fresh CapFlyer (a Seagull), holds it a second, then moves it with
// CapFlyer's CapMoving, logging it every tick for 5 s -- its place,
// velocity, acceleration, rotation, physics and state, the water flags of
// its zone and feet, its base, bCanJump and MoveTimer:
//   a. flying from 150 out over the water, its feet 10 under the ledge's
//      top, to 200 back over the pier, 20 over its floor: into the pier's
//      wall, level, which it steps up;
//   b. from 150 out, its feet 80 under the top, to 100 back and 100 over
//      the floor: into the wall rising, which it slides up;
//   c. MoveToward the player from 300 out, 150 over the top: the aim 0.7
//      of the player's height over its middle;
//   d. walking from 60 back on the pier to 300 out, bCanJump on: off the
//      ledge, where the original's Falling has a bird take off;
//   e. let fall from 100 over the water, waiting for its landing
//      (WaitForLanding): LongFall after 2.5 s of falling;
//   f. a stock Seagull in its own Flying state, on the pier, for 15 s: a
//      random flight, only to be plausible.
// The flyer's movement properties are logged first, with bCanGlide and
// bCanStrafe, which pick moveToward's glider path. Then an exit.
//=============================================================================
class FlightConsole extends Console;

var float CapTime, MapTime, StepTime;
var string CurrentMap;
var int Phase, Scenario, Ticks;
var vector Edge, EdgeDir, Over;
var float FloorZ, LedgeZ, WaterZ;
var Pawn Flyer;
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

// The top under a spot, from 64 over the floor: 0 when nothing stands
// within 60 under the floor, the water's edge passed (RestConsole's).
function float TopAt(PlayerPawn P, vector Spot)
{
	local vector HitLocation, HitNormal, From;

	From = Spot;
	From.Z = FloorZ + 64;
	if (P.Trace(HitLocation, HitNormal, From - vect(0,0,124), From, false) == None)
		return 0;
	return HitLocation.Z;
}

// RestConsole's edge: from a spot on the pier south of the start, the last
// top before the water, halved to a quarter unit.
function bool PickEdge(PlayerPawn P)
{
	local vector HitLocation, HitNormal, Spot;
	local float D, Lo, Hi, Mid;

	Spot = vect(-4760.569824, 10046.811523, -256.200012);
	EdgeDir = vect(0, -1, 0);
	if (P.Trace(HitLocation, HitNormal, Spot - vect(0,0,200), Spot, false) == None)
		return false;
	FloorZ = HitLocation.Z;
	Lo = 0;
	for (D = 4; D < 200; D += 4)
	{
		if (TopAt(P, Spot + D * EdgeDir) == 0)
		{
			Hi = D;
			break;
		}
		Lo = D;
	}
	if (D >= 200)
		return false;
	while (Hi - Lo > 0.25)
	{
		Mid = (Lo + Hi) / 2;
		if (TopAt(P, Spot + Mid * EdgeDir) != 0)
			Lo = Mid;
		else
			Hi = Mid;
	}
	Edge = Spot + Lo * EdgeDir;
	LedgeZ = TopAt(P, Edge - EdgeDir);
	Edge.Z = LedgeZ;
	return true;
}

// SwimConsole's water line over the spot 80 past the edge: a probe moved
// there, 50 over the ledge (dry) and 400 under it (water), the gap halved
// to a quarter unit by the zone it is in.
function bool FindWater(PlayerPawn P)
{
	local Effects Probe;
	local vector Spot;
	local float Lo, Hi, Mid;

	Over = Edge + 80 * EdgeDir;
	Spot = Over;
	Spot.Z = LedgeZ + 50;
	Probe = P.Spawn(class'Effects',,, Spot);
	if (Probe == None)
		return false;
	Probe.SetCollision(false, false, false);
	Probe.bHidden = true;
	Hi = Spot.Z;
	Lo = LedgeZ - 400;
	Spot.Z = Lo;
	Probe.SetLocation(Spot);
	if (!Probe.Region.Zone.bWaterZone)
	{
		Log("DXFLIGHT: no water 400 under the ledge at " $ Spot);
		Probe.Destroy();
		return false;
	}
	Spot.Z = Hi;
	Probe.SetLocation(Spot);
	if (Probe.Region.Zone.bWaterZone)
	{
		Log("DXFLIGHT: water 50 over the ledge at " $ Spot);
		Probe.Destroy();
		return false;
	}
	while (Hi - Lo > 0.25)
	{
		Mid = (Lo + Hi) / 2;
		Spot.Z = Mid;
		Probe.SetLocation(Spot);
		if (Probe.Region.Zone.bWaterZone)
			Lo = Mid;
		else
			Hi = Mid;
	}
	Probe.Destroy();
	WaterZ = (Lo + Hi) / 2;
	Over.Z = WaterZ;
	Log("DXFLIGHT: the edge at " $ Edge $ " along " $ EdgeDir $ ", the pier's floor at z " $ FloorZ $ ", the ledge's top at z " $ LedgeZ
	    $ ", the water line 80 past it at z " $ WaterZ);
	return true;
}

function LogPawn(Pawn A)
{
	Log("DXFLIGHT: " $ A.Name $ " radius " $ A.CollisionRadius $ " height " $ A.CollisionHeight $ " mass " $ A.Mass $ " buoyancy " $ A.Buoyancy
	    $ " accel rate " $ A.AccelRate $ " air speed " $ A.AirSpeed $ " ground speed " $ A.GroundSpeed $ " water speed " $ A.WaterSpeed
	    $ " jump z " $ A.JumpZ $ " step " $ A.MaxStepHeight $ " min hit wall " $ A.MinHitWall $ " can fly " $ A.bCanFly
	    $ " can glide " $ A.bCanGlide $ " can strafe " $ A.bCanStrafe $ " can swim " $ A.bCanSwim $ " can jump " $ A.bCanJump
	    $ " rotation rate " $ A.RotationRate $ " max desired speed " $ A.MaxDesiredSpeed $ " physics " $ int(A.Physics));
}

function LogTick(Pawn S)
{
	Log("DXFLIGHT: " $ ScenarioName $ " tick " $ Ticks $ " t " $ StepTime $ " loc " $ S.Location $ " vel " $ S.Velocity $ " accel " $ S.Acceleration
	    $ " rot " $ S.Rotation $ " physics " $ int(S.Physics) $ " state " $ S.GetStateName() $ " water " $ S.Region.Zone.bWaterZone
	    $ " foot " $ S.FootRegion.Zone.bWaterZone $ " base " $ S.Base $ " jump " $ S.bCanJump $ " timer " $ S.MoveTimer);
}

// The scenario's flyer, fresh, at Start facing Dir, in its own state,
// still, flying or falling.
function CapFlyer SpawnFlyer(PlayerPawn P, vector Start, vector Dir, bool bFlying)
{
	local CapFlyer F;

	if (Flyer != None)
		Flyer.Destroy();
	F = P.Spawn(class'CapFlyer',,, Start, rotator(Dir));
	Flyer = F;
	if (F == None)
	{
		Log("DXFLIGHT: " $ ScenarioName $ ": no room for the flyer at " $ Start);
		return None;
	}
	F.InitializePawn();
	F.bInvincible = true;
	F.GotoState('CapMoving');
	F.Velocity = vect(0,0,0);
	F.Acceleration = vect(0,0,0);
	if (bFlying)
		F.SetPhysics(PHYS_Flying);
	else
		F.SetPhysics(PHYS_Falling);
	return F;
}

// One scenario's flyer, set up: false where it cannot be.
function bool SetUp(PlayerPawn P)
{
	local vector Start, Dest;
	local CapFlyer F;
	local Seagull G;

	ScenarioTime = 5.0;
	if (Scenario == 0)
	{
		Start = Edge + 150 * EdgeDir;
		Start.Z = LedgeZ - 10 + class'CapFlyer'.Default.CollisionHeight;
		F = SpawnFlyer(P, Start, -EdgeDir, true);
		if (F == None)
			return false;
		LogPawn(F);
		Dest = Edge - 200 * EdgeDir;
		Dest.Z = LedgeZ + 20 + F.CollisionHeight;
		F.CapDest = Dest;
	}
	else if (Scenario == 1)
	{
		Start = Edge + 150 * EdgeDir;
		Start.Z = LedgeZ - 80 + class'CapFlyer'.Default.CollisionHeight;
		F = SpawnFlyer(P, Start, -EdgeDir, true);
		if (F == None)
			return false;
		Dest = Edge - 100 * EdgeDir;
		Dest.Z = LedgeZ + 100 + F.CollisionHeight;
		F.CapDest = Dest;
	}
	else if (Scenario == 2)
	{
		Start = Edge + 300 * EdgeDir;
		Start.Z = LedgeZ + 150;
		F = SpawnFlyer(P, Start, -EdgeDir, true);
		if (F == None)
			return false;
		F.CapTarget = P;
	}
	else if (Scenario == 3)
	{
		Start = Edge - 60 * EdgeDir;
		Start.Z = LedgeZ + class'CapFlyer'.Default.CollisionHeight + 2;
		F = SpawnFlyer(P, Start, EdgeDir, false);
		if (F == None)
			return false;
		Dest = Edge + 300 * EdgeDir;
		Dest.Z = Start.Z;
		F.CapDest = Dest;
	}
	else if (Scenario == 4)
	{
		ScenarioTime = 6.0;
		F = SpawnFlyer(P, Over + vect(0,0,100), EdgeDir, true);
		if (F == None)
			return false;
	}
	else
	{
		ScenarioTime = 15.0;
		if (Flyer != None)
			Flyer.Destroy();
		Start = Edge - 100 * EdgeDir;
		Start.Z = LedgeZ + class'Seagull'.Default.CollisionHeight + 2;
		G = P.Spawn(class'Seagull',,, Start, rotator(EdgeDir));
		Flyer = G;
		if (G == None)
		{
			Log("DXFLIGHT: f: no room for the gull at " $ Start);
			return false;
		}
		G.InitializePawn();
		G.bInvincible = true;
		G.SetPhysics(PHYS_Falling);
		LogPawn(G);
	}
	return true;
}

// Scenario's set-up: false once there are no more.
function bool StartScenario(PlayerPawn P)
{
	while (Scenario < 6)
	{
		ScenarioName = Chr(97 + Scenario);
		if (SetUp(P))
			return true;
		Scenario++;
	}
	return false;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local CapFlyer F;
	local vector Stand;
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
			if (!PickEdge(P) || !FindWater(P))
			{
				Log("DXFLIGHT: no edge over deep water, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Stand = Edge - 150 * EdgeDir;
			Stand.Z = LedgeZ + P.CollisionHeight + 2;
			if (!P.SetLocation(Stand))
				Log("DXFLIGHT: cannot put the player at " $ Stand);
			P.SetRotation(rotator(EdgeDir));
			P.ViewRotation = rotator(EdgeDir);
			P.Velocity = vect(0,0,0);
			P.Acceleration = vect(0,0,0);
			P.SetPhysics(PHYS_Falling);
			Scenario = 0;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (!StartScenario(P))
		{
			Log("DXFLIGHT: done at " $ CapTime $ ", exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
			return;
		}
		Ticks = 0;
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3)
	{
		// Held a second, then the move
		if (Flyer == None)
		{
			Scenario++;
			Phase = 2;
			return;
		}
		if (StepTime > 1.0)
		{
			Log("DXFLIGHT: " $ ScenarioName $ ": the flyer at " $ Flyer.Location $ " physics " $ int(Flyer.Physics) $ " state " $ Flyer.GetStateName());
			F = CapFlyer(Flyer);
			if (Scenario == 4)
				F.GotoState('CapMoving', 'Drop');
			else if (Scenario == 5)
				Flyer.GotoState('Flying');
			else
			{
				F.GotoState('CapMoving', 'Go');
				// Walking, able to jump
				if (Scenario == 3)
				{
					F.SetPhysics(PHYS_Walking);
					F.bCanJump = true;
				}
			}
			Ticks = 0;
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		if (Flyer != None)
		{
			Ticks++;
			LogTick(Flyer);
		}
		if (StepTime > ScenarioTime || Flyer == None)
		{
			Scenario++;
			StepTime = 0;
			Phase = 2;
		}
	}
}
