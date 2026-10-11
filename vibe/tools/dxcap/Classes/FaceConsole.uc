//=============================================================================
// FaceConsole: a character's mesh close up, shot in both engines to compare
// how its translucent faces (lenses, a visor) are drawn among its opaque
// ones (vibe/docs/DEVELOPMENT.md, scripted runs; vibe/docs/NATIVES.md, draw
// order and movers). At Liberty Island's start, level and facing the
// start's way: the mesh, skins, draw scale and animation of MeshOf
// ([DXCapture.FaceConsole], DeusEx.UNATCOTroop by default) on a plain actor
// with no AI or collision, unlit, its animation's first frame held, Ahead
// units (200 by default) ahead and turned to the player; the view put
// Distance units (40 by default) in front of it at the class's eye height,
// facing its eyes; the player out of the world, unseen by the NPCs, the HUD
// hidden, conversations and datalinks stopped as they start. Two shots 2 s
// apart, each marked for the original's grabber; then exit. Each line is
// logged with "DXFACE:" in front.
//=============================================================================
class FaceConsole extends Console;

var config string MeshOf;
var config float Ahead;
var config float Distance;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Object Stopped;
var Effects Model;
var vector Eye, Look;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

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

// A conversation's camera would take the view, and a datalink's text the
// frame's foot.
function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXFACE: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXFACE: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

function bool Stand(PlayerPawn P)
{
	local class<Pawn> C;
	local vector X, Y, Z, Spot;
	local rotator R;
	local float Dist, Far;
	local int i;

	if (MeshOf == "")
		MeshOf = "DeusEx.UNATCOTroop";
	C = class<Pawn>(DynamicLoadObject(MeshOf, class'Class'));
	if (C == None)
	{
		Log("DXFACE: no class " $ MeshOf);
		return false;
	}
	Far = Ahead;
	if (Far <= 0)
		Far = 200;
	Dist = Distance;
	if (Dist <= 0)
		Dist = 40;
	R = P.ViewRotation;
	R.Pitch = 0;
	R.Roll = 0;
	GetAxes(R, X, Y, Z);
	Spot = P.Location + X * Far;
	Model = P.Spawn(class'Effects',,, Spot, rotator(-X));
	if (Model == None)
	{
		Log("DXFACE: no model at " $ Spot);
		return false;
	}
	Model.SetCollision(false, false, false);
	Model.bCollideWorld = false;
	Model.SetPhysics(PHYS_None);
	Model.LifeSpan = 0;
	Model.bHidden = false;
	Model.DrawType = DT_Mesh;
	Model.Style = STY_Normal;
	Model.Mesh = C.default.Mesh;
	Model.Skin = C.default.Skin;
	Model.Texture = C.default.Texture;
	for (i = 0; i < 8; i++)
		Model.MultiSkins[i] = C.default.MultiSkins[i];
	Model.DrawScale = C.default.DrawScale;
	Model.Fatness = C.default.Fatness;
	Model.PrePivot = C.default.PrePivot;
	Model.AnimSequence = C.default.AnimSequence;
	Model.AnimFrame = 0;
	Model.AnimRate = 0;
	// Unlit, so the faces' own textures and how they are drawn are all that show
	Model.bUnlit = true;
	Eye = Spot + Z * C.default.BaseEyeHeight - X * Dist;
	Look = Spot + Z * C.default.BaseEyeHeight;
	Log("DXFACE: " $ MeshOf $ " mesh " $ Model.Mesh $ " sequence " $ Model.AnimSequence $ " at " $ Model.Location
		$ " rotation " $ Model.Rotation $ " scale " $ Model.DrawScale $ " eye height " $ C.default.BaseEyeHeight
		$ " view at " $ Eye);
	return true;
}

function HoldView(PlayerPawn P)
{
	P.SetPhysics(PHYS_None);
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.EyeHeight = P.BaseEyeHeight;
	P.SetLocation(Eye - vect(0,0,1) * P.EyeHeight);
	P.ViewRotation = rotator(Look - Eye);
	P.SetRotation(P.ViewRotation);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';
	if (DXP != None && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0)
		Silence(DXP);

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXFACE: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				MapTime = 0;
				Phase = -1;
			}
		}
		else if (MapTime > 6.0 && DXP != None)
		{
			if (!Stand(P))
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
	else if (Phase == -1 && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0)
	{
		MapTime = 0;
		Phase = 0;
	}
	else if (Phase == 1)
	{
		HoldView(P);
		// Settled after 3 s; a shot every 2 s from then, the mark shown for
		// the 0.6 s before each.
		if (StepTime > 3.0 && ShotsTaken < 2)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 2.4;
			}
			else
			{
				Log("DXFACE: shot " $ ShotsTaken $ " view at " $ (P.Location + vect(0,0,1) * P.EyeHeight) $ " rotation " $ P.ViewRotation);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 1.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXFACE: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
