//=============================================================================
// FrobConsole: the frob highlight -- the box FrobDisplayWindow draws round
// the player's frob target, and its label -- logged with "DXFROB:" and the
// console's clock, with marked shots (vibe/docs/DEVELOPMENT.md, scripted
// runs).
//
// On Liberty Island: the player stood 90 units from each of two decorations
// near the start, facing its middle; a shot with the crosshair on its
// middle, and with the view turned 5 degrees left, right, up and down. Each
// shot logs the frob target. What is compared between the engines: the
// target, and the box's place and size round the decoration in the shots.
//=============================================================================
class FrobConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int ShotCount;
var int Target;
var int Turn;
var DeusExDecoration Deco;
var rotator Facing;
var bool bTravelled;

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

function Shot(PlayerPawn P, string Label)
{
	Log("DXFROB: shot " $ ShotCount $ " = " $ Label $ ", target " $ DeusExPlayer(P).FrobTarget);
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

// The decorations by name, the same in both engines' level.
function DeusExDecoration FindDeco(PlayerPawn P, name N)
{
	local DeusExDecoration D;
	foreach P.AllActors(class'DeusExDecoration', D)
		if (D.Name == N)
			return D;
	return None;
}

// The player 90 units from the decoration, at its own height, its eye
// facing the decoration's middle.
function StandBefore(PlayerPawn P, DeusExDecoration D)
{
	local vector Dir, Spot;

	Dir = D.Location - P.Location;
	Dir.Z = 0;
	Dir = Normal(Dir);
	Spot = D.Location - Dir * 90;
	Spot.Z = P.Location.Z;
	if (!P.SetLocation(Spot))
		Log("DXFROB: could not stand at " $ Spot);
	Facing = rotator(D.Location - (P.Location + vect(0,0,1) * P.EyeHeight));
	P.SetRotation(Facing);
	P.ViewRotation = Facing;
	Log("DXFROB: before " $ D.Name $ " (" $ D.Class.Name $ ") at " $ D.Location $ " radius " $ D.CollisionRadius
	    $ " height " $ D.CollisionHeight $ ", player at " $ P.Location $ " facing " $ Facing);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
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
	TickShot(P, Delta);
	if (bShotPending)
		return;

	if (!(M ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0 && !bTravelled)
		{
			Log("DXCAP: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			bTravelled = true;
		}
		return;
	}
	if (DeusExPlayer(P) == None)
		return;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (MapTime > 6.0)
		{
			if (Target == 0)
				Deco = FindDeco(P, 'TrashCan1');
			else
				Deco = FindDeco(P, 'CrateBreakableMedGeneral0');
			if (Deco == None)
			{
				Log("DXFROB: no decoration " $ Target);
				Phase = 2;
				return;
			}
			P.SetPhysics(PHYS_None);
			StandBefore(P, Deco);
			Turn = 0;
			StepTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == 1)
	{
		// Each view held a second before its shot: the frob target is
		// traced every tenth of a second.
		R = Facing;
		if (Turn == 1)
			R.Yaw += 910;
		else if (Turn == 2)
			R.Yaw -= 910;
		else if (Turn == 3)
			R.Pitch += 910;
		else if (Turn == 4)
			R.Pitch -= 910;
		P.SetRotation(R);
		P.ViewRotation = R;
		if (StepTime > 1.0)
		{
			Shot(P, Deco.Name $ " turn " $ Turn);
			StepTime = 0;
			Turn++;
			if (Turn > 4)
			{
				Target++;
				Phase = 0;
				MapTime = 6.0;
				if (Target > 1)
					Phase = 2;
			}
		}
	}
	else if (Phase == 2)
	{
		if (StepTime > 1.0)
		{
			Log("DXFROB: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 3;
		}
	}
}
