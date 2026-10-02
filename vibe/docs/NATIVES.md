# What Surreal Engine lacks

Deus Ex's natives and native C++ against the engine fork's: what is missing or
wrong, what the player sees of it, and the work that would fill it. What each
original function does is in its DLL's doc ([the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#the-binaries));
what each patch changed is in [what the fork changes](ENGINE.md#what-the-fork-changes).
Which item is taken up, and when, is the owner's call; the decided order is
[`ROADMAP.md`](ROADMAP.md). With M0-M6's code landed (2026-09-25) and M7's,
[multiplayer](#multiplayer) (2026-09-27), most sections record what changed
and its by-hand check; what stays the fork's own is noted in place.

## How it is known

- **The audit.** [`vibe/tools/natives_audit.py`](../tools/natives_audit.py)
  lists every native the Deus Ex packages declare against the fork function
  registered for it, with Surreal's per-game conditions evaluated as Deus Ex.
  It tells whether that function is a stub (it, or the package method it hands
  over to, logs `Unimplemented`), an iterator that makes no iterator, or
  missing. It counts each native's call sites in the game's scripts, and with
  `--runs` it reads engine logs for which stubs fired where. Run it for the
  current state; this doc does not copy its listing.
- **Play.** Unaltered desktop runs of five maps, 80 s each from the start with
  the player untouched (2026-09-24, the fork at patch 0034): the menu map
  `DX.dx`, `01_NYC_UNATCOIsland.dx`, `01_NYC_UNATCOHQ.dx`,
  `02_NYC_BatteryPark.dx` and `06_HongKong_WanChai_Market.dx`. A stub logs
  once per session, with the script function that called it, so a run shows
  which stubs a map reaches, not how often. Two more runs, with a temporary
  hook that typed console commands, tried saving and loading
  ([below](#saving-loading-and-travel)). One more, of Liberty Island, logged
  the conversations the fork gives each NPC ([conversations](#conversations)).
  One more put eight of the game's texts through the fork's text parser
  ([what the player reads](#what-the-player-reads)).
- **The DLLs.** C++ that is not a native -- a class's own `Tick`, what the
  renderer does with an actor, the audio -- leaves no stub behind: only
  reading the original shows it is missing.
- **Reading both.** Only reading the original shows whether an implemented
  native does what it does. Found so far: `IsValidEnemy` (fixed by patch
  0034), and the ones under [not as the original](#implemented-not-as-the-original).
- **The data.** Where a difference depends on content, the game's
  conversation, text and mesh packages, its classes' defaults and its maps
  were read for what it reaches (a throwaway reader of the package format).
  Code compiled out with `#if 0` also leaves no stub; the audit reports it as
  partial.
- **The captures.** Scripted runs of both engines (2026-09-26,
  [scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)): Liberty
  Island's tripwires and lamps shot from the same places in both, a tripwire
  walked into, Jughead's deal played through, and recordings of a sound behind
  a wall, gunshots in and out of Battery Park's reverb zone and beeps from
  either side. Each result is under its feature.

## Stops the game

Nothing known does. The fork still ends the game on an error it does not
catch -- a script error, an unknown native, a failed save: the engine exits
with 1, and the launcher shows its crash banner -- but each trigger found is
fixed: `Pawn.ReachablePathnodes` makes an iterator, which yields nothing
([moving](#moving-wandering-and-tactical-movement)); the save's
`DeusExSaveInfo` lives in package DeusEx, which its save once refused
([saving, loading and travel](#saving-loading-and-travel)); `GetConfig` is
registered, as the original's ([`GetConfig`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#getconfig));
`GetPawnAllianceType(None)` answers Neutral; integer division by zero gives
0; and string `>` is native 116. An 80 s Battery Park run, whose opening
fight once ended the game about 20 s in ("Iterator statement without an
iterator in Terrorist11.GetOvershootDestination"), and a quick save in
UNATCO HQ, which once died on "Object does not belong to this package",
both ran out their clocks (2026-09-24). Two more, fixed 2026-09-26, each in
its level whatever the way in
([what an actor collides as](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces),
[falling in water](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)):

- **`09_NYC_ShipBelow` crashed** in a falling actor's trace: its
  `DeusExMover34` has no brush, and the fork's mover trace read a model that
  was not there. A brushless mover collides as its cylinder now, as the
  original's, and a model with no BSP nodes is hit by nothing.
- **`14_OceanLab_Lab` stopped** on "Failed to play AL source": its
  `GeneratorScout`s, pawns of mass 0, fall into water, where the fork's
  swimming gravity divided 0 by 0; the NaN height reached their splash
  sounds, which the audio layer refuses under any driver. The mass is
  floored at 1, as the original's falling in water floors it.

## Saving, loading and travel

Deus Ex keeps a mission's maps as the player left them, and a save is those
maps plus the one being played. `DDeusExGameEngine`, its C++ game engine,
does both ([travel and saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving)).
The fork's `Engine` has a save of its own for Deus Ex and none of the rest,
and the game's own screens and keys cannot save or load with it. Two runs
with a temporary hook (2026-09-24, UNATCO HQ) typed the game's console
commands and showed three of these. The rest is read from the code:

- **Saving** follows the original (seen with a console hook, 2026-09-24):
  slot 0 takes the highest `SaveNNNN` plus one, the quick save writes
  `QuickSave`, the slot is emptied and `Current` copied in with the level
  saved on top, and the `SaveInfo` -- `MyDeusExSaveInfo`, as the original
  names it -- carries the description (the level's `Title` without one),
  the place, the map, the slot, the player's save count, play time and
  cheats flag, the full-date time stamp, and the picture
  ([save pictures](#the-ui)).
- **The save screens** work through the game's own script (seen,
  2026-09-26, with a hook driving them): the Save Game screen shows the new
  save's picture and saves through its own button, and the Load Game
  screen lists every save, newest first, with the selected one's picture,
  place, save count, play time and size -- the original game's saves too,
  their pictures read from their `SaveInfo`. Before, both lists were drawn
  empty ([lists](#lists)), `GetSaveInfoFromDirectoryIndex` took a slot
  number for the listing's place (one row read "SAVEINFO.DXS Missing!!!"
  and the newest save never listed), the free space was measured on an
  unread path and threw, so the Save Game screen took the disk for full
  and never saved, a slot's size was in bytes where the screen expects KB,
  and the new save's row was dated 00/00/0 for want of the temporary save
  info. To check by hand: saving into a new slot from the screen, and each
  save's picture in the Load Game screen.
- **Loading works for the fork's own saves** (seen: a slot and the quick
  save round-trip, the saved pawn possessed, 2026-09-24). `?loadgame=N`
  does what the original's `Browse` does: the slot's `SaveInfo` names the
  map, `Current` is emptied, the slot copied in, and the map loads from
  `Current`. The original game's saves load too (2026-09-25, seen: the
  reference Liberty Island save loads and plays), and since 2026-09-27 with
  every listener of their event manager
  ([hearing](#hearing-the-ai-event-system)): after a load of the reference
  save the same four terrorists attack the player in both engines.
- **The original loads the fork's saves** (2026-09-27, seen: a Liberty
  Island save the fork made, which the original loads and plays on 5 s
  with the same player -- place, health, inventory -- and the same 2,604
  actors, 75 pawns and 228 items; the fork's reads its own back alike).
  What it took, each as the original's saves have it:
  - every export with the three load contexts -- client, server, editor --
    and a spawned actor transactional, with a state frame: the original
    makes no export without a context, so its load left out the player,
    the game and all else spawned;
  - the save info public, and the game named as the original's spawn
    names it (`DeusExGameInfo0`);
  - the level's URL the one it was entered by -- the map as travelled to,
    its options and port --, where the fork's kept the map file's
    (`Index.dx`), and the rest of the original's level after the reach
    specs: its time, the first deleted actor, 16 text blocks and the travel
    info, where the original's load read on into the next export;
  - the event manager in the original's layout
    ([hearing](#hearing-the-ai-event-system));
  - a state frame's latent action by the number of the native that polls
    it (`Sleep` 384, `FinishAnim` 385), where the fork's own (257 and 262)
    ran no native in the original, which stopped at the first sleeping NPC
    with "Unknown code token"; the fork's load of an original save cut that
    sleep or animation short. And the state as the original writes it: its
    most derived state beside the one whose code runs, and the class for
    an object in no state, whose name the original reads at the object's
    next `GotoState` (the fork wrote none).
- **Deleting** works: the screens' `DeleteGame N` console command removes
  the slot, and `DeleteSaveInfo` lets go of a kept info without touching
  the disk, as the original's does. To check by hand.
- **Maps remember** (seen: a travel out and back, the revisit loaded from
  `Current` with the saved pawn found and reused by the game's own login,
  2026-09-25). Within a mission the departing level is pruned and saved
  into `Current`; a new mission, a player starting a new game, or
  `?restart` empties it. The pruning destroys the augmentations and skills
  with their managers, as the original's does; the fork keeps no offset
  for a carried decoration, the original's other prune, and saves the
  destroyed actors the original drops. To check by hand: a hub map's
  doors and bodies staying as left.
- **The player's history, log and notes** are made in the level
  (`CreateHistoryObject` and its kin, 2026-09-25), as the original makes
  them, so a save keeps them; to check by hand with the screens.

**The fix:** the original's travel and save logic, all of it read and in
the fork (2026-09-26, the picture last). The fork reads its own saves back
(2026-09-24) and the original game's (2026-09-25, whole since 2026-09-27),
and the original reads the fork's (2026-09-27).

## Flags

The flag base holds what missions and conversations set and test: every
conversation played sets `<name>_Played` (the game has 1,955 conversations),
and the mission scripts set their events' flags
([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#flags)).

- **As the original's now** (2026-09-25, checked by an in-engine
  self-test): 64 buckets by a CRC of the flag's name in upper case, each a
  chain in order of hash then type, with no limit; every set stamps the
  expiration -- the one given, or the base's default for -1 -- an
  expiration of 0 never expires, `DeleteExpiredFlags` deletes up to its
  criteria, and `GetExpiration` answers -1 for a flag that is not there
  and reads the flag's own type. A typed flag is found again: the fork
  wrote every flag's type as bool, so an int or float flag could be set
  but never read. The hash is the original's since 2026-09-27, UE1's
  `appStrihash`
  ([names](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#names-hashed-and-compared)),
  and the chains are in order of it as a signed number: each engine finds
  the other's saved flags. With its own CRC the fork looked for an
  original save's flag in another bucket and missed it.
- **Kept and carried** (2026-09-25, seen: 21 flags set across the buckets,
  travelled and loaded back). The flag base and its flags live in the
  level package, so a save keeps them, and they cross a travel in the
  pawn's travel graph -- the base is the pawn's travel property, and the
  fork's travel now walks every element of a fixed-array object property,
  which the base's 64 buckets are. The pre-travel prune deletes them from
  the departing level, as the original's does, so they are not saved
  twice.

## Conversations

What the original binds and plays is ConSys's ([`ConSys.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md));
the game's script does the rest. The fork's differences here were closed on
2026-09-25; what each was, and what remains to check by hand:

- **Comment events are kept.** The fork deleted them from a mission's
  conversations at level load; the script passes over one, but a comment can
  carry a label, and 11 jumps in 10 conversations go to a label only a
  comment has, which logged "Label ... NOT FOUND" and ended the conversation,
  marked played. The two large ones, each playing once: Maggie Chow's meeting
  (`MeetMaggie`, Hong Kong), for a player who has not heard of the Dragon's
  Tooth, ended after "A nanotech blade." -- losing the goal to search the Wan
  Chai police station, the notes of its vault code, the `KnowsAboutNanoSword`
  flag and the `MaggieWanders` trigger -- and Max Chen's meeting
  (`MeetMaxChen`), for a player who brings the evidence without having heard
  of the sword, ended before its goals and before `MaxChenConvinced`, the
  flag that starts the MJ12 raid on the Lucky Money (`Mission06`) and that
  only this conversation sets. Smaller: Jughead's deal at the Brooklyn Bridge
  station (`M03MeetJugHead`), and lines of Harley Filben, a sick bum, three
  goths, Carmela and the mission 4 troopers. To check by hand: both meetings
  play past those lines. Jughead's deal plays alike in both engines'
  captures (2026-09-26): "I'll think about it." goes on through the
  comment's label to "You want it for free -- you know what to do."
- **An actor's conversations bind as the original's** (seen: a Liberty
  Island run with a temporary hook printed the lists, 2026-09-25). A bark
  (`_Bark` in its name) is owned by the actor's `BarkBindName`, or its
  `BindName` when it has none; any other conversation by its `BindName`,
  never its bark name
  ([an actor's conversations](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md#an-actors-conversations)). The
  fork gave an actor every conversation its bark name owns, so the 490
  actors with a bark name not their own -- the named troopers of missions 1
  to 4 under `UNATCOTroop`, two Paris policemen under `MetroCop` -- started
  the generic conversations instead of their own, Kaplan's `MeetKaplan`
  among them. The list is now the one the level's `ConversationPackage`
  names, as a mod's own conversations need, and each binding counts into
  `ownerRefCount`, so the script rebinds a shared conversation's invoker
  (`BindActorEvents`, matching by bark name only for the invoker). To check
  by hand: Kaplan's own greeting on Liberty Island.
- **Lines that cycle once keep the last.** 169 of the game's random-label
  events give their lines in turn and then hold the last, 164 of them the
  chatter of NPCs walked up to or frobbed; the fork's `GetRandomLabel`
  started them over ([random labels](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md#random-labels)).
- **A destroyed actor ends its conversation.** `BindEvents` empties and
  fills the script's ten bound-actor slots with each actor an event took,
  which is how `ActorDestroyed` knows the conversation's actors; the fork
  never filled them. A pawn killed is destroyed. Each event binds by name --
  or by bark name for the invoker (speech) or in a first-person conversation
  (transfer, trade, animation) -- and of several actors with one name, the
  invoker is the one bound. A transfer's and a check's item class loads as
  the event binds; `ClearBindEvents` does nothing, as the original's, so an
  event keeps the last actor bound to it. To check by hand: a conversation
  partner killed mid-line.
- **Speech loads one sound per line** (seen: the intro's lines played at
  their own lengths, 2026-09-25). `GetSpeechAudio` loads
  `ConAudio<name>_<id>` by name from `<package>Audio<name>.u`, `<package>`
  the conversation's own package less a final `Text`
  ([speech audio](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md#speech-audio)). The fork went through the
  package's audio list, which loaded every sound in the package the first
  time any line from it played -- 12.4 MB for mission 1, 14.8 MB for the
  NPCs' barks, 31.7 MB for Hong Kong's -- and named the package
  `DeusExConAudio<name>` outright, where the original's prefix follows the
  conversation's package, as a mod's would need. `GetSpeechLength` answers
  0 for -1 or no sound, as the original.
- **A skipped line's speech stops** (2026-10-01). `ConPlay.StopSpeech`
  calls the *player's* `StopSound` with the *speaker's* sound ID, and the
  original's `Actor.StopSound` (Engine.dll, [small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small))
  stops the channel by ID alone, whoever played it -- Galaxy's
  `StopSoundId`. The fork's audio device matched the caller as well and
  found nothing, so every skipped NPC line played on under the next (the
  player's own lines stopped, as the player was both caller and actor).
  It matches by ID alone now. Proven by `SkipConsole`
  (vibe/tools/dxcap): MeetKaplan with every line skipped mid-line,
  `skip.py` reading the recorded runs -- all 7 NPC tails +1.1..+2.9 dB
  louder in the unfixed fork, the player's 4 level, and nothing above the
  speech level in the fixed fork or the original.

## What the player reads

Books, datacubes, newspapers, emails, bulletins and the credits are the
game's 492 tagged texts, which `DeusExTextParser` breaks into tokens for the
script ([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusextext-dll.md)). The fork's parser now reads as
the original's -- its tokens, its tag table, its reading to an end tag
(2026-09-25; a run with a temporary hook put five texts through it against
their SDK sources). Its own tokenizer changed most texts:

- **No computer listed an email** (an `EMAIL` tag read as a file: all 66
  accounts, 136 emails, and the passwords and codes they hold) and **no
  bulletin opened** (the `=` of a `FILE` tag kept in the name). The fields
  now split at commas and trim, a missing one empty, and the last row of an
  account or board -- a tag at the text's very end, which the fork missed
  with 10 others' closing `</B>`/`</I>` -- is kept.
- **The designers' comments showed** in 166 texts (where a datacube lies,
  whose inbox an email is); `NOTE`, `GOAL` and `COMMENT` now hide what they
  hold, to their end tag. One of the original's quirks is not kept: at a
  comment with no end tag it reads past the text's end
  (`09_EmailMenu_ShipOps`, whose listing there depends on the memory after
  it); the fork stops at the end, hiding the rest.
- **Words ran together** ("From:Anon", 395 places in 135 texts) and **blank
  lines vanished** (242 texts): a token's text now keeps its spaces, nothing
  trimmed, each CR and LF a space, so a blank paragraph is the original's
  line of two spaces.
- **Raw tags showed**: at a tag it did not know the fork gave the rest of
  the text raw, where a tag is now the first of the 30 names its content
  starts with (`<LOG ERROR>` an `L`) and an unknown one is `TT_None`, which
  the script passes over.
- **The player's first name was empty** (9 texts): `SetPlayerName` now keeps
  the part before the first space. `DC` and `C` read their colours (the
  fork's integer reader took its target by value: every colour was black),
  and the first `<P>` is swallowed, so an email no longer starts with an
  empty line and a datacube's note runs its first two paragraphs together,
  as the original's does. `JC`/`JL`/`JR` come through as tokens; what the
  screens do with them is the script's.

To check by hand: a computer's emails and bulletins, a datacube and its
note, a book's centred title, the credits' section breaks
([open decision 1](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/agent.md#open-decisions)).

## Every NPC

### The native tick: `AScriptedPawn::Tick`

Every `ScriptedPawn` has a C++ `Tick` in `DeusEx.dll`
([what it does](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-native-tick)), and the fork runs it now
(2026-09-25), before the actor tick, in the original's order. What was
missing without it, each in place:

- **Agitation and fear decay**: the tick calls `UpdateAgitation` and
  `UpdateFear` -- the script's own versions, which nothing called and which
  mirror the DLL's.
- **The sixteen AI timers count** -- twelve down to zero; `ReloadTimer` and
  the counting-up `WeaponTimer` only with a weapon; `PotentialEnemyTimer`
  clearing `PotentialEnemyAlliance` as it runs out; `DistressTimer` counting
  up until it passes `FearSustainTime` and becomes -1. Seen: a set
  `AlarmTimer` of 5 read 2.0 three seconds on and 0 later, the
  `DistressTimer` counting toward its 25.
- **Cloaking**: with `bHasCloak`, the script's
  `EnableCloak(Health <= CloakThreshold)` every tick.
- **Burning out**: past `BurnPeriod`, the script's `ExtinguishFire`.
- **Bleeding**: `SpurtBlood` as `DropCounter` passes the wound's period,
  faster the harder it bleeds and the faster it moves, clotting away over
  `ClotPeriod` -- and, with `bTickVisibleOnly`, only within 1,200 units of
  the player (seen gated off beyond it).
- **A `bDisappear` NPC** in stasis or unseen for 5 s is destroyed
  ([render time and stasis](#out-of-sight)).
- **The pivot eases** to `DesiredPrePivot` as `PrePivotTime` runs out, under
  the script's own `PlayAnimPivot` values (seen).
- **The advanced-tactics manoeuvre ends** once the pawn stops accelerating,
  leaves walking or has no turn direction.

To check by hand: a cloaked commando, a burning NPC going out, a rat
disappearing once out of sight
([open decision 1](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/agent.md#open-decisions)).

### Starting up

A `ScriptedPawn` starts in its `StartUp` state, which hands it to its
orders two ways: its `Tick`, at once, when the pawn was drawn in the last
second; its code, after `InitializePawn`, a sleep of 0.2 to 1.2 s and a
landing. `InitializePawn` takes a pawn placed out of the world (`bInWorld`
false) out of it -- hidden, moved 20,000 up, into `Idle` --, and only the
code stops there, the state change taking the code with it. The original
starts a level with every actor 10 s undrawn, whatever its map kept, and
ticks every actor in its first tick
([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)), so each pawn out of
sight starts from its code. The fork kept the map's render times and
passed over every actor in a level's first tick, drawing a frame before any
had ticked: every pawn took the `Tick`, straight on to its orders. Both
are the original's since 2026-09-27. Seen with `AIConsole` against the
original on Liberty Island: the seven UNATCO troops out of the world stay
in `Idle`, listening for nothing, where the fork's patrolled, listening;
and the three NPCs ordered to sit -- UNATCOTroop6, BumFemale0 and
Terrorist18 -- sit, where they wandered. Still different: a few pawns far
off count as drawn ([out of sight](#out-of-sight)), and one NSF terrorist
backs off for a few seconds early on where the original's patrols.

### Hearing: the AI event system

NPCs learn of gunfire, footsteps, noises, alarms, bodies and distress through
events that actors raise and NPCs listen for
([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-ai-event-system)). The fork has the
original's manager now (2026-09-25): `UEventManager`, one per level in
`LevelInfo.EventManager`, ticked by the level after the actors and saved
with it, with the raising and listening natives (`AISetEventCallback` 710,
`AIClearEventCallback` 711, `AISendEvent` 713, `AIStartEvent` 714,
`AIEndEvent` 715, `AIClearEvent` 716, `LevelInfo.InitEventManager` 650 --
all stubs before, reached in every map run, so no NPC heard anything),
`AICanHear` 706 as the original's, and `AICanSmell` 707 answering 0 as the
original's does. Its class is the DLL's own with no script, made at startup
into the Engine package. Seen on Liberty Island: terrorists took
`HandleDistress` by sight of a distressed civilian, a security bot heard
footstep pulses fade with distance, a raised `WeaponFire` reached
`HandleShot` and the terrorists' answering gunfire became senders in turn,
and a quick save carried 10 event types and 325 listeners through a load.
To check by hand: a shot fired around a corner turning guards, a thrown
body found ([open decision 1](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/agent.md#open-decisions)).

Saved in the original's layout (2026-09-27;
[the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#saved)):
its event types, senders and listeners each an object of the original's
classes, the types in its hash buckets, the listeners in its ring from
where it resumes, so each engine loads the other's saves with every
listener. Seen: an original Liberty Island save's 10 event types, 55
senders and 261 listeners kept through the fork's load, and the fork's own
48 and 318 through its load and the original's. The fork's saves before
then have its own layout, still read; the original's were skipped, their
listeners lost.

The calls come as the original's since 2026-09-28: all after the pass,
the senders' slots moved on first, so a pulse a listener's call raises
lands in the next frame's slot, for every listener to weigh. The fork
called each listener as it had its turn, so such a pulse landed in this
frame's slot, missed by the listeners that had their turn before it. Seen:
Battery Park's opening minute made 39 calls, none raising another event
from inside the call -- the order shows only where one does.

### Moving: wandering and tactical movement

All the original's now (2026-09-25; the originals:
[moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)); they were stubs, so a wandering NPC never
picked where to go, one in a fight or a search never tested a direction,
and a seeking NPC got no overshoot destination:

- **`AIPickRandomDestination` 709** (`Wandering.PickDestination`, reached
  in every map but the menu): up to its tries of biased random directions,
  each tested through `AIDirectionReachable`, the multiplier stopping the
  pawn short of what it can reach.
- **`AIDirectionReachable` 708** (17 call sites: `PickDestination`,
  `TryLocation`, `GetNextLocation`, `FindBackupPoint`, `CleanerBot`,
  animals): the pawn itself walks, swims or flies the direction in steps of
  its collision radius and is put back; walls, ledges, steps, the void,
  pain zones and water stop it. Seen: a probe from a dock pawn found a spot
  280 units along its facing inside the asked range.
- **`ReachablePathnodes` 1004** (`GetOvershootDestination`,
  `ComputeAwayVector`) iterates up to 32 nodes nearest first, from the
  start node's usable reach specs, or the nearest reachable nodes within
  1,000 units; `ComputePathnodeDistances` 1020 floods `visitedWeight` over
  the network from the same list. Seen: 13 nodes nearest first by the dock,
  and the flood reaching 876 of Liberty Island's 1,198 navpoints.
- **`MoveTo` 500 and `MoveToward` 502** (2026-09-28): each tick as the
  original's `moveToward` and its polls: a spot reached within 16 units
  across (and the pawn's height, at least 48, up or down), a pawn target
  within reach, or the time out; the acceleration straight at the spot at
  the full rate, a fast pawn's velocity steered onto the line, and near
  the spot the speed halved once and held to 200 units a second over the
  speed; a pawn walking around what it bumped has its script's
  `AlterDestination` turn the destination each tick; the first step taken
  in the call itself, and a pawn target given 1.2 s. The fork took a spot
  reached within a fifth of the speed across -- 20 units or more at a
  walk, outside the pawn -- and never asked `AlterDestination`, so no NPC
  walked around what it bumped; and a path's first node the pawn already
  stood on counted as not touched when it lay a step below the pawn's
  middle, the height taken into the distance across. Together they sent
  a patroller back to the node it stood on until `CheckDestLoc` backed it
  off (`BackingOff`, 4 s in, Terrorist15 on Liberty Island); it patrols
  now, within 4 units of the original's at 20 s (`AIConsole`, 374 before).
- **A directly reachable target is walked to straight** (2026-10-01): the
  original's `findPathToward` (Engine.dll `0x103db3f0`) first asks
  whether the target itself can be walked to -- `CanMoveTo` for a
  navigation point, `pointReachable` for a spot, then a pass over the
  candidate nodes -- and returns the target as the route when so. The
  fork always searched, so a bot whose next patrol point was in plain
  sight walked off through path nodes to reach it. Proven by
  `MoveConsole` (vibe/tools/dxcap): SecurityBot1's route now matches the
  original's exactly.
- **The search itself** (2026-10-02): the original's `breadthPathFrom`
  (`0x103dcd60`, [the search](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search)),
  where the fork's was a Dijkstra by reach-spec distance. What a node
  costs to reach is the spec's distance plus the node's own penalty
  (`cost`: the `SpecialCost` event or `ExtraCost`) plus what was spent to
  reach the node expanded so far, and an end point's `bestPathWeight`
  besides; the nodes not yet expanded are kept in an open list sorted by
  that cost, out of the nodes' own `nextOrdered`/`prevOrdered`, with the
  route in `previousPath` and the cost in `visitedWeight` -- which
  `ClearPaths` now resets to 10,000,000, clearing the list with it, as
  `APawn::clearPaths` (`0x103da050`) does. The caps are the original's: the
  script's node cap, which none of Deus Ex's eleven `FindPathToward` calls
  passes, so the search gives up after 1000 nodes and says so in the log,
  and the open list's own walk gives up after 500.
  **Proven by `MoveConsole`**: UNATCOTroop1, frozen against geometry from
  8 s on in the fork (`moved 0` for the rest of the run), now walks its
  whole patrol as the original's does, reaching the patrol points it never
  reached -- the original's route through PathNode405, where the fork's
  kept re-picking PathNode406. 50 of Liberty Island's 52 pawns' distance
  moved is the original's, as before, SecurityBot1's route among them.
  **One pawn stalls where it did not**: Terrorist35 stops against geometry
  at 34 s, where the fork's walked on (its route through PathNode964 and
  PathNode644, where the original's and the fork's both go by
  PatrolPoint86). With the search, what decides which node it settles on
  is the set of end points it stops at, and that is still the fork's
  (`MarkReachableNavEndPoints`: what is within 1,000 units and
  `actorReachable`, up to eight) where the original's is
  `APawn::definePathsFor` -- a flood of a node's reach specs, each traced,
  marking each spec's end actor and giving it the spec's distance as its
  `bestPathWeight`, with what stops a link blocking only a pawn that
  cannot open its way through (`bCanOpenDoors`, and `bIsPlayer` or the
  mover's `bPlayerOnly`). **Tried 2026-10-02 and not landed**: ported in
  full, with the reach spec's own `reachFlags` checked against
  `APawn::calcMoveFlags`' seven bits (`bCanWalk`, `bCanFly`, `bCanSwim`,
  `bCanJump`, `bCanOpenDoors`, `bCanDoSpecial`, `bIsPlayer`), and marked
  from each of the two nodes the original could be given -- the goal-side
  node `GetPathnodeList` returns and the pawn's own -- **both measure
  worse**: 47 of Liberty Island's 52 pawns' distance moved is the
  original's, where the fork's own marking gives 50 (UNATCOTroop1, fixed
  by the search above, stops finding an end point at all). The missing
  piece is therefore in `GetPathnodeList`'s node list or in the goal-side
  `findPathToward` flow between the two calls, not in the marking: with
  the goal's node the end points are the goal's own forward neighbours,
  and the search walks the level's list backwards from the goal, so it
  would never meet one. What `findPathToward` does between getting the
  node list and searching -- the candidates it walks, `CanMoveTo` and
  `pointReachable` over each, `bHunting` (the pawn word's bit 21) forcing
  one to count -- is the next thing to read. All of it is written up in
  [the search](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search).
  Compared on the way (DeathConsole): the death path matches the
  original's throughout -- the same animation, lurch, hide timing and
  carcass mesh -- and the robots' freeze in Dying forever is the
  original's own behaviour.

To check by hand: NPCs wandering their bit of Liberty Island, and a
searching NSF stepping around corners in a fight
([open decision 1](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/agent.md#open-decisions)).

## Out of sight

The original's renderer records when each actor and each zone was last drawn
([render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-time)), and the engine and the scripts
skip work for what the player has not seen lately
([stasis and render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#stasis-and-render-time)). The fork
keeps both now (2026-09-25), beside Distant AI's own `LastVisibleFrame`
([its patches](ENGINE.md#settings-the-launcher-exposes)):

- **Render time.** `LastRenderTime` is stamped where the renderer's own
  visibility test passes, a spawned actor starts 10 s undrawn -- and since
  2026-09-27 every actor at a level's start, whatever its map kept, as the
  original's ([starting up](#starting-up)) --, and a zone's
  is the frame's own zone and both zones a visible portal borders (this
  renderer draws the BSP whole behind a span clipper, not zone by zone as
  the original's `OccludeBsp`). `LastRendered()` answers the time since,
  never below 0 -- it returned 0 for every actor, so everything counted as
  just drawn and the scripts spared nothing: `ParticleGenerator`s unseen
  for 2 s, `bTickVisibleOnly` NPCs' enemy and body checks, light-beam
  checks, and NPC shadows laid each tick all ran, on the handheld too. A
  decal's own `LastRenderedTime` is stamped when drawn and never read, as
  the original's: a `Shadow` never counts as drawn. What counts as drawn
  is the original's way since 2026-09-27
  ([which actors are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#which-actors-are-drawn)): the actor's
  screen rectangle, set back in the world at its depth, goes down the BSP
  with the walk, cut at each plane it crosses, and each piece is tested
  against the span clipper at its own place in the front-to-back walk; a
  piece in solid space, in a subtree hidden whole, or in a zone no visible
  portal has led to is dropped. The fork's test took the actor's box at
  the first node whose plane it crossed, before the world in front of it
  was in the clipper, so an NPC far off behind the island counted as
  drawn: on Liberty Island (`AIConsole`) Terrorist7, so counted, checked
  the pawns around it as a `bTickVisibleOnly` NPC unseen for 5 s and more
  than 600 units from the player does not, saw the UNATCO security bot and
  fought it, where the original's patrols on -- and patrols on now. A zone
  portal seen from behind leads into its zone too, as the original's does;
  the fork skipped it as a back face. Drawing is unchanged: the fork draws
  what its box test passes and leaves the rest to the depth buffer. The
  rectangle is the original's since 2026-09-28: a mesh's from its render
  box -- the boxes of its animation's frame and the next, grown by a unit
  --, as `BoundVisible` finds it, running to the frame's edge on a side
  the box reaches past; a sprite's its texture's size; set back at the
  depth of the actor's location, and none for an actor behind the viewer,
  where the fork took the whole mesh's box and counted any actor with a
  corner at the near plane as drawn. Still the fork's own: one span
  buffer for the frame, where the original keeps one per zone -- tried
  (2026-09-28), a zone's own buffer changed no test in 15,500 at the
  starts of Liberty Island, UNATCO HQ and Battery Park, the walls round a
  portal being drawn before what lies beyond it. At Liberty Island's start
  the fork counts 9 NPCs as drawn and the original 3 (10 before): five
  terrorists and a thug far off, whose rectangles show 1 to 9 pixels over
  the seawall and the pier's roof in the fork -- nothing in front of them
  there, the pixels covered later by what stands behind them --, where the
  original's closes them, a pixel's difference between the two engines'
  edges. **[perf]** The pieces' filtering is render CPU: to re-measure on
  the Smart Pro, against what it spares the tick.
- **Stasis.** `InStasis()` is the original's -- `bStasis`; `bForceStasis`,
  or physics none or rotating; not drawn for 5 s; its zone not drawn for
  5 s, or more than 1,200 units from the player -- where the fork's old one
  answered whether stasis was *allowed*. The tick of an actor in stasis
  does nothing -- no script tick, physics, animation or timers -- and
  destroys a `bTransient` one. Seen on Liberty Island: of ~2,600 actors,
  ~220 allow stasis, and the count in it grew from 6 to 30 over 40 s as
  unseen trees and lamps aged past 5 s. **[perf]** To re-measure on the
  Smart Pro when M3's AI work lands with it.
- **The event manager** reads both: a listener drawn in the last 5 s or
  within 1,200 units, and not in stasis, weighs every sender; any other
  only those within 400 ([hearing](#hearing-the-ai-event-system)).

## On screen

### Particles and lasers: render iterators

An actor with a `RenderIteratorClass` is drawn as the many things its
iterator lists: the renderer makes the iterator (`Actor.RenderInterface`) and
draws each item it lists ([render iterators](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-iterators)).
Deus Ex uses this for two classes:

- **`ParticleGenerator`**: smoke, steam, water, sparks. It is in 32 maps, and
  fires, rockets, faucets, damaged robots and fragments spawn one.
- **`LaserEmitter`**: the beams of `LaserTrigger` (18 maps) and `BeamTrigger`
  (11), a weapon's laser sight, and `ElectricityEmitter` (21 maps).

The fork keeps the whole mechanism now (2026-09-25): the interface made and
dropped as the original's renderer does; Init, First, IsDone, CurrentItem
and Next each scene frame; each item drawn as the proxy stood when listed --
place, turn, scale and glow -- a sprite at its captured place, a mesh with
the proxy put back for the draw and restored after; and the proxy's
`LastRenderTime` stamped per listed item, which the generators' freeze logic
reads. `UpdateParticles` 3017 ages, drifts, rises or sinks, grows, fades and
moves the 64 particles; `LaserIterator` lists an item every 16 units of each
beam, every 15 with `bRandomBeam`, whose segment ends jitter by a random
unit vector and chain -- the electricity -- and one extra item at a segment
chosen at random, where the proxy is left. Differences, read from both
codes:

- **Occlusion.** The original occludes each item's sprite through its span
  buffer; the fork clips items to the view (and a portal's spans) before
  its BSP walk, and the depth buffer hides what a wall covers -- the cost
  of a hidden item is paid, the look is the same. Whether the proxy counts
  as drawn, which the generators' freezing reads, is the original's since
  2026-09-27: each item tested in the walk as an actor is
  ([out of sight](#out-of-sight)).
- **The particle curves.** The documented shapes are kept -- the drift
  offset -3 to +2 a frame, growth from 0.01 to 3 times the draw scale over
  the life, the fade with the remaining life, the rise as acceleration at
  the rise rate -- but the original's exact curves are unread: whether
  smoke reads the same is the by-hand check's judgement.

To check by hand: steam from a Hell's Kitchen street grate against the
original -- drift, growth, fade; a laser tripwire's beam seen on Liberty
Island (its trace fix is
[implemented, not as the original](#implemented-not-as-the-original)'s);
electricity arcing on a damaged panel; a weapon's laser sight in play.

Captured (2026-09-26), from the original's own places 70 units off Liberty
Island's four tripwires: the original draws each beam as a red dashed line,
the fork drew none. The beam's texture, `LaserBeam1`, is a fire texture
(64 × 8, spark kind 27), which the fork left black until it drew them as
`Fire.dll` does ([fire, water and ice textures](#fire-water-and-ice-textures)):
since, it draws the beams (2026-09-27, `LaserConsole`), both engines' logs
agreeing on the emitter, its iterator and its proxy -- a translucent sprite
of that texture, moved along the beam --, the dashes where the original's
are and as far apart. They looked fainter over a floor the fork drew 3.7
to 4 times too bright; with the light maps the original's
([lighting](#lighting)) the floor matches, and the dashes are a little
stronger than the original's: in the two beams' bands 60 to 70% more
pixels red over green by 40 or more, their red about 7% higher
(2026-09-28, against `D3DDrv`).
Walked into, a tripwire sounds its alarm in both.

### Coronas

A light with `bCorona` and a `Skin` texture shows a glow over it on screen
([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#coronas)). The fork keeps the original's now
(2026-09-25): the lights shining into the player's own leaf of the BSP at
any distance -- the leaf's permeating list, and the dynamic corona lights
standing in it -- hidden by the world, movers, pawns and other actors but
the player's own pawn; each fading in and out over about a third of a
second on real time, up to 32 kept from frame to frame; drawn in the
colour of the light's hue and saturation times the fade, at the same
screen size as before. Other games keep the fork's old take.

To check by hand: coronas near and far and behind an NPC (already in the
list below), and a lamp's glow coming up and going over about a third of
a second as a corner hides and shows it.

Captured (2026-09-26), from the same places near and far from four of
Liberty Island's lamps: the fork's glows were smaller and dimmer than the
original's, and a lamp at the frame's right edge glowed in the original and
not in the fork. Two causes, read in `Render.dll` (2026-09-27,
`CoronaConsole`): the colour -- the fork took the light maps' colour for
the hue and saturation, some 40% of the original's, which whitens the hue
by the saturation itself -- and the lights -- the fork's came from the leaf
the eye is in, the original's from the one the player stands in. With both
as the original's, the three lamps from CaptureConsole's shot 7 glow in
both, where each glow's core is and as bright; further out the fork's
follows the texture's falloff times the colour exactly. The original's
frames only look brighter there, more so the fainter the texel, for their
gamma ([brightness](#brightness)): given the same gamma, the fork's shot
has the glow of the lamp against the sky fall off as the original's does,
to within 4 of 255 out to 60 pixels (2026-09-27).

### What a pawn holds

- **Attachments** (2026-10-02; [a pawn's attachments](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-pawns-attachments)):
  after a pawn's mesh, the original draws the pawn's `Weapon` in its
  third-person mesh and scale at the triangle its own mesh holds a weapon
  at, in the pawn's style and lit as the pawn -- the fork does that -- and,
  **where the mesh has no such triangle, the pawn's `SelectedItem` in its
  third-person mesh, drawn where the item is**. The fork drew nothing there,
  so an NPC holding something that is not a weapon had nothing in its
  hands. `VisibleMesh::DrawMesh` now draws the item through the renderer,
  in the pawn's light.
  To check by hand: an NPC carrying an item its own mesh cannot hold a
  weapon at (a datacube, say).

### Mesh detail

The fork works the original's vertex budget out each draw now (2026-09-25;
[the formula and its numbers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-detail)): faces whose
`FaceLevel` is past the clamped budget go, each kept corner walks down its
collapse list into it, and the top `LODMorph` fraction of the raw budget
slides toward what it collapses to, texture coordinates with it, so detail
fades rather than pops. A temporary log matched the doc's own numbers on
Liberty Island (a trooper's 244 vertices at 7,865 deep at 1,920 pixels wide
is the doc's ~200 at 5,000 at 853, scaled by the resolution term). The
per-vertex work falls with the faces, since the fork animates and lights a
vertex once a draw, the first time a kept face uses it. **[perf]** To
re-measure on the Smart Pro: the per-vertex work of ~40 meshes was ~8 ms of
the render
([where a frame goes](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#where-a-frame-goes)).
Still different: the original's exact morph curve is unread (the fork
slides linearly over the zone), and the original lights only the vertices
of faces turned to the eye, which is [lighting](#lighting)'s to take up.

To check by hand: an NPC walking away on Liberty Island -- detail fades
with no pop or seam as it recedes, and reads whole again as it comes back.

### Lighting

Read from both codes ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#lighting)):

- **Light maps.** The fork keeps the original's three kinds now
  (2026-09-25): a surface's still lights are its static map, built once
  and kept until one changes; its animating lights -- pulse, flicker, an
  animating effect -- are added over the loaded static colors every
  frame, each through its own shadow bits; the moving lights stay the
  shadowless per-frame pass; and a mover's maps are rebuilt when the
  mover moved or turned since, not every frame. And a map holds the
  original's bytes now (2026-09-28,
  [the maps](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#light-maps)): the zone's ambient light,
  `FGetHSV`'s colour times 64; each light's illumination -- its shadow
  byte, 254 lit (the original's 3 x 3 kernel), 127 all over for a light
  without shadow bits, times its effect's shape, the plain one
  1 − 3v² + 2v³ of v, its distance over its radius, times the cosine of
  its angle to the surface -- times its colour, `FGetHSV`'s at full value
  by `GlobalLighting`'s brightness and the level's `Brightness`, at most
  127; each channel held to 127 as the lights add up; a byte a 255th,
  which the shader's doubling makes `D3DDrv`'s 2/255
  ([brightness](#brightness)). The fork had the colour of
  √`LightBrightness`, a falloff of (1 − 3v² + 2v³) / v held to 1, a lit
  shadow of 2, the ambient light doubled and no ceiling short of twice
  the texture: its lit surfaces were 3.7 to 4 times the original's indoors
  and about 2.2 outdoors, in linear terms. Now `ViewConsole`'s Liberty
  Island and `LaserConsole`'s corridor come out within a level or two of
  256 of `D3DDrv`'s frames, region for region. A pulse is the original's
  0.6 + 0.39 sin (a subtle one 0.9 + 0.09 sin), 35 turns a second over
  `LightPeriod` from `LightPhase`'s 256ths of a turn; a flicker's draw,
  when at least one half, lights it at that fraction; a palette light's
  colour and brightness are its palette's. Still the fork's own: the
  maps are floats, converted for the Smart Pro's GPU on the CPU (engine
  patches 0002 and 0024); a changed map goes to the GPU whole in both; the
  lookup is a `std::map` where the original's cache hashes and first
  checks the item it found last; a still-shaped animated light is re-run
  rather than kept as its shadowed light and rescaled; blink and strobe
  keep the fork's timing, where the original's blink goes by the lowest
  bit of its turn count and its strobe turns over each frame, both at the
  frame rate; a flicker draws at most 25 times a second; the sample
  points of a map's texels are the fork's; and the other effects' shapes
  are the fork's, on the plain falloff, as the original's spotlight is
  (the waves' and the rest unread). To check by hand:
  a flickering sconce's wall (the 'Ton's entrance), a pulsing light
  throbbing, and a triggered light going dark, each with its shadows still
  there.
- **`NoDynamicLights`**: works now (2026-09-25) -- animated lights count
  as still and bake into the static map, and moving ones are left out;
  by hand with the ini setting on.
- **`LE_CloudCast`**: the fork builds it once, and its effect's shape is
  a placeholder (upstream's to-do); in the original the cloud shape
  changes over time and is run every frame, its formula unread. The torch
  and fire wavers and the watery shimmer are the original's now
  (2026-09-28): the plain shape, each texel dimmed at random by up to 5%,
  20% and 40% as the light is added -- the shimmer's randomness plain,
  where the original's has a table of its own; and the omni bump map is a
  plain light, as in the original's table of effects.
- **Meshes.** The fork keeps the original's now (2026-09-25): the
  candidates from the actor's leaf of the BSP plus the moving lights near
  it and last frame's, the strongest picked first -- statics until 8, none
  below an eighth of the strongest, `bCorona` lights counting -- shadows
  checked through the BSP every 16 frames, each light fading in and out
  over about a third of a second and lighting as it fades, and the
  original's per-vertex formula: the (cos + 1)^2 - 1.5 diffuse, the
  6 cos^2 highlight toward the eye, linear falloff, 1.4 x `ScaleGlow`,
  ambient added, channels clamped. And the original's colours since
  2026-09-28: each light's the light maps' (`GlobalLighting`'s colour times
  its brightness and the level's `Brightness`), the zone's ambient light in
  `FGetHSV`'s colour, the `AmbientGlow` of 255 pulsing 0.25 + 0.2 sin(8t),
  and the lights ranked by (1 − d/r) × `LightBrightness`; the fork's
  √`LightBrightness` colours and half the ambient light had
  `MeshConsole`'s Paul Denton, crate, barrel and box 5 to 14% brighter
  than `D3DDrv`'s frames in their terms, now within a level of 256. One
  knowing difference: the moving lights come from the fork's light tree
  near the actor, not the leaf's own list; and the glow's pulse runs on the
  level's time, where the original's runs on the viewport's. To check by
  hand: an NPC walking from light into shadow (the fade), and a fire's
  glow on a face.

### Brightness

The Brightness slider is the original's display gamma (2026-09-27): the
picture raised to 1 / (2.5 × Brightness), as Deus Ex's `D3DDrv` sets its
ramp ([gamma](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#gamma)) -- 0.4 is neutral, the GOG
build's 0.6 a gamma of 1.5 --, where the fork took 2 × Brightness, a
darker picture at the same setting (mid-grey about 11% darker at 0.6). The
light maps already doubled as the original's do. The launcher's other
gamma mode (XOpenGL's curve) takes the same product. A screenshot shows
the gamma only with the engine's `GammaCorrectScreenshots`, which this
machine's settings leave off, so the scripted runs' shots carry none. The
original's frames on Xvfb do carry theirs (measured 2026-09-27,
[the screen flash](#the-screen-flash)): the ramp its `OpenGLDrv` sets
shows in what the grabber reads, a gamma of 1.5 at the runs' Brightness of
0.6 -- so a fork shot takes the same gamma before its brightness is
compared with them. `D3DDrv`, the game's own renderer, draws the same
frames there, ramp and all, region for region at Liberty Island's start
(2026-09-28, `DXCAP_RENDERER=D3D`,
[scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)): the two
draw a light map alike, a byte of it worth 1/128 of the texture's
brightness in `D3DDrv`'s default, one-pass path
([the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness)).
The fork's shaders take 3.1/255 off every texture's colour (upstream's
`darkClamp`, so that a glow's black stays black), where `D3DDrv`'s
16-bit textures lose half a 5-bit step of each texture's own brightest
colour on average -- more on a bright texture, less on a dark one:
without it Liberty Island's pier, a dark wood, comes out one level of 256
brighter in the frames' terms and a laser's corridor none, the pier still
a level short of `D3DDrv`'s (2026-09-28, the clamp kept).

### The screen flash

The flash -- a grenade's blast, the healing augmentation, the vision
augmentation's glare, `FadeViewTrigger`'s fades, the drunk colours, a
zone's tint -- is the original's (2026-09-27): the player's `FlashScale`
and `FlashFog` go to the
device as Deus Ex's game engine hands them over, the scale halved and both
clamped to 0-1, none at all with the client's `ScreenFlashes` off (on in
the game's ini), which a net game overrides
([the screen flash](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-rest-of-a-frame)). The fork handed the
scale over whole, so a flash dimmed the picture half as much as the
original's -- a `FlashScale` of 0.5 not at all --; its test for a flash
leaned on a wrong four-component inequality to pass. Checked with
`FlashConsole` against the original: a steady glow (`FlashScale` 0.5, a
red fog of 0.2) halves the picture and adds the fog in the fork's shots
as the formula gives, and the original's frames fit the same formula
through their gamma ([brightness](#brightness)) to a unit across the
whole range.

### Fire, water and ice textures

The fractal textures are the original's now (2026-09-27): Fire.dll's fire,
water, wet, wave and ice textures, which the energy weapons, lasers,
fires, smoke, gas, water and the drunk effect are made of
([`fire-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md)) -- every spark kind and
what it lets go, the drops, the fire and water passes, the tables, the wet
texture's shift of its source and the wave texture's lighting, and ice,
each step the original's to the byte: checked against the DLL's own
routines, run in an emulator on the same inputs. The fork's were
upstream's own takes: a fire drew random dots for the kinds it did not
know -- two of the Dragon's Tooth's three --, and water was a float
simulation of its own.

Found with them, in how a mesh shows its textures, each now the original's
([`render-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md) and
[`fire-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md#stepping)):

- **A mesh's changing texture** went to the GPU once and never again:
  since engine patch 0019 drew a run of faces with one texture in one
  call, the texture's changed flag was cleared before the run was drawn.
  Every fractal texture on a mesh stood at its first image -- the
  Dragon's Tooth's wet blade at all zeros, a wide magenta slab where the
  original's is a narrow blue blade.
- **A mesh's slots** are filled as the original fills them: its
  `MultiSkins` entry, else the mesh's texture (the `Skin` first for the
  first slot), else the `Skin`; a slot with none draws the environment
  map (the actor's `Texture`, its zone's, the level's, the last slot's),
  where the fork put the actor's `Texture` or its last `MultiSkins` entry
  in the slot.
- **Animated textures on a mesh** play: the fork drew each at its first
  frame.
- **A masked texture** on a mesh face is drawn masked whatever the face's
  own flags, as the original draws it.
- **The viewer's `Sprite`**, Deus Ex's Matrix easter egg, stands in for
  every mesh's textures.
- **The pace.** A texture steps as the original's `UTexture::Tick` steps
  it, `PrimeCount` first and at most once a drawn frame, by its
  `MaxFrameRate` and `MinFrameRate`. One knowing difference: with no
  `MaxFrameRate` -- most of Deus Ex's -- the original steps it every
  frame, so it runs faster at a higher frame rate; the fork steps it at
  most 60 times a second, the original's pace at 60 frames. The fork
  used to step those 25 times a second.

Seen in the check of the blade: the fork's weapon sat higher and smaller
than the original's -- the whole view at 90 degrees, not Deus Ex's 75
([small](#small)); as the original's since 2026-09-28. To check by hand: the Dragon's Tooth in hand
and on the ground, the flamethrower's flame, the riot prod's arcs, the EMP
grenade's blast, tear gas and poison gas, the drunk effect, a burning NPC,
a laser sight's spot, and water.

### Head turns and lip sync: blend animations

The fork keeps the original's now (2026-09-25;
[blend animations](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#blend-animations)): `PlayBlendAnim` with
the original's defaults, `TweenBlendAnim` as TweenAnim for a slot from its
kept last pose, and the slots' tick moving only while the main animation
plays or tweens, up to three times their rate -- what the game's head
turns and lip sync were made with -- a slot that ends leaving the rest
only the time over. `Pawn.PlayTurnHead` (NPCs turning to look),
`Pawn.LipSynch` (mouths in conversations) and the player's
`ViewModelBlendPlay` drive it. The per-call logs a handheld paid for are
gone. The mesh side was already upstream's: each slot's pose added as its
difference from the mesh's first frame.

To check by hand: a conversation partner's mouth moving with the speech
(Tech Sergeant Kaplan is in the conversations list already), an NPC's head
turning to follow the player and easing back, and blinking.

### Lists

The list window is behind the load and save screens, emails, the logs,
images, the conversation history, the key bindings, the colour themes and a
new game's skills ([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#lists)). Its differences
were closed on 2026-09-25 (an in-engine self-test drove the sorting, the
number reading, the moves and the focus); what each was, and the by-hand
checks:

- **Fields read back** (2026-09-25; the test was the wrong way round, and a
  field past the row's last was read out of bounds). The screens that keep
  what a row stands for in a hidden column -- the load and save screens'
  slots, the colour editor, the images screen -- get their data; a float
  field keeps the number its text reads as, which `GetFieldValue` answers,
  and shows it through the column's format
  ([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#rows-and-fields)).
- **Rows activate.** A double click or Enter sends `ListRowActivated` to
  the list's parents with the activate sound; the fork counted every click
  as one and never sent it, so the game's Customize Keys screen -- which
  starts rebinding a key only that way -- could rebind nothing. To check by
  hand: rebinding a key there.
- **Keys move.** `MoveRow` moves the focus row up, down, a page, first or
  last -- clamped, selecting or extending from the anchor, the move sound
  played, the row scrolled into view -- and the list's script sends it the
  arrow keys, Page Up and Down, Home and End, so a pad whose d-pad maps to
  the arrows moves through a list too. To check by hand with a pad.
- **Sorting is the original's.** The keys are an ordered list with reverse
  and case flags per column (`SetSortColumn`, `AddSortColumn`,
  `ResetSortColumns`, `Sort`, and auto sort keeping new and changed rows in
  place); a float or time column compares its numbers, a string column its
  text, stable ([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#sorting)). The load game
  list now sorts by its hidden date column, and emails by sender or subject
  from their headers' clicks; to check by hand.
- **Columns.** A new column is 26 wide (20 plus both margins), the window's
  text colour and font, and a sort key; auto-expanding columns, on by
  default, widen a column to each field put in it; hidden columns take no
  space and do not draw. A click below the last row selects the last row.
- **Its size.** Landed (2026-09-26): the list asks for the original's size
  -- its visible columns side by side, a row size for each row, the row
  size its tallest column font plus the row margins -- and every change to
  its rows or columns asks its parent to lay it out again
  ([the list's size](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-lists-size)). The fork's list
  asked for nothing, so every list in a scroll area -- the save screens,
  and likely the emails, logs, key bindings and the rest -- was sized to
  nothing and drew no rows. To check by hand with the checks above.

### The UI

- **The pointer.** Landed (2026-09-27): drawn while a modal window is up,
  as before, and only while `ShowCursor` has not hidden it, as the
  original's ([the pointer](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#showing-and-hiding)) -- the fork
  ignored `ShowCursor`, and drew it over a conversation, the credits, a
  key waiting to be bound and the multiplayer windows. Checked: the main
  menu keeps its pointer (`GetConsole`); over "Connection failed" in the
  Entry level only the crosshair is left, as in the original's frame.
- **Keys held under a menu.** Landed (2026-09-25): when the UI takes a
  key, every key the input holds down is released -- the tracked buttons
  false, the axes zero -- and when it takes a mouse button, only `bFire`
  and `bAltFire` clear, both as the original's
  ([the input](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-engine-and-the-input),
  [the root window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-root-window)). To check by
  hand: a movement key held into a menu and let go there moves nothing
  when the menu closes, and a click on the HUD mid-fight stops fire but
  not the walk.
- **Showing and hiding.** Landed (2026-09-25): `Show` and `Hide` ask the
  window's parent -- its `ChildRequestedVisibilityChange` decides, the
  script's default calling `SetChildVisibility` back on the child, the
  root setting its own -- and `SetChildVisibility` is whole: the flag,
  and when that changes what can be seen, focus and grabs moved away
  from what is hidden, `VisibilityChanged` down the tree, and the tree
  laid out again; so `DeusExHUD` lays itself out as the InfoLink and
  the log come and go
  ([showing and hiding](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#showing-and-hiding)). To check
  by hand: an InfoLink message pushing the HUD's other parts into
  place as it appears and goes.
- **The vision augmentation.** Landed (2026-09-25): `GC.DrawActor` draws
  the actor through the renderer into the scene being drawn -- the GC's
  style, the glow and unlit given, the draw scale multiplied, a given
  skin replacing every skin, as if not hidden, all put back afterwards
  -- so heat sources draw in their grid skin at twice their glow
  ([actors in a window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#actors-in-a-window)).
  `bConstrain` is not honoured, the fork's own: the augmentation's
  calls cover the whole view. To check by hand: the vision
  augmentation at level 1 showing a warm NPC through its grid, and
  what a wall in front does staying the renderer's.
- **Borders.** Landed (2026-09-25): `GC.DrawBorders` tiles each edge and
  the centre at one texel a pixel, as the original's `DrawIconPattern`
  does, and honours the stretch flags the game never passes
  ([borders](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#borders)). **Margins (2026-10-02):**
  each side's is the largest of its own textures -- left from the two left
  corners and the left edge, and so on -- a margin given above 0 replaces
  it, and a box narrower or shorter than two of them has both shrink in
  proportion, which is what the centre is inset by. The last `Unimplemented`
  in a native the game calls goes; the audit has `GC.DrawBorders`
  implemented rather than partial. The game's own 13 calls pass no margins,
  so what the margins are for it is the inset the centre already had.
  **That commit dropped the edges** (fixed the same day): it moved the other
  three corners' edge ends into the top-left corner's, so every edge of a
  frame with a top-left corner came out with a negative length and only the
  corners drew -- an inventory item's selection frame was four dots
  (`BorderConsole`, which selects items on the inventory screen and shoots
  them in both engines; the HUD's panels and the belt, which `BeltConsole`
  shoots and the commit was checked with, draw their own frames and never
  showed it). Each corner ends its own edges again, an edge without a
  corner runs to the box's own corner, and a selected item's frame is
  whole, as the original's is.
  **Left open:** where the *edges* sit. The RE's rule reads as the edges
  tiling along the margin lines, where the fork draws them flush with the
  corners; `BorderConsole`'s shots of the original are the capture to
  settle it with, and until they are read for it the edges stay where
  Surreal puts them. To check by hand: a selection
  border in the inventory and a themed HUD frame crisp, their patterns
  repeating instead of smearing over the run.
- **Save pictures.** Landed (2026-09-26): `GenerateSnapshot` makes the
  original's grey picture of the frame last drawn, read back between
  frames -- a read inside one ends the frame the renderer is still
  recording, which is what had kept it out -- and a save takes its
  160 × 120 one when it is asked for, into a texture beside its save info
  ([save pictures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#save-pictures)); the screens show it
  ([saving](#saving-loading-and-travel)).
- **The menus' background.** Landed (2026-09-26): the UI background
  option's Snapshot and Black work -- with the root's rendering off the
  world is not drawn, and the raw background is drawn under the windows
  ([the raw background](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-raw-background)); before,
  the world was drawn under every menu whatever the option. To check by
  hand: the option's three settings under the main menu, and the credits
  over black.
- **Text with no width.** Landed (2026-09-27): a width of 0 or less is no
  limit to a line, as the original's line breaking has it
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small)), so
  `GetTextExtent(0, ...)` measures each line whole. The multiplayer message
  window measures its progress lines that way and draws them in a box that
  wide: the fork broke them a word to a line. Checked: a join's
  Connecting lines drawn whole and centred, in a run's shot.
- **Centred and right-aligned text with word wrap off.** Landed
  (2026-10-01): the fork centered and right-aligned each line within the
  *wrap* width, which the no-wrap path had widened to 100,000 -- every
  such text drew tens of thousands of pixels off-screen, so the object
  belt's descriptions, counts and slot numbers never showed. The
  original's `XGC::DrawText` (Extension.dll `0x10028180`) hands the wrap
  width only to line breaking and aligns within the width as passed;
  the fork keeps an alignment width beside the wrap width now. Proven by
  `BeltConsole` (vibe/tools/dxcap): the belt's text draws, structure
  matching the original's, and the menu regression (GetConsole) is
  99.9 % identical. Checked on the way: the fork's own `shot` command
  reads the framebuffer before the present pass's gamma, so its shots
  are darker than the screen shows.
- **Focus moves between windows.** Landed (2026-10-01): the fork's
  `MoveFocusDown`, `MoveFocusUp`, `MoveFocusLeft` and `MoveFocusRight` were stubs and its buttons were not
  selectable, so no keyboard focus ever moved -- a conversation's choices
  had no selector (the blue) and never answered Up/Down, and a menu's
  buttons could not be focused by key. The original's
  `XWindow::MoveFocus` (Extension.dll `0x1004ef30`) walks the focus's
  group's row/column-major window lists (position-sorted, wrapping,
  skipping `IsTraversable` failures: selectable, the parent chain visible
  and sensitive, under the topmost modal), seeds the focus when nothing
  has it (the root's tick, `0x1003a540`, through the topmost modal's
  group), and buttons draw their focused state
  (`ChangeButtonAppearance` `0x10008380`). All ported: the lists are
  built on demand (same membership and order as the original's
  maintained tables), `windowType` is set at creation (root 3, modal 2,
  tab group 1) so `GetTabGroupWindow` finds any non-generic ancestor --
  a conversation window is its choices' group -- buttons are selectable
  by default, and `UButtonWindow` draws by state instead of always the
  normal colour. Proven by `ChoiceConsole` (vibe/tools/dxcap): the focus
  cycles exactly as the original's (seeded on the first choice, Down to
  the second, wrapping) and the blue moves with it.
- **Keys.** Landed (2026-09-25): `EditWindow.Undo` and `Redo` walk a real
  change list -- typing joins, `maxUndos` caps, Ctrl+Z and Ctrl+Y call
  them -- with two edit bugs fixed on the way (inserting over a selection
  dropped the wrong span; backspace at 0 pushed the insertion point to
  −1); `MoveTabGroupNext`/`Prev` move focus between the visible tab
  groups for Tab and Shift+Tab, `GetTabGroupWindow` real; and
  `RootWindow.LockMouse` holds the pointer and eats buttons as asked,
  so the Customize Keys screen pins it while binding
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small),
  [the root window](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-root-window)). To check by
  hand: Ctrl+Z and Ctrl+Y in a save name, Tab between a screen's
  control groups, and the pointer pinned while binding a key.

## Sound

The original's audio is `Galaxy.dll`
([`galaxy-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md)). The fork's scans every actor for ambient
sounds each frame, keeps one record a channel and chooses which sound wins
as it does, and since 2026-09-28 mixes Deus Ex's sounds as Galaxy's mixer
does -- its pan, its volumes, its resampling and its reverb, at
`OutputRate` --, the mix going out through one OpenAL source; the music is
OpenAL's own source still, and other games' sounds OpenAL's, placed in 3D.
Every difference read from both codes landed 2026-09-25 or 28; each bullet
says what changed, what stays the fork's own, and its by-hand check:

- **Sounds behind walls.** Landed (2026-09-25): a sound fades over half a
  second to a third of its volume while the level's BSP stands between the
  player's eyes and its actor, and back as the line clears, speech and
  actorless sounds excepted; movers and actors do not block, as the
  original's `FastLineCheck`
  ([sounds behind walls](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#sounds-behind-walls)). A
  temporary transition log on Liberty Island showed ambients clear at
  their true distances, one blocked behind terrain at a third, and a
  boat's idle crossing both ways. To check by hand: a guard's radio or
  a generator dulling through a wall and opening back up in a doorway,
  never a conversation line. Measured (a fan heard 400 units off in the
  open and 422 behind Liberty Island's rock): 10.0 dB down in the original
  and in the fork, neither filtering, and the fan in the open at −17.5 and
  −17.4 dB of full scale (2026-09-28; −20.1 in the fork before its
  mixer was Galaxy's).
- **Reverb.** Landed (2026-09-25): a zone with `bReverbZone` gives every
  sound its reverb -- 21 zones in 16 maps, Battery Park to the endgame
  (the data) -- set again only when it changes, starting from silence;
  music stays dry, as the original's. Galaxy's own since 2026-09-28
  ([reverb](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#reverb)): three stages of stereo allpass
  filters, each echo one side of a stage, a lowpass at the cutoff in their
  feedback, the last stage fed back into the first with its sides swapped,
  the echoes' delays whole samples at `OutputRate`, where the fork mapped
  the zone onto OpenAL's EFX reverb, a model of its own. To check by
  hand: Battery Park's underground echoing against the open park, and
  the echo gone on stepping back out. Measured (gunshots in Battery
  Park's `ZoneInfo5`, dry outside it in both): the original's ring
  2.04 s before falling 60 dB under their peak and the fork's 2.00, and
  their tail, 0.5 to 1.5 s after the peak, is 32.4 dB under the shot in
  both (2026-09-28; 0.74 s and 40.3 dB with EFX).
- **Ambient sounds on lights.** Landed (2026-09-25): the fork scales such
  a sound by `LightBrightness` ÷ 255 -- a quarter or less for 235 of the
  402 actors in 46 maps that carry both -- and follows the light's pulse,
  blink, strobe or flicker, capped at 1: the renderer's `GlobalLighting`
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame)), the fork's the original's
  since 2026-09-28, a palette light's with it, but for the blink's and
  strobe's timing ([lighting](#lighting)). To check by hand: a security camera's hum
  quieter than an unlit machine's of the same volume, and a flickering
  or pulsing light's hum wavering with it.
- **Music.** Landed (2026-09-25): a transition fades the playing music
  out first -- 1 s, 5 s after a fight, 1/3 s into one, at once for the
  rest, plus twice `Latency` -- then the song starts at full volume at
  the order `SongSection`, a different song loaded, the same one only
  jumping; the playing order is written back each frame while no
  transition waits, so the ambient track resumes where it was; and
  section 255 is silence ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#music)). To
  check by hand: the music after a fight (already in the list below) --
  combat music in fast, the ambient back in slow and where it left off,
  not from the top.
- **The Speech slider.** Landed (2026-09-25): `SpeechVolume` is a setting
  of the fork's audio device (default 255, the game's), speech -- the talk
  slot -- gains by it and the rest by the Sound slider, and the three
  instant-volume natives set the sliders themselves, so a menu drag holds
  instead of lasting one frame ([volume](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#volume)). Since
  2026-09-28 the sliders' law is Galaxy's: each sound plays at its own
  slider times the louder one, the mixer squaring the louder -- at the
  game's 204 and 255, as before, the other sounds at 0.8 and speech at 1
  (times Galaxy's 0.97); with the Speech slider lowered below the Sound
  one, both quieter than the fork had them. Not carried: Galaxy's
  equal-sliders quirk, both scaled by the slider once more. To check by
  hand: the Speech slider moving a conversation's loudness mid-line and
  not the world's, the Sound slider the other way round.
- **Loudness.** Landed (2026-09-25): the fork plays the script's volume
  as the original does -- no rescale toward 1, no halving -- with
  fall-off linear from the sound to its radius and silent there, and the
  product capped at full ([volume](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#volume)); the fall-off
  runs from the eyes the view is drawn from since 2026-09-28, as
  Galaxy's, where the fork's ran from the view's actor. Not carried: the
  original's 1/256 volume floor, an integer artifact. Other games keep
  the fork's old loudness. To check by hand: a humming light or a
  generator fading steadily on the walk away and silent right at its
  radius, not gone early; effects sitting louder against the music than
  before. A gunshot from the player records at −3.6 dB of full scale in
  the fork and −3.7 in the original (2026-09-28; −6.7 before, the pan's).
- **Pan.** Galaxy's since 2026-09-28
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame), [the mixer](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#the-mixer)): the
  sound's angle off straight ahead, front and back alike, seen from the
  view's eyes and scaled down within a tenth of its radius, is a pan of at
  most seven-eighths to a side, and each side plays at the square root of
  its share of it, where the fork left the sound's place to OpenAL, which
  panned it hard and dropped a sound ahead to half on each side.
  `ReverseStereo` swaps the sides, and `UseSurround` (off in the game's
  ini) plays a sound behind centred with its right side inverted.
  Measured (a beep 234 units off, 90° to one side): the far channel
  5.1 dB under the near one in both, each channel at −7.1 dB of full scale
  straight ahead in the fork and −7.2 in the original (2026-09-28; 42 dB
  and −11.5 before). To check by hand: with headphones, a sound off to one
  side heard in both ears, the far one softer.
- **Resampling and loops.** Galaxy's since 2026-09-28
  ([the mixer](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#the-mixer)): a sound steps through its
  samples at its rate times its pitch in whole hertz, interpolating
  linearly as the original does on a CPU with SSE, and a looping sound
  loops exactly between its `smpl` points, where OpenAL looped the whole
  sample and the fork jumped back once a frame after the loop's end.
- **Doppler.** Landed (2026-09-25): the fork shifts only an ambient
  sound's pitch, by its actor's speed away from the view target at
  `DopplerSpeed` (a real setting, default 6,500 units a second), kept to
  0.5–2, working it out itself with OpenAL's own Doppler off -- it used
  to shift every sound by the player's speed at about 14,800
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame)). To check by hand: a walk
  toward and away from a humming light leaves its pitch alone, and only
  a moving ambient carrier (a patrolling bot's hum) bends.
- **Smaller.** Landed (2026-09-25): a sound beyond its radius is dropped,
  as the original drops it -- its priority goes negative, never beating an
  empty channel; the mouth shapes lose the fork's own `M` band, `E`
  reaching to 250 Hz as the original's table has it
  ([lip sync](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#lip-sync)); and `bIsSpeaking` is the script's
  alone -- the fork writes `nextPhoneme` only while ConPlay has it set,
  and no longer forces the mouth closed on a channel's teardown, the
  script's `LipSynch` closing it as the original's does. To check by
  hand: an NPC's bark moving no mouth outside a conversation, and a
  conversation partner's mouth closing on its own at a line's end.

## Implemented, not as the original

- **The list window, the flag base, conversations, the text parser and
  coronas:** [lists](#lists), [flags](#flags), [conversations](#conversations),
  [what the player reads](#what-the-player-reads) and [coronas](#coronas).
- **`DeusExPlayer.GetDeusExVersion`.** The fork's own string, by choice; the
  original's is "Mon Mar 19 12:06:14 2001 v1.112fm".
- **`LevelInfo`'s clock** (2026-10-02): the full year and the month 1 to 12,
  where the fork's main loop filled the year counted from 1900 and the month
  from 0. What pins the original's convention is its own scripts, not the DLL:
  the properties are `transient` ints (`Engine.u`, `LevelInfo`), and the one
  reader of them, `StatLog`, zero-pads a month below 10 and writes the year as
  it stands -- a month counted from 0 or a year from 1900 would come out wrong
  there; `MenuScreenSaveGame` builds its stamp the same way from the save
  info's, which the fork already stores the original's way. (Where the original
  fills them is not found: no C++ in `Engine.dll` writes those offsets outside
  the property system's copies.) In Deus Ex only `StatLog` reads them.
- **The roadmap's M3 traces-and-moves item is the original's now**
  (2026-09-25; the originals: [traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces),
  [moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving),
  [events and probes](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#events-and-probes),
  [the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives)). What each was:
  - **`Object.Enable` 117 and `Disable` 118**: one probe mask per object,
    set afresh at every `GotoState` even into the same state, saved as the
    original's `FStateFrame` keeps it. The fork kept per-state sets of
    disabled names that no state change cleared and that every scripted
    call looked its name up in -- `Wandering.Bump`'s disabled `AnimEnd`
    stayed off, where the state's `Wander` label re-enables it now. To
    check by hand with the rest of M3's AI.
  - **Conversions**: a bool prints `True`/`False` and reads back as one; a
    vector or rotator reads what is there, a missing part 0; a rotator
    prints its parts unwrapped; an object prints its path name, its
    package first even when it sits in a group (`Effects.Laser.LaserBeam1`,
    which printed as `Laser.LaserBeam1` before 2026-09-27).
  - **`Object.VRand` 252** (70 call sites): the points kept are those
    inside the unit sphere, which lean nowhere.
  - **`Actor.RandomBiasedRotation` 717**: the offsets spread over the
    range at last -- an NPC sprinting aside in a fight no longer always
    goes square to its enemy.
  - **The trace iterators `TraceTexture` 1000 and `TraceVisibleActors`
    1003** run over the original's `MultiLineCheck`: nothing beyond the
    first wall, the wall itself listed as the `LevelInfo`, a level hit
    giving the surface's texture and `PolyFlags` and an actor hit no
    texture. A laser beam stops at the player and NPCs now (they have no
    `Skin`, which the fork required) and at walls; an NPC seeking a spot
    no longer sees it through a wall. To check by hand: Liberty Island's
    laser tripwires. Since 2026-09-28 a level hit's texture and flags are
    the node's the line meets the level at, the first of its plane's
    coplanar nodes, as the original's BSP check gives them, where the fork
    took the polygon of them it crossed -- another texture where coplanar
    polygons differ (`TraceConsole`: 104 of Liberty Island's 107 traces
    alike, from 81; the rest are NPCs a little apart in the two runs).
  - **A line that starts inside an actor's cylinder** (2026-09-28) is
    stopped at once if it heads in toward the axis and passes out freely
    otherwise, as the original's cylinder check counts only a line coming
    in ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)). The fork gave the point where it left the
    cylinder: a trace straight down from inside a pawn found the pawn's own
    bottom before the floor (every NPC's on Liberty Island, `TraceConsole`),
    and a move that began inside another actor's cylinder met its far side.
    The scripts' footsteps passed over it (they read on to the level); a
    shot or a turret's aim traced from inside its shooter did not.
  - **Where a trace stops** (2026-09-28): short of what it hits as the
    original's traces are -- a hit on the level or a mover's brush half a
    unit short for a line and a tenth of the trace for a box (a tenth of a
    unit for one under a unit long), a box's hit up to a tenth past the end
    counted, a hit on an actor's cylinder a thousandth of the trace short
    --, the level's hulls bounded by their boxes as the original's are, and
    `FastTrace` and the engine's own clear-line tests (an actor's lights, a
    noise heard, a corona) asked along the line and not past it
    ([traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)). The fork stopped every hit a unit short and
    looked a unit past the end, a box's hit another tenth of a unit short:
    a script's `Trace` down found Liberty Island's floor at −303.0 where
    the original's finds −303.5, and a laser beam ended at 1999.0 where
    the original's ends at 1999.5; both are the original's now
    (`StandConsole`, `LaserConsole`, `CaptureConsole`), and so is where
    `SetLocation` fits the player under the first tripwire's ceiling
    (below). Walking's float takes the original's own measure
    ([small](#small)), and the fork's own reach test falls a step at a
    time, so that the tenth off one long fall does not leave it in the
    air. Other games keep the unit. What falls comes to rest where its
    last step's trace stops: `MeshConsole`'s crate, box and barrel 0.1
    over Liberty Island's pier (1.0 before), where the original's stop
    over it -- the crate and box together, in the air, 0.06 s after they
    are placed, falling half a unit a tick, their bottoms level at
    −301.75 (2.25 over the floor, the height of `DataLinkTrigger0`'s
    cylinder, which both stand in), the barrel later at −301.62. Lines
    and a box traced down there find the floor at −304 in both engines;
    what stops the original's is unread (logged per tick, 2026-09-28).
  - **The visible-actor iterators `VisibleActors` 311 and
    `VisibleCollidingActors` 312** (2026-09-28;
    [traces](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#traces)): `VisibleCollidingActors` lists each
    colliding actor, movers too, whose location lies within the radius
    (1000 for none), passes over a hidden one only when `bIgnoreHidden`
    asks, and asks the line to each as `FastTrace` does, where the fork's
    took the actors whose cylinders reached into a sphere, passed over the
    hidden unless asked not to, and asked no line -- a robot exploding at
    its death, or a MIB, Gunther or Anna (`HurtRadius`), hurt what stood
    behind walls and spared hidden actors. `VisibleActors` takes a radius
    of 0, its default, for no limit. Checked with `VisibleConsole`: both
    lists the original's at Liberty Island's start, where the fork's
    listed the police boat, its middle past the radius, and had
    `DataLinkTrigger0` the wrong way round; `FastTrace` stops at a closed
    door in both engines -- the original's BSP holds the movers' polygons
    too -- (19 of Liberty Island's movers alike).
  - **`Pawn.LineOfSightTo` 514, `Pawn.CanSee` 533 and
    `Actor.PlayerCanSeeMe` 532** (2026-09-28;
    [the senses](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-senses)): the original's --
    `LineOfSightTo` UT's, its reaches set by the other's `Visibility`,
    looking to the enemy's middle, to 0.8 of the other's height and to its
    cylinder's corners, `CanSee` the same with the LOS flag, and
    `PlayerCanSeeMe` a player's view within (collision radius + 3.6) ×
    100,000 squared units and 60 degrees of its line either way, then its
    `LineOfSightTo`. The fork's `LineOfSightTo` was three lines, to the
    other's middle, top and bottom, within its sight radius, `CanSee` that
    in its peripheral vision, and `PlayerCanSeeMe` asked every pawn, in a
    cone that took a length for a cosine, so that a player looking saw
    nothing: Deus Ex's `SequenceEvents` and `RandomEvents` that wait for
    the player to see them never fired. Checked with `SightConsole`: 85 of
    Liberty Island's 86 lines the original's (64 before), the other a
    patrolling bot a little farther off in the fork's run.
  - **`Actor.ParabolicTrace` 722**: the original's defaults, gravity the
    right way up, the zone's velocity, terminal velocity and water,
    per-step tracing, bounces, and failure to the start. NPCs judge a
    grenade's landing again.
  - **`Actor.GetBoundingBox` 724** with a test place or rotation puts the
    actor there for the moment: the HUD's highlight on a door and a
    `DeusExMover`'s area sit right. To check by hand: a door's highlight.
  - **`Actor.SetPhysics` 3970** takes the floor it is given as the base
    (its `SupportActor` event): a grenade or pool ball coming to rest
    moves with what it landed on.
  - **`Pawn.StrafeTo` 504 and `StrafeFacing` 506** take Deus Ex's speed:
    an NPC strafing in a fight runs at its full `MaxDesiredSpeed`.
- **`Actor.SetLocation` 267 is the original's `FarMoveActor` now**
  (2026-09-26, fitting in and encroaching 2026-09-28;
  [teleporting an actor](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#teleporting-an-actor),
  [the zone an actor is in](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-zone-an-actor-is-in)). What
  each was:
  - the actor kept its old zone until physics next moved it (a teleport
    into a reverb zone read the old one); it moved when static or not
    `bMovable`; it kept its `OldLocation`, was not marked
    `bJustTeleported`, and carried what stood on it;
  - `ZoneChange` ran with `Region` already the new zone, so an item or a
    decoration falling into water never splashed;
  - the fork destroyed stray inventory in a `bNoInventory` zone and
    carcasses in a `bDestructive` one, and set pain zones' `PainTime`,
    which Deus Ex's scripts do themselves -- other games keep that;
  - the actor was fitted in by trying whole collision sizes up, down and
    aside, where the original's `FindSpot` pushes it off the walls along
    each axis and then its box's corners -- under the ceiling at Liberty
    Island's first laser tripwire the fork moved the player 43 units down
    and now 7.2, as the original does (7.7 until the traces stopped where
    the original's do, 2026-09-28, above); at the three tripwires below
    it neither moves the player;
  - nothing at the spot was asked: now what blocks it may stop the move
    (the actor's `EncroachingOn` -- an NPC's refuses another pawn or a
    brush) and hears it come (`EncroachedBy`), and only what does not
    block it is touched.

  Spawning is the original's the same ways since 2026-09-28: the actor
  fitted in by `FindSpot`, left as it is where it fits already, and
  destroyed after `PostBeginPlay` when something there stops it; and a
  mover moving into actors now asks each of them, where a typo had it ask
  only the first in each of its collision cells. Still the fork's own:
  its own reachability tests, `pointReachable` and `actorReachable`, keep
  the whole-size tries, and the shape of `AIDirectionReachable`'s walk.
  **A note corrected 2026-10-02**: this said the fork's
  `AIDirectionReachable` "walks its steps as real moves, zone events and
  touches included, and puts the pawn back as one; the original's are
  tests". That was wrong -- all three `Reachable*` move with `TryMove`
  and `dryRun` set, which returns before `FinishMove`, so no touch, no
  `Bump` and no `UpdateActorZone` runs: the probes raise nothing, which
  is what the original's `walkMove`/`flyMove`/`swimMove` do too. What is
  left is the walk's *shape*: the original steps along the direction by
  its collision radius held between 5 and 25 units, at most 100 steps,
  each step the engine's own walk/fly/swim move (walls, ledges and steps
  count), retrying a walk stopped by a ledge once with a step of
  `MaxStepHeight`, and stopping in the void, in a pain zone whose damage
  the pawn does not resist, and on entering water; the fork steps up,
  across and settles onto the floor itself, over at most 32 iterations,
  with its own fall. Changing that moves every wandering NPC's and every
  animal's destination, so it wants its own console as a proof.
- **`Object.DynamicLoadObject`** with a group (`Package.Group.Name`): the fork
  looks the rest up as one name and finds nothing. The game's scripts name
  no group.
- **`Object.Mid` 127** with a negative start: the original returns an empty
  string, the fork counted from 0 -- now the original's (2026-10-02; the
  start is clamped as an unsigned, so one before the string wraps past its
  end, [conversions](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives)).
- **`Actor.LastRendered` 723 and `Actor.InStasis` 721:**
  [out of sight](#out-of-sight).
- **`Actor.PlaySound` 264** with no radius, from an actor with no
  `TransientSoundRadius`: 800 units in the original, 1,500 in the fork --
  now the original's 800 (`USurrealAudioDevice::PlaySound`, 2026-10-02;
  a radius of 0 or less is 800, [small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)),
  which also works out the sound's priority `(1 - distance / radius) *
  volume` on it. A script sound with no radius of its own is heard
  800 units out where the fork heard it 1,500, and takes a free channel
  where the fork's lost one to a channel playing nearer.

## Housekeeping, not seen directly

- **`Object.CriticalDelete` 751** (20 call sites): the original frees the
  object at once, whatever still refers to it
  ([`CriticalDelete`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#criticaldelete)), and the game's callers
  delete objects of their own and drop their reference. The fork's stub
  leaves them to its garbage collector, which frees them later: no other
  difference, now that `CreateGameDirectoryObject` makes a new object each
  call as the original does (2026-09-25).

## Small

All landed 2026-09-25:

- **`Pawn.FindStairRotation` 524:** with Look Up Stairs on, the view
  eases toward looking down (−5,000) or up (5,400) a flight of stairs,
  or back to level, from a floor probe ahead at eye height with a frame
  of 0.33 s or less, as the original's
  ([`Engine.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)); the probe distances and the
  easing rate are the fork's own reading. To check by hand: Look Up
  Stairs on, the view tilting down UNATCO's stairs and easing level at
  the bottom.
- **`PlayerPawn.ResetKeyboard` 544** (every level change): reads the
  player's bindings from `User.ini` again, the original's effective
  behaviour in Deus Ex ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)).
- **`Actor.AIGetLightLevel` 700:** the light the AI-sight work computes
  (`AILightAt`); no script calls it.
- **`InputExt`:** `SET InputExt ...` lands in the key bindings as
  `SET Input` does, Deus Ex's input class being Extension's
  ([the original](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-engine-and-the-input)): the
  multiplayer key bindings bind once instead of failing on every map.
- **Window sounds:** a UI sound plays one unit from the player, turned
  left or right by the point's place across the screen (a quarter turn
  at either edge) when positional sound is on, the point defaulting to
  the window's centre and the volume to the window's own where set
  ([window sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#window-sounds)); an unset window
  volume falls back to full, the native default unread. To check by
  hand: with positional sound on, a click at a screen's left edge
  sounding from the left.
- **Walking over the floor** (2026-09-27): a walking pawn floats over its
  floor as the original's does -- 2.1 over where the original's trace down
  of `MaxStepHeight` + 2 stops, a tenth of it short: 4.8 for a
  `MaxStepHeight` of 25 --, its base the level on the world's floor
  ([walking](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)), where the fork's
  stood on the floor, short of it only by the unit its traces kept, with
  no base. Since 2026-09-28, the traces stopping where the original's do
  ([implemented, not as the original](#implemented-not-as-the-original)), it takes the original's own measure:
  a pawn that trace finds nearer than 1.9 goes up to 2.1. The original
  leaves one it finds 1.9 to 2.4 away as it is, where the fork's own step
  down has always just brought it nearer, so the fork's stands at 4.8
  each time and the original's anywhere from 4.6 to 5.1 as it came to
  rest. Checked with `StandConsole`: at Liberty Island's start the player
  stands at Z −256.20 in the fork and −256.25 in the original, both on
  `LevelInfo0`, a line down finding the floor at −303.5 in both, where
  the fork's stood at −260.00; proving runs on Liberty Island, UNATCO HQ
  and Battery Park are clean.
- **The player's input, before its physics** (2026-09-28): Deus Ex's
  player reads its input and runs `PlayerInput` and `PlayerTick` in its
  own tick before its state code and physics, as the original's actor
  tick does for a pawn with a player (`AActor::Tick`,
  [a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)), so its physics take the move it
  makes the same tick; the fork read it after them, a tick late -- a
  frame of lag, 33 ms at the handheld's 30 frames a second. Checked with
  `StandConsole`: from the walk's second tick the player's acceleration
  is the original's to the thousandth ((780.74, −624.86), its full
  `AccelRate` of 1,000), where the fork's was still the script's 2,400
  and its velocity 0; after the two walks it stands within a unit and 7
  units of the original's, on the same step. The console walked the
  fork less than half as far before it set the forward axis a held key
  gives: the original's input scales an axis the console sets, the
  fork's uses it as it is, and 300 made an acceleration of 120 ([scripted
  runs](DEVELOPMENT.md#scripted-runs-of-both-engines)).
- **The field of view** (2026-09-28): the player's `DefaultFOV` is Deus
  Ex's own config, 75 (`[Engine.PlayerPawn]` in `User.ini`), as the
  original's. Upstream read it from a key of its own, `MainFOV`, which the
  game's ini lacks, and so made it 90: the first time a weapon came up the
  view reset to it, the whole scene wider and the weapon smaller and
  higher. Checked with `ViewConsole`: the view stays at 75 with the
  Dragon's Tooth in hand, the scene and the blade where the original's
  are, but for the idle sway. The key is neither read nor written for
  Deus Ex now; a `MainFOV` an earlier run saved is left unread.
- **A long frame** (2026-09-27): the actors' step is at most 0.4 s, as
  the original's level tick holds it, the level's clock taking the whole
  time ([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)): a load or a hitch moves
  nothing further in one step, where the fork's steps reached 1 s. Not
  carried: its floor of 5 ms, the step of every tick past 200 frames a
  second, which would run the game fast there.
- **The console's `GET` and `SET`** (2026-09-27), as the original's
  ([`GET` and `SET`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)): a class found by
  its name alone in any package loaded, where the fork wanted its package
  -- the game's menus name `DeusExMPGame`, `DXMapList`,
  `MenuScreenHostGame`, `Player` and `DeusExPlayer` so, and the
  multiplayer Host screen's settings were neither read nor set --; `GET`
  gives a string without the quotes the fork put round it, and an object
  with its path name; `SET` takes the rest of the line as the value (a
  server's name has spaces) and sets it on every object of the class and
  its subclasses, then the class's defaults, then saves the class's
  config. Checked with `GetConsole` against the original: each kind of
  property alike, and after `SET`s of `DeusExMPGame ScoreToWin`, a spaced
  `ServerName` and `PlayerPawn MouseSensitivity` the same values read
  back, the player's own sensitivity changed and the same keys written to
  each run's inis.

## The command line

The original's command line reaches the fork as one string, `--cmdline=`,
which the recreated launcher's `run-game.sh` passes on as the player gave it
(2026-09-27; [running it](ENGINE.md#running-it)); its flags are found as the
original's code finds them
([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)).
What the fork does with each:

- **The start URL**: with `-hax0r` or `-server`, the first word -- the
  second after `SERVER` -- unless it starts with `-`; else the game's own
  start, `DX.dx`
  ([starting the engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine)).
- **`-server`**: a dedicated server, as `--server`.
- **`INI=` and `USERINI=`**: the inis read and written back, as `--ini` and
  `--userini`.
- **`EXEC=<file>`**: `exec <file>` on the player once the first map is in.
  The console's `exec` runs a file's lines as commands, as the original's
  does; it had none.
- **Safe mode's flags**
  ([what the original does with each](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md#flags-the-launcher-emits-safe-mode)):
  - `-nosound`: no sound and no music, as the original makes no audio
    subsystem then. The fork keeps its device, on OpenAL Soft's null
    driver.
  - `-defaultres`: the window and fullscreen sizes 640×480 for the run,
    not saved.
  - `-nohard -noddraw`, "Run the game in a window": a window. The original
    gets there through its software renderer, which has no fullscreen mode
    without DirectDraw; the fork has no software renderer, so the window is
    what it keeps of the two, and either flag alone does nothing.
  - `-nojoy`: no pad at all -- neither the fork's own pad handling nor its
    buttons as keys.
  - **Nothing to do:** `-no3dsound` (the original's turns off A3D or EAX
    hardware; OpenAL Soft mixes in software), `-nommx`, `-nokni` and `-nok6`
    (the original's mixer picks its routines by them; the fork has no such
    routines), and `-safe`'s no DirectInput (the fork's input is SDL's).

## Mods

What a mod needs of the engine is the original's loader (2026-09-27):

- **Its folders.** A mod's packages and maps are found by the `Paths` of
  its own ini (`--ini`), relative to `System` and written with backslashes
  as the game's are, and by their extensions in any case, as the original
  finds them in any case
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
- **Protected packages.** An export whose flags give it no context --
  client, server or editor -- is never made, a reference to it None, as the
  original's linker has it: the live servers' anti-cheat packages (ANNA,
  DXNMS) point their classes' `ScriptText` at such exports, where the fork
  used to make them and fail. A save's exports are all made whatever their
  flags: the fork's saves before 2026-09-27 wrote spawned actors without
  them ([saving](#saving-loading-and-travel)).
- **Config files.** A class's config file that is not there is empty -- its
  properties keep their defaults -- until the class writes it, as the
  original's ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  DXMTL's classes read a `DXMTL.ini` no player without the mod has, where
  the fork failed. A key with an empty value is a value, as the original's
  config cache has it (2026-09-27): a string takes it empty, a name, object
  or class None, a float 0, and an int, byte or bool keeps its default --
  what the original's `ImportText` makes of no text. The fork took an
  empty value for a missing key. And of a key given twice in a section the
  last counts, as in the original (the fork took the first) -- but for the
  player's own settings, which the fork reads again by hand as it spawns
  one, still the first.

Checked: a mod laid out as Deus Ex's are -- its own folder with `System`
and `Maps`, named by relative, backslashed `Paths` in its own ini -- holding
a custom map and its two packages as a live server had sent them
(`DXMB_Mini_Dust`, `CDX_154`, `MapDirectorV1`): the fork played the map
standalone, its textures found; and the live servers' mods loaded on the
fork as their client ([below](#multiplayer)).

## Multiplayer

The fork joins a server and sees its world (2026-09-26): a client's side of
[the network](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md) up to its player possessed. An address in a URL
joins it -- the Join screens' `open`, or the command line --, while the
menu's map plays on; the driver is the fork's own over UDP with
`[IpDrv.TcpNetDriver]`'s timeouts, the connection, its channels and the
handshake the original's; the map loads as a client's; the package map
numbers as the original's; actor channels spawn or find the server's actors,
take their properties and run the server's calls, `PostNetReceive`'s moves
and unpacking with them; the server's pawn is possessed. Until it arrives the
fork draws no world. A server lost after the join, or one that refuses the
player then, takes the fork to its Entry level (below); a join refused or failing leaves it in the level it was in,
showing why; quitting or leaving closes the connection.

Checked against the original, run as a listen server on DXMP_Cathedral
(`ServeConsole`): the fork joined twice, the server logging `Join succeeded`
for each and the second's leaving as a close, and showed the level from its
player's spawn with the HUD and the belt's items; its handshake matched the
server's 32 packages, `CoreTexDetail.utx` among them by its heritage GUID.

The client plays (2026-09-26): its calls to the server go as the original's
rule sends them -- `ServerMove` and the rest --, a simulated proxy runs only
simulated functions, actors tick by their roles (another player's pawn
eased along its velocity, the local player's physics in its moves), the
viewport takes the speed and update intervals the moves are paced by, and
the frame rate is capped at the speed over 64. Checked the same way: the
fork's player walked into a wall and slid along it, and the server's log
ended with it at the fork's position exactly, no correction sent; the
host's player, walking to and fro in front of it, moved smoothly on the
fork between the server's updates. At rest the fork's player stood 3.75
units lower than the original's server had it, X and Y exact: the
original's walking floats a pawn over the floor, which the fork's does too
since 2026-09-27 ([walking over the floor](#small)) -- the fork's player at
Z −64.20 and the original's server's view of it at −64.25.

The fork serves too (2026-09-26): a map opened with `?listen` listens on
`[URL]`'s port before its game begins; the handshake's server side is the
original's, with the same packages in the same order; `JOIN` spawns the
player as the original's `SpawnPlayActor`, possessed by a Player object for
its connection. Checked with the original as the client: it joined the
fork's DXMP_Cathedral and the fork spawned its player.

The fork's server replicates (2026-09-26): each tick, before the packets go,
each client is sent what [the original's server](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#replication)
would send it -- the viewer, the actors due, their priority and relevancy,
channels opened and closed, and each actor's changed values against what
that client last got, with the original's roles, temporaries and resends --,
and what a client sends is taken only as the original's server takes it. A
client's pawn on the server runs its state code and timers, moving by the
client's moves; the animation natives pack `SimAnim` for clients; the local
player's pawn is simulated on clients, as the original spawns it. Every
replicated value goes through its script replication statement, but for
what the original's native lists decide instead: eight engine classes'
values, whose lists hold their statements but for a few
([the lists](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#the-native-lists)),
the fork's as theirs since 2026-09-27 -- `SimAnim`, `AnimMinRate` and
`bAnimNotify` go when `AnimSequence` does, where Deus Ex's statement sent
`SimAnim` to no simulated proxy and the original's clients saw the fork
server's pawns run with their animations frozen (the rate 0 in
`JoinConsole`'s log, the original's own rates since); the blended
animations and `PlayerRestartState` go never; a player replication info
sends `Actor`'s values in its first bunch only, and an always-relevant
inventory item after its first only `bHidden`; a simulated item with an
ambient sound sends its place to all but its owner. Checked with the original as the
client: it was welcomed, possessed its pawn, saw the map's actors and the
host's player walking to and fro in front of it, and walked; the fork's
server moved its pawn by its moves. At the client's default 2,600 bytes a
second one actor goes a tick, so its own `PlayerReplicationInfo` came some
8 s in, behind the map's weapons -- the order the original's priorities give --
and the original's `ReplicateMove` read the missing one's `Ping` meanwhile.

The server calls its clients too (2026-09-26): a call on an actor a
client's player owns goes to that client as the original's rule sends it,
the actor sent first if the client has no channel for it. Checked the same
way: the original client got its music, skins, augmentation displays, the
server's time and a position correction, and ended exactly where the fork's
server had it. The game's own check that a joining player's console is the
stock one then ran as well -- and disconnected the scripted client, as the
original's server would; the net tests' server game leaves it out
([the consoles](DEVELOPMENT.md#scripted-runs-of-both-engines)).

A listening fork spawns the `ServerActors` its ini lists (2026-09-26), as
the original's listen does, and reports Deus Ex's engine version and the
machine's name as the original does. Checked on this machine against the
original: the LAN beacon's and the query answerer's replies are the
original's word for word, the host name too -- empty, from the ini's
`ServerName=`, since the fork takes an empty value as one
([config files](#mods); 2026-09-27: it had the class default, "Another UT
Demo Server"). The
uplinks the game's own ini lists announce nothing, in either engine: an
uplink stops as it begins without `DoUplink`, which that ini does not set
([the master server](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-master-server)), so a game
hosted from the menus is never listed. With it set, against a master on
this machine (2026-09-27, `fakemaster.py`), both engines' uplinks sent the
same heartbeat from the same port, and their query answerers gave the
master the same `\validate\`, `\basic\` and `\info\` answers -- the
host name alike since the empty value's fix.

Downloads go both ways (2026-09-27), as
[the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#downloads): at `WELCOME`
each package is found on the paths by its name or in the cache by its GUID
-- then loaded from there under its name --, and the rest are downloaded
one after another over file channels, when the client allows downloads
(`[IpDrv.TcpNetDriver] AllowDownloads`) and the server flags the package
downloadable, each into the cache as its GUID with `CacheExt` (`.uxx`, as
the original writes). A package of the same name but another GUID ends the
join as a version mismatch before the map loads, and the level plays on;
each join finds its packages afresh. A package a join loaded from the
cache goes at the next map load that does not use it, as the original's
map load collects it (2026-09-27): another server's package of that name
then loads in its place, where the fork kept the first until it restarted.
Checked with `RejoinConsole` and two fork servers one after the other
offering a package of one name with two GUIDs (`DXCAP_SERVERPKGS`): the
fork's client, as the original's, joined the second after going back to
the menu, where, the release left out, its join failed as a version
mismatch. The cache is cleaned as the engine starts. A server sends any package of its list flagged downloadable; its
list is the map's, the `ServerPackages` and the game class's package, as
the original's. A join shows the original's lines -- connecting, receiving
with the size and the share done, each failure -- in the game's message
window; F10 there cancels it (`CANCEL`); `DISCONNECT`, `RECONNECT`,
`NETSPEED` and `LANSPEED` are the original's.

Checked on live servers (2026-09-27, the owner's go-ahead): the fork
downloaded from original servers -- 23 packages, some 23 MB in about a
minute, from an ANNA server, and a custom map and two packages from
another --, loaded their mods and joined; on a DXMTL server, with the stock
console, it stayed in the game, took the server's calls and position
corrections and drew the map with the HUD, and walked there (the
harness's live mode, below): on a live MTL deathmatch server it walked
5 s into a wall and stood, the server's one correction at the stop 1.5
units back -- when the fork's traces still stopped a unit short, not
checked since they stop where the original's do
([implemented, not as the original](#implemented-not-as-the-original)). The original downloaded a map it
lacked from the fork's server, then joined and walked, and the fork did the
same against its own server. With a scripted console the game's console
check (above) disconnects the run seconds in, on every server; the
harness's live mode keeps the stock console and drives the run from the
engine instead ([live servers](DEVELOPMENT.md#scripted-runs-of-both-engines)).

A server travels, and its clients follow (2026-09-27), as
[the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#server-travel): `servertravel`
and a game's own map change send each client a relative `ClientTravel`,
which a client takes relative to its server's address; the server takes no
one new until its countdown runs out, then loads the next map, listening
again with the same options. Checked both ways: the fork's server travelled
from DXMP_Cathedral to DXMP_Smuggler with the original following, and the
original's with the fork following, each client in the next map and playing
(`TravelServeConsole`, `TravelJoinConsole`).

A client whose server connection closes, or whose server sends `FAILURE`
after the join, goes to its Entry level, a new player spawned there, as
the original's client browses `?failed` (2026-09-27; its `?entry` alike):
in a travel only until its join to the next map loads, which it then
does; with no join pending, "Connection failed" shows for 6 s and the
player stays there. The fork kept the old level through a travel's join,
and went back to the menu's map for a lost server. Checked: in a travel the
fork's client spent a second in `Entry.dx` between the maps, as the
original's does; with its fork server killed mid-game each engine's client
timed out into the Entry level with "Connection failed" over the HUD, the
two frames alike but for the fork's stats.

A dedicated server too (2026-09-27): `--server`, as the original's
`-SERVER` ([the network](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#packets)): no window,
sound or picture, no player or console of its own, no Entry level; the map
listens without `?listen` and travels as a listen server's does; it ticks
`NetServerMaxTickRate` times a second, `LanServerMaxTickRate` with
`--lanplay`. A map from the command line now has the game's port in its
URL, as any URL has -- it had 7777, so a dedicated server's query answerer
bound 7777, and a local game's `GetLocalURL` read `:7777/` in front of the
map. Checked: the original and the fork joined the fork's dedicated
DXMP_Cathedral and walked; its LAN beacon and query answerer replied
(`listenserver` False) on their ports; it idled at under 1% of a core.

A client's login is the original's to the byte (2026-09-27): to a server
that logs world stats (`STATS=1`) it carries the player's checksum -- the
MD5 of its name and its world stats password (`ngWorldSecret`), each as
the original's two-byte characters, in lowercase hex, or `NoChecksum`
without a password, where the fork sent `NoChecksum` always --, and its
URL the player options the user ini's `[DefaultPlayer]` has, as the
original's default URL takes them, where the fork added seven, the unset
ones empty (`team=`, `skin=`, `Face=`, `Voice=`, `OverrideClass=`).
Checked with `DXCAP_STATS` ([net tests](DEVELOPMENT.md#scripted-runs-of-both-engines)) against a fork
server logging world stats: the original's login and the fork's alike,
`Index.dx?Name=Player?Class=DeusEx.JCDentonMale?Checksum=2a08f7474acf9013c4ec638e29b1997c`,
the MD5 the formula gives.

A bunch goes into the last one sent, one header for both, when both are
the same channel's and that one still ends the packet being built with no
ack written since, as the original's (2026-09-27): the fork sent each on
its own. Checked both ways: the original's client took the fork server's
merged bunches and its server a fork client's, each side joining and
walking.

The scripts' sockets are the original's now (2026-09-26,
[the script's links](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-scripts-links)): `InternetLink`'s
lookups, conversions and GameSpy answer, `UdpLink` and `TcpLink` with their
events, set up with the original constructor's link and receive modes, which
the class defaults lack. Checked against the original with a scripted run
(`NetConsole`): the same conversions and answers, and both asked
333networks' master server (`master.333networks.com`, TCP 28900) for Deus
Ex's servers as the Join Internet screen asks, got the same list, and pinged
the first five for their status over UDP; then the game's own Join
Internet screen, opened in both, asked and pinged alike, and the fork's
listed the live servers with their maps, game types, players and pings.
The game's own `MasterServerAddress` (`[DeusEx.MenuScreenJoinGame]` in
`DeusEx.ini`) names GameSpy's, which closed in 2014: the screen lists
servers once it names a live one, as the scripted runs' ini does.

## Not needed for single player

`DumpLocation` (21 stubs: Ion Storm's bug-location tool, though
`DeusExGameInfo.Login` calls `HasLocationBeenSaved` on every map),
`StatLog`/`StatLogFile`, `DebugInfo` (compiled out in the
original too: [`DebugInfo`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#debuginfo)), `SaveTimeDemo`,
`Commandlet.Main`, and `Object`'s `clock`, `unclock` and `CyclesToSeconds`
([timing by hand](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#clock-unclock-and-cyclestoseconds)). The
network's -- `InternetLink`, network numbers and addresses -- are
[multiplayer](#multiplayer)'s. `ComputerWindow` has 22 stubs, but no script calls them; the InfoLink's text
window, its only user, calls only implemented ones. No script calls
`ClipWindow`'s unit sizes, `GC`'s `PushGC`, `PopGC`, `CopyGC` and
`Intersect`, or 16 more of the windows' stubs.
