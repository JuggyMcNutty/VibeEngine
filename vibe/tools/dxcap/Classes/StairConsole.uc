//=============================================================================
// StairConsole: Pawn.FindStairRotation, the native a walking player's
// PlayerMove calls to look up or down stairs (with bLookUpStairs, off in the
// game's User.ini), asked straight in both engines
// (vibe/docs/DEVELOPMENT.md, scripted runs). On Liberty Island, datalinks and
// conversations stopped, the player out of the world is stood on the floor
// under points 20 units apart: from the start to 90 past the pier's edge
// (SwimConsole's), facing the edge and away from it, and then 200 either
// side of SlideConsole's slope, a rise of one in two, facing up it and down
// it. At each, from pitches 0, 3000 and -3000, it is asked with frames of
// 0.05 and 0.2 s, and once with 0.4, past the native's gate: each answer
// logged with "DXSTAIR:"; then an exit.
//=============================================================================
class StairConsole extends Console;

var float MapTime;
var int Phase, Point;
var vector Start;
var Object Stopped;

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		DXP.conPlay.TerminateConversation();
	}
}

function Ask(PlayerPawn P, int Yaw, int Pitch, float Frame)
{
	local rotator R;

	R.Yaw = Yaw;
	R.Pitch = Pitch;
	P.ViewRotation = R;
	Log("DXSTAIR: point " $ Point $ " yaw " $ Yaw $ " pitch " $ Pitch $ " frame " $ Frame $ " -> " $ P.FindStairRotation(Frame)
		$ " (view pitch then " $ P.ViewRotation.Pitch $ ")");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local vector At, HitLocation, HitNormal;
	local int Pitches[3], i;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	MapTime += Delta;
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
				Log("DXSTAIR: opening 01_NYC_UNATCOIsland");
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
			Start = P.Location;
			Log("DXSTAIR: the player at " $ Start $ " radius " $ P.CollisionRadius $ " height " $ P.CollisionHeight
				$ " eye " $ P.BaseEyeHeight);
			P.SetPhysics(PHYS_None);
			P.SetCollision(false, false, false);
			P.bCollideWorld = false;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// The pier's edge is at y 10000, the start some 430 north of it;
		// the slope rises toward -y from its middle at y 6686.67.
		At = Start;
		At.Y = 10430 - 20 * Point;
		if (Point > 26)
		{
			At.X = -2356.164;
			At.Y = 6886.67 - 20 * (Point - 27);
			At.Z = -174.84;
		}
		if (P.Trace(HitLocation, HitNormal, At - vect(0,0,400), At + vect(0,0,200), false) == None)
			Log("DXSTAIR: point " $ Point $ " no floor under " $ At);
		else
		{
			P.SetLocation(HitLocation + vect(0,0,1) * P.CollisionHeight);
			P.Velocity = vect(0,0,0);
			Log("DXSTAIR: point " $ Point $ " at " $ P.Location $ " floor " $ HitLocation.Z);
			Pitches[0] = 0;
			Pitches[1] = 3000;
			Pitches[2] = -3000 & 65535;
			for (i = 0; i < 3; i++)
			{
				Ask(P, 49152, Pitches[i], 0.05);
				Ask(P, 49152, Pitches[i], 0.2);
				Ask(P, 16384, Pitches[i], 0.05);
				Ask(P, 16384, Pitches[i], 0.2);
			}
			Ask(P, 49152, 3000, 0.4);
		}
		Point++;
		if (Point > 47)
		{
			Log("DXSTAIR: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 9;
		}
	}
}
