//=============================================================================
// RejoinConsole: a client that joins twice -- from the menu map it opens
// 127.0.0.1:7790 and, 6 s into the game, disconnects to the menu map; 30 s
// later it opens the address again (by then the harness has another server
// there) and logs whether that join comes in -- 6 s into its game, or never
// after 40 s --; then an exit.
//=============================================================================
class RejoinConsole extends Console;

var float RunTime, GameTime, StepTime;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (P.Level.NetMode == NM_Client)
		GameTime += Delta;
	else
		GameTime = 0;

	if (Step == 0 && RunTime > 3.0)
	{
		Log("DXREJOIN: opening 127.0.0.1:7790");
		P.ConsoleCommand("open 127.0.0.1:7790");
		Step = 1;
		StepTime = RunTime;
	}
	else if (Step == 1 && GameTime > 6.0)
	{
		Log("DXREJOIN: in the first server's game; disconnecting");
		P.ConsoleCommand("disconnect");
		Step = 2;
		StepTime = RunTime;
	}
	else if (Step == 1 && RunTime - StepTime > 40.0)
	{
		Log("DXREJOIN: never in the first server's game; exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
	else if (Step == 2 && RunTime - StepTime > 30.0)
	{
		Log("DXREJOIN: opening 127.0.0.1:7790 again");
		P.ConsoleCommand("open 127.0.0.1:7790");
		Step = 3;
		StepTime = RunTime;
	}
	else if (Step == 3 && GameTime > 6.0)
	{
		Log("DXREJOIN: in the second server's game; exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
	else if (Step == 3 && RunTime - StepTime > 40.0)
	{
		Log("DXREJOIN: never in the second server's game; exiting");
		P.ConsoleCommand("exit");
		Step = 9;
	}
}
