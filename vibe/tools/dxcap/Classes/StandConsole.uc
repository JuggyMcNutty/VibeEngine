//=============================================================================
// StandConsole: where a walking player rests over the floor -- Liberty
// Island opened from wherever the run starts; 8 s in, the player at rest
// where the map starts it, then walked forward 2 s and let stand 3 s, then
// turned and walked 2 s more and let stand 3 s: at each rest its place, its
// collision height and the floor a line straight down finds are logged, and
// the gap between the cylinder's bottom and that floor; then an exit.
//=============================================================================
class StandConsole extends Console;

var float MapTime, StepTime;
var int Step;
var string CurrentMap;

function string MapOf(PlayerPawn P)
{
	local string U;
	local int i;

	U = P.Level.GetLocalURL();
	i = InStr(U, "/");
	while (i >= 0)
	{
		U = Mid(U, i + 1);
		i = InStr(U, "/");
	}
	i = InStr(U, ".");
	if (i >= 0)
		U = Left(U, i);
	i = InStr(U, "?");
	if (i >= 0)
		U = Left(U, i);
	return U;
}

function LogRest(PlayerPawn P, string Label)
{
	local vector HitLocation, HitNormal;
	local Actor A;

	A = P.Trace(HitLocation, HitNormal, P.Location - vect(0,0,300), P.Location, false);
	Log("DXSTAND: " $ Label $ " at " $ P.Location $ " height " $ P.CollisionHeight $ " physics " $ P.Physics $ " floor " $ HitLocation.Z $ " normal " $ HitNormal $ " gap " $ (P.Location.Z - P.CollisionHeight - HitLocation.Z) $ " base " $ P.Base);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local rotator R;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	if (MapOf(P) != CurrentMap)
	{
		CurrentMap = MapOf(P);
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Step == 0 && !(CurrentMap ~= "01_NYC_UNATCOIsland"))
	{
		if (MapTime > 3.0)
		{
			Log("DXSTAND: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step == 0 && MapTime > 8.0)
	{
		LogRest(P, "start");
		Step = 1;
		StepTime = MapTime;
	}
	else if (Step == 1)
	{
		P.aBaseY = 300.0;
		if (MapTime - StepTime > 2.0)
		{
			P.aBaseY = 0.0;
			Step = 2;
			StepTime = MapTime;
		}
	}
	else if (Step == 2 && MapTime - StepTime > 3.0)
	{
		LogRest(P, "after a walk");
		R = P.ViewRotation;
		R.Yaw += 16384;
		P.ViewRotation = R;
		P.SetRotation(R);
		Step = 3;
		StepTime = MapTime;
	}
	else if (Step == 3)
	{
		P.aBaseY = 300.0;
		if (MapTime - StepTime > 2.0)
		{
			P.aBaseY = 0.0;
			Step = 4;
			StepTime = MapTime;
		}
	}
	else if (Step == 4 && MapTime - StepTime > 3.0)
	{
		LogRest(P, "after a turn and a walk");
		Log("DXSTAND: exiting");
		P.ConsoleCommand("exit");
		Step = 5;
	}
}
