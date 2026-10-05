//=============================================================================
// CaptureConsole: the acceptance captures -- what the original shows of
// things the fork read but had never seen, run the same
// in both engines (vibe/docs/DEVELOPMENT.md, scripted runs).
//
// On Liberty Island: a shot of each laser tripwire's beam from its side, and
// of each corona light near and far. In the Brooklyn Bridge station: every
// conversation with a jump to a label only a comment carries, played through
// and logged event by event, its choices taken first first.
// Each shot and event is logged with "DXCAP:" in front.
//=============================================================================
class CaptureConsole extends Console;

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;

// The shots queued for the map: where to stand, where to look, a label.
var vector ShotLoc[48];
var rotator ShotRot[48];
var string ShotLabel[48];
var int NumShots;
var int NextShot;
var float EyeZ;
var int ShotsTaken;

// The tripwire walked through after the shots: does its alarm sound?
var LaserTrigger TripLaser;
var vector TripSpot;

// A mark in the view's corner while a shot is due -- a magenta block, then
// the shot's number in eight black or white blocks -- by which a grab of the
// screen from outside knows the frame (the original's own SHOT reads back
// nothing under Proton).
var int MarkShot;
var bool bMarking;

// The conversations queued for the map.
var Conversation ConvList[16];
var Actor ConvOwner[16];
var ConEvent ConvGoal[16];
var int NumConvs;
var int NextConv;
var ConEvent LastEvent;
var float ConvTime;

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

function AddShot(vector Loc, vector LookAt, string Label)
{
	if (NumShots >= ArrayCount(ShotLoc))
		return;
	ShotLoc[NumShots] = Loc;
	ShotRot[NumShots] = rotator(LookAt - (Loc + vect(0,0,1) * EyeZ));
	ShotLabel[NumShots] = Label;
	NumShots++;
}

// A place to look at a spot from: with a clear line to it and room behind
// it, at the distance or nearer, from sixteen directions round it -- the
// one most across Axis when one is given.
function bool FindViewpoint(PlayerPawn P, vector Target, float Dist, out vector Result, optional vector Axis, optional float Rise)
{
	local int i, j;
	local vector Dir, Cand;
	local float Score, Best;
	Best = -1;
	for (j = 0; j < 3; j++)
	{
		for (i = 0; i < 16; i++)
		{
			Dir.X = cos(i * 0.392699);
			Dir.Y = sin(i * 0.392699);
			Dir.Z = Rise;
			Dir = Normal(Dir);
			Cand = Target + Dir * Dist * (1.0 - 0.3 * j);
			if (!P.FastTrace(Target, Cand) || !P.FastTrace(Cand + Dir * 40, Cand))
				continue;
			Score = 1;
			if (VSize(Axis) > 0)
				Score = 1.0 - Abs(Normal(Axis) Dot Dir);
			if (Score > Best)
			{
				Best = Score;
				Result = Cand;
			}
		}
		if (Best >= 0)
			return true;
	}
	return false;
}

// Whether playing on from Start -- following the conversation's jumps
// within it, stopping at its end or at another choice -- reaches Goal.
function bool PathReaches(Conversation Con, ConEvent Start, ConEvent Goal)
{
	local ConEvent E;
	local int Steps;
	E = Start;
	while (E != None && Steps < 300)
	{
		if (E == Goal)
			return true;
		if (int(E.eventType) == 18 || int(E.eventType) == 1)
			return false;
		if (int(E.eventType) == 9 && ConEventJump(E).jumpLabel != "" && (ConEventJump(E).jumpCon == None || ConEventJump(E).jumpCon == Con))
			E = Con.GetEventFromLabel(ConEventJump(E).jumpLabel);
		else
			E = E.nextEvent;
		Steps++;
	}
	return false;
}

// Where the player stands to shoot each of Liberty Island's four lasers,
// as a search found them in the original: 70 units off the beam's middle,
// straight across it. The engines' SetLocations fit the player under the
// ceiling differently, so a search would stand them apart.
function vector LaserView(int k)
{
	if (k == 0)
		return vect(1920.30, -878.64, 2.35);
	if (k == 1)
		return vect(1920.29, -878.60, -22.35);
	if (k == 2)
		return vect(1920.19, -878.55, -54.60);
	return vect(1920.20, -878.59, -86.50);
}

function QueueLibertyShots(PlayerPawn P)
{
	local LaserTrigger LT;
	local Actor A;
	local SkyZoneInfo Sky;
	local vector Start, Hit, Mid, View;
	local int Lasers, Coronas;

	EyeZ = P.BaseEyeHeight;
	foreach P.AllActors(class'SkyZoneInfo', Sky)
		Log("DXCAP: sky zone " $ Sky.Name $ " at " $ Sky.Location $ " turned " $ Sky.Rotation);
	foreach P.AllActors(class'LaserTrigger', LT)
	{
		if (Lasers >= 4)
			break;
		Start = LT.Location;
		Hit = Start + vector(LT.Rotation) * 256;
		if (LT.emitter != None && LT.emitter.spot[0] != None)
			Hit = LT.emitter.spot[0].Location;
		Mid = (Start + Hit) * 0.5;
		AddShot(LaserView(Lasers), Mid, "laser " $ LT.Name $ " on " $ LT.bIsOn $ " from " $ Start $ " to " $ Hit);
		if (Lasers == 0)
		{
			TripLaser = LT;
			TripSpot = Mid;
		}
		Lasers++;
	}

	foreach P.AllActors(class'Actor', A)
	{
		if (Coronas >= 4)
			break;
		if (!A.bCorona || A.LightType == LT_None || A.Skin == None)
			continue;
		if (FindViewpoint(P, A.Location, 260, View, vect(0,0,0), 0.15))
			AddShot(View, A.Location, "corona " $ A.Name $ " near");
		if (FindViewpoint(P, A.Location, 1400, View, vect(0,0,0), 0.15))
			AddShot(View, A.Location, "corona " $ A.Name $ " far");
		Coronas++;
	}
	Log("DXCAP: queued " $ NumShots $ " shots: " $ Lasers $ " lasers, " $ Coronas $ " coronas");
}

// Every conversation bound in the map with a jump to a label that only a
// comment event carries.
function QueueCommentJumps(PlayerPawn P)
{
	local Actor A;
	local ConListItem Item;
	local Conversation Con;
	local ConEvent E, Target;
	local int i;
	local bool bSeen;

	foreach P.AllActors(class'Actor', A)
	{
		for (Item = ConListItem(A.ConListItems); Item != None; Item = Item.next)
		{
			Con = Item.con;
			if (Con == None || NumConvs >= ArrayCount(ConvList))
				continue;
			bSeen = false;
			for (i = 0; i < NumConvs; i++)
				if (ConvList[i] == Con)
					bSeen = true;
			if (bSeen)
				continue;
			for (E = Con.eventList; E != None; E = E.nextEvent)
			{
				if (int(E.eventType) != 9 || ConEventJump(E).jumpLabel == "")
					continue;
				if (ConEventJump(E).jumpCon != None && ConEventJump(E).jumpCon != Con)
					continue;
				Target = Con.GetEventFromLabel(ConEventJump(E).jumpLabel);
				if (Target != None && int(Target.eventType) == 17)
				{
					Log("DXCAP: conversation " $ Con.conName $ " owned by " $ A.Name $ " jumps to comment label " $ ConEventJump(E).jumpLabel);
					ConvList[NumConvs] = Con;
					ConvOwner[NumConvs] = A;
					ConvGoal[NumConvs] = E;
					NumConvs++;
					break;
				}
			}
		}
	}
	Log("DXCAP: queued " $ NumConvs $ " conversations");
}

function LogEvent(ConEvent E)
{
	local string Line;
	local ConEventSpeech S;
	local ConChoice C;

	Line = "DXCAP: event " $ int(E.eventType) $ " label '" $ E.label $ "'";
	S = ConEventSpeech(E);
	if (S != None && S.conSpeech != None)
		Line = Line $ " " $ S.speakerName $ ": " $ S.conSpeech.speech;
	if (ConEventJump(E) != None)
		Line = Line $ " jump to '" $ ConEventJump(E).jumpLabel $ "'";
	if (ConEventChoice(E) != None)
		for (C = ConEventChoice(E).ChoiceList; C != None; C = C.nextChoice)
			Line = Line $ " [" $ C.choiceText $ " -> " $ C.choiceLabel $ "]";
	Log(Line);
}

// The choice whose branch plays on to the jump to the comment label, else
// the first.
function PlayChoiceToward(DeusExPlayer DXP, Conversation Con, ConEventChoice Choice, ConEvent Goal)
{
	local ConChoice C, Pick;
	Pick = Choice.ChoiceList;
	for (C = Choice.ChoiceList; C != None; C = C.nextChoice)
	{
		if (PathReaches(Con, Con.GetEventFromLabel(C.choiceLabel), Goal))
		{
			Pick = C;
			break;
		}
	}
	Log("DXCAP: choosing '" $ Pick.choiceText $ "'");
	DXP.conPlay.PlayChoice(Pick);
}

function HideHud(PlayerPawn P)
{
	if (DeusExPlayer(P) != None && DeusExRootWindow(DeusExPlayer(P).rootWindow) != None)
		DeusExRootWindow(DeusExPlayer(P).rootWindow).ShowHud(False);
}

function bool StandBy(PlayerPawn P, Actor Other)
{
	local vector Spot;
	if (!FindViewpoint(P, Other.Location, 90, Spot))
		return false;
	P.SetLocation(Spot);
	P.SetRotation(rotator(Other.Location - Spot));
	P.ViewRotation = rotator(Other.Location - Spot);
	return true;
}

event PostRender(canvas C)
{
	local int i;
	Super.PostRender(C);
	if (!bMarking)
		return;
	C.Style = 1;
	C.SetPos(0, 0);
	C.DrawColor.R = 255;
	C.DrawColor.G = 0;
	C.DrawColor.B = 255;
	C.DrawRect(Texture'Solid', 12, 12);
	for (i = 0; i < 8; i++)
	{
		if ((MarkShot & (1 << i)) != 0)
		{
			C.DrawColor.R = 255;
			C.DrawColor.G = 255;
			C.DrawColor.B = 255;
		}
		else
		{
			C.DrawColor.R = 0;
			C.DrawColor.G = 0;
			C.DrawColor.B = 0;
		}
		C.SetPos(12 + 12 * i, 0);
		C.DrawRect(Texture'Solid', 12, 12);
	}
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local DeusExPlayer DXP;
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
			QueueLibertyShots(P);
			NextShot = 0;
			StepTime = 0;
			Phase = 2;
		}
	}
	else if (Phase == 2)
	{
		// Stand, wait for the view to settle (a corona fades in over a
		// third of a second), shoot.
		if (NextShot >= NumShots)
		{
			if (TripLaser != None)
			{
				Log("DXCAP: walking into " $ TripLaser.Name $ ", alarm time before " $ TripLaser.lastAlarmTime);
				P.SetPhysics(PHYS_Walking);
				P.SetLocation(TripSpot);
				StepTime = 0;
				Phase = 7;
			}
			else
			{
				Travel(P, "03_NYC_BrooklynBridgeStation");
				Phase = 3;
			}
		}
		else if (StepTime > 0.2 && StepTime < 0.5)
		{
			HideHud(P);
			P.SetLocation(ShotLoc[NextShot]);
			P.SetRotation(ShotRot[NextShot]);
			P.ViewRotation = ShotRot[NextShot];
			P.Velocity = vect(0,0,0);
			P.SetPhysics(PHYS_None);
		}
		else if (StepTime > 1.2 && StepTime < 2.0)
		{
			MarkShot = ShotsTaken;
			bMarking = true;
		}
		else if (StepTime > 2.0)
		{
			P.ViewRotation = ShotRot[NextShot];
			Log("DXCAP: shot " $ ShotsTaken $ " = " $ ShotLabel[NextShot] $ " at " $ P.Location $ " facing " $ P.ViewRotation);
			P.ConsoleCommand("shot");
			bMarking = false;
			ShotsTaken++;
			NextShot++;
			StepTime = 0;
		}
	}
	else if (Phase == 7)
	{
		if (StepTime > 2.0)
		{
			Log("DXCAP: in the beam at " $ P.Location $ ": alarm time " $ TripLaser.lastAlarmTime $ ", sound " $ TripLaser.AmbientSound $ ", beam hits " $ TripLaser.emitter.HitActor);
			Travel(P, "03_NYC_BrooklynBridgeStation");
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (M ~= "03_NYC_BrooklynBridgeStation" && MapTime > 6.0)
		{
			P.SetPhysics(PHYS_Walking);
			QueueCommentJumps(P);
			NextConv = 0;
			StepTime = 0;
			Phase = 4;
		}
	}
	else if (Phase == 4)
	{
		if (NextConv >= NumConvs)
		{
			Log("DXCAP: done, exiting");
			P.ConsoleCommand("exit");
			Phase = 6;
		}
		else if (StepTime > 1.0)
		{
			Log("DXCAP: playing " $ ConvList[NextConv].conName $ " with " $ ConvOwner[NextConv].Name);
			StandBy(P, ConvOwner[NextConv]);
			LastEvent = None;
			ConvTime = 0;
			if (DXP == None || !DXP.StartConversationByName(ConvList[NextConv].conName, ConvOwner[NextConv], False, True))
			{
				Log("DXCAP: did not start");
				NextConv++;
				StepTime = 0;
			}
			else
				Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		ConvTime += Delta;
		if (DXP.conPlay == None || ConvTime > 240.0)
		{
			Log("DXCAP: ended after " $ ConvTime $ " s");
			NextConv++;
			StepTime = 0;
			Phase = 4;
		}
		else if (DXP.conPlay.currentEvent != LastEvent)
		{
			LastEvent = DXP.conPlay.currentEvent;
			if (LastEvent != None)
			{
				LogEvent(LastEvent);
				if (ConEventChoice(LastEvent) != None && ConEventChoice(LastEvent).ChoiceList != None)
					PlayChoiceToward(DXP, ConvList[NextConv], ConEventChoice(LastEvent), ConvGoal[NextConv]);
			}
		}
	}
}
