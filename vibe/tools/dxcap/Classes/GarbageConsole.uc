//=============================================================================
// GarbageConsole: destroyed actors and the references to them, logged with
// "DXGARBAGE:" (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island, 5 s in: a GarbageHolder and 129 GarbageMarkers, each
// marker's Next the one after it. From 7 s, one marker destroyed each tick,
// the first 128 in turn; marker 128 is never destroyed. Each tick logs how
// many of the holder's Held slots are still set and whether First (marker 0),
// Pair.A (marker 1) and Kept (marker 128) are, and Kept's Next (None
// throughout: it is the last). The original frees destroyed actors at the end
// of a level tick once 128 are waiting, clearing every live actor's
// references to them then (dx-reverse-info engine-dll.md, destroyed actors):
// the references stay set until the tick the level's queue, the markers and
// whatever else it destroyed, reaches 128, and go None together. It exits 2 s
// after the last.
//=============================================================================
class GarbageConsole extends Console;

var float MapTime;
var int Phase;
var int Ticks;
var float EndTime;
var GarbageHolder Holder;
var string CurrentMap;

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

function Setup(PlayerPawn P)
{
	local GarbageMarker M, Last;
	local int i;

	Holder = P.Spawn(class'GarbageHolder');
	for (i = 0; i < 129; i++)
	{
		M = P.Spawn(class'GarbageMarker');
		if (Last != None)
			Last.Next = M;
		Last = M;
		if (i < 128)
			Holder.SetHeld(i, M);
		else
			Holder.Kept = M;
	}
	Holder.First = Holder.GetHeld(0);
	Holder.Pair.A = Holder.GetHeld(1);
	Holder.Pair.N = 1;
	Log("DXGARBAGE: spawned " $ Report());
}

function Kill(int From, int To)
{
	local int i, n;
	local GarbageMarker M;
	for (i = From; i < To; i++)
	{
		M = Holder.GetHeld(i);
		if (M != None && M.Destroy())
			n++;
	}
	Log("DXGARBAGE: destroyed marker " $ From $ ": " $ n);
}

function string Report()
{
	local string s;
	s = "held " $ Holder.CountHeld() $ " first " $ (Holder.First != None) $ " pair " $ (Holder.Pair.A != None) $ " kept " $ (Holder.Kept != None);
	if (Holder.Kept != None)
		s = s $ " kept.next " $ (Holder.Kept.Next != None);
	return s;
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Map;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	Map = MapOf(P);
	if (Map != CurrentMap)
	{
		CurrentMap = Map;
		MapTime = 0;
		Log("DXGARBAGE: in " $ Map);
	}
	MapTime += Delta;
	if (Map != "01_NYC_UNATCOIsland")
	{
		if (Phase == 0 && MapTime > 2.0)
		{
			Phase = -1;
			Log("DXGARBAGE: opening 01_NYC_UNATCOIsland");
			P.ConsoleCommand("open 01_NYC_UNATCOIsland");
		}
		return;
	}
	if (Phase <= 0 && MapTime > 5.0)
	{
		Setup(P);
		Phase = 1;
	}
	else if (Phase >= 1 && Phase <= 128 && MapTime > 7.0)
	{
		Kill(Phase - 1, Phase);
		Phase++;
		if (Phase > 128)
			EndTime = MapTime + 2.0;
	}
	else if (Phase == 129 && MapTime > EndTime)
	{
		Log("DXGARBAGE: exiting");
		P.ConsoleCommand("exit");
		Phase++;
	}
	if (Phase >= 2 && Phase <= 129)
	{
		Ticks++;
		Log("DXGARBAGE: tick " $ Ticks $ " at " $ MapTime $ ": " $ Report());
	}
}
