//=============================================================================
// FlushConsole: a mover flush with a wall, shot in both engines to see what
// shows where the two share a plane (vibe/docs/DEVELOPMENT.md, scripted
// runs; vibe/docs/NATIVES.md, draw order and movers). On TargetMap
// ([DXCapture.FlushConsole], Liberty Island by default), the first door
// from the Pick-th in name order (0 by default), or the one named DoorName,
// with a flat wall beside its doorway: a DeusExMover with bIsDoor and no
// glass, upright; on the side of its thinnest axis with more room, a line
// toward the wall from in front at each of nine points over a door's width
// past its edge, all meeting one plane facing back, and no other mover's
// box there (a line meets a mover where the BSP filed it, as the level).
// The door is made not to collide, its face put in the wall's plane and
// slid half its width into the wall: a pocket door's face, flush with the
// wall. The view put View units (160 by default) from the door's edge in
// the wall, Angle degrees (0 by default) off the wall's normal toward the
// door's side, less what is in the way, facing that edge: the depth
// buffer's fight shows from afar and aslant. The player out of the world,
// with no shadow, unseen by the NPCs, the HUD hidden, conversations and
// datalinks stopped as they start. Four shots 1 s apart; then the view 4
// units along the wall, as the fight moves with the view, and four more;
// each marked for the original's grabber; then exit. Each line is logged
// with "DXFLUSH:" in front.
//=============================================================================
class FlushConsole extends Console;

var config string TargetMap;
var config int Pick;
var config string DoorName;
var config float View;
var config float Angle;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Object Stopped;
var DeusExMover Door;
var vector Focus, Eye, Along;

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
		Log("DXFLUSH: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXFLUSH: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
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

// How far the world is from From along Dir, up to Most
function float Room(PlayerPawn P, vector From, vector Dir, float Most)
{
	local vector HitLocation, HitNormal;
	if (P.Trace(HitLocation, HitNormal, From + Dir * Most, From, false) != None)
		return VSize(HitLocation - From);
	return Most;
}

// Where along S the wall in front of From faces S: a line from From back
// along -S, its hit's normal S's; false when it meets none so
function bool WallAt(PlayerPawn P, vector From, vector S, out float Plane)
{
	local vector HitLocation, HitNormal;
	if (P.Trace(HitLocation, HitNormal, From - S * 256, From, false) == None || (HitNormal dot S) < 0.99)
		return false;
	Plane = HitLocation dot S;
	return true;
}

// Whether another mover's box reaches the wall's patch: Wd*[Edge, Edge +
// 2 WHalf] beside the doorway, from Low to High, within 16 of its plane
function bool MoverThere(PlayerPawn P, DeusExMover Which, vector Middle, vector S, vector Wd, float WHalf, float Low, float High, float Plane)
{
	local Mover M;
	local vector Min, Max, C, E;
	local float Along, Across;

	foreach P.AllActors(class'Mover', M)
	{
		if (M == Which || !M.GetBoundingBox(Min, Max))
			continue;
		C = (Min + Max) * 0.5;
		E = (Max - Min) * 0.5;
		Along = (C - Middle) dot Wd;
		Across = Abs(E dot Wd);
		if (Along + Across < WHalf || Along - Across > 3 * WHalf)
			continue;
		if (C.Z + E.Z < Low || C.Z - E.Z > High)
			continue;
		if (Abs((C dot S) - Plane) > Abs(E dot S) + 16)
			continue;
		Log("DXFLUSH: " $ M.Name $ " by the wall beside " $ Which.Name);
		return true;
	}
	return false;
}

// A flat wall facing S over the width Wd*[Edge, Edge + 2 WHalf] beside the
// doorway, from Low to High: nine lines meeting one plane
function bool FlatWall(PlayerPawn P, vector Middle, vector S, vector Wd, float Half, float WHalf, float Low, float High, out float Plane)
{
	local vector From;
	local float Other;
	local int a, b;

	for (a = 0; a < 3; a++)
	{
		for (b = 0; b < 3; b++)
		{
			From = Middle + Wd * (WHalf + 8 + (2 * WHalf - 16) * a / 2) + S * (Half + 64);
			From.Z = Low + 8 + (High - Low - 16) * b / 2;
			if (!WallAt(P, From, S, Other))
				return false;
			if (a == 0 && b == 0)
				Plane = Other;
			else if (Abs(Other - Plane) > 0.1)
				return false;
		}
	}
	return true;
}

// The Pick-th door in name order, put flush with the wall, and the view on it
function bool Choose(PlayerPawn P)
{
	local DeusExMover D, Cands[128], T;
	local vector Min, Max, Middle, E, S, W, Wd, Dir;
	local float Half, WHalf, Plane, Shift;
	local int Count, i, k;

	foreach P.AllActors(class'DeusExMover', D)
	{
		if (D.bIsDoor && D.FragmentClass != class'GlassFragment' && Count < 128)
		{
			Cands[Count] = D;
			Count++;
			for (i = Count - 1; i > 0 && Caps(string(Cands[i].Name)) < Caps(string(Cands[i - 1].Name)); i--)
			{
				T = Cands[i]; Cands[i] = Cands[i - 1]; Cands[i - 1] = T;
			}
		}
	}
	Log("DXFLUSH: " $ Count $ " doors");
	if (DoorName != "")
	{
		for (i = 0; i < Count && !(string(Cands[i].Name) ~= DoorName); i++)
			;
		Pick = i;
	}
	for (i = Pick; i < Count; i++)
	{
		if (!Cands[i].GetBoundingBox(Min, Max))
			continue;
		Middle = (Min + Max) * 0.5;
		E = (Max - Min) * 0.5;
		// Upright: thinnest along X or Y, its width along the other
		if (E.X <= E.Y)
		{
			Half = E.X; WHalf = E.Y; S = vect(1,0,0); W = vect(0,1,0);
		}
		else
		{
			Half = E.Y; WHalf = E.X; S = vect(0,1,0); W = vect(1,0,0);
		}
		if (E.Z < Half)
		{
			Log("DXFLUSH: " $ Cands[i].Name $ " lies flat");
			continue;
		}
		if (Room(P, Middle - S * (Half + 2), -S, 512) > Room(P, Middle + S * (Half + 2), S, 512))
			S = -S;
		// The wall beside the doorway, either way along W
		for (k = 0; k < 2; k++)
		{
			Wd = W;
			if (k == 1)
				Wd = -W;
			if (FlatWall(P, Middle, S, Wd, Half, WHalf, Min.Z, Max.Z, Plane) && !MoverThere(P, Cands[i], Middle, S, Wd, WHalf, Min.Z, Max.Z, Plane))
				break;
		}
		if (k < 2)
			break;
		Log("DXFLUSH: no flat wall beside " $ Cands[i].Name $ " box " $ Min $ " to " $ Max);
		if (DoorName != "")
			return false;
	}
	if (i >= Count)
	{
		Log("DXFLUSH: no door from " $ Pick);
		return false;
	}
	Door = Cands[i];
	Shift = Plane - ((Middle dot S) + Half);
	Log("DXFLUSH: door " $ Door.Name $ " at " $ Door.Location $ " box " $ Min $ " to " $ Max $ " face along " $ S
		$ " width along " $ Wd $ " wall at " $ Plane $ ", the face " $ Shift $ " from it");
	Door.SetCollision(false, false, false);
	Door.SetLocation(Door.Location + S * Shift + Wd * WHalf);
	Focus = Middle + Wd * WHalf;
	Focus = Focus + S * (Plane - (Focus dot S));
	Along = Wd;
	if (View <= 0)
		View = 160;
	Dir = Normal(S * Cos(Angle * 3.14159265 / 180) + Wd * Sin(Angle * 3.14159265 / 180));
	Eye = Focus + S * 8 + Dir * FMax(64, FMin(View, Room(P, Focus + S * 8, Dir, View + 16) - 16));
	Log("DXFLUSH: door now at " $ Door.Location $ " focus " $ Focus $ " eye " $ Eye $ " zone " $ Door.Region.Zone.Name);
	return true;
}

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = rotator(Focus - Eye);
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
				Log("DXFLUSH: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			if (!Choose(P))
			{
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.KillShadow();
			P.bDetectable = false;
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
		// Settled after 3 s; a shot every second from then, the mark shown
		// for the 0.6 s before each; the view moved along the wall after
		// the fourth.
		if (StepTime > 3.0 && ShotsTaken < 8)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 2.4;
			}
			else
			{
				Log("DXFLUSH: shot " $ ShotsTaken $ " view at " $ (P.Location + vect(0,0,1) * P.EyeHeight) $ " rotation " $ P.ViewRotation
					$ " FOV " $ P.FOVAngle);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 2.0;
				if (ShotsTaken == 4)
				{
					Eye = Eye + Along * 4;
					Focus = Focus + Along * 4;
					Log("DXFLUSH: the view 4 units along the wall");
				}
			}
		}
		else if (ShotsTaken >= 8 && StepTime > 3.0)
		{
			Log("DXFLUSH: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
