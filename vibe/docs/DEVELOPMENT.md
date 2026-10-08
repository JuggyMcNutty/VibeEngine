# Working on the engine

Driving both engines with the dxcap harness, debugging a crash, proving an engine change. This
clone is `VibeEngine/` in the parent folder of the workspace's repositories (`<parent>` below),
beside the game (`gamefiles/`), the SDK (`reference/`) and the builds (`build/`). A `vibe/`
command runs from this clone's root, anything else from the parent folder. How to work in the
workspace:
[its `DEVELOPMENT.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md).
Keeping, running and profiling the fork, and what it changes: [`ENGINE.md`](ENGINE.md).

## When a fidelity change is done

A fidelity change is done when `vibe/tools/natives_audit.py --runs <logs>` shows no stub firing
on its path in the maps that reached one, a side-by-side
[scripted run](#scripted-runs-of-both-engines) against the original proves it, and
[`NATIVES.md`](NATIVES.md) says what still differs. The audit reads a native "partial" when
another game's branch on its call chain logs `Unimplemented`: `ParabolicTrace`, `actorReachable`,
`FindPathToward` and `ReachablePathnodes` read so, though their Deus Ex path is the original's.
It reads `AICanSmell` "empty": the original's answer is always 0 too. The runs say whether a stub
fired.

## Temporary debug hooks

A temporary debug hook (a screenshot from the renderer, extra logging) carries a
`TEMPORARY DEBUG TOOL` comment. Remove it before committing by replacing its exact text, never by
a looser scripted cut: a cut can take live code with it and still compile. Prove the change with
a run of 60 s or more that looks at what was drawn, not only at the log:
`vibe/tools/dxcap.sh prove <map>`.

## Scripted runs of both engines

`vibe/tools/dxcap.sh` runs the original game (under Proton's `wine`, in the development container) and
the fork alike, each driven by a console class of the DXCapture package: UnrealScript in
`vibe/tools/dxcap/Classes`, compiled by the SDK's `UCC.exe` (`reference/ReleaseSDK1112f`) into
`build/dxcap`.

```sh
vibe/tools/dxcap.sh setup                          # once, and again after the parent folder moves
vibe/tools/dxcap.sh compile                        # after setup, and after changing vibe/tools/dxcap
vibe/tools/dxcap.sh prove 01_NYC_UNATCOIsland.dx   # the fork: shots at 20 s and 60 s, exit at 65 s
vibe/tools/dxcap.sh fork <console> <map> [<secs>]  # the fork, straight into <map> or a server's address; - the stock console
vibe/tools/dxcap.sh original <console> [<secs>]    # the original, from its menu map
vibe/tools/dxcap.sh live <address> [<secs>]        # the fork on a live server (below)
```

A run stops after `<secs>`, 120 by default. `prove` fails on an unclean exit or a missing or
nearly black shot, and lists the log's errors and `Unimplemented` lines. `setup` links the
game's files into `build/dxcap` by absolute path, and after a move `compile` does not notice they
are gone.

- **The ini.** Each run gets a private ini made from the game's `DeusEx.ini` and `User.ini`: the
  console class, a 1280x720 window, 333networks' master server for the Join Internet screen (the
  game names GameSpy's, which is closed), no uplink, and `OpenGLDrv` for the original. Both
  engines take the game's settings from it; the game's inis are never written.
- **`Settings.json`.** The fork's renderer, MSAA, VSync, gamma mode, `GammaCorrectScreenshots`,
  HDR and bloom, AI level of detail and render scale come from
  `~/.config/SurrealEngine/Settings.json`, an input of every fork run that no run's ini changes,
  or from the file `DXCAP_SETTINGS` names. Without one the fork runs on Vulkan with 4x MSAA.
- **Results.** A run's log (`engine.log` or `DeusEx.log`), shots and recording land in
  `build/dxcap/runs/<engine>-<console>-<time>/`.

| Variable | Effect |
|---|---|
| `DX_ROOT=<dir>` | the parent folder, if not the one this clone is in |
| `DXCAP_DISPLAY=<display>` | another X display than `$DISPLAY`, the desktop's ([the display](#the-display)) |
| `DXCAP_AUDIO=1` | the fork with real audio; it is silent otherwise |
| `DXCAP_RECORD=1` | either engine's sound recorded into the run's `audio.wav` (a private null sink, `PULSE_SINK`, and `parecord`); the music off |
| `DXCAP_RENDERER=D3D` | the original through `D3DDrv`, the game's own renderer, not `OpenGLDrv` |
| `DXCAP_PREFIX=<dir>` | another Wine prefix than `build/dxcap/prefix` |
| `DXCAP_PROTON=<name>` | another Proton build in `~/.local/share/Steam/compatibilitytools.d` than `Proton-CachyOS Latest` |
| `DXCAP_MISSION_MAP=<map>` | the map a run's console travels to (`MissionConsole`'s, `PerfConsole`'s): an original run starts at its menu map, a fork run at the map it is given |
| `DXCAP_SETTINGS=<file>` | a fork run's `Settings.json`, in place of `~/.config/SurrealEngine`'s (through a `HOME` of the run's own; the caches stay the usual ones) |
| `DXCAP_NOVSYNC=1` | Mesa's GL draws either engine without waiting for the display (`vblank_mode=0`): the original's `OpenGLDrv` asks for no swap interval and gets the driver's vsync |
| `DXCAP_FPS=<n>` | either engine held to n frames a second: MangoHud's limiter, its display off (`mangohud`, and `lib32-mangohud` for the original's 32-bit process) |
| `DXCAP_PERF=1` | the engine's main thread sampled by perf into the run's `perf.data`, on the wall clock, its cycles and instructions counted each 100 ms (`stat.csv`), with the process's `maps.txt` ([measuring both engines](#measuring-both-engines)) |
| `DXCAP_TIMELINE=<file>` | a `live` run's timeline, not `JoinConsole`'s walk; a `fork` run's [timeline](#timelines) |
| `DXCAP_ENGINE=<binary>` | a fork run on that build, not linux-x86_64's (an [ASan build](#crashes)) |
| `DXCAP_MEMLOG=1` | a fork run's resident memory each second in its `memlog.txt`: seconds, MB |
| `DXCAP_UPLINK=<host>:<port>` | the server announces itself to that master (`DoUplink`); otherwise to none |
| `DXCAP_PORT=<port>` | the server listens there (`[URL] Port`), not on 7790: for `netrelay.py` between it and a client ([net tests](#net-tests)) |
| `DXCAP_STATS=<password>` | on both runs: the server logs world stats (`bWorldLog`); the player holds that world stats password, so its login carries the checksum (a fork server logs the login's URL: `login request`) |
| `DXCAP_SERVERPKGS=<dir>` | the server offers the packages in `<dir>` (`ServerPackages`) for a client to download |

### The console classes

Each class's header comment in `vibe/tools/dxcap/Classes` says what it does; the scripts are in
`vibe/tools/dxcap/`. *Both*: run in each engine and compare, side by side where no script reads
the run. *Either*: one engine on each side of a pair.

| Console | Engines | Drives and logs | Prefix | Read with |
|---|---|---|---|---|
| `ProveConsole` | fork | the URL's map: two shots, an exit | `DXPROVE:` | `prove` |
| `CaptureConsole` | both | tripwires, coronas, comment-jump conversations | `DXCAP:` | |
| `ViewConsole` | both | the player's weapon in view; the level's light | `DXVIEW:` | |
| `CoronaConsole` | both | Liberty Island's lamp coronas; a movable corona light of its own, in the player's BSP leaf and out of it | `DXCORONA:` | |
| `LaserConsole` | both | a laser tripwire's beam and its actors | `DXLASER:` | |
| `MeshConsole` | both | meshes lit on the pier | `DXMESH:` | |
| `FlashConsole` | both | the screen flash | `DXFLASH:` | |
| `HeldConsole` | both | what a pawn holds | `DXHELD:` | |
| `DeathConsole` | both | an NPC's death and its carcass | `DXDEATH:` | |
| `AIConsole` | both | every NPC's state, orders, enemy, time since drawn | `DXAI:`, `DXAISTATE:` | |
| `MoveConsole` | both | every pawn's moves, every 2 s for 120 s | `DXMOVE:` | `move.py <original run> <fork run>` |
| `ReachConsole` | both | `AIDirectionReachable`, `PointReachable`, `ActorReachable` | `DXREACH:` | |
| `TraceConsole` | both | `TraceTexture`'s hits; toward decorations, `TraceActors`, `TraceTexture`, `Trace` and `FastTrace` | `DXTRACE:` | |
| `FloatConsole` | both | the crates in the water by the pier, every quarter second | `DXFLOAT:` | |
| `SwimConsole` | both | at the pier's edge, every tick for 4 s: the player falling into the water from three heights, diving, swimming up; a troop dropped and thrown in | `DXSWIM:` | |
| `LadderConsole` | both | Liberty Island's first ladder found from the navigation points, every tick: the player at its foot, climbing 1 s, hanging, going down looking down, hanging, climbing onto the top | `DXLADDER:` | |
| `VisibleConsole` | both | `FastTrace`, `Trace`, the visible-actor iterators | `DXVIS:` | |
| `SightConsole` | both | `LineOfSightTo`, `CanSee`, `PlayerCanSeeMe` | `DXSIGHT:` | |
| `StandConsole` | both | where a walking player rests over the floor | `DXSTAND:` | |
| `StompConsole` | both | a pawn landing on another, and on crates: the player onto Paul's head, Paul onto the player's, the player onto a crate that cannot be a base and onto a breakable one | `DXSTOMP:` | |
| `RestConsole` | both | where a falling decoration rests; a crate and the player dropped over the pier's edge | `DXREST:` | |
| `MissionConsole` | both | whether a mission map's script comes up | `DXMISSION:` | |
| `AugVisionConsole` | both | an NPC behind a wall that the vision augmentation draws: its time since drawn, the augmentation off, on, on and turned about, off | `DXAUGVIS:` | |
| `PortalConsole` | both | a map opened at a portal (`open <map>#<portal>`): the level's URL, where the player lands | `DXPORTAL:` | |
| `ReturnConsole` | both | Liberty Island, UNATCO HQ and back: the game, its base mutator, the player's augmentations, skills and keys; a `LoadMarker`'s `PostPostBeginPlay` calls; a basketball carried off the island, the player's hands and every basketball in each map | `DXRETURN:` | |
| `GetConsole` | both | the console's `GET` and `SET`; the main menu | `DXGET:` | |
| `PerfConsole` | both | what a frame costs: the map (`TargetMap`, Liberty Island by default) from its start, the player idle, the view held; from 15 s in, sixty one-second windows on the wall clock; the original's own cycle counters | `DXPERF:` | `vibe/tools/perf/frame-report.py`, with `DXCAP_PERF=1` ([measuring both engines](#measuring-both-engines)) |
| `PerfTurnConsole` | both | `PerfConsole` with the view turning 45 degrees a second | `DXPERF:` | the same |
| `BeltConsole` | both | the object belt | `DXBELT:` | |
| `FrobConsole` | both | the frob highlight round two decorations, the view turned about them | `DXFROB:` | |
| `LootConsole` | both | two carcasses searched, the inventory grid against its items | `DXLOOT:` | with `vibe/tools/dxcap/timelines/loot-drags.txt` on the fork |
| `CarcassConsole` | both | what the map's carcasses and a killed NPC's hold, their search, the looted weapon in hand and away | `DXCARC:` | |
| `ChoiceConsole` | both | a conversation's choices and their focus | `DXCHOICE:` | |
| `KeypadConsole` | both | the root's focus with no modal up and round the keypad; the player walking; the keys a modal of the package's own is handed | `DXKEYPAD:` | with `vibe/tools/dxcap/timelines/stray-release.txt` on the fork |
| `BorderConsole` | both | `GC.DrawBorders` on the inventory screen | `DXBORDER:` | |
| `ColorsConsole` | both | `DrawBorders` on the Colors screen | `DXCOLORS:` | |
| `RotatorConsole` | both | `rotator(v)`, a rotator's string, `vector(r)`, `GetAxes`, `GetUnAxes` | `DXROT:` | |
| `MidConsole` | both | `Object.Mid` at its edges | `DXMID:` | |
| `DynLoadConsole` | both | `Object.DynamicLoadObject` with a group in the name, its own, another or none; as a class not the object's own; a name not there | `DXDYNLOAD:` | |
| `GarbageConsole` | both | 128 actors destroyed, one a tick, and when a live actor's references to them go None | `DXGARBAGE:` | |
| `ChurnConsole` | fork | 20 actors spawned and 20 destroyed every tick for 120 s (with `DXCAP_MEMLOG=1`) | `DXCHURN:` | |
| `DeleteConsole` | both | `CriticalDelete` as the game uses it: nano keys, the log, the history, a game directory; what `AllObjects` still finds | `DXDELETE:` | |
| `SoundConsole` | both | a sound behind a wall; who hears the wall's source, in the open and behind the wall; a reverb zone; beeps from three sides | `DXCAP:` | `sound.py <run>`, with `DXCAP_RECORD=1` |
| `SkipConsole` | both | a conversation's lines skipped | `DXSKIP:` | `skip.py <run> [<run> ...]`, with `DXCAP_RECORD=1` |
| `SaveConsole`, `LoadConsole` | either | a save to slot 9, and its load; the mission script, a `LoadMarker`'s `PostPostBeginPlay` calls | `DXSAVE:` | |
| `NetConsole` | both | the script's sockets, a master server's list | `DXCAP:` | |
| `ServeConsole` | either | a listen server: its own URL and address as it starts; every 2 s each player's place and address, its own number and the frames it drew; 8 beeps 1 s apart, 12 s after the other player is in; it exits after 290 s | `DXCAP:` | `netquery.py`, `fakemaster.py`, `netrelay.py` |
| `JoinConsole` | either | a client of `127.0.0.1:7790` | `DXNET:` | `sound.py --onsets <run>`, the client alone with `DXCAP_RECORD=1`: the server's beeps |
| `RejoinConsole` | either | a client that joins twice | `DXREJOIN:` | |
| `TravelServeConsole` | either | a server that travels; it exits after 150 s | `DXCAP:` | |
| `TravelJoinConsole` | either | a client that follows it | `DXNET:` | |

- `SaveConsole` and `LoadConsole` use the game's own `Save\Save0009` and `Save\Current`: delete
  them after. The original's own saves are in `reference/original-saves/`: Liberty Island's start
  (no cheats), and a quick save, a hub save (`bCheatsEnabled`) and the `Current` folder from a
  travel to UNATCO HQ and back. Copied into `Save0009`, the hub save loads in both engines; with
  the quick save there the original never finishes its run.
- In `VisibleConsole`'s logs the original numbers the player and its shadow one higher: its menu
  map made the first.
- A sweep runs `MissionConsole` over every mission map; no driver for it is committed.

### Timelines

A fork run does what `DXCAP_TIMELINE`'s file says, in [the live mode's format](#live-servers),
beside its console's own script; with `-` for its console it keeps the stock one. Keys reach the
game as the window's do, so a timeline drives the UI, which no console can.
`vibe/tools/dxcap/timelines` has the map loads that show what a session keeps of the levels it
left, both from Liberty Island, ten loads of each map 25 s apart, then an exit:

| Timeline | Between | So each load |
|---|---|---|
| `reload-fresh.txt` | the island and Battery Park | crosses a mission: `Current` emptied, the map from `Maps` |
| `reload-current.txt` | the island and UNATCO HQ | stays in mission 1: the map left saved into `Current`, the next one from there |

and two more: `loot-drags.txt`, `LootConsole`'s drags in the inventory screen;
`stray-release.txt`, `KeypadConsole`'s release of a key no press came before.

```sh
DXCAP_MEMLOG=1 DXCAP_TIMELINE=vibe/tools/dxcap/timelines/reload-fresh.txt \
    vibe/tools/dxcap.sh fork - 01_NYC_UNATCOIsland 580
```

### Net tests

One engine on each side, on this machine. Start the server (`original ServeConsole 300` in the
background, or `fork ServeConsole DX.dx 300`: the default 120 s would end it before its own 290),
wait until its game port is bound (`ss -uln | grep :7790`; an original server answers some 13 s
after it starts), then run the client (`fork JoinConsole DX.dx`, or `original JoinConsole`).
Never start a second server before the first has exited: it cannot bind the port ("Net: cannot
listen" in its log), and the client joins the old one. To read what passes between them, start
the server with `DXCAP_PORT=7792`, wait for that port, start `vibe/tools/dxcap/netrelay.py` (it
listens on 7790 and forwards to `127.0.0.1:7792`), then the client: each second a row each way
-- packets, bytes, acks, bunches by channel and reliability -- and with `--log <file>` a line
for every datagram, each bunch's header decoded. The server's game is the package's
`CapDeathMatch`, without the game's check that a joining player's console is the stock one.
`vibe/tools/dxcap/netquery.py` asks a server's LAN beacon and GameSpy query answerer, and
`vibe/tools/dxcap/fakemaster.py` acts as a master on UDP 27900 (`DXCAP_UPLINK=127.0.0.1:27900`):
the same questions for either engine.

### Live servers

Every server's game disconnects a player whose console is not the stock one
(`Invalid Console class, disconnecting`;
[the check](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/network.md#deus-exs-additions)).
So `live <address> [<secs>]` keeps the stock console, and the engine drives the run from a
timeline (`--timeline=<file>`, `SurrealEngine/Timeline.h`). Each line is
`<clock> <seconds> <action>`: the clock `start` (from the engine's start) or `game` (seconds of
the net game); the action `press <key>` or `release <key>` as the key in the window (a mouse button
pressed where the pointer is), `pointer <x> <y>` the pointer moved there in the root window's
coordinates, or a console command (`shot`, `exit`, ...). The timeline is `DXCAP_TIMELINE`'s file, or `JoinConsole`'s walk:
stand 5 s, walk forward 5 s (`W`), stand, shots at the stops, out at 25 s. Each second of the
game the player's place and every other pawn's are logged (`DXLIVE:`); a run dropped to the menu
exits. A join downloads what the fork lacks into the game's `Cache`. Which servers are up, and
how full: `https://master.333networks.com/json/deusex`. Pick an empty server.

### Shots

- The original's own `shot` gives noise under `D3DDrv` and black under the others in Proton. Its
  window, drawn through `OpenGLDrv`, is read five times a second by `vibe/tools/dxcap/grab.py`
  (the window found by its name), keeping each frame whose corner carries the console's mark (a
  magenta block, then the shot's number in eight black or white blocks).
- **Gamma.** The original's frames carry its gamma ramp, 1.5 at the runs' Brightness of 0.6, the
  same through `D3DDrv` (`DXCAP_RENDERER=D3D`). The fork's `shot` reads the frame before the
  present pass applies gamma (`GammaCorrectScreenshots` off), so a fork shot takes that gamma
  before its brightness is compared: `magick <shot> -gamma 1.5 <out>`
  ([brightness](NATIVES.md#brightness)).
- A renderer that fails to start falls back to `SoftDrv` without a word, its frames passing for
  the renderer's: the script then fails, naming the log's `Bound to SoftDrv`.

### Running the original

- **It boots its menu map** whatever map its command line or ini names; a console class travels
  with `open <map>` itself.
- **`UCC.exe` crashes on a long base directory** while it reads its ini, and wants both
  `UCC.ini` and `DeusEx.ini`: hence `build/dxcap`. 92 characters as Wine names it
  (`Z:\...\build\dxcap\System\`) work; after a move to a longer path, `setup` and `compile`
  show whether it still holds.
- **It runs in the development container, never on the host outside it**, with the container's
  32-bit libraries
  ([dependencies](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)),
  under the Proton build's own `wine`, in a Wine prefix of its own (`build/dxcap/prefix`). A
  prefix's Wine desktop is on the display of whatever started its wineserver: a prefix shared
  with a Windows program started from the desktop, as
  [`tools/ida/idalib-mcp.sh`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/tools/ida/idalib-mcp.sh)
  starts IDA, puts the game's first window there, where it fails with `BadWindow`.
- **It runs in a view of the game**, `build/dxcap/game`, made afresh for each run: the game's
  folders linked, and its `System` folder's files except what the game writes there (its log,
  `Running.ini`, shots), the IDA databases (`*.i64`) and any file with no extension. The
  original takes a package's bare name in its working directory before any of its paths
  ([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)),
  so the recreated launcher's `DeusEx`, installed beside `DeusEx.exe`, cannot stand in for the
  `DeusEx` package there. The run's log, `Running.ini` (a stale one opens the recovery wizard,
  which waits for a click) and shots stay in the view, never in the game's folder.
- `dxcap.sh`'s comments document the rest of its set-up: vkd3d's libraries in the prefix for
  `D3DDrv`, no Wine desktop, and only the game's own process killed at the end, never the
  wineserver.

### The display

Both engines draw on the desktop's X display, Xwayland (`$DISPLAY`; `DXCAP_DISPLAY` names
another), each in a 1280x720 window: the original through Wine's X11 driver, the fork through
its window's X11 backend (`SURREALWIDGETS_DISPLAY_BACKEND=X11`, `WAYLAND_DISPLAY` unset,
`SDL_VIDEODRIVER=x11`). One X server and the GPU serve both, so their runs compare; no run uses a
hidden display, as runs on different displays do not compare. The development machine is given
over to it: windows come and go on its desktop.

### Measuring both engines

`PerfConsole` times either engine's frames the same way, and `DXCAP_PERF=1` samples its main
thread meanwhile. Both draw through OpenGL with no vsync, at 1280x720, the fork without MSAA:

```sh
DXCAP_NOVSYNC=1 DXCAP_PERF=1 vibe/tools/dxcap.sh original PerfConsole 150
DXCAP_NOVSYNC=1 DXCAP_PERF=1 DXCAP_SETTINGS=vibe/tools/dxcap/perf-settings.json \
    vibe/tools/dxcap.sh fork PerfConsole DX.dx 150
vibe/tools/perf/frame-report.py <original run> <fork run>
```

- **The windows** are the wall clock's, the level's `Hour` to `Millisecond`, which both engines
  set from the local time each tick: each `DXPERF:` line gives a window's span in milliseconds of
  the day and the frames in it. The total line gives frames a second, the mean, p50, p99 and the
  worst frame. Started at `DX.dx`, the fork travels to the map as the original does.
- **The original's own split**: its `Engine` object's `GameCycles` (the game's tick) and
  `ClientCycles` (the frame drawn), read each frame where they are not 0 (the fork keeps neither
  and has no `CyclesToSeconds`), are each line's `game` and `client`.
- **`frame-report.py`** keeps the samples inside the windows and gives the main thread's time a
  frame by area and by function, and its cycles and instructions a frame. The original's
  functions are each DLL's nearest export below the sample (its base from `maps.txt`):
  `Core.dll` and `Engine.dll` export most of theirs, `Render.dll` 97 of them, so names in it
  are only near. The fork's are perf's own symbols. `--mean` averages each engine's runs.
- **One run at a time**, nothing else running; three of each, alternating, before a number is
  taken. The machine's speed drifts from run to run by 10% and more, its CPU at full clock
  (its power and memory clocks, the scene's own changes): a change to the fork is measured by
  alternating runs of the two builds (`DXCAP_ENGINE`), and by the time of the functions it
  changes, which the report gives.

### Crashes

An original run that crashes shows a "Critical Error" dialog on the desktop and hangs
until the harness kills it, its log cut short: the game writes the log only as it exits.
Dismissed, the dialog lets the game log the error and the calls under it, and exit. A fork run
that crashes leaves a core, which systemd keeps.

```sh
# The original's dialog, dismissed
xdotool search --name 'Critical Error'    # its <window>
xdotool windowfocus --sync <window>
xdotool key Return
# The fork's core: the functions on the stack (the Release build has no lines)
coredumpctl dump <pid> --output=<scratchpad>/core
gdb -batch -ex bt <parent>/build/linux-x86_64/engine/SurrealEngine <scratchpad>/core
# Lines: a copy with symbols (some two and a half minutes), run with the last fork run's ini
cmake -S VibeEngine -B <scratchpad>/dbg -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build <scratchpad>/dbg --target SurrealEngine
cd gamefiles && gdb -batch -ex run -ex bt --args <scratchpad>/dbg/SurrealEngine --no-launcher <parent>/gamefiles \
    --ini=<parent>/build/dxcap/System/Fork.ini --userini=<parent>/build/dxcap/System/ForkUser.ini --url=<map>
```

An object freed while something still points at it shows only in an AddressSanitizer build,
which stops at the first bad access with the stacks of the access, the free and the allocation.
A harness run takes it through `DXCAP_ENGINE`:

```sh
cmake -S VibeEngine -B <scratchpad>/asan -C deusex-launcher/linux-x86_64/ports/linux-x86_64/engine.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address
cmake --build <scratchpad>/asan --target SurrealEngine
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 DXCAP_ENGINE=<scratchpad>/asan/SurrealEngine \
    vibe/tools/dxcap.sh fork <console> <map>
```

## Gotchas

The workspace's:
[gotchas](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#gotchas-that-cost-time).
Starting the engine (a map only after `=`, SIGTERM under SDL, `Running.ini`):
[running it](ENGINE.md#running-it). Profiling the handheld on the handheld, and the hooks off
before changing the engine: [profiling](ENGINE.md#profiling-and-validating-on-the-desktop),
[the profiling hooks](ENGINE.md#the-profiling-hooks).

- **`compile` takes `DXCapture.u` away before it builds it again**: a run started meanwhile, or
  one of a sweep, finds no console class. Compile between runs, never under one.
- **A fork run straight into a mission map leaves `Save/Current` as earlier runs left it**: only
  travel to another mission, or a new game, empties it. A run that returns to a map within its
  mission takes that map from there; a timeline that first crosses a mission starts it empty.
- **An unattended run sees no NPC react to the player.** Liberty Island starts the player 7,000
  to 20,000 units from every NSF, and no NPC reacts to a player it cannot see. `AIConsole`,
  `MoveConsole` and `ReachConsole` watch the AI's own work (states, routes, the reachability
  tests) without a player. To see NPCs react, a console class stands the player in front of one,
  at a spot written into the class, rather than a hook in the engine.
