//=============================================================================
// SoundConsole: the acceptance captures heard, not seen -- a sound in the
// open and behind a wall, and a sound inside a reverb zone and outside it
// (vibe/docs/ROADMAP.md, M0), then beeps to the right, left and ahead. The game's
// audio is recorded outside it; three beeps start each scenario, and each
// move is logged with "DXCAP:" and the console's clock, by which a recording
// is read (vibe/tools/dxcap/sound.py).
//=============================================================================
class SoundConsole extends Console;

#exec OBJ LOAD FILE=..\Sounds\Ambient.uax PACKAGE=Ambient

// Seconds between hushing a level and a scenario's first beep: sounds
// already playing (speech, gunfire) run out.
const SETTLE = 4.0;

// The wall scenario's source and spots, as a search found them in the
// original: an AmbientSound on Liberty Island, a spot in its line of sight
// 400 units away, and one behind the level's geometry 422 units away. Both
// engines stand in the same places (their SetLocations would place a
// search's candidates differently).
const WALLSOURCE = 'AmbientSound15';

var float CapTime;
var float MapTime;
var float StepTime;
var string CurrentMap;
var int Phase;
var int Step;

var Actor Source;
var vector SpotOpen, SpotWall;
var ZoneInfo ReverbZone;
var vector SpotIn, SpotOut;
var Object Stopped;
var Actor PanSource;

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

function Stand(PlayerPawn P, vector Spot, vector LookAt, string Label)
{
	P.SetLocation(Spot);
	P.SetRotation(rotator(LookAt - Spot));
	P.ViewRotation = rotator(LookAt - (Spot + vect(0,0,1) * P.BaseEyeHeight));
	P.Velocity = vect(0,0,0);
	Log("DXCAP: " $ Label $ " at " $ CapTime $ " (" $ P.Location $ ", " $ VSize(P.Location - LookAt) $ " from it, in " $ P.Region.Zone.Name $ ")");
}

// Faces the player so that A is to its right, left or ahead; the listener
// turns with the view.
function Face(PlayerPawn P, Actor A, int Side)
{
	local rotator R;

	R = rotator(A.Location - P.Location);
	R.Pitch = 0;
	R.Roll = 0;
	if (Side == 0)
		R.Yaw -= 16384;
	else if (Side == 1)
		R.Yaw += 16384;
	P.SetRotation(R);
	P.ViewRotation = R;
}

// Nothing sounds but what is measured: every other ambient sound is
// silenced, and every other pawn and everything that watches for the
// player goes (destroyed, not killed, so no mission script hears of a death).
function Hush(PlayerPawn P, Actor Keep)
{
	local Actor A;

	foreach P.AllActors(class'Actor', A)
	{
		if (A == Keep || A == P)
			continue;
		if (A.IsA('Pawn') || A.IsA('SecurityCamera') || A.IsA('AutoTurret') || A.IsA('AlarmUnit') || A.IsA('LaserTrigger') || A.IsA('BeamTrigger')
			|| A.IsA('ShellCasing') || A.IsA('Fragment') || A.IsA('Projectile'))
		{
			A.Destroy();
			continue;
		}
		// A tree's timer plays gusts of wind; a RandomSounds' its sounds.
		if (A.IsA('Tree') || A.IsA('RandomSounds'))
			A.SetTimer(0, false);
		if (A.AmbientSound != None)
			A.AmbientSound = None;
	}
	Silence(P);
	Sounding(P, Keep);
}

// No datalink or conversation speaks; an aborted datalink waits in the
// queue, and nothing resumes it during a scenario.
function Silence(PlayerPawn P)
{
	local DeusExPlayer DXP;

	DXP = DeusExPlayer(P);
	if (DXP == None)
		return;
	if (DXP.dataLinkPlay != None && DXP.dataLinkPlay.con != None && DXP.dataLinkPlay.con != Stopped)
	{
		Stopped = DXP.dataLinkPlay.con;
		Log("DXCAP: datalink " $ Stopped.Name $ " stopped");
		DXP.dataLinkPlay.AbortDataLink();
	}
	if (DXP.conPlay != None && DXP.conPlay.con != None && DXP.conPlay.con != Stopped)
	{
		Stopped = DXP.conPlay.con;
		Log("DXCAP: conversation " $ Stopped.Name $ " stopped");
		DXP.conPlay.TerminateConversation();
	}
}

function Sounding(PlayerPawn P, Actor Keep)
{
	local Actor A;
	local int Left;

	foreach P.AllActors(class'Actor', A)
	{
		if (A != Keep && A.AmbientSound != None)
		{
			Log("DXCAP: sounding: " $ A.Name $ " " $ A.AmbientSound $ " " $ VSize(A.Location - P.Location) $ " away");
			Left++;
		}
	}
	Log("DXCAP: " $ Left $ " ambient sounds besides the measured one");
}

function Beep(PlayerPawn P)
{
	P.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 4000, 1.0);
}

function Shoot(PlayerPawn P)
{
	P.PlaySound(Sound'DeusExSounds.Weapons.AssaultGunFire', SLOT_None, 2.0, false, 4000, 1.0);
}

event Tick(float Delta)
{
	local PlayerPawn P;
	local AmbientSound AS;
	local ZoneInfo Z;
	local PathNode N;
	local vector Home;
	local bool bFoundOut;
	local float D, PanTime;
	local string M;

	Super.Tick(Delta);
	if (Viewport == None || Viewport.Actor == None)
		return;
	P = Viewport.Actor;
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
			// Moving about touches nothing: a teleporter or a trigger
			// would send the player off or start something.
			P.SetCollision(false, false, false);
			// The source plays a large fan instead of its own loop: steady (a
			// device's hum can stop, or come with an alarm; wind gusts) and
			// broad, so a filter would show.
			foreach P.AllActors(class'AmbientSound', AS)
				if (AS.Name == WALLSOURCE)
					Source = AS;
			if (Source == None)
			{
				Log("DXCAP: no " $ WALLSOURCE);
				Travel(P, "02_NYC_BatteryPark");
				Phase = 3;
			}
			else
			{
				SpotOpen = vect(2864.35, -378.14, 2578.79);
				SpotWall = vect(2853.50, -214.30, 2578.79);
				Source.AmbientSound = Sound'Ambient.Ambient.FanLarge';
				Log("DXCAP: wall scenario: " $ Source.Name $ " sound " $ Source.AmbientSound $ " volume " $ Source.SoundVolume $ " radius " $ Source.SoundRadius * 25);
				Hush(P, Source);
				P.SetPhysics(PHYS_None);
				Stand(P, SpotOpen, Source.Location, "open");
				Step = 0;
				StepTime = 0;
				Phase = 2;
			}
		}
	}
	else if (Phase == 2)
	{
		Silence(P);
		// After SETTLE, beeps 0.5 s apart, then open and wall in turn, 5 s
		// each.
		if (Step < 3 && StepTime > SETTLE + 0.5 * Step)
		{
			Log("DXCAP: beep at " $ CapTime);
			Beep(P);
			Step++;
		}
		else if (Step >= 3 && Step < 7 && StepTime > SETTLE + 3.0 + 5.0 * (Step - 3))
		{
			if ((Step & 1) == 1)
				Stand(P, SpotWall, Source.Location, "wall");
			else
				Stand(P, SpotOpen, Source.Location, "open");
			Step++;
		}
		else if (Step >= 7 && StepTime > SETTLE + 23.0)
		{
			Log("DXCAP: wall scenario ends at " $ CapTime);
			P.SetPhysics(PHYS_Walking);
			Travel(P, "02_NYC_BatteryPark");
			Phase = 3;
		}
	}
	else if (Phase == 3)
	{
		if (M ~= "02_NYC_BatteryPark" && MapTime > 6.0)
		{
			// Path nodes are places a player stands: one in a reverb zone,
			// one in a dry zone without reverb.
			Home = P.Location;
			P.SetCollision(false, false, false);
			foreach P.AllActors(class'PathNode', N)
			{
				Z = N.Region.Zone;
				if (Z == None || Z.bWaterZone)
					continue;
				if (!P.SetLocation(N.Location) || P.Region.Zone != Z)
					continue;
				if (Z.bReverbZone && ReverbZone == None)
				{
					SpotIn = P.Location;
					ReverbZone = Z;
				}
				else if (!Z.bReverbZone && !bFoundOut)
				{
					SpotOut = P.Location;
					bFoundOut = true;
				}
			}
			P.SetLocation(Home);
			if (ReverbZone == None || !bFoundOut)
			{
				Log("DXCAP: no reverb zone and dry zone to stand in");
				Phase = 5;
			}
			else
			{
				Log("DXCAP: reverb scenario: " $ ReverbZone.Name $ " (gain " $ ReverbZone.MasterGain $ ", cutoff " $ ReverbZone.CutoffHz $ " Hz, delays " $ ReverbZone.Delay[0] $ "/" $ ReverbZone.Delay[1] $ "/" $ ReverbZone.Delay[2] $ "/" $ ReverbZone.Delay[3] $ "/" $ ReverbZone.Delay[4] $ "/" $ ReverbZone.Delay[5] $ ", gains " $ ReverbZone.Gain[0] $ "/" $ ReverbZone.Gain[1] $ "/" $ ReverbZone.Gain[2] $ "/" $ ReverbZone.Gain[3] $ "/" $ ReverbZone.Gain[4] $ "/" $ ReverbZone.Gain[5] $ ")");
				Hush(P, None);
				P.SetPhysics(PHYS_None);
				Stand(P, SpotIn, SpotIn + vect(100,0,0), "in the zone");
				Step = 0;
				StepTime = 0;
				Phase = 4;
			}
		}
	}
	else if (Phase == 4)
	{
		Silence(P);
		// After SETTLE, beeps 0.5 s apart; then shots 3 s apart, three in
		// the zone and three outside it.
		if (Step < 3 && StepTime > SETTLE + 0.5 * Step)
		{
			Log("DXCAP: beep at " $ CapTime);
			Beep(P);
			Step++;
		}
		else if (Step >= 3 && Step < 6 && StepTime > SETTLE + 3.0 + 3.0 * (Step - 3))
		{
			if (Step == 3)
				Sounding(P, None);
			Log("DXCAP: shot in the zone at " $ CapTime);
			Shoot(P);
			Step++;
		}
		else if (Step == 6 && StepTime > SETTLE + 11.0)
		{
			Stand(P, SpotOut, SpotOut + vect(100,0,0), "outside the zone");
			Step++;
		}
		else if (Step >= 7 && Step < 10 && StepTime > SETTLE + 13.0 + 3.0 * (Step - 7))
		{
			Log("DXCAP: shot outside at " $ CapTime);
			Shoot(P);
			Step++;
		}
		else if (Step == 10 && StepTime > SETTLE + 22.0)
		{
			// The pan: beeps from a path node in sight, to the right, to
			// the left and ahead.
			foreach P.AllActors(class'PathNode', N)
			{
				D = VSize(N.Location - P.Location);
				if (D > 150 && D < 600 && P.FastTrace(N.Location, P.Location + vect(0,0,1) * P.BaseEyeHeight))
				{
					PanSource = N;
					break;
				}
			}
			if (PanSource == None)
			{
				Log("DXCAP: no path node in sight to pan");
				Phase = 5;
			}
			else
			{
				Log("DXCAP: pan source " $ PanSource.Name $ ", " $ D $ " away");
				Step++;
			}
		}
		else if (Step >= 11 && Step < 20)
		{
			// Per side: turn, then beeps 1.5 and 2.5 s later.
			PanTime = SETTLE + 23.0 + 3.0 * ((Step - 11) / 3);
			if ((Step - 11) % 3 > 0)
				PanTime += 0.5 + (Step - 11) % 3;
			if (StepTime > PanTime)
			{
				if ((Step - 11) % 3 == 0)
					Face(P, PanSource, (Step - 11) / 3);
				else
				{
					if ((Step - 11) / 3 == 0)
						Log("DXCAP: pan right at " $ CapTime);
					else if ((Step - 11) / 3 == 1)
						Log("DXCAP: pan left at " $ CapTime);
					else
						Log("DXCAP: pan ahead at " $ CapTime);
					PanSource.PlaySound(Sound'DeusExSounds.Generic.Beep4', SLOT_None, 2.0, false, 4000, 1.0);
				}
				Step++;
			}
		}
		else if (Step >= 20 && StepTime > SETTLE + 35.0)
		{
			Phase = 5;
		}
	}
	else if (Phase == 5)
	{
		Log("DXCAP: done, exiting");
		P.ConsoleCommand("exit");
		Phase = 6;
	}
}
