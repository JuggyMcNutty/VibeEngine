# Differences from the original

Where the engine fork still differs from the original Deus Ex, feature by
feature. Each section opens with its status -- whether the fork matches the
original, the original's write-up, the console that compares the two --
then lists what still differs. What each engine patch changes is
[what the fork changes](ENGINE.md#what-the-fork-changes).

## How it is known

- **The audit**, [`vibe/tools/natives_audit.py`](../tools/natives_audit.py):
  every native the game's packages declare against the fork's -- a stub, a
  partial one, an iterator that makes none, or none registered -- with its
  call sites in the game's scripts; `--runs` adds which stubs a run's log
  fired. A stub logs once a session, naming the script function that called
  it, so a run shows which stubs a map reaches, not how often. Run it for
  the current list of stubs; this doc does not copy it.
- **Reading the DLLs**
  ([dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info)): C++
  that is not a native leaves no stub, and only the original shows whether
  an implemented native does what it does.
- **Scripted runs of both engines**
  ([scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)), by the
  consoles named below.

## Stops the game

Differs. An error the fork does not catch -- a script error, an unknown
native, a failed save -- exits the engine with 1, and the launcher shows its
crash banner. No map or action is known to reach one; the audit lists the
natives whose call would.

## Saving, loading and travel

Matches the original ([travel and saving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-game-engine-travel-and-saving),
[the save directory](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-save-directory)):
each engine loads the other's saves (`SaveConsole`, `LoadConsole`). Differs:

- The pre-travel prune leaves a carried decoration, which the original's
  destroys with the augmentations and skills (`PruneTravelActors`).
- A saved level keeps the destroyed actors the original's save drops.

## Flags

Matches the original ([flags](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#flags)):
each engine finds the other's saved flags. Mission scripts come up as the
original's: `MissionConsole` over all 83 maps finds every mission map's
script running. `DX`, `DXOnly` and `Entry` are not mission maps, and
`12_Vandenberg_Tunnels` (no `MissionScript` actor) and `99_Endgame4` (no
`DeusExLevelInfo`) run none in the original either.

## Conversations

Matches the original ([`ConSys.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/consys-dll.md)):
`SkipConsole` compares a skipped line's speech, `CaptureConsole` a jump to a
comment's label.

## What the player reads

Matches the original's parser ([`DeusExText.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusextext-dll.md)),
but for one quirk left out on purpose: at a comment (`NOTE`, `GOAL`,
`COMMENT`) with no end tag the original reads on past the text's end, so
`09_EmailMenu_ShipOps`'s listing depends on the memory after it. The fork
stops at the end, hiding the rest.

## Every NPC

### The native tick: `AScriptedPawn::Tick`

Matches the original ([the native tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#the-native-tick)),
run before the actor tick, in the original's order.

### Starting up

Matches the original ([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)):
every actor starts a level 10 s undrawn and ticks in its first tick, so a
pawn out of sight starts from its state code (`AIConsole`). A few pawns far
off count as drawn: [out of sight](#out-of-sight).

### Hearing: the AI event system

Matches the original ([the AI event system](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-ai-event-system)),
saved in its layout ([saved](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#saved)):
each engine loads the other's saves with every listener. The fork also reads
the manager as its own older saves lay it out.

### Moving: wandering and tactical movement

Matches the original ([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving),
[reaching](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#reaching),
[the search](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#the-search)):
`MoveConsole` (`move.py`) has 51 of Liberty Island's 52 pawns move within
15% (or 200 units) of the original's distance. A dead robot frozen in
`Dying` for good is the original's own behaviour, and the rest of a death
is alike too (`DeathConsole`). Differs:

- **The anchored search.** With the pawn anchored and a goal reachable from
  the anchor but not one of its own reach specs away, the original's route
  is the anchor; the fork's (`UPawn_Path.cpp`) is a navigation-point goal
  itself.
- **Terrorist10** moves 1,010 units to the original's 652. Both stop at
  PatrolPoint102 with no move target, but at 20 s the fork's finds a route
  on toward PathNode580 and walks some 8 s more, where the original's finds
  none.
- **`ReachConsole`**: 143 of 144 verdicts alike and 134 answers to the unit.
  The other ten, from the island's two patrolling security bots asked where
  their patrols had taken them, are 26 to 67 units off.

## Out of sight

Matches the original ([render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-time),
[which actors are drawn](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#which-actors-are-drawn),
[stasis and render time](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#stasis-and-render-time)):
whether an actor counts as drawn is tested piece by piece in the BSP walk,
and the proxies' rectangles are the original's (`AIConsole`). Beside it the
fork keeps Distant AI's own `LastVisibleFrame`
([its patches](ENGINE.md#settings-the-launcher-exposes)). Differs:

- **`GC.DrawActor` stamps no render time.** The original stamps an actor
  drawn through it, as the vision augmentation's heat sources are; the fork
  stamps only what the scene draws, so an NPC seen only through the
  augmentation does not count as drawn for stasis, `bTickVisibleOnly` or the
  event manager.
- **9 NPCs count as drawn at Liberty Island's start, to the original's 3**:
  five terrorists and a thug far off, whose rectangles show 1 to 9 pixels
  over the seawall and the pier's roof in the fork, where the original's
  edges close them: a pixel's difference in rasterizing.
- **One span buffer** for the frame, where the original keeps one per zone.
  A buffer per zone would change no test in 15,500 at the starts of Liberty
  Island, UNATCO HQ and Battery Park: the walls round a portal are drawn
  before what lies beyond it.
- **Drawing** is the fork's own box test, the depth buffer hiding the rest;
  only whether an actor counts as drawn is the original's.

## On screen

### Particles and lasers: render iterators

Matches the original ([render iterators](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#render-iterators),
[particles and lasers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/deusex-dll.md#particles-and-lasers)):
`LaserConsole` has the beams' red over the floor within 0.5% of `D3DDrv`'s.
Differs:

- **Occlusion.** The original occludes each item's sprite through its span
  buffer; the fork clips items to the view (and a portal's spans) before its
  BSP walk and leaves the rest to the depth buffer. The look is the same; a
  hidden item's cost is paid. Whether the proxy counts as drawn is the
  original's.
- **The particle curves** keep the documented shapes: a drift of −3 to +2 a
  frame, growth from 0.01 to 3 times the draw scale over the life, the fade
  with the remaining life, the rise as acceleration at the rise rate. The
  original's exact curves are unread.

### Coronas

Matches the original ([coronas](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#coronas);
`CoronaConsole`) but for one thing: the fork takes every dynamic corona
light in the level, the original those in the player's leaf. No Deus Ex map
places one, so only a spawned one would show from farther.

### What a pawn holds

Matches the original ([a pawn's attachments](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-pawns-attachments));
`HeldConsole`.

### Mesh detail

Matches the original's vertex budget ([mesh detail](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-detail)),
but for the morph: the fork slides linearly over the `LODMorph` zone, and
the original's exact curve is unread.

### Lighting

Matches the original's light maps, mesh lighting and the faces a mesh draws ([lighting](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#lighting)):
`ViewConsole`, `LaserConsole`, `MeshConsole` and `CoronaConsole` come out
within a level or two of 256 of `D3DDrv`'s frames. Still the fork's own:

- **Light maps**
  - floats, converted on the CPU for the Smart Pro's GPU (engine patches
    0002 and 0024); a changed map goes to the GPU whole;
  - the lookup a `std::map`, where the original's cache hashes and first
    checks the item it found last;
  - a still-shaped animated light re-run, where the original keeps it as its
    shadowed light and rescales it;
  - blink and strobe on the fork's timing; the original's blink goes by the
    lowest bit of its turn count and its strobe turns over each frame, both
    at the frame rate;
  - a flicker drawn at most 25 times a second;
  - the sample points of a map's texels;
  - the shapes of the effects whose original is unread (the waves' and the
    rest), on the plain falloff, as the original's spotlight is;
  - `LE_CloudCast`, the plain shape, built once, where the original re-runs
    it every frame (its table counts the shape as changing): the same light.
- **Meshes**
  - the moving lights come from the fork's light tree near the actor, not
    the leaf's own list;
  - the `AmbientGlow` pulse runs on the level's time, the original's on the
    viewport's.

### Brightness

Matches the original's picture ([gamma](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#gamma),
[the light maps' brightness](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness),
[textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#textures)):
the Brightness slider raises the picture to 1 / (2.5 × Brightness), as
`D3DDrv` sets its ramp, and the launcher's other gamma mode (XOpenGL's
curve) takes the same product. Deus Ex's shaders take nothing off a
texture's colours (`NO_DARKCLAMP`), as `D3DDrv` draws 32-bit textures on any
display of 24 bits or more; other games keep upstream's `darkClamp`. How a
fork shot meets the original's frames:
[scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines). Differs:

- `MeshConsole`'s Liberty Island pier, where nothing is in hand, is 1.7 to
  3.0% under `D3DDrv`'s frames, half a level to a level; why is unread. Its
  crate, box and barrel are within 0.8%, Paul Denton a level over, and
  `LaserConsole`'s corridor within 0.9%.
- **Known risk:** every device compiles its scene shader without
  `darkClamp`, but only the GL device's has run (the comparisons run GL);
  the Vulkan device's never has.

### The screen flash

Matches the original ([the rest of a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-rest-of-a-frame));
`FlashConsole`.

### Fire, water and ice textures

Matches `Fire.dll` step for step ([`fire-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md)),
checked against its own routines run in an emulator. How a mesh shows its
textures is the original's too
([mesh textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#mesh-textures)).
Differs:

- **The pace** ([stepping](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md#stepping)):
  a texture with no `MaxFrameRate`, as most of Deus Ex's have, steps every
  frame in the original, faster at a higher frame rate; the fork steps it at
  most 60 times a second, the original's pace at 60 frames.
- **The wave lighting table** can be one step off at a single entry, in 3 of 200 settings: the
  fork takes the C library's cosine, the original the x87's, and they differ in the last bit.
- The original's fire repeats itself from run to run; the fork's varies
  (`LaserConsole`).

### Head turns and lip sync: blend animations

Matches the original ([blend animations](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#blend-animations)).

### Lists

Matches the original ([lists](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#lists)).

### The UI

Matches the original ([the UI in front of the game](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#the-ui-in-front-of-the-game),
[drawing](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#drawing),
[small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#small)):
`GetConsole`, `BeltConsole`, `BorderConsole`, `ColorsConsole`, and
`ChoiceConsole` for a conversation's choices. Differs:

- **`GC.DrawActor` ignores `bConstrain`**: the vision augmentation's calls
  cover the whole view. Its render time: [out of sight](#out-of-sight).
- **Keyboard focus** ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#keyboard-focus)),
  read from both codes, unchecked in a run:
  - `MoveTabGroup` (Tab, Shift+Tab, and a focus move with nowhere to go in
    its group) gives the focus to the next tab group window itself, among
    every visible tab group under the root in `tabGroupIndex` order. The
    original gives it to the first traversable window of the next group in
    the topmost modal's table.
  - With no focus, `MoveFocus` seeds it from the topmost modal's own group,
    where the original goes through `MoveTabGroup`.
  - The root's tick seeds the focus whatever the topmost modal's
    `focusMode`, so the keypad (`MFOCUS_EnterLeave`) gets a focused key as
    it opens, and seeds nothing with no modal up. The original's leaves an
    `MFOCUS_EnterLeave` modal alone and, with no modal up, seeds through the
    root.

## Sound

Matches `Galaxy.dll` ([`galaxy-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md)):
Deus Ex's sounds are mixed as its mixer mixes them (pan, volumes,
resampling and reverb, at `OutputRate`) into one OpenAL source.
`SoundConsole` and `SkipConsole` record both engines. Differs:

- The music plays on an OpenAL source of its own.
- Not carried: Galaxy's quirk with the Sound and Speech sliders equal, both
  kinds of sound scaled by the slider once more.
- Not carried: the original's 1/256 volume floor, an integer artifact.
- An ambient sound on a light follows its blink and strobe on the fork's
  timing ([lighting](#lighting)).
- A sound's ID packs a number the fork gives each object that plays one,
  never given out twice, where the original packs the object's index
  ([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small));
  the slot and `bNoOverride` are packed alike.

## Implemented, not as the original

Every other native the scripted runs compare matches the original
([the natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-natives),
[`Engine.dll`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md)):
the traces (`TraceConsole`, `VisibleConsole`), sight (`SightConsole`), the
reach tests (`ReachConsole`), falls coming to rest (`RestConsole`),
`SetLocation`, and `Object.Mid` 127 at its edges, its end's clamp known from
`MidConsole`'s ten probes of both. `LevelInfo`'s clock is the original's
([small](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).
Differs:

- **`DeusExPlayer.GetDeusExVersion`**: the fork's own string, by choice, so a player can tell
  which engine they run: `1.112fm VibeEngine <commit> (<date>)`, the engine's commit and its
  date as built (`-dirty` after the commit with uncommitted changes), shown under the main
  menu ([the version](ENGINE.md#running-on-our-devices)). The original's is
  "Mon Mar 19 12:06:14 2001 v1.112fm".
- **Landing** (`processLanded`,
  [moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)):
  its other branches are not ported. Those are a decoration nudged off a
  ledge it overhangs (four traces down at its corners, up to five times), a
  carcass's bounce off a slope, a `bBounceVelocity` zone throwing again what
  is not a pawn, and a pawn with nothing under it fitted (`FindSpot`) and
  pushed on at random.
- **`Object.DynamicLoadObject`** with a group (`Package.Group.Name`): the
  fork looks the rest up as one name and finds nothing. The game's scripts
  name no group.
- **Known risk:** a pawn landing on an NPC or the player bounces off and
  stomps it, and one landing on a decoration that cannot be a base is pushed
  off, through the scripts' `SupportActor`; untried in play. Check: jumping
  onto an NPC's head.

## Housekeeping, not seen directly

**`Object.CriticalDelete` 751** (20 call sites) is a stub. The original frees
the object at once, whatever still refers to it
([`CriticalDelete`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#criticaldelete)),
and the game's callers delete objects of their own and drop their
reference. The fork has no object lifecycle: nothing deletes a `UObject`,
and the collector never runs (`GC::Collect`, its one call commented out in
`Engine::UnloadMap`). A bare delete would leave the package's object table
pointing at freed memory, so freeing at once waits for a lifecycle. A
deleted object lives on unreferenced, which costs memory and nothing else:
`CreateGameDirectoryObject` makes a new object each call, as the original's
does.

## Small

- **`Pawn.FindStairRotation` 524**: the probe distances and the easing rate
  are the fork's own reading ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#small)).
- **Window sounds**: an unset window volume falls back to full; the native
  default is unread ([window sounds](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/extension-dll.md#window-sounds)).
- **Walking over the floor** ([moving](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#moving)):
  a walking pawn floats over its floor as the original's does, but the
  original leaves one its trace finds 1.9 to 2.4 away as it is, where the
  fork's own step down always brings it nearer. The fork's player stands 4.8
  over the floor every time (a `MaxStepHeight` of 25), the original's 4.6 to
  5.1 as it came to rest (`StandConsole`).
- **A long frame**: the actors' step is at most 0.4 s, as the original's
  ([a level's tick](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#a-levels-tick)).
  Not carried: its floor of 5 ms, the step of every tick past 200 frames a
  second, which would run the game fast there.
- **Input axes**: an axis a console command sets is used as it is; the
  original's input scales it (the harness's `StandConsole` and `JoinConsole`
  allow for it).
- **The field of view** matches: the player's `DefaultFOV` is Deus Ex's own
  config, 75 (`[Engine.PlayerPawn]` in `User.ini`). Upstream's own key,
  `MainFOV`, which the game's ini lacks (it would make the view 90), is
  neither read nor written for Deus Ex.

## The command line

The original's command line reaches the fork as one string, `--cmdline=`,
which the recreated launcher's `run-game.sh` passes on as given
([running it](ENGINE.md#running-it)) but for its quotes, which that launcher
drops ([its known defects](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#known-defects)).
Its flags are found as the original finds them
([cli-flags.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md));
what the fork does with each:

- **The start URL**: with `-hax0r` or `-server`, the first word (the second
  after `SERVER`) unless it starts with `-`; else the game's own start,
  `DX.dx` ([starting the game engine](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#starting-the-game-engine)).
- **`-server`**: a dedicated server, as `--server`.
- **`INI=` and `USERINI=`**: the inis read and written back, as `--ini` and
  `--userini`.
- **`EXEC=<file>`**: `exec <file>` on the player once the first map is in;
  `exec` runs a file's lines as commands, as the original's does.
- **Safe mode's flags** ([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md#flags-the-launcher-emits-safe-mode)):
  - `-nosound`: no sound and no music. The original makes no audio
    subsystem; the fork keeps its device, on OpenAL Soft's null driver.
  - `-defaultres`: the window and fullscreen sizes 640×480 for the run, not
    saved.
  - `-nohard -noddraw` ("Run the game in a window"): a window. The original
    gets there through its software renderer, which has no fullscreen mode
    without DirectDraw; the fork has no software renderer, so either flag
    alone does nothing.
  - `-nojoy`: no pad at all, neither the fork's pad handling nor its buttons
    as keys.
  - **Nothing to do:** `-no3dsound` (the original's turns off A3D or EAX
    hardware; the fork's mixer is software, [sound](#sound)), `-nommx`,
    `-nokni` and `-nok6` (the original's mixer picks its routines by them),
    and `-safe`'s no DirectInput (the fork's input is SDL's).

## Mods

A mod gets the original's loader:

- **Its folders.** Its packages and maps are found by the `Paths` of its own
  ini (`--ini`), relative to `System` and written with backslashes as the
  game's are, and by their extensions in any case, as the original's
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
- **Protected packages.** An export whose flags give it no context (client,
  server or editor) is never made, and a reference to it is None, as the
  original's linker has it: the live servers' anti-cheat packages (ANNA,
  DXNMS) point their classes' `ScriptText` at such exports. A save's exports
  are all made whatever their flags, for the fork's older saves wrote
  spawned actors without them.
- **Config files.** A class's config file that is not there is empty, its
  properties keeping their defaults, until the class writes it
  ([configuration](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#configuration)):
  DXMTL's classes read a `DXMTL.ini` no player without the mod has. A key
  with an empty value is a value, what the original's `ImportText` makes of
  no text: a string empty; a name, object or class None; a float 0; an int,
  byte or bool its default. Of a key given twice in a section the last
  counts, but for the player's own settings, which the fork reads again by
  hand as it spawns a player: there the first.

## Multiplayer

Matches the original's client and server ([the network](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md)):
joining and playing, listen and dedicated servers, replication with the
original's native lists, the server's calls, downloads both ways, server
travel, the Entry level for a lost server, the login, merged bunches, the
uplink and query answerer, and the scripts' sockets
([the script's links](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-scripts-links)).
Each runs against the original as client and as server: `ServeConsole`,
`JoinConsole`, `RejoinConsole`, `TravelServeConsole`, `TravelJoinConsole`,
`NetConsole`. Until its pawn arrives, a joining fork draws no world. The
server-only stubs are under
[not needed for single player](#not-needed-for-single-player). Differs:

- **The net driver** is the fork's own, over UDP with
  `[IpDrv.TcpNetDriver]`'s timeouts; the connection, its channels and the
  handshake are the original's.
- **The download cache.** A package is cached as its GUID with `CacheExt`,
  where the original writes `.uxx` whatever `CacheExt` says (the game's ini
  names `.uxx`). A package a join loaded from the cache is let go by name at
  the next map load that does not use it, where the original's map load
  collects it ([housekeeping](#housekeeping-not-seen-directly)).
- **Unchecked:** whether a live server corrects the client at a stop, now that
  the traces and moves are the original's.

As in the original:

- A game hosted from the menus is never listed on a master server: an uplink
  announces a server only with its `DoUplink` set, which the game's
  `DeusEx.ini` does not set ([the master server](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-master-server)).
- The Join Internet screen lists servers only once `MasterServerAddress`
  (`[DeusEx.MenuScreenJoinGame]` in `DeusEx.ini`) names a live master; the
  game's names GameSpy's, which is closed.

## Not needed for single player

Stubs a single-player game does without; the audit lists each:

- `DumpLocation`'s 21: Ion Storm's bug-location tool, though
  `DeusExGameInfo.Login` calls `HasLocationBeenSaved` on every map.
- `DebugInfo` (compiled out in the original too:
  [`DebugInfo`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#debuginfo)),
  `SaveTimeDemo`, `Commandlet.Main`, and `Object`'s `clock`, `unclock` and
  `CyclesToSeconds` ([the timing natives](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#clock-unclock-and-cyclestoseconds)).
- Two of the network's, both only a server's: `GameInfo.GetNetworkNumber`
  answers "0" to `StatLog`'s server line, and
  `PlayerPawn.GetPlayerNetworkAddress` answers "", so `KickBan` on a fork
  server bans no address. `StatLog`'s and `StatLogFile`'s natives do
  nothing, so a fork server logging world stats writes no log of them.
- `ComputerWindow`'s 22, which no script calls: the InfoLink's text window,
  its only user, calls only implemented ones.
- `ClipWindow`'s unit sizes, `GC`'s `PushGC`, `PopGC`, `CopyGC` and
  `Intersect`, and 6 more of the windows' stubs, which no script calls.
