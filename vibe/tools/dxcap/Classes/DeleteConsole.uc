//=============================================================================
// DeleteConsole: Object.CriticalDelete as the game's callers use it, logged
// with "DXDELETE:". On Liberty Island, 5 s in: three nano keys given, the
// second removed (NanoKeyRing.RemoveKey), then all (RemoveAllKeys, which
// reads the next key from the one it just deleted); two log entries added and
// cleared (DeusExPlayer.ClearLog); a conversation history made and reset
// (ResetConversationHistory); a game directory made and deleted, as the Load
// Game screen does. After each, the player's lists and how many objects of
// the class AllObjects still finds. Then the console's OBJ GARBAGE, the same
// counts 1 s later, and an exit. The original deletes at once (dx-reverse-info
// core-dll.md, CriticalDelete); a deleted object is gone to AllObjects.
//=============================================================================
class DeleteConsole extends Console;

var float MapTime;
var int Phase;
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

function int CountObjects(class<Object> C)
{
	local Object O;
	local int n;
	foreach AllObjects(C, O)
		n++;
	return n;
}

function string Keys(DeusExPlayer P)
{
	local NanoKeyInfo K;
	local string s;
	for (K = P.KeyList; K != None; K = K.NextKey)
		s = s $ K.KeyID $ " ";
	return "[" $ s $ "] " $ CountObjects(class'NanoKeyInfo') $ " key objects";
}

function Report(DeusExPlayer P, string What)
{
	Log("DXDELETE: " $ What $ ": keys " $ Keys(P) $ "; log " $ (P.FirstLog != None) $ " " $ CountObjects(class'DeusExLog')
		$ " log objects; history " $ (P.conHistory != None) $ " " $ CountObjects(class'ConHistory') $ " history objects; "
		$ CountObjects(class'GameDirectory') $ " game directories");
}

function Run(DeusExPlayer P)
{
	local GameDirectory Dir;

	Report(P, "before");
	P.KeyRing.GiveKey('DeleteA', "a");
	P.KeyRing.GiveKey('DeleteB', "b");
	P.KeyRing.GiveKey('DeleteC', "c");
	Report(P, "three keys given");
	P.KeyRing.RemoveKey('DeleteB');
	Report(P, "the second removed");
	P.KeyRing.RemoveAllKeys();
	Report(P, "all removed");

	P.AddLog("one");
	P.AddLog("two");
	Report(P, "two log entries");
	P.ClearLog();
	Report(P, "log cleared");

	P.conHistory = P.CreateHistoryObject();
	Report(P, "history made");
	P.ResetConversationHistory();
	Report(P, "history reset");

	Dir = P.CreateGameDirectoryObject();
	Report(P, "game directory made");
	CriticalDelete(Dir);
	Dir = None;
	Report(P, "game directory deleted");
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
	if (Phase <= 0 && MapTime > 5.0 && DeusExPlayer(P) != None)
	{
		Run(DeusExPlayer(P));
		P.ConsoleCommand("obj garbage");
		Phase = 1;
	}
	else if (Phase == 1 && MapTime > 6.5)
	{
		Report(DeusExPlayer(P), "after obj garbage");
		Log("DXDELETE: exiting");
		P.ConsoleCommand("exit");
		Phase = 2;
	}
}
