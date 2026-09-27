# What Surreal Engine lacks

Deus Ex's natives and native C++ against the engine fork's: what is missing or
wrong, what the player sees of it, and the work that would fill it. What each
original function does is in its DLL's doc ([the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#the-binaries));
what each patch changed is in [what the fork changes](ENGINE.md#what-the-fork-changes).
Which item is taken up, and when, is the owner's call; the decided order is
[`ROADMAP.md`](ROADMAP.md). With M0-M6's code landed (2026-09-25), most
sections record what changed and its by-hand check; what stays the fork's
own is noted in place, and [multiplayer](#multiplayer) is the one whole
area left.

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
  reference Liberty Island save loads and plays): their saved event manager
  is recognized and skipped, its listeners lost
  ([hearing](#hearing-the-ai-event-system)); what its NPCs still do is to
  check by hand.
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
(2026-09-24), and the original game's load past their saved event manager
now (2026-09-25), the manager skipped and its listeners lost until its
exact bytes are read ([hearing](#hearing-the-ai-event-system)).

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
  but never read. The original's exact CRC polynomial is unread; it
  matters only for reading the original game's own saved chains.
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

The fork saves the manager in a layout of its own. The original game's
saved manager -- its exact bytes unread -- is recognized and skipped on
load with a message, its listeners lost: what an original save's NPCs
still hear is to be checked when those bytes are read.

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
  visibility test passes, a spawned actor starts 10 s undrawn, and a zone's
  is the frame's own zone and both zones a visible portal borders (this
  renderer draws the BSP whole behind a span clipper, not zone by zone as
  the original's `OccludeBsp`). `LastRendered()` answers the time since,
  never below 0 -- it returned 0 for every actor, so everything counted as
  just drawn and the scripts spared nothing: `ParticleGenerator`s unseen
  for 2 s, `bTickVisibleOnly` NPCs' enemy and body checks, light-beam
  checks, and NPC shadows laid each tick all ran, on the handheld too. A
  decal's own `LastRenderedTime` is stamped when drawn and never read, as
  the original's: a `Shadow` never counts as drawn.
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
  of a hidden item is paid, the look is the same.
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
are and as far apart, but fainter, over a floor and walls the fork draws
brighter than the original's frames show them -- in linear terms, the
frames' gamma taken out ([brightness](#brightness)), 3.7 to 4 times,
where `FlashConsole`'s outdoor view is about 2.2 times (2026-09-27) --,
which the capture renderer (`OpenGLDrv`, not the game's `D3DDrv`) may
account for in part: the dashes are not yet compared for brightness.
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
  mover moved or turned since, not every frame. Still the fork's own:
  the maps are floats, converted for the Smart Pro's GPU on the CPU
  (engine patches 0002 and 0024), where the original's are bytes -- and
  the fork clamps each texel at 1.0 on upload, where the original's byte
  reaches twice unit brightness (64 unit, 127 double:
  [the driver](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness)), so its
  brightest lights top out at half the original's overbright; a
  changed map goes to the GPU whole in both; the lookup is a `std::map`
  where the original's cache hashes and first checks the item it found
  last; and a still-shaped animated light is re-run rather than kept as
  its shadowed light and rescaled. To check by hand: a flickering
  sconce's wall (the 'Ton's entrance), a pulsing light throbbing, and a
  triggered light going dark, each with its shadows still there.
- **`NoDynamicLights`**: works now (2026-09-25) -- animated lights count
  as still and bake into the static map, and moving ones are left out;
  by hand with the ini setting on.
- **`LE_CloudCast`**: the fork builds it once, and its effect's shape is
  a placeholder (upstream's to-do), as are the torch and fire wavers and
  the watery shimmer; in the original the cloud shape changes over time
  and is run every frame, and the wavers dim each texel by up to 5% and
  20% as a map is merged. The formulas are unread.
- **Meshes.** The fork keeps the original's now (2026-09-25): the
  candidates from the actor's leaf of the BSP plus the moving lights near
  it and last frame's, the strongest picked first -- statics until 8, none
  below an eighth of the strongest, `bCorona` lights counting -- shadows
  checked through the BSP every 16 frames, each light fading in and out
  over about a third of a second and lighting as it fades, and the
  original's per-vertex formula: the (cos + 1)^2 - 1.5 diffuse, the
  6 cos^2 highlight toward the eye, linear falloff, 1.4 x `ScaleGlow`,
  ambient added, channels clamped. One knowing difference: the moving
  lights come from the fork's light tree near the actor, not the leaf's
  own list. Whether the brightness pairs with the fork's light maps as the
  original's does is judged against the original's display driver, read
  in [`d3ddrv-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md) (the light maps' brightness, the
  blends); a by-hand look first: an NPC
  under a street lamp, one walking from light into shadow (the fade), and
  a fire's glow on a face.

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
compared with them.

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

Seen in the check of the blade: the fork's weapon sits higher and smaller
in a 16:9 view than the original's -- the field of view at a wide
aspect, not yet looked into. To check by hand: the Dragon's Tooth in hand
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
  ([borders](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#borders)); margins, which the game never
  passes either, stay unimplemented. To check by hand: a selection
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

The fork's audio is its own, over OpenAL; the original's is `Galaxy.dll`
([`galaxy-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md)). Both scan every actor for ambient sounds
each frame, keep one record a channel and choose which sound wins alike.
Every difference read from both codes landed 2026-09-25; each bullet says
what changed, what stays the fork's own, and its by-hand check:

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
  never a conversation line. Measured (2026-09-26, a fan heard 400 units
  off in the open and 422 behind Liberty Island's rock): 10.0 dB down in
  the original, 9.9 in the fork, neither filtering.
- **Reverb.** Landed (2026-09-25): a zone with `bReverbZone` gives every
  sound its reverb -- 21 zones in 16 maps, Battery Park to the endgame
  (the data) -- set again only when the view target's zone changes,
  over OpenAL's EFX ([reverb](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#reverb)). The mapping is
  the fork's own, since Galaxy's reverb is a six-tap echo network and
  EFX's a reverb model: `MasterGain` the gain, `CutoffHz` a one-pole
  lowpass read at EFX's 5 kHz reference, the echo train's longest tap
  the decay time, its earliest the reflections delay; music stays dry,
  as the original's. OldUnreal's `ALAudio.dll` remains the
  read-if-needed reference for a closer take
  ([the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#the-binaries)). To check by hand: Battery
  Park's underground echoing against the open park, and the echo
  gone on stepping back out. Measured (2026-09-26, gunshots in Battery
  Park's `ZoneInfo5`, dry outside it in both): the original's ring about
  2 s before falling 60 dB under their peak, the fork's 0.74 s, and their
  tail, 0.5 to 1.5 s after the peak, is 32 dB under the shot in the
  original and 40 in the fork -- the mapping falls short, open.
- **Ambient sounds on lights.** Landed (2026-09-25): the fork scales such
  a sound by `LightBrightness` ÷ 255 -- a quarter or less for 235 of the
  402 actors in 46 maps that carry both -- and follows the light's pulse,
  blink, strobe or flicker with the renderer's own animation shapes,
  capped at 1, where the original reads its renderer's `GlobalLighting`
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame)); the palette light types stay
  steady, the fork's own. To check by hand: a security camera's hum
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
  instead of lasting one frame ([volume](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#volume)). Not
  carried: Galaxy's equal-sliders quirk, both scaled by the slider twice.
  To check by hand: the Speech slider moving a conversation's loudness
  mid-line and not the world's, the Sound slider the other way round.
- **Loudness.** Landed (2026-09-25): the fork plays the script's volume
  as the original does -- no rescale toward 1, no halving -- with
  fall-off linear from the sound to its radius and silent there, and the
  product capped at full, the Sound slider its ceiling
  ([volume](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#volume)). Not carried: the original's 1/256
  volume floor, an integer artifact. Other games keep the fork's old
  loudness. To check by hand: a humming light or a generator fading
  steadily on the walk away and silent right at its radius, not gone
  early; effects sitting louder against the music than before. A sound
  straight ahead records 2.5 to 3 dB quieter in the fork than in the
  original: the pan's, below.
- **Pan.** Not carried: the fork leaves a sound's place to OpenAL, which
  pans it hard, where the original turns the sound's angle into Galaxy's
  pan, at most seven-eighths of the way to a side
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame)). Measured (2026-09-26, a beep
  234 units off, 90° to one side): the far channel 5.1 dB under the near
  one in the original, 42 in the fork; straight ahead, each channel 1.8 dB
  under the side's near one in the original, 6.2 in the fork -- open.
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
- **`LevelInfo`'s clock.** The fork's main loop fills `Year` counted from 1900
  and `Month` from 0, as in its save dates; the original's are the full year
  and 1 to 12. In Deus Ex only `StatLog` reads them.
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
    laser tripwires.
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
  (2026-09-26, [teleporting an actor](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#teleporting-an-actor),
  [the zone an actor is in](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-zone-an-actor-is-in)) but for
  two things. What each was:
  - the actor kept its old zone until physics next moved it (a teleport
    into a reverb zone read the old one); it moved when static or not
    `bMovable`; it kept its `OldLocation`, was not marked
    `bJustTeleported`, and carried what stood on it;
  - `ZoneChange` ran with `Region` already the new zone, so an item or a
    decoration falling into water never splashed;
  - the fork destroyed stray inventory in a `bNoInventory` zone and
    carcasses in a `bDestructive` one, and set pain zones' `PainTime`,
    which Deus Ex's scripts do themselves -- other games keep that.

  Still the fork's own: fitting the actor in -- the original's `FindSpot`
  moves it a little, the fork's tries whole collision sizes up, down and
  aside (under a ceiling on Liberty Island: 7 units down in the original,
  43 in the fork); and no encroachment check at the spot. The fork's
  `AIDirectionReachable` walks its steps as real moves, zone events and
  touches included, and puts the pawn back as one; the original's are tests.
- **`Object.DynamicLoadObject`** with a group (`Package.Group.Name`): the fork
  looks the rest up as one name and finds nothing. The game's scripts name
  no group.
- **`Object.Mid` 127** with a negative start: the original returns an empty
  string, the fork counts from 0.
- **`Actor.LastRendered` 723 and `Actor.InStasis` 721:**
  [out of sight](#out-of-sight).
- **`Actor.PlaySound` 264** with no radius, from an actor with no
  `TransientSoundRadius`: 800 units in the original, 1,500 in the fork.

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
  flags: the fork's own saves write spawned actors without them.
- **Config files.** A class's config file that is not there is empty -- its
  properties keep their defaults -- until the class writes it, as the
  original's ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  DXMTL's classes read a `DXMTL.ini` no player without the mod has, where
  the fork failed. A key with an empty value is a value, as the original's
  config cache has it (2026-09-27): a string takes it empty, a name, object
  or class None, a float 0, and an int, byte or bool keeps its default --
  what the original's `ImportText` makes of no text. The fork took an
  empty value for a missing key.

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
fork draws no world. A server lost after the join takes the fork back to the
menu's map; a join refused or failing leaves it in the level it was in,
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
fork between the server's updates. At rest the fork's player stands 3.75
units lower than the original's server has it, X and Y exact -- in every
run pair so far, not yet looked into.

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
replicated value goes through its script replication statement, where the
original replicates eight engine classes' values by native lists instead
(not yet compared with their statements). Checked with the original as the
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
each join finds its packages afresh. The cache is cleaned as the engine
starts. A server sends any package of its list flagged downloadable; its
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
corrections and drew the map with the HUD. The original downloaded a map it
lacked from the fork's server, then joined and walked, and the fork did the
same against its own server. With a scripted console the game's console
check (above) disconnects the run seconds in, on every server
([live servers](DEVELOPMENT.md#scripted-runs-of-both-engines)).

A server travels, and its clients follow (2026-09-27), as
[the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#server-travel): `servertravel`
and a game's own map change send each client a relative `ClientTravel`,
which a client takes relative to its server's address; the server takes no
one new until its countdown runs out, then loads the next map, listening
again with the same options. A client whose old connection closes while its
join to the next map is pending stays in the old level until the next map
loads, where the original goes to its Entry level meanwhile. Checked both
ways: the fork's server travelled from DXMP_Cathedral to DXMP_Smuggler with
the original following, and the original's with the fork following, each
client in the next map and playing (`TravelServeConsole`,
`TravelJoinConsole`).

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

Not yet, of a client: the world-stats checksum (`NoChecksum` always). Each
bunch goes on its own: the original merges one into the last when both are
the same channel's, which only saves bits.

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
