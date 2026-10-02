//=============================================================================
// LaserConsole: one laser tripwire's beam, looked into -- Liberty Island
// opened from wherever the run starts, then 6 s in the player stood where
// CaptureConsole shoots the first LaserTrigger from, the HUD hidden; each
// second the trigger's, its emitter's and the emitter's proxy's state logged
// (on, hidden, frozen, draw type and style, textures, render interface, the
// spot; the proxy's mesh and lighting); shots 2 s and 4 s after standing,
// a third 6 s after with the proxy's skin drawn large on the canvas over
// black, plain and translucent, so the texture is seen apart from the beam,
// and a fourth 8 s after with the proxy unlit, its mesh drawn apart from its
// lighting -- each marked for the original's grabber; exits 2 s later.
//=============================================================================
class LaserConsole extends Console;

var float MapTime, LogTime;
var int Step;
var LaserTrigger LT;
var string CurrentMap;

// The map of the level's URL: past the last '/', before '.' or '?'.
function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function LogLaser()
{
	local LaserEmitter E;

	if (LT == None)
	{
		Log("DXLASER: no LaserTrigger");
		return;
	}
	E = LT.emitter;
	Log("DXLASER: trigger " $ string(LT) $ " on " $ LT.bIsOn $ " at " $ LT.Location $ " turned " $ LT.Rotation $ " emitter " $ string(E));
	if (E == None)
		return;
	Log("DXLASER: emitter on " $ E.bIsOn $ " hidden " $ E.bHidden $ " frozen " $ E.bFrozen $ " hiddenbeam " $ E.bHiddenBeam $ " drawtype " $ E.DrawType $ " style " $ E.Style $ " texture " $ string(E.Texture) $ " skin " $ string(E.Skin) $ " iterator class " $ string(E.RenderIteratorClass) $ " interface " $ string(E.RenderInterface) $ " spot " $ string(E.spot[0]));
	if (E.proxy != None)
	{
		Log("DXLASER: proxy " $ string(E.proxy) $ " hidden " $ E.proxy.bHidden $ " drawtype " $ E.proxy.DrawType $ " style " $ E.proxy.Style $ " texture " $ string(E.proxy.Texture) $ " skin " $ string(E.proxy.Skin) $ " scale " $ E.proxy.DrawScale $ " at " $ E.proxy.Location $ " distance " $ E.proxy.DistanceFromPlayer $ " iterator class " $ string(E.proxy.RenderIteratorClass));
		Log("DXLASER: proxy look mesh " $ string(E.proxy.Mesh) $ " rotation " $ E.proxy.Rotation $ " glow " $ E.proxy.ScaleGlow $ " ambient " $ E.proxy.AmbientGlow $ " unlit " $ E.proxy.bUnlit $ " fatness " $ E.proxy.Fatness $ " lighttype " $ E.proxy.LightType);
		if (E.proxy.Skin != None)
			Log("DXLASER: proxy skin " $ string(E.proxy.Skin) $ " size " $ E.proxy.Skin.USize $ "x" $ E.proxy.Skin.VSize $ " maxcolor " $ E.proxy.Skin.MaxColor.R $ "," $ E.proxy.Skin.MaxColor.G $ "," $ E.proxy.Skin.MaxColor.B);
	}
	if (E.spot[0] != None)
		Log("DXLASER: spot at " $ E.spot[0].Location $ " hidden " $ E.spot[0].bHidden $ " drawtype " $ E.spot[0].DrawType $ " texture " $ string(E.spot[0].Texture));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector Mid, Hit;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXLASER: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
	}
	else if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;
	else if (Step == 0 && MapTime > 6.0)
	{
		foreach P.AllActors(class'LaserTrigger', LT)
			break;
		if (LT != None)
		{
			Hit = LT.Location + vector(LT.Rotation) * 256;
			if (LT.emitter != None && LT.emitter.spot[0] != None)
				Hit = LT.emitter.spot[0].Location;
			Mid = (LT.Location + Hit) * 0.5;
			P.SetLocation(vect(1920.30, -878.64, 2.35));
			P.ViewRotation = rotator(Mid - (P.Location + P.EyeHeight * vect(0,0,1)));
			P.SetRotation(P.ViewRotation);
			Log("DXLASER: standing at " $ P.Location $ " looking at " $ Mid);
		}
		if (DeusExPlayer(P) != None)
			DeusExPlayer(P).ShowHud(False);
		Step = 1;
	}
	if (Step >= 1 && MapTime - LogTime >= 1.0)
	{
		LogTime = MapTime;
		LogLaser();
	}
	if (Step == 4 && MapTime > 13.0 && LT != None && LT.emitter != None && LT.emitter.proxy != None && !LT.emitter.proxy.bUnlit)
	{
		LT.emitter.proxy.bUnlit = True;
		Log("DXLASER: proxy unlit");
	}
	if ((Step == 1 && MapTime > 8.0) || (Step == 2 && MapTime > 10.0) || (Step == 3 && MapTime > 12.0) || (Step == 4 && MapTime > 14.0))
	{
		Log("DXCAP: shot " $ Step);
		P.ConsoleCommand("shot");
		Step++;
	}
	else if (Step == 5 && MapTime > 16.0)
	{
		Log("DXLASER: exiting");
		P.ConsoleCommand("exit");
		Step = 6;
	}
}

// The proxy's skin, 8 times its size, over black: plain at the top, then
// translucent, as the beam draws it.
function DrawSwatch(canvas C, Texture T)
{
	C.DrawColor.R = 0;
	C.DrawColor.G = 0;
	C.DrawColor.B = 0;
	C.Style = 1;
	C.SetPos(96, 96);
	C.DrawRect(Texture'Solid', T.USize * 8 + 32, T.VSize * 16 + 48);
	C.DrawColor.R = 255;
	C.DrawColor.G = 255;
	C.DrawColor.B = 255;
	C.SetPos(112, 112);
	C.DrawTile(T, T.USize * 8, T.VSize * 8, 0, 0, T.USize, T.VSize);
	C.Style = 3;
	C.SetPos(112, 128 + T.VSize * 8);
	C.DrawTile(T, T.USize * 8, T.VSize * 8, 0, 0, T.USize, T.VSize);
}

// The original's grabber keeps the frames marked as CaptureConsole marks
// them: a magenta block, then the shot's number in eight blocks.
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (Step < 1 || Step > 4)
		return;
	if (Step == 3 && LT != None && LT.emitter != None && LT.emitter.proxy != None && LT.emitter.proxy.Skin != None)
		DrawSwatch(C, LT.emitter.proxy.Skin);
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((Step & (1 << i)) != 0)
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
