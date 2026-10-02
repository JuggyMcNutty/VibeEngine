# Working on the engine

How VibeEngine is changed, run and checked. It is worked on beside the
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina) workspace:
this clone is `VibeEngine/` in the parent folder of its repositories
(`<parent>` below), beside the game install (`gamefiles/`), the SDK
(`reference/`) and the builds (`build/`). A `vibe/` command runs from this
clone's root, anything else from the parent folder. The workspace itself --
its cold start, this machine, commits and the docs rules -- is its
[`DEVELOPMENT.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md).
How the fork is kept, run and profiled, and what it changes, is
[`ENGINE.md`](ENGINE.md).

## Temporary debug hooks

Temporary debug hooks (screenshots from the renderer, extra logging)
carry a `TEMPORARY DEBUG TOOL` comment and are reverted before committing --
by replacing their exact text, never by a looser scripted cut: one such cut
took live main-loop code with it, the build still compiled, and the engine
died half a minute into a run. A slice's proving run goes 60 s or more --
25 s once hid exactly that -- and looks at what was drawn:
`vibe/tools/dxcap.sh prove <map>` ([scripted runs](#scripted-runs-of-both-engines)).
A clean log once hid a world that was not drawn at all.

## Scripted runs of both engines

`vibe/tools/dxcap.sh` runs the original game (under Proton's wine, in this
container) and the engine fork alike, each driven by a console class of the DXCapture package --
UnrealScript in `vibe/tools/dxcap`, compiled by the SDK's `UCC.exe`
(`reference/ReleaseSDK1112f`) into `build/dxcap`. Each run gets a private ini
made from the game's own, naming the console class, with a 1280x720 window;
both engines take the game's settings from it, and the game's inis are never
written. A run's shots, log and recording land in `build/dxcap/runs/`.

```sh
vibe/tools/dxcap.sh setup && vibe/tools/dxcap.sh compile   # once, and after changing vibe/tools/dxcap
vibe/tools/dxcap.sh prove 01_NYC_UNATCOIsland.dx            # the fork: shots at 20 s and 60 s, checked, exit at 65 s
vibe/tools/dxcap.sh fork <console> <map>                    # the fork with any console class
vibe/tools/dxcap.sh original <console>                      # the original, from its menu map
DXCAP_RECORD=1 vibe/tools/dxcap.sh ...                      # either, its audio recorded into the run's audio.wav
DXCAP_HIDDEN=1 vibe/tools/dxcap.sh fork|prove|live ...      # the fork on a hidden display, not the desktop
```

The console classes:

- **`ProveConsole`**: the proving run.
- **`CaptureConsole`**: M0's pictures -- Liberty Island's lasers and coronas,
  a tripwire walked into, and in Brooklyn a conversation whose jump lands on
  a comment's label, played through.
- **`ViewConsole`**: the player's own weapon in view -- at Liberty Island's
  start, the Dragon's Tooth given as the game gives its starting items and
  put in hand, the HUD hidden, its look and the view's field of view
  logged and shot three times, a second apart; on arrival the level's
  `Brightness` and each zone's ambient light logged.
- **`CoronaConsole`**: Liberty Island's lamps' glows from `CaptureConsole`'s
  shot 7 (three lamps in view), the player held there as that console holds
  it, the HUD hidden: each corona light near logged with its draw scale,
  hue, saturation and skin, two marked shots.
- **`LaserConsole`**: Liberty Island's first laser tripwire looked into from
  where `CaptureConsole` shoots it, the HUD hidden: the trigger's, its
  emitter's and its proxy's state logged each second, two marked shots.
- **`MeshConsole`**: meshes lit -- at Liberty Island's start, the HUD
  hidden, Paul Denton (who stands still) moved 220 units ahead and a large
  crate, a barrel and a large box placed around him on the pier; each one's
  place and look logged, two marked shots once they have settled.
- **`FlashConsole`**: the screen flash -- the player stood where
  `CoronaConsole` stands it, the HUD hidden, a shot with no flash and one
  under a steady glow (`FlashScale` 0.5, a red fog), each flash logged.
- **`GetConsole`**: the console's `GET` and `SET` as the game's menus use
  them -- a class default of each kind of property read, then three
  `SET`s read back --, from the menu map; then the main menu opened and
  shot, pointer and all.
- **`NetConsole`**: the scripts' sockets -- conversions and GameSpy answers
  logged, 333networks' master server asked for Deus Ex's servers and five
  of them pinged, then the game's own Join Internet screen opened (the
  run's ini names that master server: the game's names GameSpy's, closed).
- **`ServeConsole`**, either engine: a listen server for the other to join -- a
  deathmatch on DXMP_Cathedral, never on the master servers' lists (the
  run's ini has no uplink), in the package's own `CapDeathMatch`: the game's
  check that a joining player's console is the stock one would disconnect
  `JoinConsole`. Once another player is in, the host's own player stands in
  its sight -- in front of it where there is room -- and walks to and fro
  across its view; where each player stands is logged every 2 s. It exits
  after 290 s: give the original's run 300 s.
- **`JoinConsole`**, either engine: the joining side -- from the menu map it
  logs its player's name and world stats password, opens
  `127.0.0.1:7790`, stands its player 5 s, walks it forward 5 s and
  stands again, logging each second where it and every other pawn stand
  (the others with their animation: sequence, frame and rate), and shots
  at the stops. It exits 25 s into the game, or back in the menu --
  dropped, or never in after 40 s --, since the original's log comes only
  at its exit. The original's server answers some 13 s after it starts.
- **`RejoinConsole`**, either engine: a client that joins twice -- it opens
  `127.0.0.1:7790`, disconnects to the menu map 6 s into the game, opens it
  again 30 s later (another server there by then) and logs whether that
  join comes in.
- **`AIConsole`**: what every NPC is doing -- Liberty Island from the menu
  map, the player left at its start; at 2, 8 and 20 s of the level's own
  time each `ScriptedPawn`'s state, orders, whether it is in the world and
  hidden, its enemy, whether it looks for enemies and listens for shots
  and noises, its physics and place, and how long since it was drawn
  (`DXAI:` lines), and through the first 10 s each state an NPC enters,
  with the time (`DXAISTATE:`). The two engines' logs laid side by side
  name each NPC that differs, and which count as drawn.
- **`TraceConsole`**: what `TraceTexture` gives -- Liberty Island from
  the menu map, 2 s into the level, for each NPC in the world the hits of
  a line from the player's eye to it and the first of a line from it
  straight down (its floor, as footsteps read it): each hit's actor,
  texture, group, flags and distance (`DXTRACE:`).
- **`StandConsole`**: where a walking player rests over the floor -- at
  Liberty Island's start and after two short walks, its place, collision
  height and base and the floor a line down finds logged, and its
  velocity and acceleration through the first walk's first 12 ticks. It
  walks with the forward axis a held key gives the fork (the binding's
  speed, 300, times 20): the original's input scales an axis the console
  sets, the fork's takes it as it is.
- **`VisibleConsole`**: what `FastTrace` and the visible-actor iterators
  give -- Liberty Island from the menu map, 2 s into the level, for each
  mover a line across its box's thinnest side through its middle:
  `FastTrace`'s answer and the actor a `Trace` meets; then each actor
  `VisibleCollidingActors` lists within 1000 units of the player, with and
  without `bIgnoreHidden`, and each `VisibleActors` lists (`DXVIS:`). The
  player and its shadow are numbered one higher in the original's log,
  whose menu map made the first.
- **`RestConsole`**: where a falling decoration comes to rest -- on
  Liberty Island the large crate, the barrel and the large box placed as
  `MeshConsole` places them, the colliding actors near each listed, a line
  and two boxes traced down from each spot, and every tick of the fall for
  1.5 s logged (place, the cylinder's bottom, physics, velocity, base).
- **`SightConsole`**: what `LineOfSightTo`, `CanSee` and `PlayerCanSeeMe`
  give -- Liberty Island from the menu map, 2 s into the level, for each
  NPC in the world and each light within 4000 units of the player: the
  player's `LineOfSightTo` to it, with and without `bIgnoreDistance`, its
  `CanSee` of it, and the actor's `PlayerCanSeeMe` (`DXSIGHT:`).
- **`SaveConsole`** and **`LoadConsole`**, either engine: a save one engine
  makes for the other to load -- the first opens Liberty Island from the
  menu map, logs 8 s in what the level holds (the player's place, health
  and inventory, the game, how many actors, pawns and inventory items) and
  saves to slot 9 through a `SaveHelper` actor, as the original's console
  ticks inside a draw where a save's own draw stops it; the second loads
  slot 9 from the menu map and logs the same 5 s into the level. The helper
  is in the save and gone at its first tick after a load. Both use the
  game's own `Save\Save0009` and `Save\Current`: delete them after.
- **`TravelServeConsole`** and **`TravelJoinConsole`**, either engine:
  server travel -- the server as `ServeConsole`'s, which `servertravel`s to
  DXMP_Smuggler once another player has been in 10 s, logging its map and
  players every 2 s and exiting after 150 s; the client joins it, logs its
  map, net mode and place each second, and exits 8 s after it is a client
  in a second map (a shot first), or at 150 s; out of the game 2 s once in
  it, it takes its server as lost, shoots 4 s in (marked) and exits.
- **`SoundConsole`**: M0's sounds -- a steady sound heard in the open and from
  behind a wall, shots in a reverb zone and out of it, and beeps from the
  right, the left and ahead. It silences the level first (ambient sounds,
  pawns, whatever watches for the player, datalinks) and starts each part
  with three beeps; `vibe/tools/dxcap/sound.py <run>` lays the recording against
  the log by them and measures.
- **`SkipConsole`**: the conversation skip -- Kaplan's MeetKaplan on
  Liberty Island with every line skipped mid-line, its speech and lengths
  logged; `DXCAP_RECORD=1` records it, and `vibe/tools/dxcap/skip.py <run>
  [<run> ...]` lays the recording against the log by the run's two beeps
  and measures each line's tail: stopped, or playing on.
- **`BeltConsole`**: the object belt -- items given as the game's pickups
  give them, the belt's slots logged after each with the player's in-hand
  state through a use, a swap and a new pickup, and marked shots of the
  belt.
- **`DeathConsole`**: an NPC's death -- two isolated humans killed from
  behind and from the front (and robots on the way), the Dying state's
  animation, acceleration and place logged through the fall, then the
  carcass's class, mesh, place and base until it settles, with marked
  shots.
- **`ChoiceConsole`**: a conversation's choices -- MeetKaplan run to its
  choices, each button's selectability and the focus window logged, the
  root window's own key handler driving Down, Down and Up between them
  with a marked shot after each, then the focused choice picked.
- **`BorderConsole`**: frames `GC.DrawBorders` draws -- on Liberty Island
  the player given items as `BeltConsole` gives them, the inventory screen
  opened and shot, then the lockpick and the assault gun selected there and
  shot each time: a selected item's button draws its frame with
  `DrawBorders`.
- **`ColorsConsole`**: the Colors settings screen (`MenuScreenRGB`), whose
  example panes draw 11- and 12-pixel frames with `GC.DrawBorders` -- from
  the menu map, the main menu shown, the screen pushed, a marked shot.
- **`HeldConsole`**: what a pawn holds -- the player seen from behind on
  Liberty Island holding a multitool (no weapon, so the selected item is
  drawn at the weapon triangle) and then the assault gun, a marked shot of
  each, the player's `Weapon`, `SelectedItem` and `inHand` logged.
- **`MidConsole`**: `Object.Mid` at its edges -- negative starts and
  counts, a count past the end, the default count -- logged with `DXMID:`
  from the menu map, then an exit; the two engines' lines should be alike.
- **`MoveConsole`**: how the level moves -- every in-world ScriptedPawn's
  state, orders, move target, destination, velocity and distance moved,
  every 2 s for 120 s on Liberty Island, for a trajectory diff between
  the engines: `move.py <original run> <fork run>` (beside `skip.py`)
  lays each pawn's distance moved, longest stall and move targets beside
  the reference's.
- **`MissionConsole`**: a mission map's script -- the DeusExLevelInfo, its
  MissionScript, and the state machine's initialization polled from the
  player's flag base, `DXMISSION:` lines saying OK or what is missing.
  The fork's runs land on the map by their URL; an original's run starts
  at the menu map, so `DXCAP_MISSION_MAP=<map> original MissionConsole`
  names the target through the run's ini.

**Net tests** pair the two consoles, one engine each side, on this machine:
start the server (`original ServeConsole 300` in the background, or `fork
ServeConsole DX.dx`), wait until its game port is bound (`ss -uln | grep
:7790`), then run the client (`fork JoinConsole DX.dx`, or `original
JoinConsole`). Never start a second server before the first has exited: it
cannot bind the port ("Net: cannot listen" in its log) and the client joins
the old one. Each side's log then says what it saw -- `DXNET:` lines on the
client, `DXCAP:` player positions on the server --, and a server's LAN
beacon and GameSpy query answers are asked with
`vibe/tools/dxcap/netquery.py`, the same questions for either engine, for a diff.
A server's uplink is checked the same way: `vibe/tools/dxcap/fakemaster.py`
listens as a master on this machine (UDP 27900) and asks each server that
announces itself what a master asks, and `DXCAP_UPLINK=127.0.0.1:27900` in
front of a server's run has its uplink announce it there, `DoUplink` set.
A login's world stats checksum likewise: `DXCAP_STATS=<password>` in
front of both runs has the server log world stats (`bWorldLog`) and the
client's player hold that password, and the server's log shows the login's
URL (`login request`). And downloads: `DXCAP_SERVERPKGS=<dir>` in front of
a server's run has it find the packages of `<dir>` and name each in its
`ServerPackages`, so a client, whose paths lack `<dir>`, downloads them.

**Live servers** (the owner's go-ahead, 2026-09-27; which are up, and how
full, in 333networks' list: `https://master.333networks.com/json/deusex`):
`fork ProveConsole <address>` joins one, downloading what the fork lacks
into the game's `Cache`, but every server's game disconnects a player whose
console is not the stock one (`Invalid Console class, disconnecting`,
[the check](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#deus-exs-additions)), so the run is back in
the menu seconds after joining. `live <address> [<secs>]` keeps the stock
console and has the engine drive the run instead, from a timeline
(`--timeline=<file>`, the fork's `Timeline.h`): lines of `<clock> <seconds>
<action>`, the clock `start` (from the engine's start) or `game` (seconds
of the net game), the action `press <key>` or `release <key>` as the key in
the window, or a console command (`shot`, `exit`, ...). By default it is
`JoinConsole`'s walk -- in the game, stand 5 s, walk forward 5 s (`W`) and
stand, shots at the stops, out at 25 s --, or the file in
`DXCAP_TIMELINE`; each second of the game the player's place and every
other pawn's are logged (`DXLIVE` lines), and a run dropped to the menu
exits. Pick an empty server.

**A crash in an original run** leaves its "Critical Error" dialog waiting
on the hidden display, and the run hangs until the harness kills it, its
log cut short (the game writes it only as it exits). Dismissed, the game
writes the crash's history -- the error and the calls under it -- into its
log and exits: `DISPLAY=:99 xdotool search --name 'Critical Error'` finds
the dialog, then `xdotool windowfocus --sync <window>` and `xdotool key
Return` (with no window manager there, `windowactivate` fails).

**A crash** in a fork run leaves a core, which systemd keeps:
`coredumpctl dump <pid> --output=<scratchpad>/core` and `gdb -batch -ex bt
<parent>/build/linux-x86_64/engine/SurrealEngine <scratchpad>/core` name
the functions on the stack, the `Release` build keeping its symbols' names
but no lines. For lines, a copy
built with symbols in the scratchpad (`cmake -S VibeEngine -B
<scratchpad>/dbg -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake
-DCMAKE_BUILD_TYPE=RelWithDebInfo`, then `cmake --build <scratchpad>/dbg
--target SurrealEngine`; some two and a half minutes) runs under `gdb -batch
-ex run -ex bt --args <scratchpad>/dbg/SurrealEngine --no-launcher <parent>/gamefiles
--ini=<parent>/build/dxcap/System/Fork.ini
--userini=<parent>/build/dxcap/System/ForkUser.ini --url=<map>`, started in
`gamefiles`, with the ini the harness wrote for its last fork run (so that
run's console). The scratchpad is cleared when a session restarts, and the
copy with it.

The classes stand the player where the original's searches did, written into
them: the two engines' `SetLocation`s fitted the player in differently, and
their traces stopped apart, until 2026-09-28
([engine-dll.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/engine-dll.md#teleporting-an-actor)), so a search stood them apart;
written in, the spots stay put whatever either engine's fitting does.

**Shots.** The fork's `shot` writes the next free `ShotNNNN.bmp`. The
original's own `shot` gives noise (D3D) or black (the others) under Proton, so
its run draws through `OpenGLDrv` on a hidden X display -- Xvfb on `:99`, in
the container -- which `vibe/tools/dxcap/grab.py` reads five times a second,
keeping each frame whose corner carries the console's mark: a magenta block,
then the shot's number in eight black or white blocks. The original's
frames carry its brightness -- its `OpenGLDrv`'s gamma ramp, a gamma of
1.5 at the runs' Brightness of 0.6 --, the fork's shots none: a fork shot
takes that gamma (`magick <shot> -gamma 1.5 <out>`) before its brightness
is compared ([brightness](NATIVES.md#brightness)). `DXCAP_RENDERER=D3D`
draws the original through `D3DDrv`, the game's own renderer, instead:
the same frames there, ramp and all. A run whose renderer fails to start
stops with an error: the game falls back to `SoftDrv` without a word on
the screen, and its frames would pass for the renderer's.

**Recording.** `DXCAP_RECORD=1` sends the engine's sound to a private null
sink on the desktop's sound server (`PULSE_SINK`) and records the sink with
`parecord`; the run's ini turns the music off.

What it takes to run the original there, each found the hard way:

- **It boots its menu map** whatever map its command line or ini names; a
  console class travels with `open <map>` itself.
- **UCC needs a short base directory** (a long one crashes it while it reads
  its ini) and both `UCC.ini` and `DeusEx.ini`; hence `build/dxcap`.
- **It runs in a view of the game**, `build/dxcap/game`, made afresh for
  each run: the game's folders linked, and its `System` folder's files but
  for what the game writes there -- its log, `Running.ini`, shots -- and any
  file with no extension. The original takes a package's bare name in its
  working directory before any of its paths
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)), so the recreated
  launcher's `DeusEx`, installed beside `DeusEx.exe`, stood in for the
  `DeusEx` package and stopped it at its start. Its log, its `Running.ini`
  (a stale one opens the recovery wizard, which waits for a click) and its
  own shots stay in the view, never in the game's folder.
- **It runs in this container, never on the host**: the Proton build's own
  `wine`, as IDA's headless server runs
  ([`tools/ida/idalib-mcp.sh`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/tools/ida/idalib-mcp.sh)), but in a Wine
  prefix of its own, `build/dxcap/prefix`, which the script makes on first
  use (`wineboot`, on the hidden display): a prefix's Wine desktop is on the
  display of whatever started its wineserver, and IDA's headless server,
  started with a session, starts one on the desktop's, where the game's
  first window fails with `BadWindow`. The 32-bit game needs the
  container's 32-bit libraries ([this machine](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#this-machine)).
- **`D3DDrv` needs vkd3d's libraries in the prefix**: Wine's `wined3d`,
  under the `ddraw.dll` `D3DDrv` loads, imports `libvkd3d-1.dll` and its
  two siblings, which Proton's own prefix carries and a bare `wineboot`'s
  does not. Without them `ddraw.dll` does not load, and the game logs
  "DirectDraw not installed" and falls back to `SoftDrv`; the script copies
  them in from the Proton build's lib/vkd3d folder when they are missing
  (2026-09-28).
- **No Wine desktop**: `explorer /desktop` fails to set its display up on
  Xvfb and exits without starting the game, so the game runs straight on the
  hidden display, where the grabber finds its frames by their mark.
- **Its window goes where Wine puts it** -- a step further on each run while
  Wine's server stays up -- and the grabber reads the view from the
  display's corner: the script moves the window there once it is up
  (`xdotool`, in the container).
- **Only its own process is stopped** at the end, never the prefix's
  wineserver.

The fork's window opens on this machine's desktop, as any run's does; the
original's, on the hidden display. The fork can run on a hidden display too
-- Xvfb, with SDL's `x11` driver (`SDL_VIDEODRIVER=x11`, `WAYLAND_DISPLAY`
unset): it renders there, its `shot` right, but a grab of the display shows
its window black, so what it drew is read with `shot`. `DXCAP_HIDDEN=1`
puts a fork run on one of its own (Xvfb on `:98`, apart from the
original's `:99`, so a net test can run both), and the recreated launcher's
live check runs it so
([checking it live](https://github.com/JuggyMcNutty/deusex-launcher#checking-it-live)).

## Gotchas

The engine's own; the workspace's are in its
[gotchas](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#gotchas-that-cost-time).

- **The engine takes `--url=<map>` only** and **ignores SIGTERM**
  ([running it](ENGINE.md#running-it)). `-u <map>` silently loads the intro,
  which is how a whole round of "Liberty Island" profiling measured the intro.
- **Profile the handheld on the handheld.** Its Cortex-A53 pays far more for a
  cache miss than the desktop, so the costs come in a different order
  (`CycleActors` was ~6% of the desktop's game tick and ~18% of the device's).
  Its kernel has no perf events; the hooks' own sampler does it
  ([the Smart Pro's Performance](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance)).
- **The profiling hooks go off before changing the engine**: a commit made with
  them on carries them ([the profiling hooks](ENGINE.md#the-profiling-hooks)).
- **An unattended run tests no AI.** Liberty Island starts the player 7,000 to
  20,000 units from every NSF, and no NPC reacts to a player it cannot see: to
  check AI on the desktop, move the player in front of one with a temporary
  hook.
