//=============================================================================
// FloatConsole: decorations in water, logged with "DXFLOAT:" and the level's
// time (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, from the level's start: the two crates in the water by
// the pier (CrateBreakableMedGeneral1 and 2) every quarter second for 8 s --
// their place, rotation, physics, velocity, zone, bFloating, mass and
// buoyancy; then an exit. What is compared between the engines: where they
// float and how they move getting there.
//=============================================================================
class FloatConsole extends Console;

var float LogTime;
var string CurrentMap;
var bool bTravelled;
var bool bDone;

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

function LogCrates(PlayerPawn P)
{
	local DeusExDecoration D;
	foreach P.AllActors(class'DeusExDecoration', D)
		if (D.Name == 'CrateBreakableMedGeneral1' || D.Name == 'CrateBreakableMedGeneral2')
			Log("DXFLOAT: " $ P.Level.TimeSeconds $ " " $ D.Name $ " at " $ D.Location $ " rot " $ D.Rotation
			    $ " physics " $ D.Physics $ " vel " $ D.Velocity $ " zone " $ D.Region.Zone $ " water " $ D.Region.Zone.bWaterZone
			    $ " floating " $ D.bFloating $ " mass " $ D.Mass $ " buoyancy " $ D.Buoyancy $ " base " $ D.Base);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

	Super.Tick(Delta);
	if (bDone || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	M = MapOf(P);
	if (!(M ~= "01_NYC_UNATCOIsland"))
	{
		if (!bTravelled && P.Level.TimeSeconds > 3.0)
		{
			Log("DXCAP: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
			bTravelled = true;
		}
		return;
	}
	if (P.Level.TimeSeconds >= LogTime)
	{
		LogCrates(P);
		LogTime = P.Level.TimeSeconds + 0.25;
	}
	if (P.Level.TimeSeconds > 8.0)
	{
		Log("DXFLOAT: done, exiting");
		P.ConsoleCommand("exit");
		bDone = true;
	}
}
