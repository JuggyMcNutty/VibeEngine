//=============================================================================
// RotatorConsole: vector-to-rotator conversions, logged with "DXROT:"
// (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// From wherever the run starts, 2 s in: rotator(v) for a fixed set of
// vectors -- the axes and their diagonals, every combination of -3..3 on
// each axis, directions every 15 degrees of yaw and pitch, and vectors close
// to where an angle crosses a whole unit -- each logged as its parts read as
// ints and as the rotator's string. Then vector(r), GetAxes and GetUnAxes
// for a few rotators, and an exit. What is compared between the engines:
// every line, exactly.
//=============================================================================
class RotatorConsole extends Console;

var float RunTime;
var bool bDone;

function LogOne(vector V)
{
	local rotator R;
	R = rotator(V);
	Log("DXROT: " $ V.X $ "," $ V.Y $ "," $ V.Z $ " -> " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " = " $ string(R));
}

function LogBack(rotator R)
{
	local vector V, X, Y, Z;
	V = vector(R);
	Log("DXROT: back " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ V.X $ "," $ V.Y $ "," $ V.Z);
	GetAxes(R, X, Y, Z);
	Log("DXROT: axes " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ X $ " " $ Y $ " " $ Z);
	GetUnAxes(R, X, Y, Z);
	Log("DXROT: unaxes " $ R.Pitch $ " " $ R.Yaw $ " " $ R.Roll $ " -> " $ X $ " " $ Y $ " " $ Z);
}

function Probes()
{
	local int x, y, z, i, j;
	local vector V;
	local rotator R;

	for (x = -3; x <= 3; x++)
		for (y = -3; y <= 3; y++)
			for (z = -3; z <= 3; z++)
			{
				V.X = x;
				V.Y = y;
				V.Z = z;
				LogOne(V);
			}
	for (i = -12; i <= 12; i++)
		for (j = -6; j <= 6; j++)
		{
			R.Yaw = i * 2730;
			R.Pitch = j * 2730;
			R.Roll = 0;
			LogOne(vector(R) * 100);
		}
	// Near whole units: an angle a hair either side of 1, 2 and 3 units.
	for (i = 1; i <= 3; i++)
	{
		V.X = 1000;
		V.Y = 1000 * Tan(i * 2 * Pi / 65536.0) * 0.999;
		V.Z = 0;
		LogOne(V);
		V.Y = 1000 * Tan(i * 2 * Pi / 65536.0) * 1.001;
		LogOne(V);
		V.Y = -V.Y;
		LogOne(V);
	}
	LogBack(rot(0, 0, 0));
	LogBack(rot(-5691, 20051, 0));
	LogBack(rot(59845, 20051, 0));
	LogBack(rot(16384, 0, 0));
	LogBack(rot(-16384, 32768, 0));
	LogBack(rot(70000, -70000, 1234));
	LogBack(rot(1000, 2001, 3003));
	LogBack(rot(-12345, 23456, -4567));
}

event Tick(float Delta)
{
	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	RunTime += Delta;
	if (RunTime > 2.0)
	{
		Probes();
		Log("DXROT: done, exiting");
		Viewport.Actor.ConsoleCommand("exit");
		bDone = true;
	}
}
