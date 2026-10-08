//=============================================================================
// PerfConsole: what a frame costs, measured the same way in either engine
// (vibe/docs/DEVELOPMENT.md, scripted runs). The map is the ini's TargetMap
// ([DXCapture.<console>]), Liberty Island by default, from its start: the
// player left there with no input, invulnerable, its view held where the
// start set it (PerfTurnConsole turns it).
//
// From 15 s after the map is in, sixty one-second windows, each a "DXPERF:"
// line: its span on the wall clock (milliseconds of the day, from the
// level's clock, which both engines take from the local time each tick),
// the frames drawn (PostRender), the slowest frame and the level's own time
// beside it. vibe/tools/perf/frame-report.py cuts a run's perf samples to
// those spans.
// In the original also the engine's own cycle counters, the game's tick and
// the client's frame (what its STAT GLOBAL shows), which the fork does not
// keep: read only where they are not 0, as the fork has no CyclesToSeconds.
// Then a total line, the frame times' spread, and an exit.
//=============================================================================
class PerfConsole extends Console;

var config string TargetMap;

var string CurrentMap;
var float MapTime;
var float ViewTime;
var int Phase;
var rotator StartRot;

var int LastMs;
var int WinFrames;
var int WinStartMs;
var int WinWorstMs;
var float WinStartTime;
var float WinGame;
var float WinClient;
var int Windows;
var int TotFrames;
var int TotMs;
var int TotWorstMs;
var float TotTime;
var float TotGame;
var float TotClient;
// Frame times by the millisecond; the last bucket holds 100 ms and over.
var int Hist[101];

var Engine Eng;

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

function string Target()
{
	if (TargetMap == "")
		return "01_NYC_UNATCOIsland";
	return TargetMap;
}

// The wall clock: milliseconds of the day, from the level's clock.
function int ClockMs(LevelInfo L)
{
	return ((L.Hour * 60 + L.Minute) * 60 + L.Second) * 1000 + L.Millisecond;
}

function int Since(int Now, int Then)
{
	if (Now < Then)
		return Now + 86400000 - Then;
	return Now - Then;
}

// The view this console holds, T seconds into the hold.
function rotator ViewAt(float T)
{
	return StartRot;
}

function string Ms(float Seconds)
{
	return string(Seconds * 1000.0);
}

function StartMeasuring(PlayerPawn P)
{
	local Actor A;
	local Engine E;
	local int NumStatic, NumDynamic;

	foreach P.AllActors(class'Actor', A)
	{
		if (A.bStatic)
			NumStatic++;
		else
			NumDynamic++;
	}
	foreach AllObjects(class'Engine', E)
	{
		Eng = E;
		break;
	}
	Log("DXPERF: measuring " $ CurrentMap $ " at " $ P.Location $ " facing " $ P.ViewRotation
	    $ "; actors " $ NumStatic $ " static, " $ NumDynamic $ " dynamic; engine object " $ string(Eng));
	LastMs = ClockMs(P.Level);
	WinStartMs = LastMs;
	WinStartTime = P.Level.TimeSeconds;
}

function EndWindow(PlayerPawn P, int Now)
{
	local int Took;
	local float Time;
	local string Line;

	Took = Since(Now, WinStartMs);
	Time = P.Level.TimeSeconds - WinStartTime;
	Line = "DXPERF: window " $ Windows $ " from " $ WinStartMs $ " to " $ Now
	    $ " frames " $ WinFrames $ " ms " $ Took
	    $ " mean " $ (float(Took) / Max(WinFrames, 1)) $ " worst " $ WinWorstMs
	    $ " leveltime " $ Ms(Time);
	if (WinGame > 0 || WinClient > 0)
		Line = Line $ " game " $ Ms(WinGame / Max(WinFrames, 1)) $ " client " $ Ms(WinClient / Max(WinFrames, 1));
	Log(Line);

	TotFrames += WinFrames;
	TotMs += Took;
	TotTime += Time;
	TotGame += WinGame;
	TotClient += WinClient;
	TotWorstMs = Max(TotWorstMs, WinWorstMs);
	Windows++;
	WinFrames = 0;
	WinWorstMs = 0;
	WinGame = 0;
	WinClient = 0;
	WinStartMs = Now;
	WinStartTime = P.Level.TimeSeconds;
}

function EndRun(PlayerPawn P)
{
	local int i, Count, P50, P99;
	local string Line;

	P50 = -1;
	P99 = -1;
	for (i = 0; i < ArrayCount(Hist); i++)
	{
		Count += Hist[i];
		if (P50 < 0 && Count * 2 >= TotFrames)
			P50 = i;
		if (P99 < 0 && Count * 100 >= TotFrames * 99)
			P99 = i;
	}
	Line = "DXPERF: total frames " $ TotFrames $ " ms " $ TotMs
	    $ " fps " $ (float(TotFrames) * 1000.0 / Max(TotMs, 1))
	    $ " mean " $ (float(TotMs) / Max(TotFrames, 1))
	    $ " p50 " $ P50 $ " p99 " $ P99 $ " worst " $ TotWorstMs
	    $ " leveltime " $ Ms(TotTime);
	if (TotGame > 0 || TotClient > 0)
		Line = Line $ " game " $ Ms(TotGame / Max(TotFrames, 1)) $ " client " $ Ms(TotClient / Max(TotFrames, 1));
	Log(Line);
	Line = "DXPERF: frame ms";
	for (i = 0; i < ArrayCount(Hist); i++)
		if (Hist[i] > 0)
			Line = Line $ " " $ i $ ":" $ Hist[i];
	Log(Line);
	Log("DXPERF: done, exiting");
	P.ConsoleCommand("exit");
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	M = MapOf(P);
	if (M != CurrentMap)
	{
		Log("DXPERF: now in " $ M);
		CurrentMap = M;
		MapTime = 0;
	}
	MapTime += Delta;
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= Target()))
		{
			if (MapTime > 3.0)
			{
				Log("DXPERF: opening " $ Target());
				P.ConsoleCommand("open " $ Target());
				Phase = -1;
			}
		}
		else if (MapTime > 1.0)
		{
			StartRot = P.ViewRotation;
			ViewTime = 0;
			Phase = 1;
		}
	}
	else if (Phase == -1 && (M ~= Target()))
		Phase = 0;
	else if (Phase >= 1)
	{
		ViewTime += Delta;
		P.ViewRotation = ViewAt(ViewTime);
		if (Phase == 1 && MapTime > 15.0)
		{
			StartMeasuring(P);
			Phase = 2;
		}
	}
}

event PostRender(canvas C)
{
	local PlayerPawn P;
	local int Now, Took;

	Super.PostRender(C);
	if (Phase != 2 || Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	Now = ClockMs(P.Level);
	Took = Since(Now, LastMs);
	LastMs = Now;
	WinFrames++;
	WinWorstMs = Max(WinWorstMs, Took);
	Hist[Min(Took, ArrayCount(Hist) - 1)]++;
	if (Eng != None && Eng.GameCycles > 0)
		WinGame += CyclesToSeconds(Eng.GameCycles);
	if (Eng != None && Eng.ClientCycles > 0)
		WinClient += CyclesToSeconds(Eng.ClientCycles);
	if (Since(Now, WinStartMs) >= 1000)
	{
		EndWindow(P, Now);
		if (Windows >= 60)
		{
			EndRun(P);
			Phase = 3;
		}
	}
}
