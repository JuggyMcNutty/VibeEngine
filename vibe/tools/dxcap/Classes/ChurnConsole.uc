//=============================================================================
// ChurnConsole: destroyed actors over time -- on Liberty Island, from 5 s in,
// 20 GarbageMarkers spawned and the 20 of the tick before destroyed, every
// tick for 120 s, then an exit; every 10 s how many it has destroyed, with
// "DXCHURN:". A fork run with DXCAP_MEMLOG=1 shows whether the destroyed
// actors are freed as they go.
//=============================================================================
class ChurnConsole extends Console;

var float MapTime, NextLog;
var int Phase, Destroyed;
var string CurrentMap;
var GarbageHolder Holder;

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

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Map;
	local int i;
	local GarbageMarker M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	Map = MapOf(P);
	if (Map != CurrentMap)
	{
		CurrentMap = Map;
		MapTime = 0;
	}
	MapTime += Delta;
	if (Map != "01_NYC_UNATCOIsland")
	{
		if (Phase == 0 && MapTime > 2.0)
		{
			Phase = -1;
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		}
		return;
	}
	if (MapTime < 5.0)
		return;
	if (Phase <= 0)
	{
		Holder = P.Spawn(class'GarbageHolder');
		Phase = 1;
		NextLog = MapTime;
	}
	if (Phase == 1)
	{
		for (i = 0; i < 20; i++)
		{
			M = Holder.GetHeld(i);
			if (M != None && M.Destroy())
				Destroyed++;
			Holder.SetHeld(i, P.Spawn(class'GarbageMarker'));
		}
		if (MapTime >= NextLog)
		{
			Log("DXCHURN: at " $ int(MapTime) $ " s, " $ Destroyed $ " destroyed");
			NextLog += 10.0;
		}
		if (MapTime > 125.0)
		{
			Log("DXCHURN: exiting, " $ Destroyed $ " destroyed");
			P.ConsoleCommand("exit");
			Phase = 2;
		}
	}
}
