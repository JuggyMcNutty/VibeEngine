//=============================================================================
// StompConsole: a pawn landing on another, and on a crate -- the scripts'
// SupportActor: the stomp and the bounce -- logged with "DXSTOMP:" and the
// console's clock (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, datalinks and conversations silenced every tick, Paul
// Denton stands 220 units ahead of the player, as MeshConsole stands him.
// Each drop is RestConsole's -- the one dropped put there, still, falling
// -- and for 2 s every tick its height, velocity, physics and base, and
// both pawns' health, are logged:
//   A. the player onto Paul's head from 64, 160 and 320 over it;
//   B. Paul onto the player's from 64 and 320;
//   C. the player onto a CrateUnbreakableLarge that cannot be a base, from 64;
//   D. the player onto a CrateBreakableMedGeneral from 320.
// Before each, both are stood again where they started; then an exit.
//=============================================================================
class StompConsole extends Console;

var float CapTime, MapTime, StepTime;
var string CurrentMap;
var int Phase, Drop, Ticks;
var ScriptedPawn Paul;
var Actor Dropped, Crate;
var vector PlayerStand, PaulStand;
var rotator PlayerRot;
var string DropName;

function string MapOf(PlayerPawn P)
{
	local string URL;
	local int i;
	URL = P.Level.GetLocalURL();
	i = InStr(URL, "/");
	while (i >= 0)
	{
		URL = Mid(URL, i + 1);
		i = InStr(URL, "/");
	}
	i = InStr(URL, ".");
	if (i >= 0)
		URL = Left(URL, i);
	i = InStr(URL, "?");
	if (i >= 0)
		URL = Left(URL, i);
	return URL;
}

function Silence(DeusExPlayer DXP)
{
	if (DXP.dataLinkPlay != None)
		DXP.dataLinkPlay.AbortDataLink();
	if (DXP.conPlay != None)
		DXP.conPlay.TerminateConversation();
}

function string BaseName(Actor A)
{
	if (A.Base == None)
		return "None";
	return string(A.Base.Name);
}

function LogDrop(PlayerPawn P)
{
	Log("DXSTOMP: " $ DropName $ " tick " $ Ticks $ " at " $ CapTime $ ": z " $ Dropped.Location.Z
	    $ " velocity " $ Dropped.Velocity $ " physics " $ Dropped.Physics $ " base " $ BaseName(Dropped)
	    $ "; Paul's health " $ Paul.Health $ ", the player's " $ P.Health);
}

// Both stood again where they started, still.
function StandBoth(PlayerPawn P)
{
	P.SetLocation(PlayerStand);
	P.SetRotation(PlayerRot);
	P.ViewRotation = PlayerRot;
	P.Velocity = vect(0,0,0);
	P.Acceleration = vect(0,0,0);
	P.SetPhysics(PHYS_Falling);
	Paul.SetLocation(PaulStand);
	Paul.Velocity = vect(0,0,0);
	Paul.Acceleration = vect(0,0,0);
	Paul.SetPhysics(PHYS_Falling);
}

// The one dropped put there, still, falling.
function Fall(Actor A, vector Where)
{
	if (!A.SetLocation(Where))
		Log("DXSTOMP: " $ DropName $ ": " $ A.Name $ " cannot be put at " $ Where);
	A.Velocity = vect(0,0,0);
	A.Acceleration = vect(0,0,0);
	A.SetPhysics(PHYS_Falling);
	Dropped = A;
}

// A crate on the floor at Where, its old one gone.
function PutCrate(PlayerPawn P, class<Actor> Kind, vector Where)
{
	if (Crate != None)
		Crate.Destroy();
	Where.Z = PlayerStand.Z - P.CollisionHeight + Kind.Default.CollisionHeight + 2;
	Crate = P.Spawn(Kind,,, Where, rot(0,0,0));
	if (Crate == None)
		Log("DXSTOMP: no room for a " $ Kind.Name $ " at " $ Where);
	else
		Log("DXSTOMP: " $ Crate.Name $ " at " $ Crate.Location);
}

// Drop Drop's set-up: false once there are no more.
function bool StartDrop(PlayerPawn P)
{
	local vector X, Y, Z;
	local float H;

	GetAxes(PlayerRot, X, Y, Z);
	StandBoth(P);
	switch (Drop)
	{
		case 0: DropName = "A 64"; H = 64; break;
		case 1: DropName = "A 160"; H = 160; break;
		case 2: DropName = "A 320"; H = 320; break;
		case 3: DropName = "B 64"; H = 64; break;
		case 4: DropName = "B 320"; H = 320; break;
		case 5: DropName = "C 64"; H = 64; break;
		case 6: DropName = "D 320"; H = 320; break;
		default: return false;
	}
	if (Drop <= 2)
		Fall(P, PaulStand + vect(0,0,1) * (Paul.CollisionHeight + P.CollisionHeight + H));
	else if (Drop <= 4)
		Fall(Paul, PlayerStand + vect(0,0,1) * (P.CollisionHeight + Paul.CollisionHeight + H));
	else if (Drop == 5)
	{
		PutCrate(P, class'CrateUnbreakableLarge', PlayerStand + X * 200 + Y * 130);
		if (DeusExDecoration(Crate) != None)
			DeusExDecoration(Crate).bCanBeBase = False;
	}
	else
		PutCrate(P, class'CrateBreakableMedGeneral', PlayerStand + X * 170 - Y * 110);
	return true;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local ScriptedPawn SP;
	local vector X, Y, Z;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	DXP = DeusExPlayer(P);
	CapTime += Delta;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXCAP: now in " $ M $ " at " $ CapTime);
		CurrentMap = M;
		MapTime = 0;
		StepTime = 0;
	}
	MapTime += Delta;
	StepTime += Delta;
	if (DXP != None)
		Silence(DXP);

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Log("DXCAP: opening 01_NYC_UNATCOIsland");
				P.ConsoleCommand("open 01_NYC_UNATCOIsland");
				Phase = 1;
			}
		}
		else
			Phase = 1;
	}
	else if (Phase == 1)
	{
		if (M ~= "01_NYC_UNATCOIsland" && MapTime > 6.0)
		{
			PlayerRot = P.ViewRotation;
			PlayerRot.Pitch = 0;
			PlayerRot.Roll = 0;
			PlayerStand = P.Location;
			GetAxes(PlayerRot, X, Y, Z);
			foreach P.AllActors(class'ScriptedPawn', SP)
				if (SP.IsA('PaulDenton'))
					Paul = SP;
			if (Paul == None || !Paul.SetLocation(P.Location + X * 220))
			{
				Log("DXSTOMP: no Paul, or nowhere 220 ahead for him, exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			PaulStand = Paul.Location;
			Log("DXSTOMP: the player at " $ PlayerStand $ " mass " $ P.Mass $ ", " $ Paul.Name $ " at " $ PaulStand
			    $ " mass " $ Paul.Mass $ " state " $ Paul.GetStateName());
			Drop = 0;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Stood a second, then the drop
		if (StepTime > 1.0)
		{
			if (!StartDrop(P))
			{
				Log("DXSTOMP: done at " $ CapTime $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 9;
				return;
			}
			StepTime = 0;
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		// A crate is let come to rest a second before the player drops on it
		if (Drop >= 5)
		{
			if (StepTime > 1.0)
			{
				if (Crate == None)
				{
					Drop++;
					StepTime = 0;
					Phase = 2;
					return;
				}
				if (Drop == 5)
					Fall(P, Crate.Location + vect(0,0,1) * (Crate.CollisionHeight + P.CollisionHeight + 64));
				else
					Fall(P, Crate.Location + vect(0,0,1) * (Crate.CollisionHeight + P.CollisionHeight + 320));
				Ticks = 0;
				StepTime = 0;
				Phase = 4;
			}
		}
		else
		{
			Ticks = 0;
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		Ticks++;
		LogDrop(P);
		if (StepTime > 2.0)
		{
			Drop++;
			StepTime = 0;
			Phase = 2;
		}
	}
}
