//=============================================================================
// GlassConsole: a mover's glass, and a mesh behind it, shot in both engines
// to compare how the glass blends and what it is drawn over
// (vibe/docs/DEVELOPMENT.md, scripted runs; vibe/docs/NATIVES.md, draw
// order and movers). On TargetMap ([DXCapture.GlassConsole], Liberty Island
// by default), the Pick-th glass mover in name order (0 by default), or the
// one named GlassName: a DeusExMover whose FragmentClass is GlassFragment,
// as the game tells one. The view put View units (96 by default) off its
// box along its thinnest side, on the side with more room, facing its
// middle; the player out of the world, with no shadow, unseen by the NPCs,
// the HUD hidden, conversations and datalinks stopped as they start. Two
// shots 2 s apart of the glass alone; then a large crate's mesh, unlit and
// with no collision, stood Behind units (64 by default) beyond the glass's
// far face, and two more; the glass hidden, and two more; the glass shown
// and broken (BlowItUp, which moves it away), and 5 s on two more, of the
// crate alone. Each shot marked for the original's grabber; then exit.
// Pick -1 is a census instead: every glass mover, its box and the room on
// each side, then an exit. Each line is logged with "DXGLASS:" in front.
//=============================================================================
class GlassConsole extends Console;

var config string TargetMap;
var config int Pick;
var config string GlassName;
var config float View;
var config float Behind;

var string CurrentMap;
var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Object Stopped;
var DeusExMover Glass;
var Effects Crate;
var vector Middle, Eye, CrateSpot;

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
		Log("DXGLASS: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXGLASS: conversation " $ Stopped.Name $ " stopped");
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

// The thinnest side's axis and the box's half extent along it
function vector Thinnest(vector Min, vector Max, out float Half)
{
	local vector E;
	E = (Max - Min) * 0.5;
	if (E.X <= E.Y && E.X <= E.Z)
	{
		Half = E.X;
		return vect(1,0,0);
	}
	if (E.Y <= E.Z)
	{
		Half = E.Y;
		return vect(0,1,0);
	}
	Half = E.Z;
	return vect(0,0,1);
}

// How far the world is from the box's face along Dir, up to Most
function float Room(PlayerPawn P, vector From, vector Dir, float Most)
{
	local vector HitLocation, HitNormal;
	if (P.Trace(HitLocation, HitNormal, From + Dir * Most, From, false) != None)
		return VSize(HitLocation - From);
	return Most;
}

function Census(PlayerPawn P)
{
	local DeusExMover G;
	local vector Min, Max, A;
	local float Half;
	local int Count;

	foreach P.AllActors(class'DeusExMover', G)
	{
		if (G.FragmentClass != class'GlassFragment')
			continue;
		Count++;
		if (!G.GetBoundingBox(Min, Max))
		{
			Log("DXGLASS: census " $ G.Name $ " at " $ G.Location $ " no box");
			continue;
		}
		A = Thinnest(Min, Max, Half);
		Log("DXGLASS: census " $ G.Name $ " at " $ G.Location $ " box " $ Min $ " to " $ Max $ " axis " $ A
			$ " room " $ Room(P, (Min + Max) * 0.5 + A * (Half + 2), A, 512) $ " and " $ Room(P, (Min + Max) * 0.5 - A * (Half + 2), -A, 512)
			$ " zone " $ G.Region.Zone.Name);
	}
	Log("DXGLASS: census of " $ Count $ " glass movers");
}

// The Pick-th glass mover in name order, and the view on it
function bool Choose(PlayerPawn P)
{
	local DeusExMover G, Cands[64], T;
	local vector Min, Max, A;
	local float Half, Near, Far, Dist;
	local int Count, i;

	foreach P.AllActors(class'DeusExMover', G)
	{
		if (G.FragmentClass == class'GlassFragment' && Count < 64)
		{
			Cands[Count] = G;
			Count++;
			for (i = Count - 1; i > 0 && Caps(string(Cands[i].Name)) < Caps(string(Cands[i - 1].Name)); i--)
			{
				T = Cands[i]; Cands[i] = Cands[i - 1]; Cands[i - 1] = T;
			}
		}
	}
	Log("DXGLASS: " $ Count $ " glass movers");
	if (GlassName != "")
	{
		for (i = 0; i < Count && !(string(Cands[i].Name) ~= GlassName); i++)
			;
		Pick = i;
	}
	if (Pick >= Count || !Cands[Pick].GetBoundingBox(Min, Max))
	{
		Log("DXGLASS: no glass " $ Pick);
		return false;
	}
	Glass = Cands[Pick];
	Middle = (Min + Max) * 0.5;
	A = Thinnest(Min, Max, Half);
	Dist = View;
	if (Dist <= 0)
		Dist = 96;
	Near = Room(P, Middle + A * (Half + 2), A, Dist + 16);
	Far = Room(P, Middle - A * (Half + 2), -A, Dist + 16);
	if (Far > Near)
	{
		A = -A;
		Near = Far;
	}
	Eye = Middle + A * (Half + FMin(Dist, Near - 16));
	Dist = Behind;
	if (Dist <= 0)
		Dist = 64;
	Far = Room(P, Middle - A * (Half + 2), -A, Dist + 64);
	CrateSpot = Middle - A * (Half + FMin(Dist, Far - 48));
	Log("DXGLASS: chosen " $ Glass.Name $ " at " $ Glass.Location $ " box " $ Min $ " to " $ Max $ " middle " $ Middle
		$ " eye " $ Eye $ " room " $ Near $ " crate at " $ CrateSpot $ " room behind " $ Far
		$ " zone " $ Glass.Region.Zone.Name $ " style " $ int(Glass.Style) $ " glow " $ Glass.ScaleGlow);
	return true;
}

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = rotator(Middle - Eye);
	P.SetRotation(P.ViewRotation);
}

function StandCrate(PlayerPawn P)
{
	Crate = P.Spawn(class'Effects',,, CrateSpot, rotator(Eye - CrateSpot));
	if (Crate == None)
	{
		Log("DXGLASS: no crate");
		return;
	}
	Crate.SetCollision(false, false, false);
	Crate.bCollideWorld = false;
	Crate.SetPhysics(PHYS_None);
	Crate.LifeSpan = 0;
	Crate.bHidden = false;
	Crate.DrawType = DT_Mesh;
	Crate.Style = STY_Normal;
	Crate.Mesh = class'CrateUnbreakableLarge'.default.Mesh;
	Crate.Skin = class'CrateUnbreakableLarge'.default.Skin;
	Crate.DrawScale = class'CrateUnbreakableLarge'.default.DrawScale;
	// Unlit, so what is drawn over it is all that changes it
	Crate.bUnlit = true;
	Log("DXGLASS: crate " $ Crate.Name $ " at " $ Crate.Location $ " mesh " $ Crate.Mesh $ " zone " $ Crate.Region.Zone.Name);
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
				Log("DXGLASS: opening " $ Target());
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
			if (!Choose(P))
			{
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.KillShadow();
			// Seen through the glass, the player would draw the NPCs' fire
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
		// Settled after 3 s; a shot every 2 s from then, the mark shown for
		// the 0.6 s before each; the crate stood after the second, the glass
		// hidden after the fourth, shown and broken after the sixth, its
		// pieces given 5 s to fall.
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
				Log("DXGLASS: shot " $ ShotsTaken $ " view at " $ (P.Location + vect(0,0,1) * P.EyeHeight) $ " rotation " $ P.ViewRotation
					$ " FOV " $ P.FOVAngle);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 1.0;
				if (ShotsTaken == 2)
					StandCrate(P);
				if (ShotsTaken == 4)
				{
					Glass.bHidden = true;
					Log("DXGLASS: glass hidden");
				}
				if (ShotsTaken == 6)
				{
					Glass.bHidden = false;
					Glass.BlowItUp(P);
					Log("DXGLASS: glass broken, now at " $ Glass.Location);
					StepTime = -2.0;
				}
			}
		}
		else if (ShotsTaken >= 8 && StepTime > 2.0)
		{
			Log("DXGLASS: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
