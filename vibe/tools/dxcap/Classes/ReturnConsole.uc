//=============================================================================
// ReturnConsole: a return to a map within its mission -- Liberty Island,
// UNATCO HQ, then Liberty Island again, which comes back from Current as
// the player left it. 6 s into each map it logs, with "DXRETURN:", the game
// (Level.Game), its BaseMutator, and whether the player holds its
// augmentation and skill systems and its key ring; it exits 6 s into the
// third. The original keeps a returned level's game (dx-reverse-info
// engine-dll.md, a level's tick), so all are there each time. A LoadMarker
// spawned on the first visit to Liberty Island logs its PostPostBeginPlay
// calls each time there: one, then two on the return.
//
// On the first visit the player also leaves carrying a basketball, tagged
// DXReturnBall. Each map logs the player's CarriedDecoration and every
// basketball (tag, style, base); UNATCO HQ then destroys any tagged one that
// came along. The original's travel destroys the ball and empties the hands
// (dx-reverse-info deusex-dll.md, PruneTravelActors): neither UNATCO HQ nor
// the return has it.
//
// On the first visit the player is also given a goal, a note and a nano key
// as the game gives them (AddGoal, AddNote, the key ring's GiveKey: each
// new(Self) on the player), and each map logs the player's goals, notes and
// keys, each by its path. A console's open carries none of the player's
// travel properties, in either engine, so UNATCO HQ and the return show
// none; with Items (DXCAP_CONFIG "Items=True") each map is left as a map
// exit leaves it (GameInfo.SendPlayer: ClientTravel, TRAVEL_Relative, with
// the items), and all three travel: DeusExPlayer's FirstGoal, FirstNote and
// KeyList are travel properties, as are theirs.
//=============================================================================
class ReturnConsole extends Console;

var config bool Items;
var float MapTime;
var int Step;
var string CurrentMap;
var bool bLogged;

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

function Report(DeusExPlayer P, string Map)
{
	local LoadMarker M;

	Log("DXRETURN: in " $ Map $ " game " $ P.Level.Game $ " BaseMutator " $ P.Level.Game.BaseMutator
		$ " augs " $ (P.AugmentationSystem != None) $ " skills " $ (P.SkillSystem != None) $ " keys " $ (P.KeyRing != None));
	foreach P.AllActors(class'LoadMarker', M)
		Log("DXRETURN: in " $ Map $ " " $ M.Name $ " PostPostBeginPlay " $ M.Begun $ " times");
}

// A basketball put in the player's hands as the game does it, with nothing
// inside to drop if it breaks.
function CarryBall(DeusExPlayer P)
{
	local Basketball B;

	B = P.Spawn(class'Basketball', , 'DXReturnBall', P.Location + 64 * vector(P.Rotation));
	if (B == None)
	{
		Log("DXRETURN: no room for the ball");
		return;
	}
	B.Contents = None;
	P.CarriedDecoration = B;
	P.PutCarriedDecorationInHand();
}

// A goal, a note and a nano key, as the game gives each; the texts hold
// what a travel text could split on: a question mark, a semicolon, a percent.
function GiveRecords(DeusExPlayer P)
{
	local DeusExGoal Goal;

	Goal = P.AddGoal('DXReturnGoal', True);
	Goal.SetText("DXCapture goal; where?");
	P.AddNote("DXCapture note? 50%; %3F as typed", False, False);
	if (P.KeyRing != None)
		P.KeyRing.GiveKey('DXReturnKey', "DXCapture key");
}

// The player's goals, notes and keys, each by its path, and its flag base's.
function ReportRecords(DeusExPlayer P, string Map)
{
	local DeusExGoal Goal;
	local DeusExNote Note;
	local NanoKeyInfo Key;

	Log("DXRETURN: in " $ Map $ " flag base " $ string(P.flagBase));
	for (Goal = P.FirstGoal; Goal != None; Goal = Goal.next)
		Log("DXRETURN: in " $ Map $ " goal " $ string(Goal) $ " " $ Goal.goalName $ " primary " $ Goal.bPrimaryGoal
			$ " completed " $ Goal.bCompleted $ ": " $ Goal.text);
	for (Note = P.FirstNote; Note != None; Note = Note.next)
		Log("DXRETURN: in " $ Map $ " note " $ string(Note) $ " user " $ Note.bUserNote $ ": " $ Note.text);
	for (Key = P.KeyList; Key != None; Key = Key.NextKey)
		Log("DXRETURN: in " $ Map $ " key " $ string(Key) $ " " $ Key.KeyID $ ": " $ Key.Description);
}

// The player's hands and every basketball, by tag: names differ between
// the engines.
function ReportBalls(DeusExPlayer P, string Map)
{
	local Basketball B;
	local int Count;

	if (P.CarriedDecoration == None)
		Log("DXRETURN: in " $ Map $ " CarriedDecoration None");
	else
		Log("DXRETURN: in " $ Map $ " CarriedDecoration " $ P.CarriedDecoration.Class.Name $ " tag " $ P.CarriedDecoration.Tag
			$ " style " $ P.CarriedDecoration.Style $ " base " $ (P.CarriedDecoration.Base == P));
	foreach P.AllActors(class'Basketball', B)
	{
		Log("DXRETURN: in " $ Map $ " basketball tag " $ B.Tag $ " style " $ B.Style $ " based on the player " $ (B.Base == P)
			$ " base " $ (B.Base != None));
		Count++;
	}
	Log("DXRETURN: in " $ Map $ " " $ Count $ " basketballs");
}

// What came along to UNATCO HQ goes, so the return shows only what the
// return brings.
function DropTravelled(DeusExPlayer P)
{
	local Basketball B;

	foreach P.AllActors(class'Basketball', B)
	{
		if (B.Tag == 'DXReturnBall')
		{
			if (P.CarriedDecoration == B)
				P.CarriedDecoration = None;
			Log("DXRETURN: destroying the ball that came along");
			B.Destroy();
		}
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local string Map, Next;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
	Map = MapOf(P);
	if (Map != CurrentMap)
	{
		CurrentMap = Map;
		MapTime = 0;
		bLogged = false;
	}
	MapTime += Delta;
	if (MapTime < 6.0 || bLogged)
		return;
	bLogged = true;
	if (Step == 0 && Map != "01_NYC_UNATCOIsland")
		Next = "01_NYC_UNATCOIsland";
	else
	{
		if (Step == 0)
		{
			P.Spawn(class'LoadMarker');
			if (DeusExPlayer(P) != None)
			{
				CarryBall(DeusExPlayer(P));
				GiveRecords(DeusExPlayer(P));
			}
		}
		if (DeusExPlayer(P) != None)
		{
			Report(DeusExPlayer(P), Map);
			ReportRecords(DeusExPlayer(P), Map);
			ReportBalls(DeusExPlayer(P), Map);
			if (Step == 1)
				DropTravelled(DeusExPlayer(P));
		}
		Step++;
		if (Step == 1)
			Next = "01_NYC_UNATCOHQ";
		else if (Step == 2)
			Next = "01_NYC_UNATCOIsland";
	}
	if (Next != "")
	{
		if (Items)
		{
			Log("DXRETURN: travelling to " $ Next $ " with the items");
			P.ClientTravel(Next, TRAVEL_Relative, True);
		}
		else
		{
			Log("DXRETURN: opening " $ Next);
			P.ConsoleCommand("open " $ Next);
		}
	}
	else
	{
		Log("DXRETURN: exiting");
		P.ConsoleCommand("exit");
	}
}
