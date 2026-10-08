//=============================================================================
// SwimConsole: falling into water, swimming and leaving it -- logged with
// "DXSWIM:" and the console's clock (vibe/docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island, datalinks and conversations silenced every tick, at the
// pier's edge RestConsole finds; the water line 80 past it found by moving a
// probe up and down and halving to a quarter unit. The water's zone and the
// player's and a UNATCOTroop's movement properties are logged, then seven
// scenarios, each from the player stood a second where the map starts it,
// each logged every tick for 4 s -- its height, how far past the edge, its
// velocity, physics, the water flags of its zone, head and feet, its
// acceleration and its state:
//   a. the player off the edge, its feet 2 over the ledge, 1.5 of its radius
//      past it (clear of the ledge, where the landing would push it at
//      random);
//   b. the player 80 past the edge, its feet 200 over the ledge;
//   c. the player 80 past the edge, its middle 8 over the water line, falling
//      at 100;
//   d. the player 80 past the edge, its middle 100 under the water line,
//      facing out and looking 45 degrees down, the forward axis held;
//   e. the same, looking level, the up axis held;
//   f. a standing UNATCOTroop 80 past the edge, its middle 100 over the
//      water line;
//   g. the same troop thrown in from the pier, 40 short of the edge, at 300
//      out and 200 up.
// The axes are set every tick to 120000, 20 times a held key's 6000
// (StandConsole), so either engine's input cuts the acceleration to the
// player's AccelRate; then an exit.
//=============================================================================
class SwimConsole extends Console;

var float CapTime, MapTime, StepTime;
var string CurrentMap;
var int Phase, Scenario, Ticks;
var vector PlayerStand, Edge, EdgeDir, Over;
var rotator PlayerRot;
var float FloorZ, LedgeZ, WaterZ;
var ScriptedPawn Troop;
var Actor Subject;
var string ScenarioName;

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

// The water line over the spot 80 past the edge: a probe moved there, 50
// over the ledge (dry) and 400 under it (water), the gap halved to a
// quarter unit by the zone it is in.
function bool FindWater(PlayerPawn P)
{
	local Effects Probe;
	local vector Spot;
	local float Lo, Hi, Mid;
	local ZoneInfo Z;

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
		Log("DXSWIM: no water 400 under the ledge at " $ Spot);
		Probe.Destroy();
		return false;
	}
	Z = Probe.Region.Zone;
	Spot.Z = Hi;
	Probe.SetLocation(Spot);
	if (Probe.Region.Zone.bWaterZone)
	{
		Log("DXSWIM: water 50 over the ledge at " $ Spot);
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
	Log("DXSWIM: the edge at " $ Edge $ " along " $ EdgeDir $ ", the pier's floor at z " $ FloorZ $ ", the ledge's top at z " $ LedgeZ
	    $ ", the water line 80 past it at z " $ WaterZ);
	Log("DXSWIM: the water's zone " $ Z.Name $ " (" $ Z.Class.Name $ ") gravity " $ Z.ZoneGravity $ " velocity " $ Z.ZoneVelocity
	    $ " fluid friction " $ Z.ZoneFluidFriction $ " terminal velocity " $ Z.ZoneTerminalVelocity $ " ground friction " $ Z.ZoneGroundFriction);
	return true;
}

function LogPawn(Pawn A)
{
	Log("DXSWIM: " $ A.Name $ " radius " $ A.CollisionRadius $ " height " $ A.CollisionHeight $ " mass " $ A.Mass $ " buoyancy " $ A.Buoyancy
	    $ " accel rate " $ A.AccelRate $ " water speed " $ A.WaterSpeed $ " ground speed " $ A.GroundSpeed $ " air control " $ A.AirControl
	    $ " step " $ A.MaxStepHeight $ " min hit wall " $ A.MinHitWall $ " eye " $ A.BaseEyeHeight $ " can swim " $ A.bCanSwim
	    $ " desired speed " $ A.DesiredSpeed);
}

function LogSubject()
{
	local Pawn S;

	S = Pawn(Subject);
	if (S == None)
		return;
	Log("DXSWIM: " $ ScenarioName $ " tick " $ Ticks $ " t " $ StepTime $ " z " $ S.Location.Z $ " out " $ ((S.Location - Edge) dot EdgeDir)
	    $ " velocity " $ S.Velocity $ " physics " $ int(S.Physics) $ " water " $ S.Region.Zone.bWaterZone $ " head " $ S.HeadRegion.Zone.bWaterZone
	    $ " foot " $ S.FootRegion.Zone.bWaterZone $ " accel " $ S.Acceleration $ " state " $ S.GetStateName());
}

// The player stood where the map started it, still.
function StandPlayer(PlayerPawn P)
{
	P.aBaseY = 0;
	P.aUp = 0;
	P.SetLocation(PlayerStand);
	P.SetRotation(PlayerRot);
	P.ViewRotation = PlayerRot;
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.SetPhysics(PHYS_Falling);
}

// Put there, facing out, still but for Velocity; falling unless already
// swimming, as a move into water makes the scripts.
function Place(Pawn A, vector Where, int Pitch, vector Velocity)
{
	local rotator R;

	R = rotator(EdgeDir);
	R.Pitch = Pitch;
	if (!A.SetLocation(Where))
		Log("DXSWIM: " $ ScenarioName $ ": " $ A.Name $ " cannot be put at " $ Where);
	A.SetRotation(R);
	if (PlayerPawn(A) != None)
		PlayerPawn(A).ViewRotation = R;
	A.Velocity = Velocity;
	A.Acceleration = vect(0,0,0);
	if (A.Physics != PHYS_Swimming)
		A.SetPhysics(PHYS_Falling);
	Subject = A;
	Log("DXSWIM: " $ ScenarioName $ ": " $ A.Name $ " at " $ A.Location $ " physics " $ int(A.Physics) $ " state " $ A.GetStateName());
}

// Scenario's set-up: false once there are no more.
function bool StartScenario(PlayerPawn P)
{
	local vector Where;

	switch (Scenario)
	{
		case 0:
			ScenarioName = "a";
			Place(P, Edge + vect(0,0,1) * (P.CollisionHeight + 2) + EdgeDir * 1.5 * P.CollisionRadius, 0, vect(0,0,0));
			break;
		case 1:
			ScenarioName = "b";
			Where = Over;
			Where.Z = LedgeZ + 200 + P.CollisionHeight;
			Place(P, Where, 0, vect(0,0,0));
			break;
		case 2:
			ScenarioName = "c";
			Place(P, Over + vect(0,0,8), 0, vect(0,0,-100));
			break;
		case 3:
			ScenarioName = "d";
			Place(P, Over - vect(0,0,100), -8192, vect(0,0,0));
			break;
		case 4:
			ScenarioName = "e";
			Place(P, Over - vect(0,0,100), 0, vect(0,0,0));
			break;
		case 5:
			ScenarioName = "f";
			Troop = P.Spawn(class'UNATCOTroop',,, Over + vect(0,0,100), rotator(EdgeDir));
			if (Troop == None)
			{
				Log("DXSWIM: f: no room for the troop at " $ (Over + vect(0,0,100)));
				return false;
			}
			Troop.SetOrders('Standing', '', True);
			LogPawn(Troop);
			Place(Troop, Over + vect(0,0,100), 0, vect(0,0,0));
			break;
		case 6:
			ScenarioName = "g";
			if (Troop == None)
				return false;
			Where = Edge - 40 * EdgeDir;
			Where.Z = LedgeZ + Troop.CollisionHeight + 2;
			Place(Troop, Where, 0, EdgeDir * 300 + vect(0,0,200));
			break;
		default:
			return false;
	}
	return true;
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
			if (!PickEdge(P) || !FindWater(P))
			{
				Log("DXSWIM: no edge over deep water, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			LogPawn(P);
			Scenario = 0;
			StandPlayer(P);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Stood a second, then the scenario
		if (StepTime > 1.0)
		{
			if (!StartScenario(P))
			{
				Log("DXSWIM: done at " $ CapTime $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Ticks = 0;
			StepTime = 0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (Scenario == 3)
			P.aBaseY = 120000.0;
		else if (Scenario == 4)
			P.aUp = 120000.0;
		Ticks++;
		LogSubject();
		if (StepTime > 4.0)
		{
			Scenario++;
			StandPlayer(P);
			StepTime = 0;
			Phase = 2;
		}
	}
}
