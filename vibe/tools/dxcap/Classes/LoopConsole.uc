//=============================================================================
// LoopConsole: a long loop, logged with "DXLOOP:" from the menu map -- 2 s
// in, one loop of the ini's Jumps jumps in a tick ([DXCapture.LoopConsole],
// 9,990,000 by default), then a line, and an exit. The original stops the
// game past 10,000,000 jumps in a tick (vibe/docs/NATIVES.md, stops the
// game), the rest of the tick's script taking some of them.
//=============================================================================
class LoopConsole extends Console;

var config int Jumps;
var float RunTime;
var int Phase;

event Tick(float Delta)
{
	local int i, n;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		n = Jumps;
		if (n == 0)
			n = 9990000;
		n = n / 2; // a pass is two jumps: its test and the jump back
		Log("DXLOOP: a loop of " $ (2 * n + 1) $ " jumps");
		for (i = 0; i < n; i++)
		{
		}
		Log("DXLOOP: the loop ran out");
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Log("DXLOOP: done");
		Viewport.Actor.ConsoleCommand("exit");
		Phase = 2;
	}
}
