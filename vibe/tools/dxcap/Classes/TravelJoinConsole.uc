//=============================================================================
// TravelJoinConsole: the joining side of a travel test -- from the menu map
// it opens 127.0.0.1:7790 (TravelServeConsole's server) and logs each second
// the map it is in, its net mode and where it stands; once it is a client in
// a map other than the first it joined, it logs that, shoots 8 s later and
// exits -- or at 150 s, however far it got. Out of the game 2 s or more once
// in it (a travel's Entry level lasts a moment), it takes the server as
// lost: a shot 4 s in, marked for the original's grabber, and an exit.
//=============================================================================
class TravelJoinConsole extends Console;

var float RunTime, LogTime, SecondTime, LostTime;
var int Step;
var string FirstMap;

// The map of the level's URL: what follows the last '/', up to the options.
function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	return U;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Map;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;

	if (Step == 0 && RunTime > 3.0)
	{
		Log("DXNET: opening 127.0.0.1:7790");
		P.ConsoleCommand("open 127.0.0.1:7790");
		Step = 1;
	}
	if (RunTime - LogTime >= 1.0)
	{
		LogTime = RunTime;
		Log("DXNET: t=" $ int(RunTime) $ " in " $ MapOf(P) $ " net mode " $ P.Level.NetMode $ " at " $ P.Location);
	}
	if (P.Level.NetMode != NM_Client && FirstMap != "" && Step < 2)
	{
		if (LostTime == 0)
			LostTime = RunTime;
		else if (Step == 1 && RunTime - LostTime > 2.0)
		{
			Log("DXNET: the server lost, in " $ MapOf(P));
			Step = 4;
		}
	}
	else if (Step < 2)
		LostTime = 0;
	if (Step == 4 && RunTime - LostTime > 4.0)
	{
		Log("DXCAP: shot 1 in " $ MapOf(P));
		P.ConsoleCommand("shot");
		Step = 5;
	}
	else if (Step == 5 && RunTime - LostTime > 5.0)
	{
		Log("DXNET: exiting in " $ MapOf(P));
		P.ConsoleCommand("exit");
		Step = 3;
	}
	if (P.Level.NetMode == NM_Client)
	{
		Map = MapOf(P);
		if (FirstMap == "")
		{
			FirstMap = Map;
			Log("DXNET: joined " $ Map);
		}
		else if (Map != FirstMap && Step < 2)
		{
			Log("DXNET: followed the server to " $ Map);
			SecondTime = RunTime;
			Step = 2;
		}
	}
	if (Step == 2 && RunTime - SecondTime > 8.0)
	{
		Log("DXNET: exiting in " $ MapOf(P) $ " at " $ P.Location);
		P.ConsoleCommand("shot");
		P.ConsoleCommand("exit");
		Step = 3;
	}
	else if (Step < 3 && RunTime > 150.0)
	{
		Log("DXNET: out of time, in " $ MapOf(P));
		P.ConsoleCommand("exit");
		Step = 3;
	}
}

// The lost server's frame marked as CaptureConsole marks its shots: a
// magenta block, then shot 1's number in eight blocks.
event PostRender(canvas C)
{
	local int i;

	Super.PostRender(C);
	if (Step != 4 || RunTime - LostTime < 3.0)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		C.DrawColor.R = 0;
		C.DrawColor.G = 0;
		C.DrawColor.B = 0;
		if (i == 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}
