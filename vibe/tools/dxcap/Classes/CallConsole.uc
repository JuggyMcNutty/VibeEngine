//=============================================================================
// CallConsole: singular functions and nested calls, logged with "DXCALL:"
// from the menu map. 2 s in, two CallProbes 300 units over the player: the
// first's singular Outer, which calls itself, its own singular functions
// directly and through one that is not, the second's, a singular one's value
// into a variable, and moves the second through the first, which touches it;
// then the first's Inner again, and the second through the first again. A
// tick later, a script call chain the ini's Depth deep under the console's
// Tick ([DXCapture.CallConsole], 248 by default: 250 nested script calls
// with the Tick and Recurse's first); then an exit. A singular function does
// not run while any of its object's does (vibe/docs/NATIVES.md, small), and
// the 251st nested script call stops the game (stops the game).
//=============================================================================
class CallConsole extends Console;

var config int Depth;
var float RunTime;
var int Phase;

event Tick(float Delta)
{
	local PlayerPawn P;
	local CallProbe A, B;
	local int n;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		A = P.Spawn(class'CallProbe',,, P.Location + vect(0,0,300));
		B = P.Spawn(class'CallProbe',,, P.Location + vect(200,0,300));
		Log("DXCALL: spawned " $ (A != None) $ " " $ (B != None));
		if (A != None && B != None)
		{
			A.Other = B;
			B.Other = A;
			A.Outer(1);
			A.Inner();
			B.PassThrough(A);
			A.Destroy();
			B.Destroy();
		}
		Phase = 1;
	}
	else if (Phase == 1)
	{
		n = Depth;
		if (n == 0)
			n = 248;
		A = P.Spawn(class'CallProbe',,, P.Location + vect(0,0,300));
		Log("DXCALL: Recurse " $ n);
		A.Recurse(n);
		Log("DXCALL: Recurse " $ n $ " returned");
		A.Destroy();
		Phase = 2;
	}
	else if (Phase == 2)
	{
		Log("DXCALL: done");
		P.ConsoleCommand("exit");
		Phase = 3;
	}
}
