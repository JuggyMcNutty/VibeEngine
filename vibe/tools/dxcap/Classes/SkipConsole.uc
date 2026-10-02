//=============================================================================
// SkipConsole: the proof that skipping a conversation line stops its speech
// -- the game's own skip path (ConPlay.PlayNextEvent, what a keypress in the
// conversation window calls) taken mid-line, the line's audio and its length
// logged with "DXSKIP:" and the console's clock, by which a recording of the
// run (DXCAP_RECORD=1) is read: in the original the line's voice cuts at the
// skip; the fork's StopSound once matched the caller as well as the ID, and
// the player calling it with the speaker's ID stopped nothing, so the line
// played on under the next.
//
// On Liberty Island: Tech Sergeant Kaplan's MeetKaplan, played through with
// every speech line skipped a couple of seconds in (short lines at half
// their length), the first choice taken at each choice, a beep at the start
// and one after the end marking the recorded span.
//=============================================================================
class SkipConsole extends Console;

// Seconds into a line before it is skipped; a line shorter than twice this
// is skipped at half its length, mid-line either way.
const SKIPAT = 2.0;

var float CapTime;
var float MapTime;
var float StepTime;
var float EventTime;
var string CurrentMap;
var int Phase;
var Actor Kaplan;
var Conversation ConFound;
var ConEvent LastEvent;
var ConEventSpeech Speech;
var bool bSkipped;
var int Skips;
var int Lines;
var float LineLen;

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

function Travel(PlayerPawn P, string Map)
{
	Log("DXCAP: opening " $ Map);
	P.ConsoleCommand("open " $ Map);
}

// The actor MeetKaplan is bound to, as the level's conversations have it.
function Actor FindKaplan(PlayerPawn P)
{
	local Actor A;
	local ConListItem Item;

	foreach P.AllActors(class'Actor', A)
	{
		for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
		{
			if (Item.con != None && Item.con.conName == 'MeetKaplan')
			{
				Log("DXSKIP: " $ A.Name $ " (" $ A.BindName $ ") owns " $ Item.con.conName $ ", first-person " $ Item.con.bFirstPerson);
				ConFound = Item.con;
				return A;
			}
		}
	}
	return None;
}

// A place to stand within sight of Kaplan, as CaptureConsole stands by its
// conversation owners.
function bool StandBy(PlayerPawn P, Actor Other)
{
	local int i, j;
	local vector Dir, Cand;

	for (j = 0; j < 3; j++)
	{
		for (i = 0; i < 16; i++)
		{
			Dir.X = cos(i * 0.392699);
			Dir.Y = sin(i * 0.392699);
			Dir.Z = 0.15;
			Dir = Normal(Dir);
			Cand = Other.Location + Dir * 90 * (1.0 - 0.3 * j);
			if (!P.FastTrace(Other.Location, Cand) || !P.FastTrace(Cand + Dir * 40, Cand))
				continue;
			P.SetLocation(Cand);
			P.SetRotation(rotator(Other.Location - Cand));
			P.ViewRotation = rotator(Other.Location - Cand);
			return true;
		}
	}
	return false;
}

function Beep(PlayerPawn P)
{
	P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 4000, 1.0);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
	local ConChoice C;
	local float Wait;
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
	EventTime += Delta;
	// Nothing on the way hurts the player.
	P.ReducedDamageType = 'All';

	if (Phase == 0)
	{
		if (!(M ~= "01_NYC_UNATCOIsland"))
		{
			if (MapTime > 3.0)
			{
				Travel(P, "01_NYC_UNATCOIsland");
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
			Kaplan = FindKaplan(P);
			if (Kaplan == None)
			{
				Log("DXSKIP: no MeetKaplan in the level, exiting");
				P.ConsoleCommand("exit");
				Phase = 6;
			}
			else if (!StandBy(P, Kaplan))
			{
				Log("DXSKIP: nowhere to stand by " $ Kaplan.Name $ ", exiting");
				P.ConsoleCommand("exit");
				Phase = 6;
			}
			else
				Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Standing by Kaplan starts his radius conversation on its own; adopt
		// it, or clear whatever else is playing (as SoundConsole silences one)
		// and start MeetKaplan by name.
		if (DXP != None && DXP.conPlay != None)
		{
			if (ConFound != None && DXP.conPlay.con == ConFound)
			{
				Log("DXSKIP: " $ DXP.conPlay.con.conName $ " already playing at " $ CapTime $ ", adopting, beep");
				Beep(P);
				LastEvent = None;
				bSkipped = false;
				EventTime = 0;
				Phase = 3;
			}
			else if (StepTime > 2.0)
			{
				Log("DXSKIP: " $ DXP.conPlay.con.conName $ " in the way, terminated");
				DXP.conPlay.TerminateConversation();
				StepTime = 0;
			}
		}
		else if (StepTime > 3.0)
		{
			// The level's opening DataLink would refuse a first-person
			// conversation its turn, as SoundConsole silences one.
			if (DXP != None && DXP.dataLinkPlay != None)
			{
				Log("DXSKIP: datalink " $ DXP.dataLinkPlay.con.Name $ " aborted");
				DXP.dataLinkPlay.AbortDataLink();
			}
			Log("DXSKIP: starting MeetKaplan with " $ Kaplan.Name $ " at " $ CapTime $ ", beep");
			Beep(P);
			Beep(P);
			LastEvent = None;
			bSkipped = true;	// no line is playing before the first event
			if (DXP == None || !DXP.StartConversationByName('MeetKaplan', Kaplan, False, True))
			{
				Log("DXSKIP: did not start, exiting");
				P.ConsoleCommand("exit");
				Phase = 6;
			}
			else
			{
				EventTime = 0;
				Phase = 3;
			}
		}
	}
	else if (Phase == 3)
	{
		if (DXP == None || DXP.conPlay == None || EventTime > 240.0)
		{
			Log("DXSKIP: ended at " $ CapTime $ ": " $ Lines $ " speech lines, " $ Skips $ " skipped, beep");
			Beep(P);
			StepTime = 0;
			Phase = 4;
		}
		else if (DXP.conPlay.currentEvent != LastEvent)
		{
			LastEvent = DXP.conPlay.currentEvent;
			EventTime = 0;
			bSkipped = false;
			Speech = ConEventSpeech(LastEvent);
			if (Speech != None && Speech.conSpeech != None)
			{
				Lines++;
				LineLen = DXP.conPlay.con.GetSpeechLength(Speech.conSpeech.soundID);
				Log("DXSKIP: at " $ CapTime $ ": " $ Speech.speakerName $ ": " $ Speech.conSpeech.speech $ " (sound " $ Speech.conSpeech.soundID $ ", " $ LineLen $ " s)");
			}
			else if (ConEventChoice(LastEvent) != None)
			{
				C = ConEventChoice(LastEvent).ChoiceList;
				if (C != None)
					Log("DXSKIP: at " $ CapTime $ ": choices, first '" $ C.choiceText $ "'");
			}
		}
		else
		{
			// The event's own handling: a choice is taken once it has sat a
			// moment; a speech line with sound is skipped mid-line, one
			// without sound runs out its timer on its own.
			if (!bSkipped)
			{
				if (ConEventChoice(LastEvent) != None && ConEventChoice(LastEvent).ChoiceList != None)
				{
					if (EventTime > 0.5)
					{
						C = ConEventChoice(LastEvent).ChoiceList;
						Log("DXSKIP: at " $ CapTime $ ": taking '" $ C.choiceText $ "'");
						DXP.conPlay.PlayChoice(C);
						bSkipped = true;
					}
				}
				else if (Speech != None && Speech.conSpeech != None && Speech.conSpeech.soundID != -1 && LineLen > 0)
				{
					Wait = SKIPAT;
					if (LineLen < 2.0 * SKIPAT)
						Wait = LineLen * 0.5;
					if (EventTime >= Wait)
					{
						Log("DXSKIP: at " $ CapTime $ ": skipping sound " $ DXP.conPlay.playingSoundId $ " " $ EventTime $ " s into a " $ LineLen $ " s line");
						DXP.conPlay.PlayNextEvent();
						bSkipped = true;
						Skips++;
					}
				}
				else
					bSkipped = true;	// nothing to skip here
			}
		}
	}
	else if (Phase == 4)
	{
		if (StepTime > 3.0)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 6;
		}
	}
}
