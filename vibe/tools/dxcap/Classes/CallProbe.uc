//=============================================================================
// CallProbe: CallConsole's actor -- singular functions that call themselves,
// each other, a function that is not singular, another probe's, and that
// make the other probe touch this one; each line logged with "DXCALL:".
//=============================================================================
class CallProbe extends Actor;

var CallProbe Other;

singular function Outer(int n)
{
	local int x;

	Log("DXCALL: " $ Name $ ".Outer " $ n);
	if (n > 0)
		Outer(n - 1);
	Inner();
	Plain();
	Other.Inner();
	x = 5;
	x = Answer();
	Log("DXCALL: " $ Name $ ".Outer " $ n $ ": Answer() into 5 leaves " $ x);
	if (Answer() == 0)
		Log("DXCALL: " $ Name $ ".Outer " $ n $ ": Answer() tests as 0");
	if (n == 1)
		Other.PassThrough(self);
	Log("DXCALL: " $ Name $ ".Outer " $ n $ " ends");
}

singular function Inner()
{
	Log("DXCALL: " $ Name $ ".Inner");
}

function Plain()
{
	Log("DXCALL: " $ Name $ ".Plain");
	Inner();
}

singular function int Answer()
{
	return 7;
}

singular function Touch(Actor A)
{
	Log("DXCALL: " $ Name $ ".Touch by " $ A.Name);
}

// Through the other probe and back, touching it
function PassThrough(CallProbe Probe)
{
	local vector Start;

	Start = Location;
	SetLocation(Probe.Location);
	SetLocation(Start);
}

function Recurse(int n)
{
	if (n > 0)
		Recurse(n - 1);
}

defaultproperties
{
	bHidden=True
	CollisionRadius=20.000000
	CollisionHeight=20.000000
	bCollideActors=True
}
