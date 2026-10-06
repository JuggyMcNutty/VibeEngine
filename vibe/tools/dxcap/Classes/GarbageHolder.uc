//=============================================================================
// GarbageHolder: the live actor holding GarbageConsole's references to its
// markers -- a static array, a single reference, one inside a struct, and one
// to the marker that is never destroyed --, the references a level clears
// when it frees destroyed actors (dx-reverse-info engine-dll.md, destroyed
// actors). The console reads only these, never a destroyed marker.
//=============================================================================
class GarbageHolder extends Info;

struct GarbagePair
{
	var GarbageMarker A;
	var int N;
};

var GarbageMarker Held[128];
var GarbageMarker First;
var GarbagePair Pair;
var GarbageMarker Kept;

// The console reaches the array only through these: a context expression is
// limited to 255 bytes.
function SetHeld(int i, GarbageMarker M)
{
	Held[i] = M;
}

function GarbageMarker GetHeld(int i)
{
	return Held[i];
}

function int CountHeld()
{
	local int i, n;
	for (i = 0; i < 128; i++)
	{
		if (Held[i] != None)
			n++;
	}
	return n;
}
