//=============================================================================
// WadeConsole: a pawn walking into water -- logged with "DXWADE:" and the
// console's clock (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, datalinks and conversations silenced every tick, at
// the ledge over water nearest the player's start, found from the level's
// navigation points (FindLedge). Each scenario spawns a fresh CapWader (a
// UNATCOTroop, invulnerable), stands it a second, then walks it with MoveTo
// (CapWader's CapMoving), logging it every tick for 6 s -- its place,
// velocity, acceleration, rotation, physics and state, the water flags of
// its zone and feet, its base, bCanJump and MoveTimer:
//   a. from 60 back from the edge to 48 past it, 30 under the water line:
//      off the ledge into the water, where ScriptedPawn's FallingState aims
//      the jump it gives a pawn walking off a ledge;
//   b. the same with bCanSwim off, which ScriptedPawn's ZoneChange answers
//      by giving up the move;
//   c. down a shore into water, from 64 short of the water's edge on a
//      walkable floor toward 96 past where the floor is 70 under the water
//      line, the move reached only in the water -- the shore nearest the
//      player's start found from the level's navigation points (FindShore),
//      left out where the map has none -- of the stock maps, all but
//      14_OceanLab_Lab;
//   d. a's walk with bCanJump off, as the stock standing and wandering
//      states have it;
//   e. from 60 back to 300 past the edge, at the ledge's height: the jump
//      aimed across the water.
// The ledge's blockers (BlockMonsters) are made not to collide. Then an
// exit. TargetMap ([DXCapture.WadeConsole], DXCAP_MISSION_MAP)
// names another map than Liberty Island, for its shore alone (c).
//=============================================================================
class WadeConsole extends Console;

var config string TargetMap;

var float CapTime, MapTime, StepTime;
var string CurrentMap;
var int Phase, Scenario, Ticks;
var vector Edge, EdgeDir;
var float WaterZ;
var bool bLedge, bShore;
var Effects Probe;
// The actors that block a troop near the start, and the ledges found,
// nearest first.
var Actor Blocker[128];
var int NumBlockers;
var vector CandEdge[32], CandDir[32];
var float CandDist[32];
var int NumCands;
var vector ShoreDry, ShoreWet, ShoreDir;
var CapWader Wader;
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

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None)
		DXP.conPlay.TerminateConversation();
}

// Whether a spot is in a water zone: the probe put there.
function bool WaterAt(vector Spot)
{
	Probe.SetLocation(Spot);
	return Probe.Region.Zone.bWaterZone;
}

// The floor under a spot, within Down under it: false with none, or one
// steeper than a walkable 0.7.
function bool FloorAt(PlayerPawn P, vector Spot, float Down, out vector Floor, out vector Normal)
{
	if (P.Trace(Floor, Normal, Spot - vect(0,0,1) * Down, Spot, false) == None)
		return false;
	return Normal.Z >= 0.7;
}

// No wall between two floors for the troop: a box of its radius, 26 to 42
// over the first (over a step), level to over the second.
function bool Clear(PlayerPawn P, vector From, vector To)
{
	local vector HitLoc, HitNorm, Extent;

	Extent = vect(20,20,8);
	To.Z = From.Z + 34;
	From.Z += 34;
	return P.Trace(HitLoc, HitNorm, To, From, false, Extent) == None;
}

// Every actor within 3,500 of the start that blocks a troop: anything
// colliding and blocking actors but a pawn or a brush.
function FindBlockers(PlayerPawn P)
{
	local Actor A;

	foreach P.AllActors(class'Actor', A)
	{
		if (NumBlockers < ArrayCount(Blocker) && A.bCollideActors && A.bBlockActors && Pawn(A) == None && A.Brush == None
		    && VSize(A.Location - P.Location) < 3500)
			Blocker[NumBlockers++] = A;
	}
}

// A ledge found, kept among the 32 nearest.
function AddCandidate(vector Where, vector Dir, float Dist)
{
	local int i, j;

	for (i = 0; i < NumCands; i++)
	{
		if (Dist < CandDist[i])
			break;
	}
	if (i >= ArrayCount(CandDist))
		return;
	if (NumCands < ArrayCount(CandDist))
		NumCands++;
	for (j = NumCands - 1; j > i; j--)
	{
		CandEdge[j] = CandEdge[j - 1];
		CandDir[j] = CandDir[j - 1];
		CandDist[j] = CandDist[j - 1];
	}
	CandEdge[i] = Where;
	CandDir[i] = Dir;
	CandDist[i] = Dist;
}

// The blocker standing in a troop's way from 80 short of an edge to 200
// past it, if any.
function Actor BlockerOn(vector Where, vector Dir)
{
	local vector Spot, Off;
	local float D;
	local int i;

	for (D = -80; D <= 200; D += 8)
	{
		Spot = Where + Dir * D;
		Spot.Z += 47;
		for (i = 0; i < NumBlockers; i++)
		{
			if (Blocker[i] == None)
				continue;
			Off = Blocker[i].Location - Spot;
			if (Abs(Off.Z) < Blocker[i].CollisionHeight + 43 && Off.X * Off.X + Off.Y * Off.Y < Square(Blocker[i].CollisionRadius + 20))
				return Blocker[i];
		}
	}
	return None;
}

// The nearest ledge that no blocker stands at; failing that, the nearest,
// its blockers' collision turned off.
function bool PickCandidate(PlayerPawn P)
{
	local Actor A;
	local int i;

	if (NumCands == 0)
		return false;
	for (i = 0; i < NumCands; i++)
	{
		if (BlockerOn(CandEdge[i], CandDir[i]) == None)
		{
			Edge = CandEdge[i];
			EdgeDir = CandDir[i];
			Log("DXWADE: " $ NumCands $ " ledges found, " $ NumBlockers $ " blockers near; the " $ (i + 1) $ "th nearest is clear");
			return true;
		}
	}
	Edge = CandEdge[0];
	EdgeDir = CandDir[0];
	A = BlockerOn(Edge, EdgeDir);
	while (A != None)
	{
		Log("DXWADE: " $ NumCands $ " ledges found, none clear: " $ A $ " (" $ A.Class.Name $ ") at " $ A.Location $ " radius " $ A.CollisionRadius
		    $ " height " $ A.CollisionHeight $ " made not to collide");
		A.SetCollision(false, false, false);
		for (i = 0; i < NumBlockers; i++)
		{
			if (Blocker[i] == A)
				Blocker[i] = None;
		}
		A = BlockerOn(Edge, EdgeDir);
	}
	return true;
}

// The ledge over water nearest the player's start that no actor blocks a
// troop at (PickCandidate): from each navigation point on dry ground within
// 3,000 of it, out along the four axes in steps of 16, each floor walkable,
// within 20 up or down of the last and clear of walls, to a step with no
// floor within 40 under the last, 64 or more out, and water within 300 under
// it. The edge found to half a unit, the water line under the step just
// past it to a quarter.
function bool FindLedge(PlayerPawn P)
{
	local NavigationPoint N;
	local vector Dir, Floor, Normal, First, Last, Spot;
	local rotator R;
	local float Best, Lo, Hi, Mid, H;
	local int d, s;

	FindBlockers(P);
	foreach P.AllActors(class'NavigationPoint', N)
	{
		if (N.Region.Zone.bWaterZone || VSize(N.Location - P.Location) > 3000)
			continue;
		if (!FloorAt(P, N.Location, 200, First, Normal))
			continue;
		for (d = 0; d < 4; d++)
		{
			R.Yaw = d * 16384;
			Dir = vector(R);
			Last = First;
			for (s = 1; s <= 40; s++)
			{
				Spot = Last + Dir * 16;
				Spot.Z = Last.Z + 40;
				if (!Clear(P, Last, Spot))
					break;
				if (!FloorAt(P, Spot, 80, Floor, Normal))
				{
					Spot.Z = Last.Z;
					for (H = 20; H <= 300; H += 20)
					{
						if (WaterAt(Spot - vect(0,0,1) * H))
							break;
					}
					if (s > 4 && H <= 300)
						AddCandidate(Last, Dir, VSize(Last - P.Location));
					break;
				}
				if (Abs(Floor.Z - Last.Z) > 20)
					break;
				Last = Floor;
			}
		}
	}
	if (!PickCandidate(P))
		return false;
	Best = VSize(Edge - P.Location);
	Lo = 0;
	Hi = 16;
	while (Hi - Lo > 0.5)
	{
		Mid = (Lo + Hi) / 2;
		Spot = Edge + EdgeDir * Mid;
		Spot.Z = Edge.Z + 40;
		if (FloorAt(P, Spot, 80, Floor, Normal))
			Lo = Mid;
		else
			Hi = Mid;
	}
	Edge += EdgeDir * Lo;
	Spot = Edge + EdgeDir * 40;
	Lo = Edge.Z - 320;
	Hi = Edge.Z;
	while (Hi - Lo > 0.25)
	{
		Mid = (Lo + Hi) / 2;
		Spot.Z = Mid;
		if (WaterAt(Spot))
			Lo = Mid;
		else
			Hi = Mid;
	}
	WaterZ = (Lo + Hi) / 2;
	Log("DXWADE: the ledge at " $ Edge $ " along " $ EdgeDir $ ", " $ Best $ " from the start; the water line 40 past it at z " $ WaterZ);
	return true;
}

// A shore: from a navigation point on dry ground, out along one of 8
// directions in steps of 16, each floor walkable, within 20 up or down of
// the last and clear of walls, to a floor 70 under the water's surface (a
// troop's height and more); the dry spot 64 short of where the floor first
// meets the water, the wet one the last. The one nearest the player's start.
function bool FindShore(PlayerPawn P)
{
	local NavigationPoint N;
	local vector Dir, Floor, Normal, First, Last, Spot, FirstWet, Dry;
	local rotator R;
	local int d, s;
	local bool bWet;
	local float Best;

	Best = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		if (N.Region.Zone.bWaterZone)
			continue;
		if (!FloorAt(P, N.Location, 200, First, Normal))
			continue;
		for (d = 0; d < 8; d++)
		{
			R.Yaw = d * 8192;
			Dir = vector(R);
			Last = First;
			bWet = false;
			for (s = 1; s <= 40; s++)
			{
				Spot = Last + Dir * 16;
				Spot.Z = Last.Z + 40;
				if (!Clear(P, Last, Spot) || !FloorAt(P, Spot, 80, Floor, Normal) || Abs(Floor.Z - Last.Z) > 20)
					break;
				Last = Floor;
				if (!bWet)
				{
					if (WaterAt(Floor + vect(0,0,4)))
					{
						bWet = true;
						FirstWet = Floor;
					}
				}
				else if (WaterAt(Floor + vect(0,0,70)))
				{
					Spot = FirstWet - Dir * 64;
					Spot.Z = FirstWet.Z + 60;
					if (FloorAt(P, Spot, 120, Dry, Normal) && !WaterAt(Dry + vect(0,0,4)) && VSize(Dry - P.Location) < Best)
					{
						Best = VSize(Dry - P.Location);
						ShoreDir = Dir;
						ShoreWet = Floor;
						ShoreDry = Dry;
						Log("DXWADE: a shore from " $ N.Name $ " at " $ N.Location $ " along " $ Dir $ ": dry at " $ ShoreDry $ ", the water's edge near "
						    $ FirstWet $ ", 70 under water at " $ ShoreWet $ " (" $ (16 * s) $ " out), " $ Best $ " from the start");
					}
					break;
				}
			}
		}
	}
	return Best < 1000000;
}

function LogPawn(Pawn A)
{
	Log("DXWADE: " $ A.Name $ " radius " $ A.CollisionRadius $ " height " $ A.CollisionHeight $ " mass " $ A.Mass $ " buoyancy " $ A.Buoyancy
	    $ " accel rate " $ A.AccelRate $ " ground speed " $ A.GroundSpeed $ " water speed " $ A.WaterSpeed $ " jump z " $ A.JumpZ
	    $ " step " $ A.MaxStepHeight $ " min hit wall " $ A.MinHitWall $ " can jump " $ A.bCanJump $ " can swim " $ A.bCanSwim
	    $ " can fly " $ A.bCanFly $ " rotation rate " $ A.RotationRate $ " max desired speed " $ A.MaxDesiredSpeed);
}

function LogTick(Pawn S)
{
	Log("DXWADE: " $ ScenarioName $ " tick " $ Ticks $ " t " $ StepTime $ " loc " $ S.Location $ " vel " $ S.Velocity $ " accel " $ S.Acceleration
	    $ " rot " $ S.Rotation $ " physics " $ int(S.Physics) $ " state " $ S.GetStateName() $ " water " $ S.Region.Zone.bWaterZone
	    $ " foot " $ S.FootRegion.Zone.bWaterZone $ " base " $ S.Base $ " jump " $ S.bCanJump $ " timer " $ S.MoveTimer);
}

// The scenario's troop, fresh, at Start facing Dir, falling to its feet.
function bool SpawnWader(PlayerPawn P, vector Start, vector Dir)
{
	if (Wader != None)
		Wader.Destroy();
	Wader = P.Spawn(class'CapWader',,, Start, rotator(Dir));
	if (Wader == None)
	{
		Log("DXWADE: " $ ScenarioName $ ": no room for the troop at " $ Start);
		return false;
	}
	Wader.InitializePawn();
	Wader.bInvincible = true;
	Wader.GotoState('CapMoving');
	Wader.Velocity = vect(0,0,0);
	Wader.Acceleration = vect(0,0,0);
	Wader.SetPhysics(PHYS_Falling);
	return true;
}

// Scenario's set-up: false once there are no more.
function bool StartScenario(PlayerPawn P)
{
	local vector Start;

	while (Scenario < 5)
	{
		ScenarioName = Chr(97 + Scenario);
		if (Scenario == 2)
		{
			if (bShore && SpawnWader(P, ShoreDry + vect(0,0,52), ShoreDir))
			{
				Wader.CapDest = ShoreWet + ShoreDir * 96;
				return true;
			}
		}
		else if (bLedge)
		{
			Start = Edge - 60 * EdgeDir;
			Start.Z = Edge.Z + 52;
			if (SpawnWader(P, Start, EdgeDir))
			{
				if (Scenario == 4)
					Wader.CapDest = Edge + 300 * EdgeDir;
				else
				{
					Wader.CapDest = Edge + 48 * EdgeDir;
					Wader.CapDest.Z = WaterZ - 30;
				}
				if (Scenario == 1)
					Wader.bCanSwim = false;
				if (Scenario == 0)
					LogPawn(Wader);
				return true;
			}
		}
		Scenario++;
	}
	return false;
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
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= Target() && MapTime > 6.0)
		{
			Probe = P.Spawn(class'Effects',,, P.Location);
			Probe.SetCollision(false, false, false);
			Probe.bHidden = true;
			if (M ~= "01_NYC_UNATCOIsland")
			{
				bLedge = FindLedge(P);
				if (!bLedge)
					Log("DXWADE: no ledge over water found");
			}
			bShore = FindShore(P);
			if (!bShore)
				Log("DXWADE: no shore found");
			Probe.Destroy();
			Scenario = 0;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		if (!StartScenario(P))
		{
			Log("DXWADE: done at " $ CapTime $ ", exiting");
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
		// Stood a second, then the walk
		if (Wader == None)
		{
			Scenario++;
			Phase = 2;
			return;
		}
		if (StepTime > 1.0)
		{
			Log("DXWADE: " $ ScenarioName $ ": the troop at " $ Wader.Location $ " physics " $ int(Wader.Physics) $ " walks to " $ Wader.CapDest);
			Wader.GotoState('CapMoving', 'Go');
			// After leaving its start-up state's, which sets it back
			if (Scenario == 3)
				Wader.bCanJump = false;
			Ticks = 0;
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		if (Wader != None)
		{
			Ticks++;
			LogTick(Wader);
		}
		if (StepTime > 6.0 || Wader == None)
		{
			Scenario++;
			StepTime = 0;
			Phase = 2;
		}
	}
}

defaultproperties
{
	TargetMap="01_NYC_UNATCOIsland"
}
