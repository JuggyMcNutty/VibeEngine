//=============================================================================
// ReachConsole: the reachability tests, asked from script in both engines
// (vibe/docs/DEVELOPMENT.md, scripted runs). At Liberty Island's start, for
// the first 12 ScriptedPawns by name: AIDirectionReachable along four yaws
// (a range of 150 to 500 units from the pawn), PointReachable to a spot 300
// units along each, and ActorReachable to the player and to the nearest
// three navigation points. Each answer is logged with "DXREACH:"; then exit.
//=============================================================================
class ReachConsole extends Console;

var float MapTime;
var int Phase;

function string V(vector X)
{
	return int(X.X) $ "," $ int(X.Y) $ "," $ int(X.Z);
}

function Probe(PlayerPawn P, ScriptedPawn S)
{
	local int i, yaw;
	local vector Dest, Spot;
	local bool bFound;
	local rotator R;
	local NavigationPoint N, Nearest[3];
	local float Dist[3], D;
	local int k, j;

	for (i = 0; i < 4; i++)
	{
		yaw = i * 16384;
		bFound = S.AIDirectionReachable(S.Location, yaw, 0, 150, 500, Dest);
		Log("DXREACH: " $ S.Name $ " direction " $ yaw $ " found " $ bFound $ " at " $ V(Dest));
		R.Yaw = yaw;
		Spot = S.Location + vector(R) * 300;
		Log("DXREACH: " $ S.Name $ " point " $ yaw $ " reachable " $ S.PointReachable(Spot));
	}
	Log("DXREACH: " $ S.Name $ " player reachable " $ S.ActorReachable(P));

	for (k = 0; k < 3; k++)
		Dist[k] = 1000000;
	foreach P.AllActors(class'NavigationPoint', N)
	{
		D = VSize(N.Location - S.Location);
		for (k = 0; k < 3; k++)
		{
			if (D < Dist[k])
			{
				for (j = 2; j > k; j--)
				{
					Dist[j] = Dist[j - 1];
					Nearest[j] = Nearest[j - 1];
				}
				Dist[k] = D;
				Nearest[k] = N;
				break;
			}
		}
	}
	for (k = 0; k < 3; k++)
		if (Nearest[k] != None)
			Log("DXREACH: " $ S.Name $ " node " $ Nearest[k].Name $ " reachable " $ S.ActorReachable(Nearest[k]));
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local ScriptedPawn S, Picked[12], T;
	local int Count, i, j;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;

	if (Phase == 0)
	{
		if (InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") < 0)
		{
			if (MapTime > 3.0)
			{
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
	else if (Phase == 1 && InStr(Caps(P.Level.GetLocalURL()), "01_NYC_UNATCOISLAND") >= 0 && MapTime > 1.0)
	{
		// The first 12 by name, sorted here so both engines ask the same pawns.
		foreach P.AllActors(class'ScriptedPawn', S)
		{
			if (Count < 12)
				Picked[Count++] = S;
			else if (Caps(string(S.Name)) < Caps(string(Picked[11].Name)))
				Picked[11] = S;
			for (i = Count - 1; i > 0; i--)
			{
				if (Caps(string(Picked[i].Name)) < Caps(string(Picked[i - 1].Name)))
				{
					T = Picked[i];
					Picked[i] = Picked[i - 1];
					Picked[i - 1] = T;
				}
			}
		}
		for (j = 0; j < Count; j++)
			Probe(P, Picked[j]);
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
