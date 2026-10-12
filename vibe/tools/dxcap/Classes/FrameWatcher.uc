//=============================================================================
// FrameWatcher: FrameConsole's actor; once armed, the longest of its next
// three ticks' times.
//=============================================================================
class FrameWatcher extends Info;

var int TicksLeft;
var float Longest;

event Tick(float DeltaTime)
{
	if (TicksLeft > 0)
	{
		Longest = FMax(Longest, DeltaTime);
		TicksLeft--;
	}
}
