//=============================================================================
// LoadMarker: counts its PostPostBeginPlay calls. Spawned, it has one; the
// original's level load calls it again on every level it loads, a save's
// or one returned to (dx-reverse-info engine-dll.md, a level's tick), so
// SaveConsole's marker comes back from LoadConsole's load with two, and
// ReturnConsole's from the return to Liberty Island with two.
//=============================================================================
class LoadMarker extends Info;

var int Begun;

event PostPostBeginPlay()
{
	Begun++;
}
