//=============================================================================
// LadderConsole: a player on a ladder -- logged with "DXLADDER:" and the
// console's clock (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, datalinks and conversations silenced every tick, the
// first ladder found from the level's navigation points: from each, lines
// 200 long in 8 directions at 20 under and over it, the first surface each
// meets read with TraceTexture until one is in a Ladder group. The player is
// put 6 units out from that face, facing it, and let fall to its feet; then,
// each phase logged every tick -- its height, velocity, acceleration,
// physics, base and bIsWalking:
//   a. standing 1 s;
//   b. the forward axis held 1 s, looking level: it climbs;
//   c. let go 2 s: it hangs;
//   d. the forward axis held 2 s, looking 45 degrees down;
//   e. let go 2 s;
//   f. the forward axis held 4 s, looking level: up to the top and onto it.
// The axis is set every tick to 120000 (SwimConsole's), so either engine's
// input cuts the acceleration to the player's; then an exit.
//=============================================================================
class LadderConsole extends Console;

var float CapTime, MapTime, StepTime;
var string CurrentMap, StepName;
var int Phase, Ticks;
var vector Face, FaceNormal;
var rotator Facing;

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

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None)
		DXP.conPlay.TerminateConversation();
}

// The first ladder face a line from a navigation point meets.
function bool FindLadder(PlayerPawn P)
{
	local NavigationPoint N;
	local Actor A;
	local name TexName, TexGroup;
	local int Flags, d, h;
	local vector HitLoc, HitNorm, From;
	local rotator R;

	foreach P.AllActors(class'NavigationPoint', N)
	{
		for (d = 0; d < 8; d++)
		{
			R.Yaw = d * 8192;
			for (h = 0; h < 2; h++)
			{
				From = N.Location + vect(0,0,1) * (h * 40 - 20);
				foreach P.TraceTexture(class'Actor', A, TexName, TexGroup, Flags, HitLoc, HitNorm, From + 200 * vector(R), From)
				{
					if (TexGroup == 'Ladder')
					{
						Face = HitLoc;
						FaceNormal = HitNorm;
						Log("DXLADDER: " $ TexName $ " (" $ TexGroup $ ") at " $ HitLoc $ " normal " $ HitNorm $ ", from " $ N.Name $ " at " $ N.Location);
						return true;
					}
					break;
				}
			}
		}
	}
	return false;
}

function LogPlayer(PlayerPawn P)
{
	Log("DXLADDER: " $ StepName $ " tick " $ Ticks $ " t " $ StepTime $ " z " $ P.Location.Z $ " out " $ ((P.Location - Face) dot FaceNormal)
	    $ " velocity " $ P.Velocity $ " accel " $ P.Acceleration $ " physics " $ int(P.Physics) $ " base " $ P.Base $ " walking " $ P.bIsWalking);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector Where;
	local rotator R;
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
	if (DXP != None)
		Silence(DXP);

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
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
			if (!FindLadder(P))
			{
				Log("DXLADDER: no ladder found, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			Facing = rotator(-FaceNormal);
			Facing.Pitch = 0;
			Facing.Roll = 0;
			Where = Face + FaceNormal * (P.CollisionRadius + 6);
			Where.Z += P.CollisionHeight;
			if (!P.SetLocation(Where))
				Log("DXLADDER: cannot put the player at " $ Where);
			P.SetRotation(Facing);
			P.ViewRotation = Facing;
			P.Velocity = vect(0,0,0);
			P.Acceleration = vect(0,0,0);
			P.SetPhysics(PHYS_Falling);
			Log("DXLADDER: the player at " $ P.Location $ " facing " $ Facing $ ", ground speed " $ P.GroundSpeed $ " air speed " $ P.AirSpeed
			    $ " accel rate " $ P.AccelRate);
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Let fall to its feet a second
		if (StepTime > 1.0)
		{
			StepName = "a";
			Ticks = 0;
			StepTime = 0;
			Phase = 3;
		}
	}
	else if (Phase >= 3 && Phase <= 8)
	{
		if (Phase == 4 || Phase == 6 || Phase == 8)
			P.aBaseY = 120000.0;
		else
			P.aBaseY = 0.0;
		Ticks++;
		LogPlayer(P);
		if ((Phase == 3 || Phase == 4) && StepTime > 1.0 || (Phase == 5 || Phase == 6 || Phase == 7) && StepTime > 2.0 || Phase == 8 && StepTime > 4.0)
		{
			Phase++;
			Ticks = 0;
			StepTime = 0;
			R = Facing;
			if (Phase == 4)
				StepName = "b";
			else if (Phase == 5)
				StepName = "c";
			else if (Phase == 6)
			{
				StepName = "d";
				R.Pitch = -8192;
			}
			else if (Phase == 7)
			{
				StepName = "e";
				R.Pitch = -8192;
			}
			else if (Phase == 8)
				StepName = "f";
			else
			{
				P.aBaseY = 0.0;
				Log("DXLADDER: done at " $ CapTime $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			P.ViewRotation = R;
		}
	}
}
