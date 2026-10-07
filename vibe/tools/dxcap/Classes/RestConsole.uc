//=============================================================================
// RestConsole: where a falling decoration comes to rest (vibe/docs/
// DEVELOPMENT.md, scripted runs). At Liberty Island's start, the large crate,
// the barrel and the large box placed on the pier as MeshConsole places them;
// first walkMove's drop traced down from where the player stands (its box and
// three others), then each one's collision and the colliding actors near it
// logged, then every tick for 1.5 s its place, the bottom of its cylinder,
// physics, velocity and base.
//
// Then a landing on an edge: the pier's edge over the water south of the
// player's start, where a lip a few units high runs along it, as a scan of
// the original's traces found it; its outer face found by stepping traces
// down and halving to a quarter unit. A small crate dropped there 0.4 of its
// radius past the edge, then the player 0.95 of its own, each 2 over the
// lip; for 2 s each, every tick, its place, physics, velocity and base (and
// the crate's landings). The original's landing nudges the crate off the
// ledge and fits the player clear of it, pushing it on at random
// (dx-reverse-info engine-dll.md, moving); then exit. Each line starts
// "DXREST:".
//=============================================================================
class RestConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var Actor Placed[3];
var int Ticks;
var vector Edge, EdgeDir;
var float FloorZ, LedgeZ;
var Actor Dropped;

function Actor Place(PlayerPawn P, class<Actor> C, vector Where)
{
	local Actor A;
	A = P.Spawn(C,,, Where, P.Rotation);
	if (A == None)
		Log("DXREST: no room for " $ C $ " at " $ Where);
	return A;
}

// Traces straight down from a spot, as the level answers them: a line, a
// small box, and a box the decoration's own size.
function TraceDown(PlayerPawn P, string Label, vector From, float Radius, float Height)
{
	local vector HitLocation, HitNormal, Extent;
	local Actor Hit;

	Hit = P.Trace(HitLocation, HitNormal, From - vect(0,0,200), From, false);
	Log("DXREST: trace " $ Label $ " line hits " $ Hit $ " at z " $ HitLocation.Z $ " normal " $ HitNormal);
	Extent = vect(5,5,5);
	Hit = P.Trace(HitLocation, HitNormal, From - vect(0,0,200), From, false, Extent);
	Log("DXREST: trace " $ Label $ " box 5 hits " $ Hit $ " at z " $ HitLocation.Z $ " bottom " $ (HitLocation.Z - 5) $ " normal " $ HitNormal);
	Extent.X = Radius;
	Extent.Y = Radius;
	Extent.Z = Height;
	Hit = P.Trace(HitLocation, HitNormal, From - vect(0,0,200), From, false, Extent);
	Log("DXREST: trace " $ Label $ " box own hits " $ Hit $ " at z " $ HitLocation.Z $ " bottom " $ (HitLocation.Z - Height) $ " normal " $ HitNormal);
}

// walkMove's drop to the floor as a script trace: straight down
// MaxStepHeight + 2 from where a pawn stands, with a box its size.
function DropProbe(PlayerPawn P, string Label, vector From, float Radius, float Height)
{
	local vector HitLocation, HitNormal, Extent;
	local Actor Hit;

	Extent.X = Radius;
	Extent.Y = Radius;
	Extent.Z = Height;
	Hit = P.Trace(HitLocation, HitNormal, From - vect(0,0,26), From, false, Extent);
	if (Hit == None)
		Log("DXREST: drop " $ Label $ " from z " $ From.Z $ " hits nothing");
	else
		Log("DXREST: drop " $ Label $ " from z " $ From.Z $ " hits " $ Hit $ " at z " $ HitLocation.Z $ " moved " $ (From.Z - HitLocation.Z) $ " normal " $ HitNormal);
}

// The top under a spot, from 64 over the floor: 0 when nothing stands
// within 60 under the floor, the water's edge passed.
function float TopAt(PlayerPawn P, vector Spot)
{
	local vector HitLocation, HitNormal, From;

	From = Spot;
	From.Z = FloorZ + 64;
	if (P.Trace(HitLocation, HitNormal, From - vect(0,0,124), From, false) == None)
		return 0;
	return HitLocation.Z;
}

// The edge: from a spot on the pier south of the start (a scan of the
// original's traces found the lip there), the last top before the water,
// halved to a quarter unit.
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
	Log("DXREST: the edge at " $ Edge $ " along " $ EdgeDir $ ", the pier's floor at z " $ FloorZ $ ", the ledge's top at z " $ LedgeZ);
	return true;
}

function LogDropped(PlayerPawn P)
{
	local string Landings;

	if (Decoration(Dropped) != None)
		Landings = " landings " $ Decoration(Dropped).numLandings;
	Log("DXREST: edge tick " $ Ticks $ " t " $ StepTime $ " " $ Dropped.Class.Name $ " past the edge " $ ((Dropped.Location - Edge) dot EdgeDir)
		$ " z " $ Dropped.Location.Z $ " physics " $ int(Dropped.Physics) $ " velocity " $ Dropped.Velocity $ " base " $ Dropped.Base $ Landings);
}

function LogNear(PlayerPawn P, Actor D)
{
	local Actor A;
	Log("DXREST: placed " $ D.Name $ " at " $ D.Location $ " radius " $ D.CollisionRadius $ " height " $ D.CollisionHeight
		$ " collideActors " $ D.bCollideActors $ " blockActors " $ D.bBlockActors $ " blockPlayers " $ D.bBlockPlayers
		$ " collideWorld " $ D.bCollideWorld $ " physics " $ int(D.Physics));
	foreach P.AllActors(class'Actor', A)
	{
		if (A != D && A.bCollideActors && VSize(A.Location - D.Location) < 120 + A.CollisionRadius)
			Log("DXREST:   near " $ D.Name $ ": " $ A.Name $ " (" $ A.Class $ ") at " $ A.Location $ " radius " $ A.CollisionRadius
				$ " height " $ A.CollisionHeight $ " blockActors " $ A.bBlockActors $ " blockPlayers " $ A.bBlockPlayers);
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector X, Y, Z;
	local rotator R;
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = 1;
			}
		}
		else
		{
			MapTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0)
		{
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			GetAxes(R, X, Y, Z);
			Log("DXREST: player at " $ P.Location $ " height " $ P.CollisionHeight);
			DropProbe(P, "player", P.Location, P.CollisionRadius, P.CollisionHeight);
			DropProbe(P, "player-5", P.Location, 5, 5);
			DropProbe(P, "player-r10", P.Location, 10, P.CollisionHeight);
			DropProbe(P, "player-r40", P.Location, 40, P.CollisionHeight);
			TraceDown(P, "crate", P.Location + X * 200 + Y * 130 + vect(0,0,12), 56.5, 55.25);
			TraceDown(P, "barrel", P.Location + X * 170 - Y * 110 + vect(0,0,12), 20, 28.25);
			TraceDown(P, "box", P.Location + X * 300 - Y * 40 + vect(0,0,12), 42, 49.25);
			Placed[0] = Place(P, class'CrateUnbreakableLarge', P.Location + X * 200 + Y * 130);
			Placed[1] = Place(P, class'Barrel1', P.Location + X * 170 - Y * 110);
			Placed[2] = Place(P, class'BoxLarge', P.Location + X * 300 - Y * 40);
			for (i = 0; i < 3; i++)
				if (Placed[i] != None)
					LogNear(P, Placed[i]);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		Ticks++;
		for (i = 0; i < 3; i++)
			if (Placed[i] != None)
				Log("DXREST: tick " $ Ticks $ " t " $ StepTime $ " " $ Placed[i].Name $ " z " $ Placed[i].Location.Z
					$ " bottom " $ (Placed[i].Location.Z - Placed[i].CollisionHeight) $ " physics " $ int(Placed[i].Physics)
					$ " vz " $ Placed[i].Velocity.Z $ " base " $ Placed[i].Base);
		if (StepTime > 1.5)
		{
			for (i = 0; i < 3; i++)
				if (Placed[i] != None)
					Placed[i].Destroy();
			if (!PickEdge(P))
			{
				Log("DXREST: no edge over deep water");
				Phase = 5;
			}
			else
			{
				// The crate, its bottom 2 over the floor, 0.4 of its radius past
				// the edge.
				Dropped = P.Spawn(class'CrateUnbreakableSmall',,, Edge + vect(0,0,1) * (class'CrateUnbreakableSmall'.Default.CollisionHeight + 2)
					+ EdgeDir * 0.4 * class'CrateUnbreakableSmall'.Default.CollisionRadius, rot(0,0,0));
				if (Dropped == None)
				{
					Log("DXREST: no room for the crate");
					Phase = 5;
				}
				else
				{
					Dropped.SetPhysics(PHYS_Falling);
					Ticks = 0;
					StepTime = 0;
					Phase = 3;
				}
			}
		}
	}
	else if (Phase == 3)
	{
		Ticks++;
		LogDropped(P);
		if (StepTime > 2.0)
		{
			Dropped.Destroy();
			// The player, its feet 2 over the floor, 0.95 of its radius past
			// the edge, still.
			P.SetLocation(Edge + vect(0,0,1) * (P.CollisionHeight + 2) + EdgeDir * 0.95 * P.CollisionRadius);
			P.Velocity = vect(0,0,0);
			P.Acceleration = vect(0,0,0);
			P.SetPhysics(PHYS_Falling);
			Dropped = P;
			Ticks = 0;
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		Ticks++;
		LogDropped(P);
		if (StepTime > 2.0)
			Phase = 5;
	}
	else if (Phase == 5)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 6;
	}
}
