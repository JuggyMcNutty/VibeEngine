//=============================================================================
// DecalConsole: the Display menu's Decals, as the renderer takes it. On
// Liberty Island the player is turned to the nearest wall; a bullet hole is
// put on it, then the client's Decals is set off (as the menu sets it) and a
// second one put on the same spot; then Decals is set on again. A drawn decal
// stamps its LastRenderedTime: each step logs both decals' stamps against
// the level's clock ("DXDECAL:"). Then an exit.
//=============================================================================
class DecalConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var Decal First, Second;
var vector WallPoint, WallNormal, Along;

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

// The player turned to the nearest wall in reach at eye height.
function bool FaceWall(PlayerPawn P)
{
	local int Yaw, BestYaw;
	local rotator R;
	local vector Eye, HitLocation, HitNormal, BestLocation, BestNormal;
	local Actor HitActor;
	local float Best;

	Eye = P.Location + vect(0,0,1) * P.EyeHeight;
	Best = 100000;
	for (Yaw = 0; Yaw < 65536; Yaw += 4096)
	{
		R.Yaw = Yaw;
		HitActor = P.Trace(HitLocation, HitNormal, Eye + 3000 * vector(R), Eye, false);
		if (HitActor == P.Level && VSize(HitLocation - Eye) < Best && Abs(HitNormal.Z) < 0.3)
		{
			Best = VSize(HitLocation - Eye);
			BestYaw = Yaw;
			BestLocation = HitLocation;
			BestNormal = HitNormal;
		}
	}
	if (Best > 99999)
		return false;
	R.Yaw = BestYaw;
	P.ViewRotation = R;
	P.SetRotation(R);
	WallPoint = BestLocation;
	WallNormal = BestNormal;
	Along = Normal(WallNormal cross vect(0,0,1));
	Log("DXDECAL: facing a wall " $ int(Best) $ " units off at yaw " $ BestYaw);
	return true;
}

function Decal PutDecal(PlayerPawn P, vector Offset)
{
	local Decal D;

	// As the game's weapons put one (DeusExWeapon.SpawnEffects): on the hit
	// point, turned along the wall's normal.
	D = P.Spawn(class'BulletHole', P.Level,, WallPoint + Offset, rotator(WallNormal));
	if (D == None)
		Log("DXDECAL: could not place a bullet hole");
	return D;
}

function LogStamps(PlayerPawn P, string Label)
{
	local string S;

	S = "DXDECAL: " $ Label $ ": Decals='" $ P.ConsoleCommand("get ini:Engine.Engine.ViewportManager Decals") $ "'";
	if (First != None)
		S = S $ ", first stamped " $ (P.Level.TimeSeconds - First.LastRenderedTime < 0.5);
	if (Second != None)
		S = S $ ", second stamped " $ (P.Level.TimeSeconds - Second.LastRenderedTime < 0.5);
	Log(S);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

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
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
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
			if (!FaceWall(P))
			{
				Log("DXDECAL: no wall in reach, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Decals True");
			First = PutDecal(P, vect(0,0,0));
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2 && StepTime > 1.5)
	{
		P.ViewRotation = rotator(-WallNormal);
		LogStamps(P, "on");
		P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Decals False");
		Second = PutDecal(P, vect(0,0,0));
		StepTime = 0;
		Phase = 3;
	}
	else if (Phase == 3 && StepTime > 1.5)
	{
		LogStamps(P, "off");
		P.ConsoleCommand("set ini:Engine.Engine.ViewportManager Decals True");
		StepTime = 0;
		Phase = 4;
	}
	else if (Phase == 4 && StepTime > 1.5)
	{
		LogStamps(P, "on again");
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
