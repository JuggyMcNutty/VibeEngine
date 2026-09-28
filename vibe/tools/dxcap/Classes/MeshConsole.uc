//=============================================================================
// MeshConsole: meshes lit, the same in both engines (vibe/docs/DEVELOPMENT.md,
// scripted runs). At Liberty Island's start, level and facing the start's
// way, the HUD hidden: Paul Denton, who stands still, moved 220 units ahead,
// and a large crate, a barrel and a large box placed around him on the pier;
// each one's place and look logged, then two shots 2 s apart once they have
// settled; then exit.
// Each line is logged with "DXMESH:" in front.
//=============================================================================
class MeshConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var Actor Placed[4];

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function LogLook(string Label, Actor A)
{
	Log("DXMESH: " $ Label $ " " $ A.Name $ " at " $ A.Location $ " rotation " $ A.Rotation $ " mesh " $ A.Mesh
		$ " style " $ int(A.Style) $ " unlit " $ A.bUnlit $ " glow " $ A.ScaleGlow $ " ambient " $ A.AmbientGlow
		$ " zone " $ A.Region.Zone.Name $ " zone ambient " $ A.Region.Zone.AmbientBrightness);
}

function Actor Place(PlayerPawn P, class<Actor> C, vector Where)
{
	local Actor A;
	A = P.Spawn(C,,, Where, P.Rotation);
	if (A == None)
		Log("DXMESH: no room for " $ C $ " at " $ Where);
	return A;
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

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local ScriptedPawn SP;
	local vector X, Y, Z;
	local rotator R;
	local int i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
	StepTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		// The original starts in its menu map.
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
				Log("DXMESH: opening 01_NYC_UNATCOIsland");
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
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 6.0 && DXP != None)
		{
			R = P.ViewRotation;
			R.Pitch = 0;
			P.ViewRotation = R;
			P.SetRotation(R);
			GetAxes(R, X, Y, Z);
			foreach P.AllActors(class'ScriptedPawn', SP)
			{
				if (SP.IsA('PaulDenton'))
				{
					if (SP.SetLocation(P.Location + X * 220))
						Placed[0] = SP;
					else
						Log("DXMESH: Paul cannot stand at " $ (P.Location + X * 220));
					break;
				}
			}
			Placed[1] = Place(P, class'CrateUnbreakableLarge', P.Location + X * 200 + Y * 130);
			Placed[2] = Place(P, class'Barrel1', P.Location + X * 170 - Y * 110);
			Placed[3] = Place(P, class'BoxLarge', P.Location + X * 300 - Y * 40);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
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
				for (i = 0; i < 4; i++)
					if (Placed[i] != None)
						LogLook("shot " $ ShotsTaken, Placed[i]);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 1.0;
			}
		}
		else if (ShotsTaken >= 2 && StepTime > 2.0)
		{
			Log("DXMESH: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
