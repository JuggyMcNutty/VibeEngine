//=============================================================================
// DeathConsole: an NPC's death -- the Dying state's animation, the carcass
// it leaves and where it rests, logged with "DXDEATH:" and the console's
// clock, with marked shots of the body as it settles (vibe/docs/DEVELOPMENT.md,
// scripted runs).
//
// On Liberty Island: two isolated ScriptedPawns killed outright, one hit
// from behind (ScriptedPawn.PlayDying falls it forward, DeathFront, the
// carcass takes Mesh2), one from the front (falls back, DeathBack, the
// carcass keeps Mesh). Each kill logs the pawn's state, physics, animation
// (sequence, frame, rate, last frame), place and rotation through Dying,
// then the carcass: its class, mesh, place, scale, pivot and physics until
// it settles. What is compared between the engines: whether the death
// animation plays and completes, which mesh the carcass takes, where it
// rests over the floor and how it lies.
//=============================================================================
class DeathConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var float LogTime;
var float NextLog;
var string CurrentMap;
var int Phase;
var int ShotCount;
var ScriptedPawn Victim;
var vector DeathSpot;
var DeusExCarcass Body;
var vector HitOffset;
var int Kills;

// A mark in the view's corner while a shot is due, as CaptureConsole marks
// its shots: a grab of the screen from outside needs it on many frames.
var int MarkShot;
var bool bMarking;
var float MarkTime;
var bool bShotPending;

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

function Travel(PlayerPawn P, string Map)
{
	Log("DXCAP: opening " $ Map);
	P.ConsoleCommand("open " $ Map);
}

function Shot(PlayerPawn P, string Label)
{
	Log("DXDEATH: shot " $ ShotCount $ " = " $ Label);
	MarkShot = ShotCount;
	bMarking = true;
	MarkTime = 0;
	bShotPending = true;
}

function TickShot(PlayerPawn P, float Delta)
{
	if (!bShotPending)
		return;
	MarkTime += Delta;
	if (MarkTime > 0.8)
	{
		P.ConsoleCommand("shot");
		bMarking = false;
		bShotPending = false;
		ShotCount++;
	}
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

// The most isolated scripted pawn in the level: the one whose nearest
// neighbour is farthest away, and at least 400 units from the player, so
// a death's noise reaches nobody and the start's own pawns are skipped.
function ScriptedPawn FindVictim(PlayerPawn P)
{
	local ScriptedPawn A, B, Best;
	local float Near, BestNear;

	foreach P.AllActors(class'ScriptedPawn', A)
	{
		// The level keeps scripted pawns out of the world, hidden, in
		// stasis (NATIVES.md, out of sight): they are no victims, and
		// a robot explodes rather than leaving a carcass -- a human is the
		// case to capture.
		if (A.bImportant || A.Health <= 0 || A.bHidden || A.IsA('Robot') || A.IsA('Animal'))
			continue;
		Near = 100000;
		foreach P.AllActors(class'ScriptedPawn', B)
		{
			if (B == A || B.Health <= 0)
				continue;
			if (VSize(B.Location - A.Location) < Near)
				Near = VSize(B.Location - A.Location);
		}
		if (VSize(A.Location - P.Location) < 400)
			continue;
		if (Near > BestNear)
		{
			BestNear = Near;
			Best = A;
		}
	}
	return Best;
}

// The pawn's dying progress: its state, physics, animation and pose.
function LogPawn(ScriptedPawn A, string Label)
{
	Log("DXDEATH: " $ Label $ ": " $ A.Name $ " state " $ A.GetStateName() $ " physics " $ A.Physics
	    $ " anim " $ A.AnimSequence $ " frame " $ A.AnimFrame $ " rate " $ A.AnimRate
	    $ " last " $ A.AnimLast $ " at " $ A.Location $ " rot " $ A.Rotation
	    $ " hidden " $ A.bHidden $ " health " $ A.Health
	    $ " accel " $ A.Acceleration $ " vel " $ A.Velocity $ " groundspeed " $ A.GroundSpeed
	    $ " desired " $ A.DesiredSpeed $ " desiredrot " $ A.DesiredRotation);
}

// The carcass the kill left, once it exists.
function LogBody(DeusExCarcass C, string Label)
{
	Log("DXDEATH: " $ Label $ ": " $ C.Name $ " class " $ C.Class.Name $ " mesh " $ C.Mesh
	    $ " mesh2 " $ C.Mesh2 $ " at " $ C.Location $ " base " $ C.Base
	    $ " scale " $ C.DrawScale $ " prepivot " $ C.PrePivot
	    $ " physics " $ C.Physics $ " notdead " $ C.bNotDead);
}

function LogSurroundings(PlayerPawn P, string Label)
{
	local DeusExCarcass C;
	local ScriptedPawn A;

	// Only the carcass the kill left, not the level's own placed ones; the
	// victim itself until it is destroyed.
	foreach P.AllActors(class'DeusExCarcass', C)
		if (VSize(C.Location - DeathSpot) < 300)
			LogBody(C, Label);
	if (Victim != None)
		LogPawn(Victim, Label);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector HitLoc, Spot, Dir;
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
	TickShot(P, Delta);
	if (bShotPending)
		return;

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Travel(P, "01_NYC_UNATCOIsland");
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
			P.SetPhysics(PHYS_Walking);
			Victim = FindVictim(P);
			if (Victim == None)
			{
				Log("DXDEATH: no victim, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
			}
			else
			{
				Log("DXDEATH: victim " $ Victim.Name $ " (" $ Victim.Class.Name $ ") at " $ Victim.Location
				    $ ", health " $ Victim.Health $ ", facing " $ Victim.Rotation);
				StepTime = 0;
				Phase = 2;
			}
		}
	}
	else if (Phase == 2)
	{
		// Stand 120 units behind the victim, facing it.
		if (StepTime > 1.0)
		{
			Dir = vector(Victim.Rotation);
			Spot = Victim.Location - Dir * 120;
			P.SetLocation(Spot);
			P.SetRotation(rotator(Victim.Location - Spot));
			P.ViewRotation = P.Rotation;
			Log("DXDEATH: player behind " $ Victim.Name $ " at " $ P.Location $ ", shooting its back");
			Shot(P, "the victim alive");
			StepTime = 0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		// The kill (after the alive shot's mark has been up): a shot's own
		// damage, the hit landing on its back, so PlayDying falls it
		// forward (DeathFront, the carcass's Mesh2).
		if (StepTime > 1.5 && !bShotPending)
		{
			Dir = vector(Victim.Rotation);
			HitLoc = Victim.Location - Dir * 10 + vect(0,0,1) * (Victim.CollisionHeight * 0.6);
			LogPawn(Victim, "before the hit");
			DeathSpot = Victim.Location;
			Victim.TakeDamage(1000, P, HitLoc, -Dir * 1000, 'Shot');
			LogPawn(Victim, "after the hit");
			LogTime = 0;
			NextLog = 0.25;
			Kills++;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		// The death, logged as it runs: the anim playing out, then the
		// pawn gone and the carcass in its place.
		LogTime += Delta;
		if (LogTime >= NextLog)
		{
			LogSurroundings(P, "t+" $ LogTime);
			NextLog += 0.25;
		}
		if (StepTime > 1.0 && ShotCount == 1)
			Shot(P, "the body, 1 s in");
		if (StepTime > 3.0 && ShotCount == 2)
			Shot(P, "the body, 3 s in");
		if (StepTime > 6.0)
		{
			Shot(P, "the body, settled");
			StepTime = 0;
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		// The second kill, from the front this time: falls back, the
		// carcass keeps its own Mesh.
		if (StepTime > 2.0)
		{
			Victim = FindVictim(P);
			if (Victim == None)
			{
				Log("DXDEATH: no second victim, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
			}
			else
			{
				Log("DXDEATH: second victim " $ Victim.Name $ " (" $ Victim.Class.Name $ ") at " $ Victim.Location
				    $ ", facing " $ Victim.Rotation);
				StepTime = 0;
				Phase = 6;
			}
		}
	}
	else if (Phase == 6)
	{
		if (StepTime > 1.0)
		{
			Dir = vector(Victim.Rotation);
			Spot = Victim.Location + Dir * 120;
			P.SetLocation(Spot);
			P.SetRotation(rotator(Victim.Location - Spot));
			P.ViewRotation = P.Rotation;
			StepTime = 0;
			Phase = 7;
		}
	}
	else if (Phase == 7)
	{
		if (StepTime > 0.5)
		{
			Dir = vector(Victim.Rotation);
			HitLoc = Victim.Location + Dir * 10 + vect(0,0,1) * (Victim.CollisionHeight * 0.6);
			LogPawn(Victim, "before the hit");
			DeathSpot = Victim.Location;
			Victim.TakeDamage(1000, P, HitLoc, Dir * 1000, 'Shot');
			LogPawn(Victim, "after the hit");
			LogTime = 0;
			NextLog = 0.25;
			Kills++;
			Phase = 8;
		}
	}
	else if (Phase == 8)
	{
		LogTime += Delta;
		if (LogTime >= NextLog)
		{
			LogSurroundings(P, "t+" $ LogTime);
			NextLog += 0.25;
		}
		if (StepTime > 3.0 && ShotCount == 3)
			Shot(P, "the second body, 3 s in");
		if (StepTime > 6.0)
		{
			Shot(P, "the second body, settled");
			StepTime = 0;
			Phase = 9;
		}
	}
	else if (Phase == 9)
	{
		if (StepTime > 1.0)
		{
			Log("DXDEATH: " $ Kills $ " kills, done, exiting");
			P.ConsoleCommand("exit");
			Phase = 10;
		}
	}
}
