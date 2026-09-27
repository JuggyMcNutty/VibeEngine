//=============================================================================
// NetConsole: the script's sockets against the world -- a few of
// InternetLink's conversions and GameSpy answers logged, a master server
// asked for Deus Ex's servers as the game's Join Internet screen asks
// (NetMasterLink), and the first five of them asked for their status
// (NetPingLink); then the game's own Join Internet screen opened. Runs on
// the menu map; exits after 40 s.
//=============================================================================
class NetConsole extends Console;

var float CapTime;
var int Phase;

function Checks(PlayerPawn P)
{
	local NetPingLink L;

	L = P.Spawn(class'NetPingLink');
	L.SelfCheck();
	L.Destroy();
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local NetMasterLink Master;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	CapTime += Delta;

	if (Phase == 0 && CapTime > 2.0)
	{
		Checks(P);
		Master = P.Spawn(class'NetMasterLink');
		Master.Start("master.333networks.com", 28900, "deusex");
		Phase = 1;
	}
	else if (Phase == 1 && CapTime > 20.0)
	{
		// The game's own screen, which asks the ini's master server on
		// opening; its links log what they find.
		Log("DXCAP: opening the Join Internet screen");
		DeusExRootWindow(DeusExPlayer(P).rootWindow).InvokeMenu(class'MenuScreenJoinInternet');
		Phase = 2;
	}
	else if (Phase == 2 && CapTime > 38.0)
	{
		P.ConsoleCommand("shot");
		Phase = 3;
	}
	else if (Phase == 3 && CapTime > 40.0)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 4;
	}
}
