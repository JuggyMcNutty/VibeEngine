//=============================================================================
// ViewConsole: the player's own weapon in view, the same in both engines
// (vibe/docs/DEVELOPMENT.md, scripted runs). At Liberty Island's start, level
// and facing the start's way: the Dragon's Tooth given as the game gives the
// starting pistol (a spawned one frobbed) and put in hand, the HUD hidden,
// its look logged and shot three times over two seconds once it is up; then
// exit.
// Each line is logged with "DXVIEW:" in front.
//=============================================================================
class ViewConsole extends Console;

var float MapTime;
var float StepTime;
var int Phase;
var int ShotsTaken;

// The mark for the original's screen grabber, as CaptureConsole's.
var int MarkShot;
var bool bMarking;

function LogLook(string Label, Actor A)
{
	local int i;
	local string S;
	S = "DXVIEW: " $ Label $ " " $ A.Name $ " state " $ A.GetStateName() $ " mesh " $ A.Mesh
		$ " style " $ int(A.Style) $ " unlit " $ A.bUnlit $ " glow " $ A.ScaleGlow $ " ambient " $ A.AmbientGlow
		$ " fatness " $ A.Fatness $ " enviro " $ A.bMeshEnviroMap $ " skin " $ A.Skin $ " texture " $ A.Texture
		$ " light " $ int(A.LightType) $ " effect " $ int(A.LightEffect) $ " bright " $ A.LightBrightness
		$ " hue " $ A.LightHue $ " sat " $ A.LightSaturation $ " radius " $ A.LightRadius;
	for (i = 0; i < 8; i++)
		S = S $ " multi" $ i $ " " $ A.MultiSkins[i];
	Log(S);
	if (Viewport != None && Viewport.Actor != None)
		Log("DXVIEW: " $ Label $ " view fov " $ Viewport.Actor.FovAngle $ " desired " $ Viewport.Actor.DesiredFOV $ " at " $ Viewport.Actor.Location $ " rotation " $ Viewport.Actor.ViewRotation $ " weapon at " $ A.Location $ " rotation " $ A.Rotation $ " scale " $ A.DrawScale);
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
	local Inventory Item;
	local DeusExWeapon W;
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
				Log("DXVIEW: opening 01_NYC_UNATCOIsland");
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
			Item = P.Spawn(class'WeaponNanoSword');
			LogLook("spawned", Item);
			Log("DXVIEW: pickup query " $ P.Level.Game.PickupQuery(P, Item) $ " player health " $ P.Health $ " is player " $ P.bIsPlayer);
			// A single player's game gives the item itself, a net game a
			// copy: only a copy leaves the spawned one to go.
			Item.Frob(P, None);
			W = DeusExWeapon(P.FindInventoryType(class'WeaponNanoSword'));
			if (W != Item)
				Item.Destroy();
			if (W == None)
			{
				Log("DXVIEW: no sword");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			DXP.PutInHand(W);
			// The original's HUD draws over the grabber's mark.
			if (DeusExRootWindow(DXP.rootWindow) != None)
				DeusExRootWindow(DXP.rootWindow).ShowHud(False);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Up after 4 s; a shot every second from then, the mark shown for
		// the 0.6 s before each.
		if (StepTime > 4.0 && ShotsTaken < 3)
		{
			if (!bMarking)
			{
				MarkShot = ShotsTaken;
				bMarking = true;
				StepTime = 3.4;
			}
			else
			{
				W = DeusExWeapon(P.FindInventoryType(class'WeaponNanoSword'));
				LogLook("in hand, shot " $ ShotsTaken, W);
				P.ConsoleCommand("shot");
				bMarking = false;
				ShotsTaken++;
				StepTime = 3.6;
			}
		}
		else if (ShotsTaken >= 3 && StepTime > 4.5)
		{
			Log("DXVIEW: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
