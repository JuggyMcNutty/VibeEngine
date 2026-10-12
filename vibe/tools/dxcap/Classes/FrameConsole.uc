//=============================================================================
// FrameConsole: a long frame, logged with "DXFRAME:" from the menu map -- 2 s
// in, one frame held 2 s more by the console's own line traces, timed by
// clock and unclock in short spans (which the original's 32-bit cycle count
// allows) and under its 10,000,000 jumps a loop; then the level's time over
// that frame and the longest of an actor's next three ticks, and an exit.
// The level's clock takes the whole frame, the actors at most 0.4 s of it
// (vibe/docs/NATIVES.md, small).
//=============================================================================
class FrameConsole extends Console;

var float RunTime, LevelBefore;
var int Phase;
var FrameWatcher Watcher;

event Tick(float Delta)
{
	local PlayerPawn P;
	local vector HitLocation, HitNormal;
	local float Stalled;
	local int c, i, Spans;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	RunTime += Delta;
	if (Phase == 0 && RunTime > 2.0)
	{
		Watcher = P.Spawn(class'FrameWatcher', P);
		LevelBefore = P.Level.TimeSeconds;
		while (Stalled < 2.0 && Spans < 4000)
		{
			c = 0;
			clock(c);
			for (i = 0; i < 1000; i++)
			{
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
				P.Trace(HitLocation, HitNormal, P.Location + vect(0,0,-2000), P.Location, false);
			}
			unclock(c);
			Stalled += CyclesToSeconds(c);
			Spans++;
		}
		Log("DXFRAME: a frame held over 2 s: " $ (Stalled >= 2.0));
		Watcher.TicksLeft = 3;
		Phase = 1;
	}
	else if (Phase == 1)
	{
		Log("DXFRAME: the level's time over it over 1.5 s: " $ (P.Level.TimeSeconds - LevelBefore > 1.5));
		Phase = 2;
	}
	else if (Phase == 2 && Watcher.TicksLeft == 0)
	{
		Log("DXFRAME: the longest of the actor's next three ticks " $ Watcher.Longest);
		Log("DXFRAME: done");
		P.ConsoleCommand("exit");
		Phase = 3;
	}
}
