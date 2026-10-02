//=============================================================================
// RestConsole: where a falling decoration comes to rest (vibe/docs/
// DEVELOPMENT.md, scripted runs). At Liberty Island's start, the large crate,
// the barrel and the large box placed on the pier as MeshConsole places them;
// each one's collision and the colliding actors near it logged, then every
// tick for 1.5 s its place, the bottom of its cylinder, physics, velocity and
// base; then exit. Each line starts "DXREST:".
//=============================================================================
class RestConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var Actor Placed[3];
var int Ticks;

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
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
}
