//=============================================================================
// ProveConsole: a proving run -- the map given on the command line (or the
// ini's), shots at 20 s and 60 s after it loads, a clean exit at 65 s.
//=============================================================================
class ProveConsole extends Console;

var float MapTime;
var int Step;

event Tick(float Delta)
{
	local PlayerPawn P;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	MapTime += Delta;
	if ((Step == 0 && MapTime > 20.0) || (Step == 1 && MapTime > 60.0))
	{
		Log("DXPROVE: shot " $ Step $ " in " $ P.Level.GetLocalURL() $ " at " $ P.Location);
		P.ConsoleCommand("shot");
		Step++;
	}
	else if (Step == 2 && MapTime > 65.0)
	{
		Log("DXPROVE: exiting");
		P.ConsoleCommand("exit");
		Step++;
	}
}
