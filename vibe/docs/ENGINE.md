# The engine

VibeEngine is [Surreal Engine](https://github.com/dpjudas/SurrealEngine), an open-source
reimplementation of Unreal Engine 1 that recognises this build of Deus Ex directly (`DeusEx.exe`
SHA1 `2a933e26aa9cfb33b37f78afe21434caa031f14a` is its `DEUS_EX_1112fm` database entry), forked
for Deus Ex. It carries what
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s ports need (their launcher,
embedded GPUs, pads, a handheld's speed) and what Deus Ex needs to play as the original does.
Its branch is `deusex`. What matches the original and what still differs is
[`NATIVES.md`](NATIVES.md); how to work on it, [`DEVELOPMENT.md`](DEVELOPMENT.md).

## How it is kept

- **Pinned in the workspace.** Port Ex Machina's `ENGINE-PIN.txt` names the one commit the ports
  build, and its `scripts/engine.sh` fetches, checks, builds and pins the clone
  ([the repositories](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#the-repositories)).
  Builds go to `build/<port>/engine` in the parent folder of the repositories, never into the
  clone.
- **Upstream is not merged, and nothing goes upstream.** The fork and Surreal Engine differ at
  the core: upstream runs its scripts through its own interpreter, `Frame::RunExpr`, where the
  fork runs them through the `ExpressionEvaluator` it reworked ([script VM](#script-vm)). A fix
  of upstream's worth having is ported by hand, as a commit of the fork's own; upstream's
  contribution rules do not apply here.
- **Deus Ex gets the fork's behaviour; other games keep upstream's.** Code that makes the engine
  behave as Deus Ex's original binaries do is gated on `IsDeusEx()`, upstream's test that the
  game's executable is `DeusEx`, asked as `engine->LaunchInfo.IsDeusEx()` or
  `engine->packages->IsDeusEx()`. A render device does not ask: the engine sets its flags from
  the test (`RenderDevice::DarkClamp` before the device is made, `GammaScale`). Natives of Deus
  Ex's own classes need no gate: only Deus Ex has them. The speed-ups and device support apply
  to every game.
- **Licence.** Using and building the engine is permitted by its own licence, which grants use
  "for any purpose".

## The fork's tools

The workspace builds, stages, runs and pins the engine
([quick start](https://github.com/JuggyMcNutty/port-ex-machina#quick-start)). In this clone, the
fork's own:

```sh
vibe/tools/perf/perf.sh on|off|save         # the profiling hooks (below)
vibe/tools/perf/frame-report.py <run>...    # a PerfConsole run's frame by area and function (DEVELOPMENT.md)
vibe/tools/host-tools.sh                    # perf and the validation layer, into the repositories' deps/
vibe/tools/natives_audit.py                 # the original's natives against the fork's (NATIVES.md)
vibe/tools/dxcap.sh                         # scripted runs of both engines (DEVELOPMENT.md)
```

## Changing the engine

Commit the change here with the docs it affects (this one and [`NATIVES.md`](NATIVES.md)), push
it, then pin it in the workspace. Pushing and pinning wait for the owner's go-ahead
([the workspace's rules](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#rules)).

- Each commit's message says what it changes, why, and how it was checked.
- The fork's history is published and never rewritten.
- Temporary debug hooks never reach a commit ([the rule](DEVELOPMENT.md#temporary-debug-hooks)).

### The profiling hooks

The frame-time profiling hooks are `vibe/tools/perf/perf-instrumentation.patch`, applied for a
profile and never committed: `vibe/tools/perf/perf.sh on`, and `off` afterwards.

- **Take them off before changing the engine**: a commit made with them on carries them. Commit
  before putting them back: `on` cannot merge over uncommitted changes to a file the hooks
  touch, and says so.
- **The patch is against the fork's head**, so a fork commit that touches the same lines moves
  them. `on` then falls back to a three-way merge (and stops if that leaves conflicts), and
  `save` rewrites the patch from the tree (all of it but `vibe/`), so the next `on` and `off`
  apply cleanly.

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
[dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info) is what has been read of
them, and
[working on the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#working-on-the-binaries)
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
  (deusex-launcher's `main`) passes on: its start URL, `-server`, `INI=`, `USERINI=`, `EXEC=`
  and safe mode's flags, over the options above
  ([the command line](NATIVES.md#the-command-line)).
- **`DXL_LAUNCHER_FD`** in the environment is the recreated launcher's line to the engine, which
  stays for the game's run as the original's process does. The engine says `hello` as it starts
  and `ready` once its first map is in, takes `TakeFocus` and `Open <url>`, and passes a
  `RELAUNCH` as `relaunch <line>` before it exits; the lines and their meaning are
  [the game and the launcher](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#the-game-and-the-launcher).
  The ports' launchers set nothing, and nothing changes.
- Where there is no audio device (a container), give OpenAL Soft the null driver
  ([linux-x86_64's README](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/README.md#audio)).

## Measuring it

### Profiling and validating on the desktop

The handheld has no `perf` and no Vulkan validation layer, so validation runs against the base
port's build (`scripts/dx.sh build linux-x86_64 engine`), and so can a quick CPU profile. The
desktop's proportions are not the handheld's (its Cortex-A53 pays far more for a cache miss), so
what to work on next is decided by the handheld's own samples. `vibe/tools/host-tools.sh`
unpacks pinned copies of Linux `perf` and the Khronos validation layer into the `deps/` beside
the repositories without installing anything. From their parent folder:

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
unless the hooks are on). Against the original, the harness measures both engines' frames the
same way ([measuring both engines](DEVELOPMENT.md#measuring-both-engines)).

### Against the original

Liberty Island's level start, the player idle, both engines on the desktop's Xwayland through
OpenGL, no vsync, 1280x720 (`PerfConsole`, `DXCAP_PERF=1`), three alternating runs of each, at
engine `ebda836` on the development machine (Ryzen 9 7940HS, Radeon 780M). Milliseconds a frame
of the main thread, by area (`frame-report.py --mean`), and the same with the view turning
(`PerfTurnConsole`):

| | Original | Fork |
|---|---|---|
| Frame (frames a second) | 4.20 (238) | 2.75 (364) |
| Main thread on the CPU | 3.97 | 2.58 |
| Cycles, instructions a frame | 21.3 M, 28.7 M | 13.7 M, 27.3 M |
| Scene render | 1.40 (`Render.dll`), 0.10 fractal textures (`Fire.dll`) | 1.47, the fractal textures in it |
| Script VM and objects | 0.39 (`Core.dll`) | 0.31 |
| Game tick, AI, physics, collision | 0.37 (`Engine.dll`, `DeusEx.dll`) | 0.23 |
| Render device, GL driver | 0.32, 0.38 | 0.05, 0.20 |
| C runtime | 0.96 | 0.17 |
| Turning: frame (frames a second), cycles | 2.14 (468), 10.1 M | 1.51 (663), 6.9 M |

The original's own counters at the same runs: its game 0.45 ms a frame, its client 3.73. The
fork's frame is the shorter, held and turning: its main thread takes some two thirds of the
original's cycles a frame for about as many instructions (2.0 a cycle to 1.35), and is off the
CPU a little less, 0.17 ms a frame against 0.23. Its render takes about the original's: the
visibility walk (`DrawPolygon` and `DrawSpan`, `ProcessNodeSurface`, `ProcessNode`, the box
tests, about 0.8), meshes (`DrawLodMeshFaceDX`, about 0.25), light maps (about 0.1). The walk
waits on memory, not on arithmetic, so the original's own savings gain nothing here, or lose:
each node's box tested one frame in 16 while it shows, each point transformed once a frame into
a cache, a surface outside one side rejected without a clip
([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame));
nor does skipping the nodes of zones not yet seen, the occlusion grid's rows a cache line each,
or asking ahead (prefetch) for a node's surfaces and far child. The cache's misses cost more
than the transforms they save.

## Settings and environment

The engine reads `$HOME/.config/SurrealEngine/Settings.json`, which the ports' launcher writes;
the ports' `run-game.sh` pins `HOME` to the app's `home/`
([where the settings live](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md#where-the-settings-actually-live)).
The fork's keys:

| Key | What it does |
|---|---|
| `Performance.AiLevelOfDetail` | Distant AI, off by default. A pawn out of sight and beyond 1500 units runs its script thinking (its `Tick` event and state code) every 3rd frame, given the time it skipped; beyond 4000 units, every 6th. Its movement, physics, animation and timers run every frame ([how](#settings-the-launcher-exposes)). |
| `Performance.RenderScale` | The scene drawn at this fraction of the window's size (0.25 to 1; default 1) and scaled up to it, by the Vulkan device and the GL device ([how](#settings-the-launcher-exposes)). |
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
| `SURREAL_GC_VERIFY=1` | Each collection checks that every pointer it is handed is a live object, walks all of the code, and logs the holders of pointers that are not, of code's references to code that reach an object not permanent, and of what the list kept from the code's last walk would have missed ([objects and memory](#objects-and-memory)). |
| `SURREAL_PERF_LOG=1`, `SURREAL_PERF_DETAIL=1`, `SURREAL_PERF_TURN=<aBaseX>`, `SURREAL_PERF_SAMPLE=<file>` | Only with [the profiling hooks](#the-profiling-hooks) on: where the frame goes, every 60 frames on stderr; tick by actor class and script functions by self time (which inflates what it measures); the player turning on the spot; the sampling profiler. |

## What the fork changes

By area, as the code stands. The handheld's numbers are
[the Smart Pro's Performance](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance).

### Running on our devices

- **The version**: every build stamps the fork's commit and its date into `vibe_version.h`
  (`vibe/cmake/version.cmake`, a step of every build that rewrites the header only when the text
  changes, so only what reads it recompiles); `GetDeusExVersion` shows it under the main menu,
  as `1.112fm VibeEngine <commit> (<date>)`. A tree with uncommitted changes to tracked files
  reads `-dirty`; one without git, `unknown`.
- **Headless and embedded**: no launcher window; the log and errors on stderr; a non-zero exit
  after a caught exception, which the launcher's crash sentinel reads. The cross build for an
  embedded aarch64 device is SDL2 only (no X11, Wayland or desktop GL), takes SDL from
  pkg-config and a host-built `zipdir`, and finds fonts without GSettings or fontconfig.
- **The gamepad**: the SDL2 display backend gives the pad as polled state (`GetGamepadState`),
  and `GamepadInput` turns it into UE1 joystick keys and axes that `User.ini` binds; menus get
  their own controls. Only the SDL2 backend has it, and SurrealWidgets builds SDL3's instead
  when both are on, so the ports build the engine without SDL3.
- **The launchers' lines**: `--cmdline=` (`OriginalCommandLine`) and `DXL_LAUNCHER_FD`
  (`LauncherLine`) ([running it](#running-it)), with the console's `exec` and an `open` that
  takes a map's file name; `RELAUNCH` passed to the launcher, or a dedicated server started
  again by the engine itself (`Engine::Relaunch`, `GameApp::Relaunch`;
  [the menus' settings](NATIVES.md#the-menus-settings)).
- **Fullscreen under SDL2** is a borderless window covering the display, set up while the window
  is hidden: `SDL_SetWindowFullscreen` deadlocks on a GL window with the Smart Pro's vendor
  SDL2.

### Settings the launcher exposes

`LauncherSettings` reads the `Gamepad` and `Performance` blocks
([the keys](#settings-and-environment)). Distant AI is `UActor::ThinkThisFrame`, asked each
tick; a pawn counts as seen when the renderer drew it this frame (`LastVisibleFrame`). With it
on, Liberty Island's pawns move and change state as without it: `MoveConsole`'s 52 within
`move.py`'s tolerance, `AIConsole`'s states at 8 and 20 s the original's. Far NPCs fighting with
it on is untried. Render scale is `RenderDevice::GetRenderScale`: a device whose
`SupportsRenderScale()` says so (Vulkan, GL) makes its scene buffers at `GetRenderWidth()` by
`GetRenderHeight()` and scales them to the window as it presents; the viewport, the canvas and
the UI see the render size.

### Rendering

For every game, the renderer's CPU work is lighter:

- **Light maps** are lit only where a light reaches; a map whose animating or moving lights
  changed is built, converted and uploaded again only where they reach, now or at its last
  build, each light's shadows unpacked only there (`LightmapBuilder::FindAddedLights`,
  `TextureInfo::UpdateWidth`).
- **The light tree** and each surface's lights that can move are kept while no light changes,
  the level's surfaces' apart from each mover's, with the map it last drew.
- **The clipper's occlusion grid** is one row per image row; its non-SSE (ARM) build skips the
  clip for triangles inside the view (an upstream bug); a surface is clipped once and filled
  into the grid as one polygon, a span to a row (`BspClipper::DrawPolygon`); a span that closes
  a gap in a row goes on into the gap after it (`BspClipper::DrawSpan`; upstream's passes that
  gap over, leaving it open, so what lies behind counts as showing and is drawn).
- **One-sided surfaces** seen from behind are skipped before the visibility test, and a
  surface's points are gathered only when a test needs them.
- **Meshes**: each vertex animated, lit and fogged once a draw; a run of faces with one texture
  drawn in one device call, set up once (the GL device's `DrawGouraudTriangles` too).
- **A portal's spans** are put in order by row in linear time, not sorted (`SortPortalSpans`).

Deus Ex's look, as `Render.dll` and `D3DDrv.dll` make it, is gated and described by feature in
[`NATIVES.md`](NATIVES.md); its code is in `SurrealEngine/Render/` (`VisibleFrame.cpp`,
`VisibleMesh.cpp`), `SurrealEngine/Light/` and
`SurrealEngine/Packages/Engine/Resources/Textures/` (`FireEngine.cpp`, the fractal textures).

**The Vulkan device**: the game tick runs while the GPU draws the previous frame
(`CommandBufferManager` keeps one frame in flight, and `WaitForFrame()` collects it before
anything it uses is touched; a swapchain rebuild waits for the device). Without
`VK_EXT_descriptor_indexing` it binds a descriptor set per batch instead of bindless textures.
It decodes on the CPU what the GPU cannot sample or filter: BC1–5, RGB8, RGBA32F, and converts
the lightmaps for upload in NEON on ARM. Its translucent, modulated and highlighted draws take
upstream's depth bias toward the viewer, a step and a slope of 1; the GL device's take none, nor
do `D3DDrv`'s but its detail pass
([detail textures](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#detail-textures)).

**The GL device** runs desktop GL 4.2+ and OpenGL ES 3.2 from one code path: `RenderAPI::GLES`
and `RenderDeviceType::GLES`, an ES 3.2 context from every window backend (SDL2 and SDL3 through
`SDL_GL_CONTEXT_PROFILE_ES`, X11 and Wayland through EGL with the ES3 bit), and `IsGLES` read at
init from `glGetString(GL_VERSION)`. It draws at the render scale, as the Vulkan device does
([the setting](#settings-the-launcher-exposes)). The same shader sources compile as GLSL ES 3.20
(`CompileGlsl` prepends the version and highp precision; the sources keep to `u`-suffixed masks,
float literals and `std140` push-constant blocks). No desktop-only calls: `glClearDepthf` and
`glDepthRangef`, `glDrawBuffers` for every `glDrawBuffer`, no `GL_DEPTH_CLAMP` or
`GL_MULTISAMPLE` on ES, the plain blend calls, the null texture as `UNSIGNED_BYTE`. It asks for
GL errors once a frame, not at each flush of its batches and each texture upload: every
`glGetError` makes Mesa's GL thread finish all it has queued. A draw sets only the state that
differs from the last draw's (`DrawState`: program, samplers, textures, blend, depth), forgotten
wherever else that state is set.

- **ES 3.2, not 3.0**: RGBA16F (the scene buffers with `Hdr`) is colour-renderable only from ES
  3.2 core, and the shaders bind their samplers with `layout(binding)`, which GLSL ES 3.00
  lacks. `gl_FragCoord.w` is 1/w on ES as on desktop, so the detail-texture distance fade is
  unchanged.
- **Antialiasing on ES**: the multisampled scene buffers are made with ES 3.1's
  `glTexStorage2DMultisample`, as ES lacks desktop GL's `glTexImage2DMultisample`; on either,
  the samples are halved until every attachment takes them (an ES driver may take one sample of
  the integer hit buffer). Tried on the desktop's ES 3.2 context; the Smart Pro's launcher keeps
  antialiasing off.
- **The desktop context**: every Linux window backend asks for a desktop 4.2 core context, as
  the shaders are GLSL 4.20 (`CompileGlsl` prepends `#version 420`): a driver without 4.2 fails
  at the context, with an error that says so, not at a shader.
- **The vertices** stream through CPU staging arrays, each flush uploading the range written
  since the last one (`glBufferSubData`): the GE8300 has no `glBufferStorage` (persistent
  mapping), and mapping the buffers' unused tails on every flush costs 2/3 of its frame.
- **The scene buffers** are RGBA8 unless `Hdr` is on. They hold the frame top row first, so a
  scene drawn in a window (a security camera's view on its computer) lands where the Vulkan
  device puts it: its viewport is placed from the top, where upstream's places it from the
  bottom.
- **BC1** without `EXT_texture_compression_s3tc` and RGBA32F (the lightmaps) without
  `OES_texture_float_linear` are decoded to RGBA8 on the CPU.
- **The samplers** set no `GL_TEXTURE_LOD_BIAS` on ES (no ES sampler has it), anisotropy only
  with `EXT_texture_filter_anisotropic`, and `GL_MIRROR_CLAMP_TO_EDGE` only where a probe at
  init finds the driver takes it (else `CLAMP_TO_EDGE`).
- **`ReadPixels`** reads the buffer as it is, the right way up (upstream's is an `#if 0` stub of
  D3D11 code, its shots black); `Exit()` runs once and tolerates a failed unmap, as the intro
  can end the game after the window's context is gone.
- **The post-process passes bind their own samplers**, as the Vulkan device's do: the present,
  the bloom and the gamma-corrected read-back sample the image linearly, held at its edges, and
  the dither texel for texel (`PresentPass.PPLinearClamp`, `PPNearestRepeat`). A sampler object
  on a unit overrides the texture's own state, and the scene's draws leave theirs bound
  (`DrawEntry`): without its own, the render-scale upscale would take the last draw's,
  point-sampled after the HUD's text.
- **Anisotropy** (8) goes on every scene sampler, as the Vulkan device's, where the driver has
  it (`CreateSceneSamplers`).
- **Depth is clamped** at the near and far planes on desktop GL, as the Vulkan device clamps it
  (the weapon close up): `DepthClipEnable` is D3D11's, off meaning a clamp. ES has no clamp and
  clips there.

### Script VM

The fork reworks upstream's `ExpressionEvaluator` and `Frame::Run` for speed: no `dynamic_cast`
or heap allocation on the call path (parameters from `Properties`, functions and events found
through `UClass::VirtualFunctionCache`); each expression node classified once
(`ExpressionEvaluator::Classify`, `Expression::TypedKind`), so conditions, `&&` and `||`, the
commonest operators (`UFunction::FastOperator`) and assignments to plain variables evaluate as
plain values, not through an `ExpressionValue`; conditions, jumps, plain assignments, calls,
`return;` and a foreach's next pass run by `Frame::Run` in place, without an
`ExpressionEvalResult` each (`Frame::ClassifyStatement`). A call leaving out an optional struct
or array argument does not copy it, where upstream crashes.

Deus Ex's script runs under the original's guards
([stops the game](NATIVES.md#stops-the-game), [a singular function](NATIVES.md#small)), on
while `Frame::Guards` is: the jumps counted by `Frame::CountJump` from each level tick and map
load (`Frame::InitRunaway`), the nesting and each object's singular mark kept by
`Frame::GuardedRun` in `Frame::CallScript`. Other games keep upstream's warning, logged for each
statement past 500,000 in one function's run.

### Game tick

- **Deus Ex's level tick is the original's**: the actor list in the original's order, static
  actors first and never ticked, an actor waiting for its owner's tick (`ULevel::SortActors`,
  `ULevel::TickDeusEx`, `UActor::StartTick`;
  [housekeeping](NATIVES.md#housekeeping-not-seen-directly)).
- **The actor iterators** find a class's actors from an index, not a scan of the level.
- **Collision, for every game**: a ray trace hands each BSP child only its part of the segment
  and tests a polygon's plane before its vertex count and surface; sight lines test the actors
  of only the cells they cross; each cell's actors sit in an open-addressed table (`CellTable`),
  where upstream keeps a `std::unordered_map` of `std::list`s; moves and traces go without casts
  and heap allocations; another game's walking pawn steps to the ground with its dry run's
  trace.
- **Deus Ex's traces, moves and physics are `Engine.dll`'s**, in
  `SurrealEngine/Collision/TopLevel/TraceTest.cpp`, `UActor_Phys*.cpp` and `UPawn_Tick.cpp`: its
  backoffs; the movers a line meets where the level's BSP last filed them, filed by
  `CollisionSystem::FileMovers` at a map's load and a save and `RefileMovers` each frame drawn,
  in a `CellTable` of their own; `FindSpot` and encroachment, moves held off what they meet as
  `ULevel::MoveActor` holds them, landing as `processLanded`; walking, falling, swimming,
  flying, a player's ladders, the speed pawns move at and how they turn, as `physWalking`,
  `physFalling`, `physSwimming`, `physFlying`, `calcVelocity` and `physicsRotation`, a fall
  ending its frame where it lands
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)).
- **A whole frame at once**: a walking, swimming or flying pawn, and anything falling, takes the
  rest of the frame at once, as the original's does, the walk and the fall in their own steps
  (`UPawn::DeusExPhysWalking`, `UActor::DeusExTickFalling`), a fall a walk or a swim hands over
  going on with its count of steps; so its speed, its slides, where it comes to rest and the
  move it hands on are the original's at any frame rate. The other physics take a frame in steps
  of at most 0.02 s (`UActor::TickPhysics`).

### Objects and memory

`SurrealEngine/GC`: a mark and sweep from roots, as UE1's collector ([the
original's](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#garbage-collection)).
A collection runs at the end of the frame, with no script running (`Engine::CollectGarbage`, in
`EngineGC.cpp`), when asked for: by every map load but Entry's, by the console's `obj garbage`,
by 2048 destroyed actors let go since the last one ([destroyed
actors](NATIVES.md#housekeeping-not-seen-directly)), and every `SURREAL_GC_STRESS` frames. A map
load's and the console's do everything; the backlog's and the stress runs' mark and sweep only,
no level having departed since the last map load's. It logs the original's lines (`Collecting
garbage`, `Purging garbage`, `Garbage: objects: ...; refs: ...`, the refs those followed, not
the ones to code) with the milliseconds it took, the resident memory before and after, what it
freed by kind and the references it made None by holder, and a map load's gives the freed memory
back to the system (`malloc_trim`).

- **Roots** (`Engine::MarkRoots`, in `EngineGC.cpp`): the engine's subsystems, levels and
  objects; the net drivers' players, channels and package maps; the natives; every export of a
  save info. Any export of a package but code stays only while something reaches it, and is
  loaded from its file again when next asked for. A map's or a save's package is not a root:
  what its level reaches stays.
- **Code** (a `UField` in a package) is always kept. A collection looks over the objects made
  since the last one and moves the code among them to a permanent list
  (`GCAllocation::permanent`), which the marker never visits as a target. A reference code
  holds to code -- its class, its chain of fields, the struct, class or enum a property names --
  is not handed over from code at all (`GCMarker::MarkCode`), and a struct's properties, a
  state's functions and a bytecode's constants hand over only the entries that are not
  permanent (a field made at run time in no package, `Object`'s native properties). What code
  refers to beyond itself is walked each collection for a class, whose defaults scripts write
  (`IsGCMutableCode`); the rest of the code refers to what it referred to when it was loaded,
  so it is walked only when code was loaded or found since the last such walk
  (`GC::CodeChanged`, from `UField::Load`), and the collectable objects and names it reached
  then are kept from a list until the next.
- **What an object holds**: its class, package, delay load and state frame, and the references
  its properties hold (`UStruct::RefProps`); a native class marks its own members in its `Mark`.
  An object reached only through its package keeps the package's file and tables, not its other
  objects.
- **Where objects are made**: each in the package of the level it belongs to, as the original's:
  a spawned actor in its level's (the Entry level's own pawn in Entry's), the flag base and what
  travels with the player in the player's, the event manager in its `LevelInfo`'s, a
  conversation's camera and flag refs in the conversation's. A package played as a level is
  tagged so (`Package::IsLevel`).
- **A sound** is decoded once, when first played, for the audio device, which keeps a copy of
  its own (the mixer's 16-bit one, OpenAL's buffer); the sound keeps only its length, channels,
  rate and loop. Its lip-sync shapes decode it again once, into a buffer let go after.
- **Saving a level** writes what the level reaches (`PackageWriter`), never the rest of its
  file: an export the collector freed is not loaded back to be saved. An object of a transient
  class is not written either ([as the original's](NATIVES.md#saving-loading-and-travel)).
- **Elimination**: a reference to an object flagged `EliminateObject` is made None where the
  marker finds it (`GCMarker::Mark`), as the original's. One held where it may not be written
  (`MarkConst`: the subsystems, the net layer, what code names) keeps it, and the log says so. A
  collection flags every object of a level left behind (a package tagged as a level that is
  neither the current one nor Entry's) and, at a map load, the new level's actors that are not
  in its `Actors`, as the original's. `Object.CriticalDelete` flags the object it is given.
- **The weak holders** let go of the dying before anything is freed (`GC::IsDying`): the camera
  actor, the coronas and iterator actors, the lights the light system lists, the audio device's
  sound numbers and reverb zone, the open package files.
- **The sweep** tells each dying object first, all of them still allocated (`OnGCDestroy`): the
  properties with something to destruct are destructed (`UStruct::DestructProps`) and its
  package's export slot emptied, so a later reference loads it from its file again; a sound
  leaves the audio device, a decal its BSP nodes, a package closes its file, a freed texture has
  the render device flushed (it caches textures by address) if anything was drawn since its last
  flush. Then each is freed.
- **The names** go after the objects: a name the engine makes for an object
  (`NameString::Collectable`, through `Package::MakeUniqueObjectName`) is deleted when no live
  object holds it, its slot taken by the next name made. Each object marks its names as the
  collection reaches it (`GCObject::GCMarkNames`: its own name, its properties', its state
  frame's locals), the code as it is walked (above); an object made while the collection ran
  has them marked over every object after. A name looked up by its spelling is kept from
  then on, so no other holder can see its index taken. A cache keyed by name indexes keeps no
  collectable name (`FindScriptFunction`).

### Gameplay

What else Deus Ex needs is gated as [above](#how-it-is-kept) and described by feature in
[`NATIVES.md`](NATIVES.md). Where most of its code is:

- **The path search and reachability tests**: Deus Ex's `FindPathToward`, `FindPathTo` and the
  `pointReachable`, `actorReachable` and `Reachable` they ask are `Engine.dll`'s, walking its
  own moves (`UPawn_Path.cpp`, `UPawn_ReachDeusEx.cpp`); `RouteCache` stays empty, as the
  original never fills it; other games keep their own flow over the corrected search
  ([moving](NATIVES.md#moving-wandering-and-tactical-movement)).
- **The audio mixer**: `GalaxyMixer` (`SurrealEngine/Audio/GalaxyMixer.cpp`) mixes Deus Ex's
  sounds as `Galaxy.dll` does (pan, fall-off, sliders, resampling, its reverb) and streams them
  through one OpenAL source at `OutputRate`; other games keep OpenAL's 3D sources. The fork's
  audio device adds `SpeechVolume` and `DopplerSpeed` to its ini section, and keeps Deus Ex's
  channel count under Galaxy's name, `EffectsChannels`, other games' as `Channels`
  ([sound](NATIVES.md#sound)). Its render device keeps `DetailTextures` beside upstream's keys;
  on Deus Ex's first run it takes them from the section of the device the game's ini names
  (`GameRenderDevice`) ([the menus' settings](NATIVES.md#the-menus-settings)). Who hears a sound
  an actor plays, a remote player through its client, is `UActor::HearSound` and
  `CheckHearSound` (`UActor.cpp`), from Deus Ex's `PlaySound`, `PlayOwnedSound` and
  `DemoPlaySound` natives (`NActor.cpp`).
- **The network driver**: `SurrealEngine/Network/` is the fork's own, the original's protocol
  over UDP (`NetDriver`, channels, the package map, replication by the original's lists) with a
  client that joins the original's servers, a listen and a dedicated server, downloads and
  server travel; `IpDrv`'s `InternetLink`, `TcpLink` and `UdpLink` are the original's
  ([multiplayer](NATIVES.md#multiplayer)).
