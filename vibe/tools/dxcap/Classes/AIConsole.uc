//=============================================================================
// AIConsole: what every NPC is doing -- Liberty Island opened from wherever
// the run starts, the player left where the map starts it; 2 s, 8 s and
// 20 s into the level's own time, each ScriptedPawn's state, orders,
// whether it is in the world and hidden, its enemy, whether it looks for
// enemies and listens for shots and noises, its physics, its place and how
// long since it was drawn (tenths of a second) and whether the level's
// geometry lets the camera see it are logged, one line each, and the camera
// the frame is drawn from;
// then an exit. Through the level's first 10 s, each
// state an NPC enters is logged too, with the time, as the console's tick
// finds it.
//=============================================================================
class AIConsole extends Console;

var float MapTime;
var int Step;
var string CurrentMap;
var ScriptedPawn Watched[128];
var name WatchedState[128];
var int NumWatched;

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

function string Flag(bool b)
{
	if (b)
		return "1";
	return "0";
}

// Each ScriptedPawn, logged with "DXAI: <label>" in front.
function Census(PlayerPawn P, string Label)
{
	local ScriptedPawn S;
	local string Enemy;
	local int Count;

	foreach P.AllActors(class'ScriptedPawn', S)
	{
		Count++;
		Enemy = "None";
		if (S.Enemy != None)
			Enemy = string(S.Enemy.Name);
		Log("DXAI: " $ Label $ " " $ S.Name $ " " $ S.Class.Name
			$ " state " $ S.GetStateName()
			$ " orders " $ S.Orders $ "/" $ S.OrderTag
			$ " inworld " $ Flag(S.bInWorld) $ " hidden " $ Flag(S.bHidden)
			$ " enemy " $ Enemy
			$ " looks " $ Flag(S.bLookingForEnemy) $ Flag(S.bLookingForShot) $ Flag(S.bLookingForLoudNoise)
			$ " physics " $ S.Physics
			$ " at " $ int(S.Location.X) $ "," $ int(S.Location.Y) $ "," $ int(S.Location.Z)
			$ " drawn " $ int(S.LastRendered() * 10.0)
			$ " sight " $ Sight(P, S));
	}
	Log("DXAI: " $ Label $ " " $ Count $ " scripted pawns at " $ P.Level.TimeSeconds $ " s; player at " $ P.Location);
	LogView(P, Label);
}

// Whether the level's geometry lets the camera see a pawn: a FastTrace from
// the camera to five heights up its collision cylinder, feet first, 1 where
// the line is clear.
function string Sight(PlayerPawn P, Pawn S)
{
	local Actor ViewActor;
	local vector CameraLocation, Target;
	local rotator CameraRotation;
	local string Result;
	local int i;

	P.PlayerCalcView(ViewActor, CameraLocation, CameraRotation);
	for (i = 0; i < 5; i++)
	{
		Target = S.Location;
		Target.Z += S.CollisionHeight * (i - 2) * 0.45;
		Result = Result $ Flag(P.FastTrace(Target, CameraLocation));
	}
	return Result;
}

// Where the frame is seen from: PlayerCalcView's camera, which both engines
// draw from, and the eye height in it.
function LogView(PlayerPawn P, string Label)
{
	local Actor ViewActor;
	local vector CameraLocation;
	local rotator CameraRotation;

	P.PlayerCalcView(ViewActor, CameraLocation, CameraRotation);
	Log("DXAI: " $ Label $ " view from " $ CameraLocation $ " rotation " $ CameraRotation
		$ " eye " $ P.EyeHeight $ " base eye " $ P.BaseEyeHeight $ " bob " $ P.WalkBob
		$ " fov " $ P.FovAngle);
}

// Each state an NPC enters, logged with "DXAISTATE:" and the level's time.
function WatchStates(PlayerPawn P)
{
	local ScriptedPawn S;
	local int i;

	foreach P.AllActors(class'ScriptedPawn', S)
	{
		for (i = 0; i < NumWatched; i++)
			if (Watched[i] == S)
				break;
		if (i == NumWatched)
		{
			if (NumWatched == ArrayCount(Watched))
				continue;
			Watched[i] = S;
			WatchedState[i] = '';
			NumWatched++;
		}
		if (WatchedState[i] != S.GetStateName())
		{
			WatchedState[i] = S.GetStateName();
			Log("DXAISTATE: " $ P.Level.TimeSeconds $ " " $ S.Name $ " " $ S.GetStateName());
		}
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;

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
			Log("DXAI: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			Step = -1;
		}
		return;
	}
	if (Step == -1 && (CurrentMap ~= "01_NYC_UNATCOIsland"))
		Step = 0;

	if (Step >= 0 && P.Level.TimeSeconds < 10.0)
		WatchStates(P);

	if (Step == 0 && P.Level.TimeSeconds > 2.0)
	{
		Census(P, "2s");
		Step = 1;
	}
	else if (Step == 1 && P.Level.TimeSeconds > 8.0)
	{
		Census(P, "8s");
		Step = 2;
	}
	else if (Step == 2 && P.Level.TimeSeconds > 20.0)
	{
		Census(P, "20s");
		Log("DXAI: exiting");
		P.ConsoleCommand("exit");
		Step = 3;
	}
}
