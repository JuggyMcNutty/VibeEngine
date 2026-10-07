# The engine

VibeEngine is [Surreal Engine](https://github.com/dpjudas/SurrealEngine), an open-source
reimplementation of Unreal Engine 1 that recognises this build of Deus Ex directly (`DeusEx.exe`
SHA1 `2a933e26aa9cfb33b37f78afe21434caa031f14a` is its `DEUS_EX_1112fm` database entry), forked
for Deus Ex. It carries what [Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s
ports need (their launcher, embedded GPUs, pads, a handheld's speed) and what Deus Ex needs to play
as the original does. Its branch is `deusex`. What it still lacks of the original is
[`NATIVES.md`](NATIVES.md); how to work on it, [`DEVELOPMENT.md`](DEVELOPMENT.md).

## How it is kept

- **Pinned in the workspace.** Port Ex Machina's `ENGINE-PIN.txt` names this repository, its
  branch and the one commit the ports build. Its `scripts/engine.sh fetch` clones the fork as
  `VibeEngine/` in the parent folder of the repositories and checks that commit out; `check`
  proves the clone is at it; after a fork commit is pushed, `pin` moves the file, and the move is
  committed there. Builds go to `build/<port>/engine` in that parent folder, never into the clone.
- **Nothing goes upstream**, and upstream's contribution rules do not apply.
- **Upstream is not merged.** The fork and Surreal Engine differ at the core: upstream runs its
  scripts through its own interpreter, `Frame::RunExpr`, where the fork runs them through the
  `ExpressionEvaluator` it reworked ([script VM](#script-vm)). A fix of upstream's worth having
  is ported by hand, as a commit of the fork's own.
- **Deus Ex gets the fork's behaviour; other games keep upstream's.** Code that makes the engine
  behave as Deus Ex's original binaries do is gated on `IsDeusEx()`, upstream's test that the
  game's executable is `DeusEx`, asked as `engine->LaunchInfo.IsDeusEx()` or
  `engine->packages->IsDeusEx()`. A render device does not ask: the engine sets its flags from the
  test (`RenderDevice::DarkClamp` before the device is made, `GammaScale`). Natives of Deus Ex's
  own classes need no gate: only Deus Ex has them. The speed-ups and device support apply to
  every game.
- **Licence.** Using and building the engine is permitted by its own licence, which grants use
  "for any purpose".

## Commands

In the workspace, Port Ex Machina's:

```sh
scripts/engine.sh fetch                     # clone the fork at the pin
scripts/engine.sh check                     # the clone is at the pin, on its branch
scripts/engine.sh status                    # the pin and the fork's branch
scripts/engine.sh pin                       # after a pushed fork commit: move ENGINE-PIN.txt to it
scripts/engine.sh build <port>              # build/<port>/engine, from the port's engine.cmake
```

`scripts/dx.sh build <port>` calls `build` for ports that ship the engine, and
`scripts/dx.sh check` runs `check`. In this clone, the fork's own:

```sh
vibe/tools/perf/perf.sh on|off|save         # the profiling hooks (below)
vibe/tools/host-tools.sh                    # perf and the validation layer, into the repositories' deps/
vibe/tools/natives_audit.py                 # the original's natives against the fork's (NATIVES.md)
vibe/tools/dxcap.sh                         # scripted runs of both engines (DEVELOPMENT.md)
```

## Changing the engine

Commit the change here with the docs it affects (this one and [`NATIVES.md`](NATIVES.md)), push
it, then pin it in the workspace (`scripts/engine.sh pin`) and commit the moved `ENGINE-PIN.txt`
there. Pushing and pinning wait for the owner's go-ahead
([the workspace's rules](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#rules)).

- Each commit's message says what it changes, why, and how it was checked.
- The fork's history is published and never rewritten.
- Temporary debug hooks never reach a commit
  ([the rule](DEVELOPMENT.md#temporary-debug-hooks)).
- "Patch NNNN" is a numbered commit, 0001–0042 ([the numbered patches](#the-numbered-patches)).

### The profiling hooks

The frame-time profiling hooks are `vibe/tools/perf/perf-instrumentation.patch`, applied for a
profile and never committed: `vibe/tools/perf/perf.sh on`, and `off` afterwards.

- **Take them off before changing the engine**: a commit made with them on carries them. Commit
  before putting them back: `on` cannot merge over uncommitted changes to a file the hooks touch,
  and says so.
- **The patch is against the fork's head**, so a fork commit that touches the same lines moves
  them. `on` then falls back to a three-way merge (and stops if that leaves conflicts), and `save`
  rewrites the patch from the tree (all of it but `vibe/`), so the next `on` and `off` apply
  cleanly.

With the hooks on, the engine builds with frame pointers (~1% slower on the handheld) and reads
[the hooks' variables](#settings-and-environment). `SURREAL_PERF_SAMPLE=<file>` is a sampling
profiler for devices without `perf`: it samples the main thread's CPU time, recording each
sample's program counter and the return addresses a frame-pointer walk finds, one block per
60-frame report, with `/proc/self/maps` beside it.
[`vibe/tools/perf/sample-report.py`](../tools/perf/sample-report.py) (Python 3, and the port
toolchain's `nm` for a cross build) turns that into self and inclusive time per function,
optionally under one caller (`--root ULevel::Tick`: the game tick) or with the callers of one
(`--callers`). A leaf function keeps no frame record, so its samples show its caller's caller as
the next frame. Profiling the handheld:
[its README](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance).

### Natives from the original

Where upstream has a Deus Ex native wrong or as a stub
([how stubs show](NATIVES.md#how-it-is-known)), the original is in the game's DLLs.
[dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info) is what has been read of them,
and [working on the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#working-on-the-binaries)
says how to read more; `vibe/tools/natives_audit.py` lists the stubs.

## Running it

Each port's `run-game.sh` starts it for every real launch. By hand, from the game's directory:

```sh
SurrealEngine --no-launcher /path/to/deusex --url=01_NYC_UNATCOIsland.dx
```

- `--no-launcher`, or a game folder on the command line, skips upstream's desktop launcher
  window. `--no-launcher` (or `-v`) also puts the log on stderr.
- **A map is `--url=<map>` (or `-u=<map>`): an option's value follows its `=`.** `-u <map>` or
  `--url <map>` sets an empty URL and the map name becomes a stray argument, so the intro loads
  with no warning.
- **`--server`** runs a dedicated server of the `--url` map, as the original's `-SERVER`: no
  window, sound or player; `--lanplay` gives it the LAN tick rate
  ([multiplayer](NATIVES.md#multiplayer)).
- **Under SDL the engine ignores SIGTERM**: SDL turns it into a quit event nothing reads. On the
  handheld, whose only display backend is SDL's, stop it with SIGKILL (`kill -9`). A desktop run
  on Wayland or X11 takes SIGTERM's default, and a dedicated server ends on it (143).
- `Running.ini` is the launcher's crash sentinel, not the engine's: a run under a launcher
  stopped either way leaves it behind like any crash; a run by hand has none.
- `--ini=<file>` and `--userini=<file>` name the inis to read and write back, as the original's
  `INI=` and `USERINI=`; the `shot` console command writes the next `ShotNNNN.bmp` into the
  game's System folder; `--timeline=<file>` gives keys, the pointer and console commands at set
  times. [Scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines) use all three.
- **`--cmdline=<line>`** is the original's command line, which the recreated launcher
  (deusex-launcher's `main`) passes on: its start URL, `-server`, `INI=`, `USERINI=`, `EXEC=` and
  safe mode's flags, over the options above ([the command line](NATIVES.md#the-command-line)).
- **`DXL_LAUNCHER_FD`** in the environment is the recreated launcher's line to the engine, which
  stays for the game's run as the original's process does
  ([the game and the launcher](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#the-game-and-the-launcher)):
  the engine says `hello` as it starts and `ready` once its first map is in, and takes
  `TakeFocus` (the window to the front) and `Open <url>` (the console's `open`), what a second
  launch forwarded. The ports' launchers set nothing, and nothing changes.
- Where there is no audio device (a container), give OpenAL Soft the null driver
  ([linux-x86_64's README](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/README.md#audio)).

### Profiling and validating on the desktop

The handheld has no `perf` and no Vulkan validation layer, so validation runs against the base
port's build (`scripts/dx.sh build linux-x86_64 engine`), and so can a quick CPU profile. The
desktop's proportions are not the handheld's (its Cortex-A53 pays far more for a cache miss), so
what to work on next is decided by the handheld's own samples. `vibe/tools/host-tools.sh` unpacks
pinned copies of Linux `perf` and the Khronos validation layer into the `deps/` beside the
repositories without installing anything. From their parent folder:

```sh
VibeEngine/vibe/tools/host-tools.sh
cd gamefiles    # the engine is started from the game's directory
# CPU profile of Liberty Island, recording from 25 s in (after the load):
LD_LIBRARY_PATH=../deps/perf/usr/lib ../deps/perf/usr/bin/perf record -F 2000 --delay=25000 -o /tmp/se.data -- \
    timeout -s KILL 55 ../build/linux-x86_64/engine/SurrealEngine --no-launcher "$PWD" --url=01_NYC_UNATCOIsland.dx
LD_LIBRARY_PATH=../deps/perf/usr/lib ../deps/perf/usr/bin/perf report -i /tmp/se.data --no-children --sort symbol
# Synchronization validation (add SURREAL_VK_NO_BINDLESS=1 for the handheld's texture path):
VK_LAYER_PATH=$PWD/../deps/vulkan-layers/layers VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true \
    timeout -s KILL 60 ../build/linux-x86_64/engine/SurrealEngine --no-launcher "$PWD" --url=01_NYC_UNATCOIsland.dx
```

`--call-graph dwarf` on `perf record` gives callers (the desktop build has no frame pointers
unless the hooks are on).

## Settings and environment

The engine reads `~/.config/SurrealEngine/Settings.json`, which the ports' launcher writes
([where the settings live](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md#where-the-settings-actually-live)).
The fork's keys:

| Key | What it does |
|---|---|
| `Performance.AiLevelOfDetail` | Distant AI, off by default. A pawn out of sight and beyond 1500 units runs its script thinking (its `Tick` event and state code) every 3rd frame, given the time it skipped; beyond 4000 units, every 6th. Its movement, physics, animation and timers run every frame. |
| `Performance.RenderScale` | The scene drawn at this fraction of the window's size (0.25 to 1; default 1) and scaled up to it, by the Vulkan device and the GL device. |
| `"Type": "GLES"` in `RenderDevice` | The GL device on an OpenGL ES 3.2 context (on the Smart Pro, the `[GLES]` renderer of its `renderers.ini`). |
| `Gamepad` | The pad: `Enabled`, `DeadZone`, `LookSensitivityX` and `LookSensitivityY`, `InvertY`, `CursorSpeed`; `Layout` names the binding preset a launcher last applied, which the engine only carries ([controller support](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md#controller-support)). |

The environment:

| Variable | What it does |
|---|---|
| `SURREAL_VK_NO_BINDLESS=1` | Forces the Vulkan device's per-batch descriptor-set path, the one a GPU without descriptor indexing (the handheld's) takes. |
| `SURREALWIDGETS_DISPLAY_BACKEND` | The window backend: `SDL2`, `SDL3` or `X11`. Unset, or naming one the build lacks: the first that starts of Wayland, X11, SDL3 and SDL2. `run-game.sh` defaults it to `SDL2`, the backend with the pad. |
| `SURREALWIDGETS_FONT`, `SURREALWIDGETS_MONOSPACE_FONT` | The UI's font files, on a build without GSettings and fontconfig (the embedded cross build). |
| `DXL_LAUNCHER_FD` | The recreated launcher's line ([running it](#running-it)). |
| `SURREAL_GC_DRYRUN=1` | Each collection marks and frees nothing: the log has what would go, by where it lives and by class, and the first holder outside them of each object of a level left behind ([objects and memory](#objects-and-memory)). |
| `SURREAL_GC_STRESS=<frames>` | A collection every that many frames, besides the map loads'. |
| `SURREAL_GC_VERIFY=1` | Each collection checks that every pointer it is handed is a live object, and logs the holders of those that are not. |
| `SURREAL_PERF_LOG=1`, `SURREAL_PERF_DETAIL=1`, `SURREAL_PERF_TURN=<aBaseX>`, `SURREAL_PERF_SAMPLE=<file>` | Only with [the profiling hooks](#the-profiling-hooks) on: where the frame goes, every 60 frames on stderr; tick by actor class and script functions by self time (which inflates what it measures); the player turning on the spot; the sampling profiler. |

## What the fork changes

By area, as the code stands. A number such as (0005) is a [patch number](#the-numbered-patches).
The handheld's numbers are [the Smart Pro's Performance](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance).

### Running on our devices

- **The version**: every build stamps the fork's commit and its date into `vibe_version.h`
  (`vibe/cmake/version.cmake`, a step of every build that rewrites the header only when the
  text changes, so only what reads it recompiles); `GetDeusExVersion` shows it under the main
  menu, as `1.112fm VibeEngine <commit> (<date>)`. A tree with uncommitted changes to tracked
  files reads `-dirty`; one without git, `unknown`.
- **Headless and embedded** (0001): no launcher window; the log and errors on stderr; a non-zero
  exit after a caught exception, which the launcher's crash sentinel reads. The cross build for
  an embedded aarch64 device is SDL2 only (no X11, Wayland or desktop GL), takes SDL from
  pkg-config and a host-built `zipdir`, and finds fonts without GSettings or fontconfig.
- **The gamepad** (0003): the SDL2 display backend gives the pad as polled state
  (`GetGamepadState`), and `GamepadInput` turns it into UE1 joystick keys and axes that
  `User.ini` binds; menus get their own controls. Only the SDL2 backend has it, and
  SurrealWidgets builds SDL3's instead when both are on, so the ports build the engine without
  SDL3.
- **The launchers' lines**: `--cmdline=` (`OriginalCommandLine`) and `DXL_LAUNCHER_FD`
  (`LauncherLine`) ([running it](#running-it)), with the console's `exec` and an `open` that
  takes a map's file name.
- **Fullscreen under SDL2** is a borderless window covering the display, set up while the window
  is hidden: `SDL_SetWindowFullscreen` deadlocks on a GL window with the Smart Pro's vendor SDL2
  (0039).

### Settings the launcher exposes

`LauncherSettings` reads the `Gamepad` and `Performance` blocks
([the keys](#settings-and-environment)). Distant AI (0008, 0022) is `UActor::ThinkThisFrame`,
asked each tick; a pawn counts as seen when the renderer drew it this frame (`LastVisibleFrame`).
Render scale (0009, 0038) is `RenderDevice::GetRenderScale`: a device whose
`SupportsRenderScale()` says so (Vulkan, GL) makes its scene buffers at `GetRenderWidth()` by
`GetRenderHeight()` and scales them to the window as it presents; the viewport, the canvas and
the UI see the render size.

### Rendering

For every game, the renderer's CPU work is lighter: lightmaps lit only where a light reaches
(0005); the light tree and each surface's lights kept while no light changes (0023); the
clipper's occlusion grid one row per image row (0010), its non-SSE (ARM) build skipping the clip
for triangles inside the view (0020, an upstream bug); one-sided surfaces seen from behind
skipped before the visibility test (0011); a surface's points gathered only when a test needs
them (0021); each mesh vertex animated, lit and fogged once a draw (0018), and a run of faces
with one texture drawn in one device call (0019). Deus Ex's look, as `Render.dll` and
`D3DDrv.dll` make it, is gated and described by feature in [`NATIVES.md`](NATIVES.md); its code
is in `SurrealEngine/Render/` (`VisibleFrame.cpp`, `VisibleMesh.cpp`),
`SurrealEngine/Light/` and `SurrealEngine/Packages/Engine/Resources/Textures/` (`FireEngine.cpp`,
the fractal textures).

**The Vulkan device**: the game tick runs while the GPU draws the previous frame
(`CommandBufferManager` keeps one frame in flight, and `WaitForFrame()` collects it before
anything it uses is touched; a swapchain rebuild waits for the device; 0004). Without
`VK_EXT_descriptor_indexing` it binds a descriptor set per batch instead of bindless textures
(0002). It decodes on the CPU what the GPU cannot sample or filter: BC1–5, RGB8, RGBA32F (0002),
and converts the lightmaps for upload in NEON on ARM (0024).

**The GL device** runs desktop GL 4.2+ and OpenGL ES 3.2 from one code path (0035–0042):
`RenderAPI::GLES` and `RenderDeviceType::GLES`, an ES 3.2 context from every window backend
(SDL2 and SDL3 through `SDL_GL_CONTEXT_PROFILE_ES`, X11 and Wayland through EGL with the ES3
bit), and `IsGLES` read at init from `glGetString(GL_VERSION)`. It draws at the render scale, as
the Vulkan device does (0038; [the setting](#settings-the-launcher-exposes)). The same shader
sources compile as GLSL ES 3.20 (`CompileGlsl` prepends the version and highp precision; the
sources keep to `u`-suffixed masks, float literals and `std140` push-constant blocks). No
desktop-only calls: `glClearDepthf` and `glDepthRangef`, `glDrawBuffers` for every
`glDrawBuffer`, no `GL_DEPTH_CLAMP` or `GL_MULTISAMPLE` on ES, the plain blend calls, the null
texture as `UNSIGNED_BYTE`.

- **ES 3.2, not 3.0**: RGBA16F (the scene buffers with `Hdr`) is colour-renderable only from ES
  3.2 core, and the shaders bind their samplers with `layout(binding)`, which GLSL ES 3.00 lacks.
  `gl_FragCoord.w` is 1/w on ES as on desktop, so the detail-texture distance fade is unchanged.
- **The desktop context**: every Linux window backend asks for a desktop 4.2 core context, as the
  shaders are GLSL 4.20 (`CompileGlsl` prepends `#version 420`): a driver without 4.2 fails at
  the context, with an error that says so, not at a shader.
- The vertices stream through CPU staging arrays, each flush uploading the range written since
  the last one (`glBufferSubData`): the GE8300 has no `glBufferStorage` (persistent mapping), and
  mapping the buffers' unused tails on every flush costs 2/3 of its frame.
- The scene buffers are RGBA8 unless `Hdr` is on.
- BC1 without `EXT_texture_compression_s3tc` and RGBA32F (the lightmaps) without
  `OES_texture_float_linear` are decoded to RGBA8 on the CPU.
- The samplers set no `GL_TEXTURE_LOD_BIAS` on ES (no ES sampler has it), anisotropy only with
  `EXT_texture_filter_anisotropic`, and `GL_MIRROR_CLAMP_TO_EDGE` only where a probe at init
  finds the driver takes it (else `CLAMP_TO_EDGE`).
- `ReadPixels` reads the buffer as it is, the right way up (upstream's is an `#if 0` stub of
  D3D11 code, its shots black); `Exit()` runs once and tolerates a failed unmap, as the intro
  can end the game after the window's context is gone.

### Script VM

The fork reworks upstream's `ExpressionEvaluator` and `Frame::Run` for speed (0006, 0007, 0012,
0014–0017, 0028, 0029): no `dynamic_cast` or heap allocation on the call path (parameters from
`Properties`, functions and events found through `UClass::VirtualFunctionCache`); each
expression node classified once (`ExpressionEvaluator::Classify`, `Expression::TypedKind`), so
conditions, `&&` and `||`, the commonest operators (`UFunction::FastOperator`) and assignments
to plain variables evaluate as plain values, not through an `ExpressionValue`; conditions,
jumps, plain assignments, calls, `return;` and a foreach's next pass run by `Frame::Run` in
place, without an `ExpressionEvalResult` each (`Frame::ClassifyStatement`). A call leaving out
an optional struct or array argument does not copy it, where upstream crashes (0033).

### Game tick

The actor iterators find a class's actors from an index, not a scan of the level (0013).
Collision, for every game: a ray trace hands each BSP child only its part of the segment (0025)
and tests a polygon's plane before its vertex count and surface (0030); sight lines test the
actors of only the cells they cross (0026); each cell's actors sit in an open-addressed table,
found by `CollisionSystem::FindCell`, where upstream keeps a `std::unordered_map` of
`std::list`s (0031); moves and traces go without casts and heap allocations (0032); a walking
pawn steps to the ground with its dry run's trace (0027). Deus Ex's traces and moves are
`Engine.dll`'s (its backoffs, `FindSpot` and encroachment, moves held off what they meet as
`ULevel::MoveActor` holds them, landing as `processLanded`; [implemented, not as the
original](NATIVES.md#implemented-not-as-the-original)), in
`SurrealEngine/Collision/TopLevel/TraceTest.cpp` and `UActor_Phys.cpp`.

### Objects and memory

`SurrealEngine/GC`: a mark and sweep from roots, as UE1's collector
([the original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#garbage-collection)).
A collection runs at the end of the frame, with no script running (`Engine::CollectGarbage`,
in `EngineGC.cpp`), when asked for: by every map load but Entry's, by the console's
`obj garbage`, by 2048 destroyed actors let go since the last one
([destroyed actors](NATIVES.md#housekeeping-not-seen-directly)), and every
`SURREAL_GC_STRESS` frames. It logs the original's lines
(`Collecting garbage`, `Purging garbage`, `Garbage: objects: ...; refs: ...`) with the
milliseconds it took, the resident memory before and after, what it freed by kind and the
references it made None by holder, and gives the freed memory back to the system (`malloc_trim`).

- **Roots** (`Engine::MarkRoots`, in `EngineGC.cpp`): the engine's subsystems, levels and
  objects; the net drivers' players, channels and package maps; the natives; every export of a
  save info. Loaded code (a `UField` in a package) is always kept; any other export of a package
  stays only while something reaches it, and is loaded from its file again when next asked for.
  A map's or a save's package is not a root: what its level reaches stays.
- **What an object holds**: its class, package, delay load and state frame, and the references
  its properties hold (`UStruct::RefProps`); a native class marks its own members in its `Mark`.
  An object reached only through its package keeps the package's file and tables, not its other
  objects.
- **Where objects are made**: each in the package of the level it belongs to, as the
  original's: a spawned actor in its level's (the Entry level's own pawn in Entry's), the flag
  base and what travels with the player in the player's, the event manager in its
  `LevelInfo`'s, a conversation's camera and flag refs in the conversation's. A package played
  as a level is tagged so (`Package::IsLevel`).
- **A sound** is decoded once, when first played, for the audio device, which keeps a copy of
  its own (the mixer's 16-bit one, OpenAL's buffer); the sound keeps only its length, channels,
  rate and loop. Its lip-sync shapes decode it again once, into a buffer let go after.
- **Saving a level** writes what the level reaches (`PackageWriter`), never the rest of its
  file: an export the collector freed is not loaded back to be saved. An object of a transient
  class is not written either ([as the original's](NATIVES.md#saving-loading-and-travel)).
- **Elimination**: a reference to an object flagged `EliminateObject` is made None where the
  marker finds it (`GCMarker::Mark`), as the original's. One held where it may not be written
  (`MarkConst`: the subsystems, the net layer, what code names) keeps it, and the log says so.
  A collection flags every object of a level left behind (a package tagged as a level that is
  neither the current one nor Entry's) and, at a map load, the new level's actors that are not
  in its `Actors`, as the original's. `Object.CriticalDelete` flags the object it is given.
- **The weak holders** let go of the dying before anything is freed (`GC::IsDying`): the
  camera actor, the coronas and iterator actors, the lights the light system lists, the audio
  device's sound numbers and reverb zone, the open package files.
- **The sweep** tells each dying object first, all of them still allocated
  (`OnGCDestroy`): its properties are destructed and its package's export slot emptied, so a
  later reference loads it from its file again; a sound leaves the audio device, a decal its
  BSP nodes, a package closes its file, a freed texture has the render device flushed (it caches
  textures by address) if anything was drawn since its last flush. Then each is freed.
- **The names** go after the objects: a name the engine makes for an object
  (`NameString::Collectable`, through `Package::MakeUniqueObjectName`) is deleted when no live
  object holds it (`GCObject::GCMarkNames`: its own name, its properties', its state frame's
  locals), its slot taken by the next name made; a name looked up by its spelling is kept from
  then on, so no other holder can see its index taken. A cache keyed by name indexes keeps no
  collectable name (`FindScriptFunction`).

### Gameplay

What else Deus Ex needs is gated as [above](#how-it-is-kept) and described by feature in
[`NATIVES.md`](NATIVES.md). Where most of its code is:

- **The path search and reachability tests**: Deus Ex's `FindPathToward`, `FindPathTo` and the
  `pointReachable`, `actorReachable` and `Reachable` they ask are `Engine.dll`'s, walking its own
  moves (`UPawn_Path.cpp`, `UPawn_ReachDeusEx.cpp`); `RouteCache` stays empty, as the original
  never fills it; other games keep their own flow over the corrected search
  ([moving](NATIVES.md#moving-wandering-and-tactical-movement)).
- **The audio mixer**: `GalaxyMixer` (`SurrealEngine/Audio/GalaxyMixer.cpp`) mixes Deus Ex's
  sounds as `Galaxy.dll` does (pan, fall-off, sliders, resampling, its reverb) and streams them
  through one OpenAL source at `OutputRate`; other games keep OpenAL's 3D sources. The fork's
  audio device adds `SpeechVolume` and `DopplerSpeed` to its ini section
  ([sound](NATIVES.md#sound)). Who hears a sound an actor plays, a remote player through its
  client, is `UActor::HearSound` and `CheckHearSound` (`UActor.cpp`), from Deus Ex's
  `PlaySound`, `PlayOwnedSound` and `DemoPlaySound` natives (`NActor.cpp`).
- **The network driver**: `SurrealEngine/Network/` is the fork's own, the original's protocol
  over UDP (`NetDriver`, channels, the package map, replication by the original's lists) with a
  client that joins the original's servers, a listen and a dedicated server, downloads and
  server travel; `IpDrv`'s `InternetLink`, `TcpLink` and `UdpLink` are the original's
  ([multiplayer](NATIVES.md#multiplayer)).

### The numbered patches

Other docs cite these numbers, which git does not record.

| Patch | Commit | What it does |
|---|---|---|
| 0001 | [22a5e87](https://github.com/JuggyMcNutty/VibeEngine/commit/22a5e87cc51aa83be550abe1c17e0b4203f18c79) | Headless and embedded aarch64 support: no launcher window, the log on stderr, an SDL2-only cross build |
| 0002 | [a40bec6](https://github.com/JuggyMcNutty/VibeEngine/commit/a40bec64d33574529da21d63c1b57b3b3ebfe85e) | Vulkan without descriptor indexing (a descriptor set per batch); CPU decoders for texture formats the GPU lacks |
| 0003 | [af99616](https://github.com/JuggyMcNutty/VibeEngine/commit/af99616f537480cc63f9f781865e2e76634abe44) | The gamepad; `CycleActors` resumes where it stopped; the first pause-menu press after skipping the intro is not swallowed |
| 0004 | [e568662](https://github.com/JuggyMcNutty/VibeEngine/commit/e56866259cfd555d44669701e65643e2d0c69b2a) | Vulkan: the CPU runs the next frame while the GPU draws this one |
| 0005 | [03afa60](https://github.com/JuggyMcNutty/VibeEngine/commit/03afa604679b0e8e89ea5100bb58c1484d677f41) | Lightmaps lit only where a light reaches |
| 0006 | [af2ed86](https://github.com/JuggyMcNutty/VibeEngine/commit/af2ed868bfe107485ad905a2c183405f01139391) | VM: no casts on the call path; a per-class virtual-function cache |
| 0007 | [9cc49e2](https://github.com/JuggyMcNutty/VibeEngine/commit/9cc49e284b2e5f3d9f9a12fbd0449117afbc9d79) | VM: native frames without locals, event names looked up once, plain-data locals zero-filled |
| 0008 | [ce78355](https://github.com/JuggyMcNutty/VibeEngine/commit/ce78355fb3cc47b2ac27dd18b3751c2c564dd5ce) | Distant AI: pawns out of sight think every third frame |
| 0009 | [a1a2926](https://github.com/JuggyMcNutty/VibeEngine/commit/a1a2926f93fbd6be6288f4dd87191ca36ae9f0f9) | Render scale: the scene drawn smaller than the window and scaled up (Vulkan) |
| 0010 | [d635be4](https://github.com/JuggyMcNutty/VibeEngine/commit/d635be4bda5c027b5e0b34ee3c13aa64ebc28217) | The clipper's occlusion grid: one row per image row |
| 0011 | [7599d2b](https://github.com/JuggyMcNutty/VibeEngine/commit/7599d2b600015df7f2eec1e683cd94b6f55c2e5b) | One-sided surfaces seen from behind skipped before the visibility test |
| 0012 | [f4ea318](https://github.com/JuggyMcNutty/VibeEngine/commit/f4ea318b0b71718e83c19b0e0efd208379bfc91c) | VM: one evaluator per statement, nested values returned directly |
| 0013 | [9720823](https://github.com/JuggyMcNutty/VibeEngine/commit/9720823c814691ca1455cbef65d13c629fac2a60) | Actor iterators: a class's actors from an index, not a scan |
| 0014 | [829adcb](https://github.com/JuggyMcNutty/VibeEngine/commit/829adcbd7e1d6e109ae2cd81f67f4e46a84f5c95) | VM: calls without heap allocations or walks over every local |
| 0015 | [6ba1983](https://github.com/JuggyMcNutty/VibeEngine/commit/6ba1983af99b9fd70a1e6133a70332578a13431c) | VM: the 25 commonest operators evaluated in place |
| 0016 | [e28aa41](https://github.com/JuggyMcNutty/VibeEngine/commit/e28aa410d11e3a07848d602d814171d0b99c470f) | VM: events found through the virtual-call cache |
| 0017 | [51d45aa](https://github.com/JuggyMcNutty/VibeEngine/commit/51d45aa37c96a9bc5656d4ce5d89992e94fa8d13) | VM: the commonest leaf expressions made without the visitor |
| 0018 | [efc2a80](https://github.com/JuggyMcNutty/VibeEngine/commit/efc2a80026cbc0768503c0c365e15db0d9df2c4b) | Meshes: each vertex animated, lit and fogged once a draw |
| 0019 | [abe6d27](https://github.com/JuggyMcNutty/VibeEngine/commit/abe6d2735c4e4a2a61479e7fc7964b36b61f8e82) | Meshes: a run of faces with one texture drawn in one device call |
| 0020 | [96f1b6b](https://github.com/JuggyMcNutty/VibeEngine/commit/96f1b6b4b1d7b59cdfca4c878a93a243116cf98c) | The clipper's non-SSE build skips the clip for triangles inside the view |
| 0021 | [377cf46](https://github.com/JuggyMcNutty/VibeEngine/commit/377cf462b1452f880723cce4305087172bfe7463) | A surface's points gathered only when a test needs them |
| 0022 | [a41d14b](https://github.com/JuggyMcNutty/VibeEngine/commit/a41d14b1180e2e04957d1b19d7a5406b88800b6c) | Distant AI: every sixth frame beyond 4000 units |
| 0023 | [56e86e5](https://github.com/JuggyMcNutty/VibeEngine/commit/56e86e57548c00aa5ccb597a52aed093ceaa9172) | The light tree and each surface's lights kept while no light changes |
| 0024 | [03e4d0c](https://github.com/JuggyMcNutty/VibeEngine/commit/03e4d0cb9696bbad5b26cdc0489dcc028152c29b) | Lightmap uploads: the float-to-byte conversion in NEON on ARM |
| 0025 | [f984a78](https://github.com/JuggyMcNutty/VibeEngine/commit/f984a7800675f85cc5e7eb134de36b03ffa8aef8) | Ray traces: each BSP child gets only its part of the segment |
| 0026 | [0f9ce6c](https://github.com/JuggyMcNutty/VibeEngine/commit/0f9ce6cccd7dbc52bf0c71a57ae9490573e08d18) | Sight lines test only the collision cells they cross |
| 0027 | [469d9c8](https://github.com/JuggyMcNutty/VibeEngine/commit/469d9c8a26d8e910b14576bfca4fb650862681d3) | A walking pawn's step to the ground made with its dry run's trace |
| 0028 | [40e219a](https://github.com/JuggyMcNutty/VibeEngine/commit/40e219ac8bc05a349950766408daea74c77c57bb) | VM: conditions, operators and plain assignments evaluated as plain values |
| 0029 | [7e93fe7](https://github.com/JuggyMcNutty/VibeEngine/commit/7e93fe7b86f0e449454db03d9d8eb02b55d6dcfd) | VM: the commonest statements run in place |
| 0030 | [2d315e3](https://github.com/JuggyMcNutty/VibeEngine/commit/2d315e3602663f73f802a25a58c282ae545dafec) | Ray traces: a polygon's plane tested before its vertex count and surface |
| 0031 | [0098ca8](https://github.com/JuggyMcNutty/VibeEngine/commit/0098ca8c5f69c1d2c8ff397a75d1915afc20872b) | Collision: each cell's actors in an open-addressed table |
| 0032 | [e9a806d](https://github.com/JuggyMcNutty/VibeEngine/commit/e9a806d56f88f63efded8ec14f8487afbc863b0a) | Collision: moves and traces without casts or allocations |
| 0033 | [bec6e26](https://github.com/JuggyMcNutty/VibeEngine/commit/bec6e261edcd00d9225cb95ef7e4a8e0b7298261) | VM: an optional argument left out is not copied into the call |
| 0034 | [b5d0885](https://github.com/JuggyMcNutty/VibeEngine/commit/b5d08853dbf4e24894d56942c07a5a743438e824) | NPCs see: `IsValidEnemy`, `AICanSee` and `AIVisibility` as the original's |
| 0035 | [e77ac55](https://github.com/JuggyMcNutty/VibeEngine/commit/e77ac558510f7aa3e10d0af783d3232af9e18498) | The GL device's `ReadPixels` |
| 0036 | [494e22e](https://github.com/JuggyMcNutty/VibeEngine/commit/494e22e09391cc08e36d562e4573ff4ad9e67bec) | The GL device's `Exit()` runs once and does not throw |
| 0037 | [db0961e](https://github.com/JuggyMcNutty/VibeEngine/commit/db0961e5b06b64ad6aaf4d4babb88f4ad35851fd) | The GL device on OpenGL ES: `RenderAPI::GLES`, `"Type": "GLES"` |
| 0038 | [aa94a7a](https://github.com/JuggyMcNutty/VibeEngine/commit/aa94a7af8dc496d11dff3dca55b24e72c26337df) | The GL device: render scale; CPU decoders for BC1 and RGBA32F |
| 0039 | [b9bc870](https://github.com/JuggyMcNutty/VibeEngine/commit/b9bc8706a1288d94e35930a76241932c92a9d210) | The GL device on the Smart Pro: the GE8300's sampler set; fullscreen under SDL2 |
| 0040 | [765f180](https://github.com/JuggyMcNutty/VibeEngine/commit/765f180199b60e3e19ce9ebc56f77ffdce2c6cc8) | The GL scene buffers 8-bit unless HDR is on; `ReadPixels` reads what the buffer is |
| 0041 | [0c8e99b](https://github.com/JuggyMcNutty/VibeEngine/commit/0c8e99b3207d55149171250603fbf6896a478eaf) | The GL device streams its vertices through CPU staging arrays |
| 0042 | [469bd9c](https://github.com/JuggyMcNutty/VibeEngine/commit/469bd9cef81ec00cd8fe720cf66f78947f2f30d4) | `ReadPixels`: the GL shots the right way up |
