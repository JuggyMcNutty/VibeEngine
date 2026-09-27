//=============================================================================
// TravelJoinConsole: the joining side of a travel test -- from the menu map
// it opens 127.0.0.1:7790 (TravelServeConsole's server) and logs each second
// the map it is in, its net mode and where it stands; once it is a client in
// a map other than the first it joined, it logs that, shoots 8 s later and
// exits -- or at 150 s, however far it got.
//=============================================================================
class TravelJoinConsole extends Console;

var float RunTime, LogTime, SecondTime;
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
