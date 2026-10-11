//=============================================================================
// ParticleConsole: particles -- ParticleGenerator's, drawn by its
// ParticleIterator, whose native update moves them -- shot in both engines
// (vibe/docs/DEVELOPMENT.md, scripted runs). At Liberty Island's start, level
// and facing the start's way, the HUD hidden: a shot of the scene, then two
// generators 250 units ahead, spewing a particle every 0.05 s: smoke rising at
// 60, growing and fading over 2.5 s, and puffs thrown up at 150 at random
// under gravity for 1.5 s, 120 to the right; 4 s on, six shots 1.6 s apart,
// each marked for the original's grabber; then exit. Each generator's
// settings are logged with "DXPART:".
//=============================================================================
class ParticleConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;
var ParticleGenerator Gen[2];

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

function ParticleGenerator Spew(PlayerPawn P, vector Where, float Rise, float Eject, float Life, float Scale, bool bUp)
{
	local ParticleGenerator G;
	local rotator R;

	R.Pitch = 16384;
	G = P.Spawn(class'ParticleGenerator',,, Where, R);
	if (G == None)
	{
		Log("DXPART: no room for a generator at " $ Where);
		return None;
	}
	G.frequency = 1.0;
	G.checkTime = 0.05;
	G.numPerSpawn = 1;
	G.riseRate = Rise;
	G.ejectSpeed = Eject;
	G.particleLifeSpan = Life;
	G.particleDrawScale = Scale;
	G.bScale = bUp;
	G.bFade = bUp;
	G.bGravity = !bUp;
	G.bRandomEject = !bUp;
	G.bTranslucent = true;
	G.bParticlesUnlit = true;
	G.bTriggered = false;
	G.bSpewing = true;
	if (G.proxy != None)
	{
		G.proxy.bHidden = false;
		G.SetProxyData();
	}
	Log("DXPART: " $ G.Name $ " at " $ G.Location $ " rotation " $ G.Rotation $ " rise " $ G.riseRate $ " eject " $ G.ejectSpeed
		$ " life " $ G.particleLifeSpan $ " scale " $ G.particleDrawScale $ " scales " $ G.bScale $ " fades " $ G.bFade
		$ " gravity " $ G.bGravity $ " random " $ G.bRandomEject $ " zone gravity " $ G.Region.Zone.ZoneGravity
		$ " proxy " $ G.proxy $ " texture " $ G.proxy.Texture $ " style " $ int(G.proxy.Style));
	return G;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector X, Y, Z;
	local rotator R;

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
				Log("DXPART: opening 01_NYC_UNATCOIsland");
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
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			Log("DXPART: the player at " $ P.Location $ " rotation " $ P.ViewRotation);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2 || Phase == 3)
	{
		// The scene alone after 3 s; the generators then, and after 4 s more
		// a shot each second, the mark shown for the 0.6 s before each.
		if (Phase == 2 && StepTime > 3.0 && ShotsTaken == 1)
		{
			GetAxes(P.ViewRotation, X, Y, Z);
			Gen[0] = Spew(P, P.Location + X * 250 - Z * 30, 60, 0, 2.5, 0.3, true);
			Gen[1] = Spew(P, P.Location + X * 250 + Y * 120 + Z * 40, 0, 150, 1.5, 0.15, false);
			StepTime = -1.0;
			Phase = 3;
		}
		else if (StepTime > 3.0 && ShotsTaken < 7 && (ShotsTaken == 0 || Phase == 3))
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 2.4;
			}
			else
			{
				Log("DXPART: shot " $ ShotsTaken $ " at " $ P.Level.TimeSeconds);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 2.0;
			}
		}
		else if (ShotsTaken >= 7 && StepTime > 2.0)
		{
			Log("DXPART: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
