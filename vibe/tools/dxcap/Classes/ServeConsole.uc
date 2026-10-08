//=============================================================================
// ServeConsole: a listen server for the other engine to join -- a
// deathmatch on DXMP_Cathedral (CapDeathMatch: without the stock console
// check), started from the menu map as the game's Host screen starts one,
// never on the master servers' lists (the run's ini has no uplink). While
// serving, once another player is in, the host's own player stands in its
// sight -- 300 units in front of it, or the nearest free spot round it --
// turned across its view, and walks 2 s, stands 2 s and turns about, over
// and over, for the client to watch; where each player stands, and its
// address (GetPlayerNetworkAddress), is logged every 2 s with the server's
// own (GetNetworkNumber); its own URL and address (GetLocalURL,
// GetAddressURL) as it starts serving. 12 s after it is placed the host
// stands still and plays 8 beeps 1 s apart from this console's own
// function, which is not simulated: a server's sound every player hears,
// the client through Pawn.ClientHearSound (a client run with
// DXCAP_RECORD=1, read with vibe/tools/dxcap/sound.py --onsets). Exits
// after 290 s.
//=============================================================================
class ServeConsole extends Console;

var float CapTime, LogTime, WalkTime, PlacedTime;
var int Phase, Beeps;
var bool bPlaced;

// The first free spot in the other player's line of sight: 300 units ahead
// of it, then nearer, then turning an eighth at a time.
function bool PlaceBefore(PlayerPawn P, PlayerPawn Other)
{
	local int Turn, Dist;
	local rotator R;
	local vector Spot;

	for (Turn = 0; Turn < 8; Turn++)
	{
		R = Other.Rotation;
		R.Pitch = 0;
		R.Yaw += Turn * 8192;
		for (Dist = 300; Dist >= 150; Dist -= 50)
		{
			Spot = Other.Location + Dist * vector(R);
			if (P.FastTrace(Spot, Other.Location) && P.SetLocation(Spot))
			{
				P.ViewRotation.Yaw = R.Yaw + 16384;
				P.SetRotation(P.ViewRotation);
				return true;
			}
		}
	}
	return false;
}

event Tick(float Delta)
{
	local PlayerPawn P, Other;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;

	if (Phase == 0 && CapTime > 3.0)
	{
		Log("DXCAP: starting the listen server");
		P.ConsoleCommand("start DXMP_Cathedral?game=DXCapture.CapDeathMatch?listen");
		Phase = 1;
	}
	else if (Phase == 1 && P.Level.NetMode != NM_Standalone)
	{
		Log("DXCAP: serving " $ P.Level.GetLocalURL() $ ", address " $ P.Level.GetAddressURL() $ ", net mode " $ P.Level.NetMode);
		Phase = 2;
	}
	else if (Phase == 2)
	{
		if (!bPlaced)
		{
			foreach P.AllActors(class'PlayerPawn', Other)
			{
				if (Other != P && PlaceBefore(P, Other))
				{
					Log("DXCAP: host placed at " $ P.Location $ " before " $ Other.Location);
					bPlaced = true;
					PlacedTime = CapTime;
					WalkTime = 0.0;
					break;
				}
			}
		}
		if (bPlaced && Beeps < 8 && CapTime - PlacedTime >= 12.0)
		{
			// Still, so no footstep falls among the beeps.
			P.aBaseY = 0.0;
			if (CapTime - PlacedTime >= 12.0 + Beeps)
			{
				Log("DXCAP: beep " $ Beeps $ " at " $ CapTime);
				P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 4000, 1.0);
				Beeps++;
			}
		}
		else
		{
			WalkTime += Delta;
			if (WalkTime < 2.0)
				P.aBaseY = 300.0;
			else
				P.aBaseY = 0.0;
			if (WalkTime >= 4.0)
			{
				WalkTime = 0.0;
				P.ViewRotation.Yaw += 32768;
			}
		}
		if (CapTime - LogTime >= 2.0)
		{
			LogTime = CapTime;
			Log("DXCAP: network number '" $ P.Level.Game.GetNetworkNumber() $ "'");
			foreach P.AllActors(class'PlayerPawn', Other)
				Log("DXCAP: " $ Other.PlayerReplicationInfo.PlayerName $ " at " $ Other.Location $ " address '" $ Other.GetPlayerNetworkAddress() $ "'");
		}
	}

	if (Phase >= 1 && Phase < 9 && CapTime > 290.0)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 9;
	}
}
