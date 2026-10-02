//=============================================================================
// MidConsole: Object.Mid at its edges, logged with "DXMID:" from the menu map
// and an exit -- a negative start, a negative count that takes the end below
// the start or below 0, a count past the end, and the default count
// (vibe/docs/DEVELOPMENT.md, scripted runs).
//=============================================================================
class MidConsole extends Console;

var float RunTime;
var bool bDone;

function Probe(string S, int i, int j)
{
	Log("DXMID: Mid(\"" $ S $ "\", " $ i $ ", " $ j $ ") = \"" $ Mid(S, i, j) $ "\"");
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None || bDone)
		return;
	RunTime += Delta;
	if (RunTime < 2.0)
		return;
	Probe("hello", 1, 3);
	Probe("hello", -1, 3);
	Probe("hello", -2, -1);
	Probe("hello", 2, -1);
	Probe("hello", 2, -5);
	Probe("hello", 0, -10);
	Probe("hello", 3, 10);
	Probe("hello", 5, 2);
	Probe("hello", 9, 2);
	Log("DXMID: Mid(\"hello\", 2) = \"" $ Mid("hello", 2) $ "\"");
	Log("DXMID: done");
	bDone = true;
	Viewport.Actor.ConsoleCommand("exit");
}
