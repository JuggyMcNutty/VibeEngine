# The engine

VibeEngine is [Surreal Engine](https://github.com/dpjudas/SurrealEngine), an
open-source reimplementation of Unreal Engine 1 that recognises this build of
Deus Ex directly (`DeusEx.exe` SHA1 `2a933e26aa9cfb33b37f78afe21434caa031f14a`
is its `DEUS_EX_1112fm` database entry), forked for Deus Ex: it carries what
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s ports
need -- their launcher, embedded GPUs, pads, the speed a handheld needs -- and
what Deus Ex needs from it to play as it should. Its branch is `deusex`. Where
the work stands is [`ROADMAP.md`](ROADMAP.md); what the engine still lacks of
the original, [`NATIVES.md`](NATIVES.md); how to work on it,
[`DEVELOPMENT.md`](DEVELOPMENT.md).

## How it is kept

**Pinned in the workspace.** Port Ex Machina's `ENGINE-PIN.txt` names this
repository, its branch and the one commit its ports build. Its
`scripts/engine.sh fetch` clones the fork beside the workspace, as
`VibeEngine/` in the parent folder of its repositories, and checks that commit
out, on any machine; `check` proves the clone is at it. After a fork commit is
pushed, `pin` moves the file, and the move is committed there. Builds go to
`build/<port>/engine` in that parent folder, never into the clone.

**Not upstream's.** The fork sends nothing upstream (owner, 2026-09-26), and
does not follow upstream: its new commits reach the fork only when someone
chooses to [upgrade](#upgrading-surreal-engine) (owner, 2026-09-23). Using and
building the engine is permitted by its own licence, which grants use "for any
purpose".

## Commands

In the workspace, Port Ex Machina's:

```sh
scripts/engine.sh fetch                     # clone the fork at the pin
scripts/engine.sh check                     # the clone is at the pin, on its branch
scripts/engine.sh status                    # the pin, the fork, how far upstream has moved
scripts/engine.sh pin                       # after a pushed fork commit: move ENGINE-PIN.txt to it
scripts/engine.sh build <port>              # build/<port>/engine, from the port's engine.cmake
```

`scripts/dx.sh build <port>` calls `build` for ports that ship the engine, and
`scripts/dx.sh check` runs `check`. In this clone, the fork's own:

```sh
vibe/tools/perf/perf.sh on|off|save         # the profiling hooks (below)
vibe/tools/upgrade.sh [<ref>]               # merge upstream in (below); status, --continue, --abort
vibe/tools/host-tools.sh                    # perf and the validation layer, into the repositories' deps/
vibe/tools/natives_audit.py                 # the original's natives against the fork's (NATIVES.md)
vibe/tools/dxcap.sh                         # scripted runs of both engines (DEVELOPMENT.md)
```

## Changing the engine

Commit the change here, with the docs it affects -- this one,
[`NATIVES.md`](NATIVES.md), [`ROADMAP.md`](ROADMAP.md) -- push it, then pin it
in the workspace (`scripts/engine.sh pin`) and commit the moved
`ENGINE-PIN.txt` there. Each fork commit's message says what it changes, why,
and how it was checked; [what the fork changes](#what-the-fork-changes) below
adds what it did on the Smart Pro. The fork's history is published and never
rewritten. Its first 34 commits began as Port Ex Machina's patch stack,
engine-patches (retired 2026-09-24); "patch NNNN" here and in the game's docs
is such a commit's place in that series.

Temporary debugging hooks never go into a commit: they carry a
`TEMPORARY DEBUG TOOL` comment and are reverted before committing
([temporary debug hooks](DEVELOPMENT.md#temporary-debug-hooks)).

### The profiling hooks

The frame-time profiling hooks live in
`vibe/tools/perf/perf-instrumentation.patch` so they can be re-applied:
`vibe/tools/perf/perf.sh on`, and `off` afterwards. Take them off before
changing the engine -- a commit made with them on carries them -- and commit
before putting them back: `on` cannot merge over uncommitted changes to a
file the hooks touch, and says so. The patch is against the fork's head, so a
fork commit that touches the same lines moves them: `on` then falls back to a
three-way merge (and stops if that leaves conflicts), and `save` rewrites the
patch from the tree -- all of it but `vibe/` -- so the next `on` and `off`
apply cleanly.

The hooks build the engine with frame pointers (~1% slower on the handheld) and
carry a sampling profiler for devices without `perf`:
`SURREAL_PERF_SAMPLE=<file>` samples the main thread's CPU time, recording each
sample's program counter and the return addresses a frame-pointer walk finds,
one block per 60-frame report, with `/proc/self/maps` beside it.
[`vibe/tools/perf/sample-report.py`](../tools/perf/sample-report.py) (Python 3,
and the port toolchain's `nm` for a cross build) turns that into self and
inclusive time per function, optionally under one caller (`--root
ULevel::Tick`: the game tick) or with the callers of one (`--callers`). A leaf
function keeps no frame record, so its samples show its caller's caller as the
next frame. Profiling the handheld:
[its README](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance).

### Natives from the original

Where upstream has a Deus Ex native wrong or as a stub -- a stub logs
`Unimplemented: <class>.<name>` the first time it runs in a session, and only
then -- the original is in the game's DLLs, and
[dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info) is what has
been read of them: how to read them is
[working on the binaries](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/README.md#working-on-the-binaries),
and what the fork still lacks is [`NATIVES.md`](NATIVES.md)
(`vibe/tools/natives_audit.py` lists it). Patch 0034 was read this way.

## Upgrading Surreal Engine

Only when someone decides to. `vibe/tools/upgrade.sh status` (or the
workspace's `scripts/engine.sh status`) fetches upstream and says how many
commits it is past the fork. To take them in:

```sh
vibe/tools/perf/perf.sh off                 # if the profiling hooks are on
vibe/tools/upgrade.sh                       # upstream's latest; or upgrade.sh <sha|tag|branch>
```

`upgrade.sh` needs the clone on `deusex` with a clean tree. It merges the
chosen upstream commit into the fork's branch, keeping both histories. If
files conflict it stops: resolve them, `git add` them, then
`vibe/tools/upgrade.sh --continue` -- or `--abort`, which leaves the fork as it
was.

Then, before pinning: build and run linux-x86_64, check the Vulkan validation
layer ([below](#profiling-and-validating-on-the-desktop)), `perf.sh on` (and
`save` if the hooks moved), profile on the devices, bring
[what the fork changes](#what-the-fork-changes) up to date, push the branch,
and pin it in the workspace.

## Running it

Each port's `run-game.sh` starts it for every real launch. By hand, from the
game's directory:

```sh
SurrealEngine --no-launcher /path/to/deusex --url=01_NYC_UNATCOIsland.dx
```

- `--no-launcher` (or a game folder on the command line) skips upstream's
  desktop launcher window (patch 0001).
- **`--server`** runs a dedicated server of the `--url` map, as the
  original's `-SERVER`: no window, sound or player; `--lanplay` gives it the
  LAN tick rate ([multiplayer](NATIVES.md#multiplayer)).
- **Maps are `--url=<map>` only.** `-u <map>` sets an empty `-u` and the map name
  becomes a stray argument, so the intro loads with no warning.
- **The engine ignores SIGTERM**: stop it with SIGKILL (`timeout -s KILL`). That
  leaves `Running.ini` behind like any crash.
- `--ini=<file>` and `--userini=<file>` name the inis to read and write back,
  as the original's `INI=` and `USERINI=`; the `shot` console command writes
  the next `ShotNNNN.bmp` into the game's System folder.
  [Scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines) use both.
- **`--cmdline=<line>`** is the original's command line, which the
  recreated launcher (deusex-launcher's `main`) passes on: its start URL,
  `-server`, `INI=`, `USERINI=`, `EXEC=` and safe mode's flags, over the
  options above ([the command line](NATIVES.md#the-command-line)).
- **`DXL_LAUNCHER_FD`** in the environment is the recreated launcher's line
  to the engine, which stays for the game's run as the original's process
  does ([the game and the launcher](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md#the-game-and-the-launcher)):
  the engine says `hello` as it starts and `ready` once its first map is
  in, and takes `TakeFocus` (the window to the front) and `Open <url>`
  (the console's `open`), what a second launch forwarded. The ports'
  launchers set nothing, and nothing changes.
- Where there is no audio device (a container), give OpenAL Soft the null
  driver ([linux-x86_64's README](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/README.md#audio)).

### Profiling and validating on the desktop

The handheld has no `perf` and no Vulkan validation layer, so validation runs
against the base port's build (`scripts/dx.sh build linux-x86_64 engine`), and
so can a quick CPU profile -- but the desktop's proportions are not the
handheld's (its Cortex-A53 pays far more for a cache miss), so what to work on
next is decided by the handheld's own samples. `vibe/tools/host-tools.sh`
unpacks pinned copies of Linux `perf` and the Khronos validation layer into the
`deps/` beside the repositories without installing anything. From their
parent folder:

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

`--call-graph dwarf` on `perf record` gives callers (the desktop build has no
frame pointers unless the hooks are on).

## What the fork changes

By area; the number is the commit's place in the series (the retired patch
stack's numbering), its link the commit in the fork repository, whose message
says how it works. **Smart Pro** is what it did there, measured with the
hooks in Liberty Island's opening fight (the whole frame's numbers after each
patch are in [the Smart Pro's Performance](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#performance));
**checked** is how it was shown not to change the game, or for a gameplay fix
to work, where the message does not already say.

### Running on our devices

- [**0001**](https://github.com/JuggyMcNutty/VibeEngine/commit/22a5e87cc51aa83be550abe1c17e0b4203f18c79)
  `headless-and-embedded-support` -- the engine started by our launcher with no
  desktop: no launcher window, errors and the log on stderr, a non-zero exit
  after a caught exception (the launcher's crash sentinel reads it); and a
  cross build for an embedded aarch64 device: SDL2 only, no X11/Wayland/desktop
  GL, SDL from pkg-config, a host-built `zipdir`, fonts without GSettings or
  fontconfig (`SURREALWIDGETS_FONT`).
- [**0002**](https://github.com/JuggyMcNutty/VibeEngine/commit/a40bec64d33574529da21d63c1b57b3b3ebfe85e)
  `nonbindless-fallback-and-format-support` -- Vulkan on GPUs without desktop
  texture support: a per-batch descriptor set path when
  `VK_EXT_descriptor_indexing` is missing (`SURREAL_VK_NO_BINDLESS=1` forces
  it), and CPU decoders for texture formats the GPU cannot sample or filter
  (BC1–5, RGB8, RGBA32F). **Smart Pro:** the GE8300 has neither; the intro went
  from speckle to clean. A desktop GPU keeps the bindless path. The format
  table came from Port Ex Machina's
  [`tools/probes/probe-texture-formats.c`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/tools/probes/probe-texture-formats.c).
- [**0003**](https://github.com/JuggyMcNutty/VibeEngine/commit/af99616f537480cc63f9f781865e2e76634abe44)
  `gamepad-and-deusex-fixes` -- the pad as polled state, turned into UE1
  joystick keys and axes so `User.ini` bindings decide what it does, with
  menu-mode controls and a `Gamepad` block in `Settings.json` ([the launcher's
  controller support](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md#controller-support)); and two Deus Ex fixes:
  `CycleActors` resumes where it stopped, as the game's script expects, and the
  first pause-menu press after skipping the intro is no longer swallowed.

- [**the original's command line and the recreated launcher's line**](https://github.com/JuggyMcNutty/VibeEngine/commit/453b3af3f935278ed2b7247eb5daea667524daec) --
  `--cmdline=`, the original's command line read as its code reads it --
  the start URL, `-server`, `INI=`, `USERINI=`, `EXEC=` and safe mode's
  flags ([the command line](NATIVES.md#the-command-line)) --, and
  `DXL_LAUNCHER_FD`, the line to deusex-launcher's `main`, which stays for
  the game's run ([running it](#running-it)); the console's `exec`, and
  `open` with a map's file name. **Checked:** driven over a socketpair as
  the launcher drives it, with an ini asking for fullscreen -- a 640×480
  window, no sound, `EXEC=`'s lines run, a forwarded `Open` travelled, the
  ini unchanged after a clean exit; a proving run clean.

### Settings the launcher exposes

- [**0008**](https://github.com/JuggyMcNutty/VibeEngine/commit/ce78355fb3cc47b2ac27dd18b3751c2c564dd5ce)
  `ai-level-of-detail` -- with `Settings.json` `Performance.AiLevelOfDetail`
  (the Video tab's Distant AI), a pawn out of sight and not within 1500 units
  runs its script thinking every third frame. **Smart Pro:** game tick ~124 →
  ~104 ms; ~38 pawns a frame skip their thinking. **Checked:** the scene
  renders normally (framebuffer capture); whether distant NPCs still behave is
  not yet judged by hand.
- [**0009**](https://github.com/JuggyMcNutty/VibeEngine/commit/a1a2926f93fbd6be6288f4dd87191ca36ae9f0f9) `render-scale` --
  `Performance.RenderScale` (the Video tab's Resolution): the scene drawn
  smaller than the window and scaled up; Vulkan only. **Checked:** Liberty
  Island at 960×540 and 853×480 fills the panel, the HUD larger (framebuffer
  captures); synchronization validation clean on the desktop at scale 0.667.
- [**0022**](https://github.com/JuggyMcNutty/VibeEngine/commit/a41d14b1180e2e04957d1b19d7a5406b88800b6c) `ai-lod-far-tier` --
  with Distant AI, a pawn also beyond 4000 units thinks every sixth frame.
  **Smart Pro:** game tick ~65 → ~63 ms; ~48 pawns a frame fall in the tier, ~8
  of them thinking, ~9 fewer thinking each frame.

### Rendering

- [**0004**](https://github.com/JuggyMcNutty/VibeEngine/commit/e56866259cfd555d44669701e65643e2d0c69b2a)
  `vulkan-frame-overlap` -- the game tick runs while the GPU draws the previous
  frame; swapchain rebuilds wait for the device. **Smart Pro:** GPU wait ~76 →
  ~0.2 ms, the tick ~20 ms longer (CPU and GPU share the SoC's memory).
  **Checked:** Liberty Island mid-fight renders correctly (framebuffer
  capture).
- [**0005**](https://github.com/JuggyMcNutty/VibeEngine/commit/03afa604679b0e8e89ea5100bb58c1484d677f41)
  `lightmap-lit-spans` -- lightmaps lit only where a light reaches. **Smart
  Pro:** lightmaps ~98 → ~11 ms. **Checked:** the dock pixel-identical before
  and after (framebuffer captures).
- [**0010**](https://github.com/JuggyMcNutty/VibeEngine/commit/d635be4bda5c027b5e0b34ee3c13aa64ebc28217)
  `clipper-sized-to-image` -- the visibility clipper's occlusion grid has one
  row per image row (it was a fixed 2048×1080). **Smart Pro:** frame ~222 →
  ~213 ms native, ~208 → ~191 ms at 853×480. **Checked:** the dock
  pixel-identical at both; ~830 surfaces pass visibility where ~740 did, all
  hidden by the depth test.
- [**0011**](https://github.com/JuggyMcNutty/VibeEngine/commit/7599d2b600015df7f2eec1e683cd94b6f55c2e5b)
  `cull-one-sided-back-faces` -- one-sided surfaces seen from behind skipped
  before the visibility test. **Smart Pro:** surface tests ~4,800 → ~2,400 a
  frame and ~22 → ~12 ms; lightmaps ~10 → ~4 ms and their uploads ~11 → ~4 ms
  (most of the burning barrel's lightmaps were back faces). **Checked:**
  captures of Liberty Island and UNATCO HQ's interior differ only in the stats
  overlay's surface count.
- [**0018**](https://github.com/JuggyMcNutty/VibeEngine/commit/efc2a80026cbc0768503c0c365e15db0d9df2c4b)
  `mesh-vertices-once` -- each mesh vertex animated, lit and fogged once a
  draw, not once per face using it. **Smart Pro:** actor meshes ~28 → ~14 ms,
  render CPU ~92 → ~80 ms.
- [**0019**](https://github.com/JuggyMcNutty/VibeEngine/commit/abe6d2735c4e4a2a61479e7fc7964b36b61f8e82)
  `mesh-face-batches` -- a run of mesh faces with one texture drawn in one
  device call. **Smart Pro:** actor meshes ~14 → ~12 ms. **Checked:** a capture
  of the dock matches one from before patch 0012 except where time moves things
  (the sky, the NPCs, the stats); the statue and props identical to the pixel.
- [**0020**](https://github.com/JuggyMcNutty/VibeEngine/commit/96f1b6b4b1d7b59cdfca4c878a93a243116cf98c)
  `clipper-arm-clip-test` -- the clipper's non-SSE (ARM) build skips clipping
  for triangles inside the view, as the SSE build did (an upstream bug).
  **Smart Pro:** visibility ~25 → ~20 ms (with the per-part timers), surface
  tests ~11.8 → ~7.4 ms. **Checked:** a capture of the dock differs only in the
  sky's clouds and the NPCs.
- [**0021**](https://github.com/JuggyMcNutty/VibeEngine/commit/377cf462b1452f880723cce4305087172bfe7463)
  `surface-points-on-demand` -- a surface's points gathered only when a test
  needs them. **Smart Pro:** visibility ~20.5 → ~19.6 ms.
- [**0023**](https://github.com/JuggyMcNutty/VibeEngine/commit/56e86e57548c00aa5ccb597a52aed093ceaa9172) `light-tree-kept` --
  the light tree, and each surface's lights from it, kept while no light
  changes. **Smart Pro:** the BSP surfaces' section ~11 → ~8 ms. **Checked:** a
  capture at 853×480 shows the dock's lightmaps as before.
- [**0024**](https://github.com/JuggyMcNutty/VibeEngine/commit/03e4d0cb9696bbad5b26cdc0489dcc028152c29b)
  `lightmap-neon-conversion` -- the lightmaps' float-to-byte conversion for the
  GPU in NEON on ARM. **Smart Pro:** texture uploads ~3.9 → ~2.1 ms.

### Script VM

With 0013, these took the Smart Pro's script time from ~125 ms a frame to
~30 by 0017; with the collision patches (0025–0027) it was ~25 before 0028.

- [**0006**](https://github.com/JuggyMcNutty/VibeEngine/commit/af2ed868bfe107485ad905a2c183405f01139391)
  `vm-call-path-without-casts` -- parameters from `Properties`, and a per-class
  virtual-function cache: no `dynamic_cast` on the call path. **Smart Pro:**
  script ~125 → ~84 ms.
- [**0007**](https://github.com/JuggyMcNutty/VibeEngine/commit/9cc49e284b2e5f3d9f9a12fbd0449117afbc9d79)
  `vm-call-overheads` -- native frames without locals, event names looked up
  once, plain-data locals zero-filled. **Smart Pro:** script ~84 → ~77 ms.
- [**0012**](https://github.com/JuggyMcNutty/VibeEngine/commit/f4ea318b0b71718e83c19b0e0efd208379bfc91c)
  `vm-evaluator-per-statement` -- one expression evaluator per statement,
  nested values returned directly. **Smart Pro:** script ~60 → ~55 ms.
- [**0014**](https://github.com/JuggyMcNutty/VibeEngine/commit/829adcbd7e1d6e109ae2cd81f67f4e46a84f5c95)
  `vm-calls-without-allocation` -- script calls without heap allocations or
  walks over every local. **Smart Pro:** script ~39 → ~35 ms.
- [**0015**](https://github.com/JuggyMcNutty/VibeEngine/commit/6ba1983af99b9fd70a1e6133a70332578a13431c)
  `vm-fast-operators` -- the 25 commonest operators evaluated in place. **Smart
  Pro:** script ~35 → ~31 ms.
- [**0016**](https://github.com/JuggyMcNutty/VibeEngine/commit/e28aa410d11e3a07848d602d814171d0b99c470f)
  `vm-event-lookup-cache` -- events found through the virtual-call cache.
  **Smart Pro:** within the noise (tick ~71.2 → ~70.7 ms).
- [**0017**](https://github.com/JuggyMcNutty/VibeEngine/commit/51d45aa37c96a9bc5656d4ce5d89992e94fa8d13)
  `vm-leaf-expressions` -- the commonest leaf expressions made without the
  visitor. **Smart Pro:** tick ~70.7 → ~69.3 ms.
- [**0028**](https://github.com/JuggyMcNutty/VibeEngine/commit/40e219ac8bc05a349950766408daea74c77c57bb)
  `vm-typed-evaluation` -- conditions, `&&` and `||`, the fast operators and
  assignments to plain variables evaluated as plain values, each node
  classified once, instead of through an 88-byte `ExpressionValue`; a typed
  node carries the offset or operands it reads. **Smart Pro:** script ~25.4
  → ~22.7 ms, tick ~54 → ~51 ms; at 853×480 the frame ~108.5 → ~106 ms.
  **Checked:** also a hash of every actor's state, frame by frame, with the
  frame time and random seeds fixed (the message has both checks).
- [**0029**](https://github.com/JuggyMcNutty/VibeEngine/commit/7e93fe7b86f0e449454db03d9d8eb02b55d6dcfd)
  `vm-statements-in-place` -- conditions, jumps, assignments to plain
  variables, calls, `return;` and a foreach's next pass run by `Frame::Run`
  in place, without an `ExpressionEvalResult` each; a jump keeps its target's
  statement index. **Smart Pro:** script ~22.7 → ~21.4 ms, tick ~51 → ~50
  ms; at 853×480 the frame ~106 → ~105 ms. **Checked:** the actor-state hash,
  on Liberty Island and UNATCO HQ.

### Game tick

- [**0013**](https://github.com/JuggyMcNutty/VibeEngine/commit/9720823c814691ca1455cbef65d13c629fac2a60)
  `actor-iterators-by-class` -- the actor iterators find a class's actors from
  an index, not a scan of the level (`CycleActors` alone had been ~16 ms of the
  tick). **Smart Pro:** script ~56 → ~39 ms.
- [**0025**](https://github.com/JuggyMcNutty/VibeEngine/commit/f984a7800675f85cc5e7eb134de36b03ffa8aef8)
  `ray-trace-segment-split` -- a ray trace hands each BSP child only its own
  part of the segment. **Smart Pro:** tick ~61.5 → ~58 ms.
- [**0026**](https://github.com/JuggyMcNutty/VibeEngine/commit/0f9ce6cccd7dbc52bf0c71a57ae9490573e08d18) `sight-line-cells`
  -- sight lines test the actors of only the collision cells they cross; hull
  planes on the stack. **Smart Pro:** tick ~58 → ~56 ms.
- [**0027**](https://github.com/JuggyMcNutty/VibeEngine/commit/469d9c8a26d8e910b14576bfca4fb650862681d3)
  `step-down-one-trace` -- a walking pawn's step to the ground made with the
  trace its dry run made. **Smart Pro:** tick ~56 → ~53 ms.
- [**0030**](https://github.com/JuggyMcNutty/VibeEngine/commit/2d315e3602663f73f802a25a58c282ae545dafec)
  `ray-plane-tests-first` -- a ray tests a polygon's plane before reading its
  vertex count and surface, which lie on other cache lines. **Smart Pro:** the
  sight rays' polygon tests (`NodeRayIntersect`) ~3.3 → ~1.9 ms.
- [**0031**](https://github.com/JuggyMcNutty/VibeEngine/commit/0098ca8c5f69c1d2c8ff397a75d1915afc20872b)
  `collision-cell-table` -- each collision cell's actors found in an
  open-addressed table and kept in an array, not a `std::unordered_map` of
  `std::list`s. **Smart Pro:** the traces' walk of the cells
  (`TraceTester::Trace`) ~1.4 → ~0.5 ms, and ~0.3 in the new `FindCell`.
- [**0032**](https://github.com/JuggyMcNutty/VibeEngine/commit/e9a806d56f88f63efded8ec14f8487afbc863b0a)
  `collision-move-overheads` -- a move asks once per actor whether it is a
  player or a projectile, and traces sort and sift their hits without heap
  allocations. **Smart Pro:** `dynamic_cast` in the tick ~2.9 → ~1.5 ms;
  `TraceTexture`, which collects every hit along a laser beam, ~1.9 → ~0.8
  (0030's share included).

0030–0032 were measured on the Smart Pro together: tick ~50 → ~39 ms, the
collision traces ~12 → ~8 ms a frame, and the render CPU ~2.5 ms less. Each
was checked with the actor-state hash on Liberty Island and UNATCO HQ (their
messages say how).

### Gameplay

What Surreal Engine lacked for Deus Ex to play as it should.

- [**0033**](https://github.com/JuggyMcNutty/VibeEngine/commit/bec6e261edcd00d9225cb95ef7e4a8e0b7298261)
  `vm-omitted-optional-arguments` -- a script call that leaves out an optional
  struct or array argument no longer crashes copying it (upstream's bug): the
  first NPC to attack hit it, in `ScriptedPawn.ComputeBestFiringPosition`.
- [**0034**](https://github.com/JuggyMcNutty/VibeEngine/commit/b5d08853dbf4e24894d56942c07a5a743438e824) `deusex-ai-sight`
  -- NPCs see: `IsValidEnemy`, `AICanSee` and `AIVisibility` as the original
  DLLs have them; upstream had the first wrong and the others as stubs, so no
  NPC ever noticed the player, or anyone. **Smart Pro:** the sight checks take
  ~0.3 ms of the tick. **Checked:** on Liberty Island an NSF terrorist, with
  the player moved in front of it, made the player its enemy ~3.5 s later
  (the build-up the script gives a faint sighting at night), and it and two
  more shot at the player; NPCs of hostile alliances check each other.
- [**what stopped the game**](https://github.com/JuggyMcNutty/VibeEngine/commit/db0df9a1205ef9f55c9abf83d61c7da26efdbac0) --
  the roadmap's M0 ([`ROADMAP.md`](ROADMAP.md)): `ReachablePathnodes` makes
  an (empty) iterator instead of stopping the VM in Battery Park's opening
  fight; the save's `DeusExSaveInfo` lives in package DeusEx, so a save no
  longer dies writing it; `GetConfig` answers from the system ini;
  `GetPawnAllianceType(None)` is Neutral; integer division by zero gives 0;
  string `>` is native 116, not the typo 1186. **Checked:** an 80 s Battery
  Park run and quick saves in two maps run out their clocks; the commit's
  message has the rest.
- [**saves, the original's way**](https://github.com/JuggyMcNutty/VibeEngine/commit/4dfc7a6dd5b1571d7fbab03fac6fd4e5a899d78b) --
  the first slice of the roadmap's M1: slots numbered highest-plus-one, the
  quick save in QuickSave, Current copied into the slot with the level saved
  on top, the SaveInfo filled and named as the original's
  (`MyDeusExSaveInfo`), the save listing and kept infos, and DELETEGAME.
  **Checked:** against the original's reference saves; the commit's message
  has the runs.
- [**loading**](https://github.com/JuggyMcNutty/VibeEngine/commit/0058015cccdadd262b56eb819bbd00f7213426a3) --
  `?loadgame=N` as the original's `Browse`: the slot's SaveInfo names the
  map, Current takes the slot's copy, the map loads from Current, the saved
  pawn is possessed; -1 the quick save. Behind it, the save info in a
  package of its own, and packages born empty carrying version 68 -- with
  either wrong, a written SaveInfo.dxs cannot be read back. **Checked:** a
  slot and the quick save round-trip in play; the original's saves stop at
  their saved event manager (ported with the AI event system, later).
- [**travel keeps the mission**](https://github.com/JuggyMcNutty/VibeEngine/commit/1a654c7e481bcf5ab223d702df14d85c4f76a11f) --
  within a mission the departing level is pruned and saved into Current,
  and a map saved there is revisited as the player left it, its pawn found
  again by the game's own login; a new mission, a new game or ?restart
  empties Current. **Checked:** a travel out and back revisits from
  Current, and the slot save after it has the reference hub save's shape.
- [**history in the level**](https://github.com/JuggyMcNutty/VibeEngine/commit/33c1e3f896010ca4138d912becc71be732063f82) --
  the player's history, log and notes are made in the level, as the
  original's, so a save keeps them.
- [**flags as the original's**](https://github.com/JuggyMcNutty/VibeEngine/commit/cd973b15a6dbf9042173c2a1039dd55b6dbc3d98) --
  chains past 64 by a CRC of the name, stamped expirations, expiry to a
  criteria, -1 for a missing flag, and typed flags found again. Its cleanup
  of a test hook cut real main-loop code, restored by
  [the commit after](https://github.com/JuggyMcNutty/VibeEngine/commit/4f6ed66a469ac232245e3fa1493d44c24b4dc0c0).
  **Checked:** an in-engine self-test for the flags; a 60 s run for the loop.
- [**list fields read back**](https://github.com/JuggyMcNutty/VibeEngine/commit/fb7b29d64cbca6e48a81032b99197200e430b7e9) --
  GetField's column test was inverted, so every screen keeping what a row
  stands for in a hidden column read nothing.
- [**flags kept and carried**](https://github.com/JuggyMcNutty/VibeEngine/commit/c8bf4374ac27a97ea80d04487ad9406ee930d52f) --
  the flag base and its flags live in the level package, so a save keeps
  them, and they cross a travel in the pawn's travel graph: the travel
  serialization walks every element of a fixed-array object property (the
  base's 64 buckets), travelled non-actors land in the new level's
  package, the travel info is captured before the pre-travel prune, and
  the prune deletes the departing level's flags as the original's does.
  GameDirectory objects are made per call again. **Checked:** 21 flags
  set across the buckets survive a travel and a save's load-back.
- [**conversations as the original's**](https://github.com/JuggyMcNutty/VibeEngine/commit/39e9df99bee7938823ec3ee9ddf5a21468d1d0e6) --
  the roadmap's M2 conversations item: comment events kept, an actor's
  conversations bound by the original's bark rule from the list the level's
  `ConversationPackage` names, the bound-actor slots filled so a destroyed
  actor ends its conversation, cycle-once chatter holding its last line,
  and each line's sound loaded alone, by name
  ([conversations](NATIVES.md#conversations)). **Checked:** the intro's
  scene plays its lines at their own lengths; a temporary hook printed
  Liberty Island's bound lists, the named troopers owning their own
  conversations plus the `_Bark`s; 90 s and 75 s runs clean.
- [**the text parser as the original's**](https://github.com/JuggyMcNutty/VibeEngine/commit/752ff87d222426b2aa305eac563b74344bc88ca5) --
  the roadmap's M2 parser item: the original's tokens (CR and LF as spaces,
  nothing trimmed, the first `<P>` swallowed), its 30-name tag table matched
  by start, its fields split at commas, its hiding to an end tag, the
  player's first name, and the colours read
  ([what the player reads](NATIVES.md#what-the-player-reads)). **Checked:**
  a temporary hook put five texts through it against their SDK sources --
  emails list with their fields, bulletins open, comments hide, header
  spaces and blank lines keep, `PLAYERFIRSTNAME` gives the first name; a
  70 s run after the hook's removal is clean.
- [**the list window as the original's**](https://github.com/JuggyMcNutty/VibeEngine/commit/48c0987a562d64820081012a2602ff3e61425d85) --
  the roadmap's M2 lists item: rows activate on a double click or Enter
  (`ListRowActivated`, which key rebinding hangs on), `MoveRow` takes the
  keys and a pad's d-pad through a list, the original's sorting and column
  keys, auto-expanding columns and the original's new-column defaults,
  hidden columns unseen, float fields keeping their number and shown
  through the column's format ([lists](NATIVES.md#lists)). **Checked:**
  a temporary in-engine self-test drove sorting (name, number, reverse),
  the number reader (hex, octal, hours and minutes), the format, the moves
  and a delete's focus; a 70 s run after its removal is clean.
- [**render time and stasis**](https://github.com/JuggyMcNutty/VibeEngine/commit/c7b3e00624c314757db5655caf8f35f9549d40c7) --
  the roadmap's first M3 item: the renderer stamps when each actor, zone
  and decal was last drawn, `LastRendered()` answers the time since, and
  the tick of an actor in stasis -- the original's full test, not "stasis
  allowed" -- does nothing and destroys a transient one
  ([out of sight](NATIVES.md#out-of-sight)). **[perf]** to re-measure on
  the Smart Pro with M3's AI work. **Checked:** a temporary snapshot hook
  counted Liberty Island each 8 s -- unseen trees and lamps entered stasis
  as they aged past 5 s while the drawn-recently count fell; a 70 s run
  after its removal is clean.
- [**the AI event system**](https://github.com/JuggyMcNutty/VibeEngine/commit/248538769f65b8e3393d102bdf6136ebe369ed22) --
  the roadmap's M3 hearing item: the original's `UEventManager`, one per
  level, saved with it -- senders' 16-frame rings and current levels,
  receivers in one ring walked under the original's turn and 2 ms rules,
  scores, senses (`AICanSee`, the original's `AICanHear`), and Begin, End,
  Pulse and ChangeBest to the listeners' script
  ([hearing](NATIVES.md#hearing-the-ai-event-system)). The manager's
  class is synthesized into the Engine package, so a save's import of it
  resolves; the original game's saved manager was recognized and skipped,
  which let the original's saves load (read whole since 2026-09-27: the
  saves the original loads, below). **Checked:** a
  temporary hook stood the player beside a terrorist and raised
  WeaponFire -- distress seen by sight, footsteps heard fading, HandleShot
  fired, and the terrorists' own gunfire became senders; a quick save
  carried 10 event types and 325 listeners through a load; the reference
  Liberty Island save of the original game loads and plays; a 75 s run
  after the hooks' removal is clean.
- [**ScriptedPawn's native tick**](https://github.com/JuggyMcNutty/VibeEngine/commit/4bd245b6e24503319ae5da17eee8c84ddc1e67cd) --
  the roadmap's M3 tick item: disappearing, the pivot's easing, agitation
  and fear (the script's own unused `UpdateAgitation` and `UpdateFear`),
  the sixteen AI timers, cloaking, the advanced-tactics manoeuvre's end,
  burning out and bleeding, in the original's order before the actor tick
  ([the native tick](NATIVES.md#the-native-tick-ascriptedpawntick)).
  **Checked:** a temporary hook set a patrolling terrorist's fields and
  read the timers counting, the distress rising, the pivot tweening under
  the script's own values and the bleeding correctly gated off beyond
  1,200 units; 90 s and 65 s runs show no script error.
- [**moving**](https://github.com/JuggyMcNutty/VibeEngine/commit/9df38d9520c7daed9d78fc28bdf59eef6934d9c8) --
  the roadmap's M3 moving item plus the traces item's
  `RandomBiasedRotation`: `AIDirectionReachable` walks, swims or flies the
  pawn itself along a direction and puts it back; `AIPickRandomDestination`
  tries biased random directions through it; `ReachablePathnodes` iterates
  the original's `GetPathnodeList` and `ComputePathnodeDistances` floods
  the network from it
  ([moving](NATIVES.md#moving-wandering-and-tactical-movement)).
  **Checked:** temporary probes on Liberty Island -- a spot found 280
  units along a pawn's facing, 13 pathnodes nearest first, the flood
  reaching 876 of 1,198 navpoints; a 70 s run after their removal is
  clean, no moving native left unimplemented in it.
- [**traces, moves, probes and conversions**](https://github.com/JuggyMcNutty/VibeEngine/commit/6f9b5e80cde903b4341d5656ffefc188515b39c1) --
  the roadmap's last M3 item: one probe mask per object set at every
  `GotoState` and saved as the original's `FStateFrame` keeps it (a save
  from before this commit restores its pawns' probes wrongly -- dev saves
  only); the bool, vector, rotator and object conversions; `VRand` inside
  the unit sphere; the trace iterators over the original's
  `MultiLineCheck`; `ParabolicTrace` whole; `GetBoundingBox` at a test
  place; `SetPhysics` taking its floor; the strafes at Deus Ex's speed
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)).
  **Checked:** a terrorist's probe mask -- a real sparse mask, not the old
  all-on -- round-trips a quick save exactly; the intro's scene plays 238
  lip-sync lines through the new mask; 60-70 s runs on both maps after the
  hook's removal are clean.
- [**render iterators**](https://github.com/JuggyMcNutty/VibeEngine/commit/14d981b3be8b2053efd55888eb8e689f05ec6f13) --
  the roadmap's first M4 item: an actor with a `RenderIteratorClass` is
  drawn as the items its iterator lists and gets no sprite of its own --
  the interface made and dropped as the original's renderer does, the
  Init, First, IsDone, CurrentItem, Next protocol each scene frame, each
  item keeping the proxy's place, turn, scale and glow as it was listed,
  and the proxy's `LastRenderTime` stamped per item, which the generators'
  freeze logic reads; `ParticleIterator.UpdateParticles` 3017 and both
  iterators' native `CurrentItem`
  ([particles and lasers](NATIVES.md#particles-and-lasers-render-iterators)).
  **Checked:** temporary hooks hopped the player past every generator and
  emitter -- Hell's Kitchen's street steam rises, grows and fades from its
  grates; Liberty Island's tripwire lasers draw their segment runs across
  the statue room, and crossing one raised the alarm infolink; its
  electricity emitters list every segment when in view; frozen generators
  list nothing until their proxy is seen; 75 s runs on both maps after the
  hooks' removal are clean.
- [**mesh detail**](https://github.com/JuggyMcNutty/VibeEngine/commit/e8f93e31385fadb0a7f10ab96622121e16e30fd7) --
  the roadmap's M4 mesh-detail item: the original's vertex budget worked
  out each draw -- falling as one over the actor's depth in the view,
  sooner for a complex mesh -- where the fork drew every LOD mesh whole at
  any distance; faces past the budget go, each kept corner walks its
  collapse list, and the top `LODMorph` fraction slides toward what it
  collapses to, so detail fades rather than pops
  ([mesh detail](NATIVES.md#mesh-detail)). The per-vertex work falls
  with the faces. **[perf]** to re-measure on the Smart Pro, where ~40
  meshes' per-vertex work was ~8 ms of the render. **Checked:** a
  temporary budget log matched the RE doc's own trooper numbers scaled to
  the window; a forced-coarse run drew pawns at the floor without
  breaking the scene; 75 s runs on both maps after the hooks' exact-text
  removal are clean.
- [**coronas**](https://github.com/JuggyMcNutty/VibeEngine/commit/c475705704feb8deef5b4e2c23f88e49a0736938) --
  the roadmap's M4 coronas item: the lights shining into the viewer's own
  leaf of the BSP at any distance, seen past the world, movers, pawns and
  other actors but the viewer's own pawn, fading in and out over about a
  third of a second on real time with up to 32 kept from frame to frame,
  and drawn in the light's colour times the fade
  ([coronas](NATIVES.md#coronas)); the fork's old take -- drawn parts
  of the level within 2,000 units, world-only hiding, popping at 2.5
  times the colour -- stays for other games. **Checked:** a temporary log
  on Liberty Island listed the spawn leaf's two dock-lamp coronas fading
  0 to 1 in the first third of a second, and a frame dump shows the
  lamp's glow drawn at its head; a 75 s run after the hooks' exact-text
  removal is clean.
- [**blend animations**](https://github.com/JuggyMcNutty/VibeEngine/commit/8ff8289da6ad7f54229fe91638af16f9ce5454f4) --
  the roadmap's M4 blend-animations item: the four slots over the main
  animation (head turns, lip sync, blinking) tick as the original's --
  only while the main animation plays or tweens, up to three times their
  rate, a slot that ends leaving the rest only the time over;
  `TweenBlendAnim` tweens from the slot's kept last pose (the old take
  set a positive frame and never tweened); the original's defaults; and
  the per-call logs a handheld paid for are gone
  ([its section](NATIVES.md#head-turns-and-lip-sync-blend-animations)).
  **Checked:** a temporary slot log through the intro walks Bob Page's
  mouth through its shapes with tweens caught mid-flight; the audit
  counts `PlayBlendAnim` implemented; 75 s runs on the intro and Liberty
  Island after the hook's exact-text removal are clean.
- [**mesh lighting**](https://github.com/JuggyMcNutty/VibeEngine/commit/cdf259354a7e925f8761155f7a26d8a1e9cd710d) --
  the meshes half of the roadmap's M4 lighting item: an actor's lights
  picked once a draw from its leaf's permeating list, the moving lights
  near it and last frame's -- the strongest first, statics until 8, none
  below an eighth of the strongest, `bCorona` lights counting -- shadows
  checked through the BSP every 16 frames instead of on every move, each
  light fading over about a third of a second, and the original's
  per-vertex formula in place of the fork's own
  ([lighting](NATIVES.md#lighting)); the coronas' leaf walk moved to
  `UModel::FindLeafAt`, shared. Other games keep the old path whole.
  **Checked:** a temporary log through the intro shows 4-7 leaf lights
  taken with fades ramping and a wall marking one shadowed; the
  Page-Simons scene's frame dump shows faces in the chamber's ambient
  and the suit under its key light, nothing blown out; 75 s runs on the
  intro and Liberty Island after the hooks' exact-text removal are
  clean.
- [**light maps**](https://github.com/JuggyMcNutty/VibeEngine/commit/1cba281581db6c206eccb912f4e242ab444429b7) --
  the light-maps half of the M4 lighting item: a surface's still lights
  are its kept static map and only its animating lights are added over
  the loaded colors each frame, through their own shadow bits, where an
  animated light used to rebuild the whole list -- ambient, shadows and
  all -- every frame; `NoDynamicLights` works (animated stilled into the
  map, moving left out); a mover's maps rebuild only when it moved,
  turned or a light changed ([lighting](NATIVES.md#lighting), with
  what stays the fork's own). **[perf]** the Smart Pro's lightmap-upload
  item shares this shape; measure there. **Checked:** temporary counters
  -- both test maps bake their static maps once and rebuild nothing from
  their spawn views (the island's movers rebuilt every frame before);
  at the 'Ton's flickering sconces ~90 surfaces a frame go through the
  animated add alone, shadows held, the wall's brightness alternating
  with the flicker across frame dumps; 75 s runs on both maps after the
  hooks' exact-text removal are clean.
- [**fractal textures**](https://github.com/JuggyMcNutty/VibeEngine/commit/eb97215ea939c7d8154b88b93f6a84392492839e) --
  a late M4 item: `Fire.dll`'s fire, water, wet, wave and ice textures as
  the original steps and draws them (`FireEngine.cpp`), paced as its
  `UTexture::Tick` paces them -- those with no `MaxFrameRate` at most 60
  steps a second, where the original steps them every frame; a mesh's
  textures chosen, animated and masked as the original's, and sent to the
  GPU again when they change, which patch 0019's batching had stopped for
  any texture drawn on a run of two faces or more
  ([fire, water and ice textures](NATIVES.md#fire-water-and-ice-textures)).
  **[perf]** a fire or water step is the original's work now; to measure
  on the Smart Pro. **Checked:** against the DLL's own routines in an
  emulator, byte for byte
  ([how](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/fire-dll.md#how-it-was-checked));
  the Dragon's Tooth in hand matches the original's in both engines'
  shots; a 65 s proving run on Liberty Island is clean.
- [**loudness**](https://github.com/JuggyMcNutty/VibeEngine/commit/f8d46930a679474967358626ccfefa1f3b1e0a6c) --
  M5's first item: Deus Ex plays the script's volume -- no rescale
  toward 1, no halving -- with fall-off linear from the sound to its
  radius and silent there, and the product capped at full, the Sound
  slider its ceiling ([sound](NATIVES.md#sound)); a slider move
  reaches playing sounds now, where a stored value used to wait for the
  sound's own volume to change. Other games keep the fork's old
  loudness. **Checked:** a 90 s Liberty Island run with real audio (the
  distrobox reaches PipeWire since 2026-09-25): the device initializes,
  ambient sounds play, no AL errors.
- [**the Speech slider**](https://github.com/JuggyMcNutty/VibeEngine/commit/7fb656a02a0fbc958c6931b8e9dad2a9caf7e594) --
  `SpeechVolume` is a setting of the fork's audio device, served over
  the property interface the game's menu binds; speech (the talk slot)
  gains by it, the rest by the Sound slider; the three instant-volume
  natives set the subsystem's sliders, so a menu drag holds instead of
  lasting one frame, and `SetInstantSpeechVolume` 269 is no stub
  ([sound](NATIVES.md#sound)). **Checked:** a 90 s intro run -- its
  conversation exercises the talk slot -- with no AL errors.
- [**Doppler**](https://github.com/JuggyMcNutty/VibeEngine/commit/ccb021c132731581422cc08ec06fa27a6b453ef5) --
  only an ambient sound's pitch shifts, by its actor's speed away from
  the view target at `DopplerSpeed` (a setting, default 6,500), kept to
  0.5–2, worked out in the ambience update with AL's own
  listener-velocity Doppler off for Deus Ex -- it used to shift every
  sound by the player's speed at ~14,800
  ([sound](NATIVES.md#sound)). **Checked:** a 75 s Liberty Island
  run with real audio, no AL errors.
- [**the smaller sound notes**](https://github.com/JuggyMcNutty/VibeEngine/commit/ebf27d6d2301ad350609e6c792df191d89683cf3) --
  a sound beyond its radius is dropped as the original drops it (its
  priority goes negative, never beating an empty channel); the mouth
  shapes lose the fork's own `M` band, `E` reaching to 250 Hz; and
  `bIsSpeaking` is the script's alone -- `nextPhoneme` written only
  while ConPlay has it set, the mouth no longer forced closed on a
  channel's teardown ([sound](NATIVES.md#sound)). **Checked:** a
  75 s intro run whose conversation drives the lip sync through
  ConPlay's own flag, no AL errors.
- [**sounds behind walls**](https://github.com/JuggyMcNutty/VibeEngine/commit/839bd2ce9253d756ea446ab6a55f3715d2615539) --
  each channel keeps an obstruction time: while the level's BSP stands
  between the player's own eyes and the sound's actor (movers and
  actors never block), it grows by the update's own 0-1 s time step to
  0.5 s and shrinks as the line clears, the channel playing at
  1 − 2 × that time, at least 0.33 -- a fade to a third over half a
  second and back; speech and actorless sounds are never muffled
  ([sound](NATIVES.md#sound)). **Checked:** a temporary transition
  log on Liberty Island -- ambients clear at true distances, LightWind
  blocked behind terrain at a third, a boat's idle crossing both ways;
  a 75 s run after the hook's exact-text removal is clean.
- [**ambient sounds on lights**](https://github.com/JuggyMcNutty/VibeEngine/commit/c6205fa8cd5a76d294bdadbc2afbb61f4b27cd2c) --
  an ambient sound on a lit actor is scaled by `LightBrightness` ÷ 255
  and the light's momentary animation (the renderer's own shapes:
  pulse, subtle pulse, blink, strobe, and flicker from the renderer's
  `FlickerRandom`), capped at 1, where the original reads its
  renderer's `GlobalLighting`; the palette light types stay steady
  ([sound](NATIVES.md#sound)). **Checked:** a temporary log on
  Battery Park named the lit carriers and showed the security cameras'
  hum at exactly 2 × 0.7 × (192/255) × (120/255) = 0.496 of full; a
  75 s run after the hooks' exact-text removal is clean.
- [**music**](https://github.com/JuggyMcNutty/VibeEngine/commit/ed063b1bb828726479031c2e68315214a162ef4a) --
  a transition fades the playing music out first (1 s `MTRAN_Fade`,
  5 s `MTRAN_SlowFade`, 1/3 s `MTRAN_FastFade`, at once otherwise,
  plus twice `Latency`), then the song starts at full volume at the
  order `SongSection` -- an order now, not a libopenmpt subsong -- a
  different song loaded, the same one only jumping; the playing order
  is written back into `SongSection` each frame while no transition
  waits, and section 255 stops the music
  ([sound](NATIVES.md#sound)). **Checked:** a temporary hook drove
  a forced fade (1.08 s = 1 s + 2 × 40 ms Latency, exactly), a
  same-song jump to order 4 with no reload, a 5.08 s slow fade, and
  the write-back tracking the playing order; a 75 s run after the
  hooks' exact-text removal is clean.
- [**zone reverb**](https://github.com/JuggyMcNutty/VibeEngine/commit/ec7c4cd937d2c5cccfe52faf7c236be9b6181632) --
  a zone with `bReverbZone` gives every sound its reverb through
  OpenAL's EFX (one auxiliary slot each source sends to, a NULL effect
  while no zone asks), set again only when the view target's zone
  changes; the Galaxy-to-EFX mapping is the fork's own -- `MasterGain`
  the gain, `CutoffHz` a one-pole lowpass at EFX's 5 kHz reference,
  the echo train's longest tap the decay, the earliest the reflections
  delay -- and music stays dry ([sound](NATIVES.md#sound)).
  **Checked:** a temporary hook dropped the player into Battery Park's
  reverb zone: the watch fired once and the derivation came out exact
  (gain 0.392 = 100/255, gainhf 0.768, first tap 40 ms), no AL errors;
  a 75 s run after the hooks' exact-text removal is clean. EFX's reverb
  gave way to Galaxy's own with Galaxy's mixer (below).
- [**keys released under a menu**](https://github.com/JuggyMcNutty/VibeEngine/commit/6a14fdb01e6fb4f9e5cb9cab2f14e994fe6d3cea) --
  when the UI takes a key, every key the input holds down is released
  (the tracked buttons false, the axes zero), and a taken mouse button
  clears only `bFire`/`bAltFire`, both as the original's; a movement
  key held into a menu no longer walks the player off when it closes
  ([the UI](NATIVES.md#the-ui)). **Checked:** a temporary hook
  drove a synthetic held key into an opened menu -- 3 held axes
  released on the first key event the menu took and stayed clear;
  a 75 s run after the hook's exact-text removal is clean.
- [**showing and hiding**](https://github.com/JuggyMcNutty/VibeEngine/commit/ff41d8afcdb11881616af37ade273beca431ec1d) --
  `Show` and `Hide` ask the window's parent
  (`ChildRequestedVisibilityChange`, whose script default calls
  `SetChildVisibility` back on the child; the root sets its own), and
  `SetChildVisibility` is whole: when the flag changes what can be
  seen, focus and grabs move away from what is hidden,
  `VisibilityChanged` goes down the tree, and the tree lays out again
  -- so `DeusExHUD` re-lays itself as the InfoLink and the log come
  and go ([the UI](NATIVES.md#the-ui)). **Checked:** a temporary
  flip log showed the game's own `DeusExHUD` handler firing as the
  HUD's displays hide and show through real play, and the whole HUD
  hiding as a synthetic Escape opened the menu, the round trip
  completing; a 75 s run after the hooks' exact-text removal is clean.
- [**borders tiled**](https://github.com/JuggyMcNutty/VibeEngine/commit/dfc51c594b5cb9b1a1e2717888a8fcd9e82799b5) --
  `GC.DrawBorders` tiles each edge and the centre at one texel a pixel
  (a source rect the size of the run, the same idiom `DrawPattern`
  uses) instead of stretching them over their length, and honours the
  stretch flags; margins, which the game never passes, stay
  unimplemented ([the UI](NATIVES.md#the-ui)). **Checked:** the
  call sites need a player on the themed screens, so the look is the
  by-hand check's; a synthetic F1 probe confirmed the Persona screen
  opens modal and renders, and a 75 s run is clean.
- [**the key stubs**](https://github.com/JuggyMcNutty/VibeEngine/commit/f554c6eb89b3bbeaf03ca0e85986d6468390e08e) --
  `EditWindow.Undo`/`Redo` walk a real change list (typing joins,
  `maxUndos` caps, Ctrl+Z/Ctrl+Y call them; two edit bugs fixed on the
  way: inserting over a selection dropped the wrong span, backspace at
  0 pushed the insertion point to −1); `MoveTabGroupNext`/`Prev` move
  focus between visible tab groups for Tab and Shift+Tab with
  `GetTabGroupWindow` real; `RootWindow.LockMouse` holds the pointer
  and eats buttons ([the UI](NATIVES.md#the-ui)). **Checked:** a
  temporary hook drove all three -- the pointer held under lock and
  moved after; MoveTabGroupNext focused MenuMain; five typed
  characters and two backspaces made exactly 3 changes, undo walking
  'hel' → 'hell' → 'hello' → '' and redo back; a 75 s run after the
  hook's exact-text removal is clean.
- [**the small leftovers**](https://github.com/JuggyMcNutty/VibeEngine/commit/a24bcc10396c5aae63567bd6ee350a6e68f496f0) --
  `FindStairRotation` eases the view down (−5,000) or up (5,400) a
  flight of stairs from a floor probe ahead at eye height (the probe
  distances and easing rate the fork's reading);
  `ResetKeyboard` re-reads the bindings from `User.ini`;
  `AIGetLightLevel` returns the AI-sight work's own light;
  `SET InputExt` lands in the key bindings, so the multiplayer keys
  bind once instead of failing on every map; and a window sound plays
  one unit from the player, turned by the point's place across the
  screen when positional sound is on
  ([small](NATIVES.md#small)). **Checked:** the two log-visible
  fixes directly -- the InputExt failures and the ResetKeyboard stub
  line are gone from a 75 s run, leaving only DumpLocation's known
  not-needed stub; the stairs tilt and the positional sound need
  their options switched on by hand.
- [**GC.DrawActor**](https://github.com/JuggyMcNutty/VibeEngine/commit/ca39cb532a9a21d246b8bb2c3a88be44b765eddc) --
  the vision augmentation's heat sources: the actor draws through the
  renderer into the scene being drawn with the GC's style, the glow
  and unlit given, the draw scale multiplied and a given skin
  replacing every skin, as if not hidden, all put back afterwards;
  `RenderSubsystem::DrawActor` restores the actor's own `bHidden`
  instead of forcing it hidden (the Canvas natives shared that stomp);
  `bConstrain` stays unhonoured, the augmentation's calls covering the
  whole view ([the UI](NATIVES.md#the-ui)). **Checked:** a
  temporary hook drew a live visible pawn through the path for 120
  canvas frames at glow 2 unlit, the fields reading back their own
  values between frames; a 75 s run after removal is clean.
- [**the save picture, the menus' background and the save screens' lists**](https://github.com/JuggyMcNutty/VibeEngine/commit/7ea6d871602fc6b8440693fca85dfadc905459da) --
  the roadmap's last M1 item: the original's grey snapshot of the frame
  last drawn, read back between frames, which a save takes when asked
  into a texture beside its save info; the UI background's Snapshot and
  Black, the world not drawn under them and the raw background drawn
  under the windows; and the save screens working through the game's
  script -- the list window sized as the original's (every list in a
  scroll area had drawn no rows), the listing read by place, the free
  space measured on the save path, sizes in KB, the temporary save info
  made ([saving](NATIVES.md#saving-loading-and-travel),
  [lists](NATIVES.md#lists), [the UI](NATIVES.md#the-ui)).
  **Checked:** a temporary hook drove the game's own screens -- a save
  through the Save Game screen's button, its picture read back from disk,
  the Load Game screen listing it with the quick save and the original
  game's reference save and showing that save's own picture, and the main
  menu over the snapshot and over black; a 70 s run after the hook's
  exact-text removal is clean.
- [**M0's two findings**](https://github.com/JuggyMcNutty/VibeEngine/commit/69daeb00a50cc6b06914a6077ad2171603141226) --
  a mover with no brush collides as its cylinder and a model with no BSP
  nodes is hit by nothing (`09_NYC_ShipBelow` crashed in a trace), and the
  swimming gravity floors the mass at 1 (`14_OceanLab_Lab`'s massless
  pawns made a NaN height its splash sounds died on)
  ([stops the game](NATIVES.md#stops-the-game)). **Checked:**
  temporary hooks named the mover, the NaN actors and the failing audio
  call; 70 s runs of both maps after their removal are clean, OceanLab's
  with real audio.
- [**the root window starts with the world drawn**](https://github.com/JuggyMcNutty/VibeEngine/commit/48fd9b9055ca99734622e99be7537ccfa14c6fcc) --
  the save picture commit had the renderer skip the world while the root's
  rendering is off, and the fork made its root with it off where the
  original's init turns it on: a freshly loaded map drew only the HUD until
  a menu opened. Its runs drove the menus first and never showed it.
  **Checked:** `vibe/tools/dxcap.sh prove` on Liberty Island -- both shots
  drawn.
- [**`--ini`, `--userini` and `shot`**](https://github.com/JuggyMcNutty/VibeEngine/commit/eae19f339465de4383f356eb86760df08ac53bc4) --
  the original's `INI=` and `USERINI=`, and its `SHOT` command, for
  [scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines).
  **Checked:** one console class drove both engines to Liberty Island's
  start and shot it.
- [**`SetLocation` as the original's `FarMoveActor`**](https://github.com/JuggyMcNutty/VibeEngine/commit/df9421601397dac5808857bb152a952a809977d0) --
  the actor's zone found again on every teleport (it stayed stale until
  physics next moved the actor), `bJustTeleported` and `OldLocation` set,
  what stood on it unbased, a static actor left put; `ZoneChange` run while
  `Region` still holds the zone being left, so items splash into water; Deus
  Ex's zone rules left to its scripts
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)).
  **Checked:** proving runs on Liberty Island and Battery Park; a scripted
  run found Battery Park's reverb zone by teleporting into it.
- [**`--ini` with a game's ini**](https://github.com/JuggyMcNutty/VibeEngine/commit/57c82331a6a7a8e2faed75cfd50ce6d22df5790a) --
  the client's, audio's and render device's settings read from the game's
  sections, not left at the engine's defaults.
  **Checked:** a recorded run's music off as its ini says, its window at
  the ini's size.
- [**the script's sockets**](https://github.com/JuggyMcNutty/VibeEngine/commit/c47b8b29df56560c4397ad0014fdaa952b9a65c0) --
  `InternetLink`, `TcpLink` and `UdpLink` as `IpDrv.dll`'s: the original
  constructor's link and receive modes, host-order addresses, GameSpy's
  `Validate`, one read a tick raised by link mode, the TCP states
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** both engines
  asked 333networks' master server and pinged its servers alike; the Join
  Internet screen lists the live servers.
- [**the sky zone turned the right way**](https://github.com/JuggyMcNutty/VibeEngine/commit/11e582e6c005a5b7cd8eaea527f58a277fdd91e4) --
  the sky's view turned by the inverse of the sky zone's rotation: Liberty
  Island's city had stood 56° off the original's ([a frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/render-dll.md#a-frame)).
  **Checked:** the skyline where the original's is in three captures.
- [**joining a server**](https://github.com/JuggyMcNutty/VibeEngine/commit/deb094e39e644ad40981361e597338add798ba5a) --
  a client's side of the original's protocol: a server's address
  in a URL, the UDP connection and channels, the handshake, the map loaded as
  a client's, the package map, actor channels receiving the server's actors,
  properties and calls, the player possessed; an older package's GUID from
  its heritage ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the
  fork joined the original's listen server twice and showed its world; the
  server logged both joins.
- [**the client's calls and moves**](https://github.com/JuggyMcNutty/VibeEngine/commit/4946f4ea289608f0c10d42aedb4ec1a981773c09) --
  an actor's call in a net game sent to the server or held back by the
  original's rule, actors ticked by their roles, the viewport's speed and
  update intervals, the flush after a tick that sent, and the client's frame
  rate capped at its speed over 64 ([multiplayer](NATIVES.md#multiplayer)).
  **Checked:** the fork's player walked on the original's server and ended
  where the server had it; the host's player moved smoothly on the fork.
- [**the fork listens**](https://github.com/JuggyMcNutty/VibeEngine/commit/cbc8a8ead8d5fc7768c83780e0567c765fb750f3) --
  a `?listen` map's net driver, the handshake's server side, the packages in
  the original's order, and a joining player spawned as the original's
  `SpawnPlayActor`; a URL's last option no longer dropped
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the original
  joined the fork's server and got its player spawned.
- [**the server replicates**](https://github.com/JuggyMcNutty/VibeEngine/commit/09e7b0d8091f6643d68ce4531fd9165e530febbd) --
  each client sent what the original's server sends it: the viewer, the
  actors due, priority, relevancy, channels, and each actor's changed values
  against what that client last got; a client's sends taken as the
  original's server takes them; a client's pawn ticked as the original's
  server ticks it; `SimAnim` packed by the animation natives
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the original
  joined the fork's server, possessed its pawn, saw the map's actors and
  the host's player walking, and walked on the server.
- [**the server calls its clients**](https://github.com/JuggyMcNutty/VibeEngine/commit/19ec313296c91a3dd471c666f83bf742318586d0) --
  a call on an actor a client's player owns goes to that client, the
  actor's channel opened and the actor sent first if need be; a channel the
  other side opens is acknowledged from its opening bunch
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the original
  client got the server's calls and a position correction, and ended where
  the fork's server had it.
- [**the server's `ServerActors`**](https://github.com/JuggyMcNutty/VibeEngine/commit/77c37156542ba19f34ef2435837d0ab563a82271) --
  a listening server spawns the game engine's `ServerActors` with their
  settings; a URL's port defaults to `[URL]`'s; Deus Ex's engine and net
  versions are 1100; the computer name is the machine's
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the LAN beacon
  and the query answerer replied as the original's, but for the host name.
- [**a client's level gives no travel info**](https://github.com/JuggyMcNutty/VibeEngine/commit/50dca82dfcf5c9d9c6c59ea16b743fa9ac24f809) --
  leaving a server no longer reads the local pawn's
  `PlayerReplicationInfo`, which may not have come: a net client's player
  is the server's, and nothing of it travels. **Checked:** a live server's
  game disconnected the scripted client, which had segfaulted in
  `CreateTravelInfo`; it now goes back to `dx.dx` and exits cleanly.
- [**mods load as the original loads them**](https://github.com/JuggyMcNutty/VibeEngine/commit/1c752ab071582886b5004fdf90de0b559e168b05) --
  an export with no context flags is None, as Core's linker leaves it
  (protected mods' `ScriptText`), save files aside; a class's config file
  not there is empty; path extensions match in any case
  ([mods](NATIVES.md#mods)). **Checked:** live servers' ANNA, DXNMS and
  DXMTL packages load on the fork as their client; a mod's folder, named by
  relative backslashed `Paths` in its own ini, played a custom map
  standalone.
- [**a text width of none is no limit**](https://github.com/JuggyMcNutty/VibeEngine/commit/7ce972175ac67f8f9ecc921c2f01fa2d99b24ef1) --
  `GetTextExtent(0, ...)` measures a line whole, as Extension's line
  breaking does ([the UI](NATIVES.md#the-ui)). **Checked:** the
  multiplayer window's progress lines whole and centred in a shot, where
  they went a word to a line.
- [**downloads**](https://github.com/JuggyMcNutty/VibeEngine/commit/af63a856fedf11e9ba3dd24cde8f783cab305339) --
  M7's: a client fetches the packages it lacks over file channels into the
  cache and loads them under their names; a server sends its downloadable
  packages, its list the map's, the `ServerPackages` and the game class's
  package; the cache cleaned at start; the join's messages, `CANCEL`,
  `DISCONNECT`, `RECONNECT`, `NETSPEED` and `LANSPEED` the original's
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** downloads from
  live ANNA and CDX servers and a join that stayed in on a DXMTL server;
  the original downloading a map from the fork's server and walking, and
  the fork from its own.
- [**server travel**](https://github.com/JuggyMcNutty/VibeEngine/commit/b65d124294bd92320c8c9415eb56198f35a626b1) --
  M7's: `servertravel`, the server taking no one new until it switches, a
  client's relative travel relative to its server's address, and a pending
  rejoin not sent to the menu by the old connection's close
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the fork's server
  travelled with the original following it, and the original's with the
  fork following it.
- [**a dedicated server**](https://github.com/JuggyMcNutty/VibeEngine/commit/dd92c38d8ba29fe36c8ba9cd491d6d6aeddffc95) --
  M7's last item: `--server` (the original's `-SERVER`) serves the `--url`
  map with no window, sound or player, at the original's server tick rate;
  an empty URL's port is the game's, as any URL's
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** both engines
  joined it and walked; its beacon and query answerer replied; it idled at
  under 1% of a core; a Liberty Island proving run is clean.
- [**brightness as D3DDrv's**](https://github.com/JuggyMcNutty/VibeEngine/commit/fdb0e8e29428ff412522e02f2bde4241ceacc6d7) --
  the Brightness slider a gamma of 2.5 × Brightness, as Deus Ex's display
  driver sets its ramp, where the fork took 2 × -- mid grey about 11%
  darker at the GOG build's 0.6 ([brightness](NATIVES.md#brightness)).
  **Checked:** against `D3DDrv.dll`'s ramp as read; the harness's shots,
  which carry no gamma here, unchanged.
- [**an object's path name**](https://github.com/JuggyMcNutty/VibeEngine/commit/4ed78c7db9ff06c739646c07e07c2a267419b11f) --
  `string(Object)` starts with the package even for an object in a group
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). **Checked:** `LaserConsole`'s logs print
  the laser's texture and iterator as the original's.
- [**coronas' colour and lights**](https://github.com/JuggyMcNutty/VibeEngine/commit/58a23a6a48f960c687d8106db6c72da4b6c9b0f1) --
  a corona's colour as `DrawFrame` works it out (the hue whitened by the
  saturation, 2.4 times the light maps' colour the fork used), its lights
  from the leaf the player stands in, not the eye's
  ([coronas](NATIVES.md#coronas)). **Checked:** three lamps' glows in
  both engines at `CaptureConsole`'s shot 7, cores alike; a proving run
  on Liberty Island is clean.
- [**the screen flash**](https://github.com/JuggyMcNutty/VibeEngine/commit/5b5de01b5ab6da316a1fae3e28eb1b90e3af87d2) --
  the flash handed to the device as Deus Ex's game engine hands it: the
  player's scale halved, both values clamped, `ScreenFlashes` read and a
  net game's flash always on; `vec4`'s `!=` tests the fourth component
  for inequality ([the screen flash](NATIVES.md#the-screen-flash)).
  **Checked:** `FlashConsole`'s glow as the formula gives in the fork's
  shots and, through their gamma, in the original's frames; a proving run
  on Liberty Island is clean.
- [**an empty config value**](https://github.com/JuggyMcNutty/VibeEngine/commit/22bb8a55c24e33af10b2cc2cdb7e080702baccd6) --
  a config key with an empty value is a value, as the original's: a
  string empty, a name, object or class None, a float 0, an int, byte or
  bool its default ([mods](NATIVES.md#mods)). **Checked:** a fork
  server's beacon and GameSpy answers the original's word for word, the
  empty `ServerName=` included.
- [**a repeated config key**](https://github.com/JuggyMcNutty/VibeEngine/commit/e51ba7b02d8fc6a84b007276fb1eef1bdc6c4487) --
  of a key given twice in a section the last value counts, as the
  original's ([mods](NATIVES.md#mods)). **Checked:** a user ini with
  `ngWorldSecret` twice gives `PlayerPawn`'s default the last, as the
  original's player has it.
- [**a login's checksum and options**](https://github.com/JuggyMcNutty/VibeEngine/commit/572d9543ba70989cafe5d88c2a6ae04a393d72dc) --
  the world stats checksum the original's login carries, and only the
  player options the user ini has in the travel URL; the harness's
  `DXCAP_STATS` ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the
  original's and the fork's logins to a server logging world stats alike
  to the byte.
- [**the console's GET and SET**](https://github.com/JuggyMcNutty/VibeEngine/commit/50ff984dfe7a0de48e5868be4ea5843b7850277a) --
  a class by its name alone in any package, `GET` undelimited, `SET`
  the rest of the line on every object and the defaults with the config
  saved, as the original's -- the multiplayer Host screen's settings read
  and set at last ([small](NATIVES.md#small)). **Checked:** `GetConsole`
  alike in both engines, gets, sets and the inis written.
- [**merged bunches**](https://github.com/JuggyMcNutty/VibeEngine/commit/32e9873e97d2bd3b21a564d1c96aa5d3559e0d2c) --
  a channel's bunch goes into the last one sent while that ends the
  packet being built, one header for both, as the original's
  ([multiplayer](NATIVES.md#multiplayer)). **Checked:** joins and walks
  both ways with the original, merges counted on each side.
- [**the Entry level for a lost server**](https://github.com/JuggyMcNutty/VibeEngine/commit/7cd6760e5dbc8452df5b05e4a7319d8af95deb0e) --
  a client whose server connection closes, or which the server refuses
  after the join, goes to its Entry level with a new player, as the
  original's `?failed` -- through a travel until the join loads the next
  map, else with "Connection failed" ([multiplayer](NATIVES.md#multiplayer)).
  **Checked:** a travel's second in `Entry.dx` as the original's; both
  engines' clients alike after their server was killed.
- [**the pointer and ShowCursor**](https://github.com/JuggyMcNutty/VibeEngine/commit/ca204337a19c7cedc16e78abffde2a86c8898546) --
  the pointer drawn while a modal window is up only when `ShowCursor`
  has not hidden it, as the original's -- gone from conversations, the
  credits, key binding and the multiplayer windows ([the UI](NATIVES.md#the-ui)).
  **Checked:** the main menu's pointer kept; the lost server's Entry frame
  as the original's.
- [**downloads let go**](https://github.com/JuggyMcNutty/VibeEngine/commit/46670560ac88de7a9406c3afc058a211e502afec) --
  a package loaded from the download cache goes at the next map load
  that does not use it, as the original's map load collects it: another
  server's package of that name then loads ([multiplayer](NATIVES.md#multiplayer)).
  **Checked:** two servers' versions of one package joined one after the
  other, as the original's client does; without it a version mismatch.
- [**walking over the floor**](https://github.com/JuggyMcNutty/VibeEngine/commit/690900fee2733aacfc1dcc057879fd116ea4d7b1) --
  a walking pawn floats 2.1 over where the original's step-down trace
  stops, 4.8 over the floor for a `MaxStepHeight` of 25, the level its
  base on the world's floor, as the original's ([small](NATIVES.md#small)).
  **Checked:** `StandConsole`'s start 0.05 from the original's, where it
  was 3.75 lower; a client's place 0.05 from the original server's view;
  proving runs on three maps clean.
- [**saves the original loads**](https://github.com/JuggyMcNutty/VibeEngine/commit/b8e3abb0b3cbeab9003d32b642adfb59436494ca) --
  a save written as the original's: every export with the load context,
  the level's URL and the rest of its data, the event manager in the
  original's layout, the flag base's hash, a latent action by its poll
  native's number and a state frame's nodes as the original's; the
  original's saves read whole in turn
  ([saving](NATIVES.md#saving-loading-and-travel)). **Checked:**
  `SaveConsole` and `LoadConsole` -- a fork save played on by the original
  with its census alike; every listener kept both ways; a proving run
  clean.
- [**the native replication lists**](https://github.com/JuggyMcNutty/VibeEngine/commit/3517c553e57254662ec183e12916bea7d7f39e6f) --
  what eight engine classes declare goes by the original's C++ lists, not
  their statements, where Deus Ex's differ: the animation values with
  `AnimSequence`, the blended ones and `PlayerRestartState` never, a
  player replication info's `Actor` values once, an always-relevant item's
  `bHidden` alone ([multiplayer](NATIVES.md#multiplayer)). **Checked:** the
  original's client saw the fork server's pawns animate, where they ran
  frozen; a fork client alike.
- [**the harness's live mode**](https://github.com/JuggyMcNutty/VibeEngine/commit/d3bd44aa3c742daf32cf5b0e8648ae91546318a5) --
  `--timeline=<file>`: keys and console commands at set times, from the
  start or into a net game, and each second the player's place and the
  other pawns' logged, so a run keeps the stock console a live server's
  game wants (`dxcap.sh live`, [live servers](DEVELOPMENT.md#scripted-runs-of-both-engines)).
  **Checked:** a walk with the stock console on the original's server here
  and on a live server, the server's corrections taken.
- [**the harness's own prefix and view**](https://github.com/JuggyMcNutty/VibeEngine/commit/698a5d78ca10d6afd200eb62fd51f76da8e231a1) --
  the original runs in a Wine prefix of its own and from a view of the
  game, where IDA's server and the recreated launcher's `DeusEx` had
  stopped it at its start; `DXCAP_HIDDEN=1` runs the fork off the desktop;
  `AIConsole` logs what every NPC is doing
  ([scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)).
  **Checked:** the original's `AIConsole` run to its exit; proving runs on
  the hidden display, their shots drawn.
- [**a level's start and tick**](https://github.com/JuggyMcNutty/VibeEngine/commit/61081df3966ccff5eee6b8b62f28757841a1a46f) --
  every actor starts a level 10 s undrawn, whatever its map kept, a
  level's first tick ticks every actor it loaded (it passed over them all,
  and a frame was drawn first), and the actors' step is at most 0.4 s, as
  the original's: NPCs out of sight start from `StartUp`'s code, as the
  original's ([starting up](NATIVES.md#starting-up)). **Checked:**
  `AIConsole` against the original -- Liberty Island's troops out of the
  world in `Idle` and its three sitters sitting, where they patrolled and
  wandered; proving runs on three maps clean.
- [**what counts as drawn**](https://github.com/JuggyMcNutty/VibeEngine/commit/7b07db7cdeb8ebf5563ddcb2681d2035b9caf59c) --
  an actor's render time is stamped only when a piece of its proxy, set
  back at its depth and filtered down the BSP with the walk, shows in its
  leaf, as the original keeps a sprite; pieces in solid space, hidden
  subtrees or unseen zones drop, and a zone portal seen from behind leads
  into its zone ([out of sight](NATIVES.md#out-of-sight)). **[perf]** the
  filtering is render CPU, to re-measure on the Smart Pro. **Checked:**
  `AIConsole` -- Liberty Island's Terrorist7 patrols on, where it fought
  the security bot, and NPCs inside the island no longer count as drawn;
  proving runs on three maps clean, their shots as before.
- [**the listeners called after the pass**](https://github.com/JuggyMcNutty/VibeEngine/commit/99e01bb72537fc2b02c15c9dab5b56e1bff51101) --
  the event manager queues each turn's call and makes them all once the
  senders' slots have moved on, as the original's: a pulse a call raises
  goes to the next frame, for every listener
  ([hearing](NATIVES.md#hearing-the-ai-event-system)). **Checked:** 39
  calls over Battery Park's opening minute, none raising another event
  inside the call; proving runs on three maps clean.
- [**the field of view, Deus Ex's 75**](https://github.com/JuggyMcNutty/VibeEngine/commit/c6531074a5d66bc0e82825163fbeade73d9fd33a) --
  upstream's own `MainFOV` key, missing from Deus Ex's ini, no longer
  stands in for the game's `DefaultFOV`: the view widened to 90 degrees the
  first time a weapon came up, the weapon smaller and higher
  ([small](NATIVES.md#small)). **Checked:** `ViewConsole` in both engines,
  the view at 75 and the blade where the original's is; proving runs on
  three maps clean.
- [**the harness's `TraceConsole`**](https://github.com/JuggyMcNutty/VibeEngine/commit/8a75f0786899ebb41bffb1eb8ca7a50fdd3d69ea) and
  [**traces out of a cylinder, a level hit's node**](https://github.com/JuggyMcNutty/VibeEngine/commit/13e0d6fcab8bf7d623a71d80b19159bbae2228e7) --
  a line starting inside an actor's cylinder passes out freely and is
  stopped at once heading in, and `TraceTexture` names the surface of the
  node the line meets the level at, as the original's
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). **Checked:**
  `TraceConsole` -- every NPC's floor found, where the fork hit the NPC,
  and 104 of Liberty Island's 107 traces alike (81 before); proving runs
  on three maps clean.
- [**the harness through `D3DDrv`**](https://github.com/JuggyMcNutty/VibeEngine/commit/f834887a9685c93b60fa54f0cdf4c4fba417ab7e) and
  [**its prefix able to load it**](https://github.com/JuggyMcNutty/VibeEngine/commit/5f7125d47050c71b2be1ec64a4635e340477d0f8) --
  `DXCAP_RENDERER=D3D` draws the original through the game's own renderer;
  the prefix gets vkd3d's libraries from the Proton build, without which
  `ddraw.dll` did not load and the game fell back to `SoftDrv` -- whose
  frames the first commit's figures were, corrected by the second --, and
  a run that falls back stops with an error; `ViewConsole` logs the
  level's `Brightness` and each zone's ambient light
  ([scripted runs](DEVELOPMENT.md#scripted-runs-of-both-engines)). **Checked:** `ViewConsole`
  through `D3DDrv` with the libraries taken out first (put back, `D3DDrv`
  bound), its frames `OpenGLDrv`'s region for region; a two-pass run 1.6
  times as bright as a one-pass one on lit surfaces, as
  [`d3ddrv-dll.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/d3ddrv-dll.md#the-light-maps-brightness) has it.
- [**light maps as the original's bytes**](https://github.com/JuggyMcNutty/VibeEngine/commit/34c6dc0affa2a4f81d99139468ad5fab1ab06314) --
  Deus Ex's maps built as `Render.dll` builds them: the ambient light
  `FGetHSV`'s colour times 64; each light's shadow byte (254 lit, 127
  without shadow bits) times its effect's shape on the original's falloff,
  1 − 3v² + 2v³ times the cosine, through its table -- `GlobalLighting`'s
  colour and brightness and the level's `Brightness`, in 65536ths, at most
  127 --, the channels held to 127; the torch and fire wavers and the
  watery shimmer the plain shape dimmed at random; the ambient sound on a
  light following `GlobalLighting` ([lighting](NATIVES.md#lighting)).
  **[perf]** the build's arithmetic changed; re-measure the lightmaps on
  the Smart Pro. **Checked:** against `D3DDrv`'s frames, their gamma given
  to the fork's shots: `ViewConsole`'s Liberty Island and `LaserConsole`'s
  corridor within a level or two of 256, region for region (the means 49
  against 24 and 117 against 46 before); proving runs on three maps clean,
  their shots right.
- [**meshes lit in the original's colours**](https://github.com/JuggyMcNutty/VibeEngine/commit/ade9a2bb54a05617779a97e23c9460be8b037c12) --
  each light's colour the light maps', the zone's ambient light
  `FGetHSV`'s, an `AmbientGlow` of 255 pulsing 0.25 + 0.2 sin(8t), and a
  light's strength for the pick (1 − d/r) × `LightBrightness`; the
  harness's `MeshConsole` puts Paul Denton, a crate, a barrel and a box on
  Liberty Island's pier for both engines ([lighting](NATIVES.md#lighting)).
  **Checked:** `MeshConsole` against `D3DDrv`, each within a level of 256
  (5 to 14% brighter before); proving runs on three maps clean, their
  shots right.
- [**Galaxy's mixer: its pan, sliders and reverb**](https://github.com/JuggyMcNutty/VibeEngine/commit/393ec7388666fba992288054317302a3dab7810f) --
  Deus Ex's sounds mixed by the fork's `GalaxyMixer` as `Galaxy.dll`
  mixes them, streamed out through one OpenAL source at `OutputRate`: each
  voice's fall-off from the view's eyes, its pan of at most seven-eighths
  to a side and each side at the square root of its share, the louder
  slider squared over every voice, linear resampling and exact loops, and
  Galaxy's reverb of three allpass stages in place of OpenAL's EFX; other
  games keep OpenAL's 3D sources ([sound](NATIVES.md#sound)). **Checked:**
  `SoundConsole` against the original's recording -- the pan, the reverb's
  ring and tail and the wall's fade each within 0.2 dB or 0.05 s of the
  original's; proving runs on three maps clean, two with their sound
  recorded; the Smart Pro's cross build warning-free.
- [**what counts as drawn: the render box and `BoundVisible`**](https://github.com/JuggyMcNutty/VibeEngine/commit/1d9567042483c7ff79fbfe8646aa81c44420241f) --
  an actor's proxy rectangle as the original's sprite has it: a mesh's
  from its render box, the boxes of its animation's frame and the next,
  through `BoundVisible`'s rules, a sprite's its texture's size, set back
  at the depth of its location, none for an actor behind the viewer
  ([out of sight](NATIVES.md#out-of-sight)). **Checked:** `AIConsole` --
  9 NPCs drawn at Liberty Island's start against the original's 3 (10
  before), the rest a pixel's difference at the edges; `LaserConsole`'s
  emitter as before; proving runs on three maps clean.
- [**fitting in and encroaching as the original**](https://github.com/JuggyMcNutty/VibeEngine/commit/8702cc99396afe3e1588819c7657a439e52e9168) --
  `SetLocation` and spawns fit the actor in with the original's
  `FindSpot` (the spot pushed off the walls along each axis, then from its
  corners) and check the spot's encroachment: what blocks it may stop it
  through the actor's `EncroachingOn` and hears `EncroachedBy`; a spawn so
  stopped is destroyed; a mover now asks every actor it moves into, not
  the first of each cell ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). **Checked:**
  Liberty Island's four laser views within half a unit of the original's
  search; `MeshConsole`, `CaptureConsole` and `AIConsole` as before;
  proving runs on three maps clean.
- [**traces stopping where the original's do**](https://github.com/JuggyMcNutty/VibeEngine/commit/23143b4eae4651089cfc2ce62b334636526709c9) --
  each hit given short of what it hits by the original's backoffs -- half
  a unit for a line and a tenth of the trace for a box on the level or a
  mover's brush, a thousandth of the trace on an actor's cylinder --, the
  box check counting a hull up to a tenth past the end and bounding the
  level's hulls by their boxes as the original's does, and the clear-line
  tests asking along the line alone, where the fork stopped every hit a
  unit short and looked a unit past the end; walking's float the
  original's measure, the reach test's fall a step at a time; other games
  keep the unit ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). **Checked:**
  `StandConsole`'s floor and the laser beams' ends the original's (−303.5,
  1999.5), and the first laser view fitted as the original's; `TraceConsole`
  and `AIConsole` as before; `MeshConsole`'s decorations resting 0.1 over
  the floor (1.0 before); proving runs on three maps clean.
- [**the visible-actor iterators as the original's**](https://github.com/JuggyMcNutty/VibeEngine/commit/819b54210f05559061da01a7f5494a89d2c4cdd4) --
  `VisibleCollidingActors` lists the colliding actors, movers too, whose
  locations lie within the radius (1000 for none), passes over the hidden
  only when asked and asks the line to each as `FastTrace` does, where the
  fork's asked no line and so let `HurtRadius` hurt through walls;
  `VisibleActors`' radius of 0 is no limit; other games keep the fork's;
  the harness's `VisibleConsole`
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). **Checked:** `VisibleConsole`
  alike in both engines, movers and lists; proving runs on three maps
  clean.
- [**`LineOfSightTo`, `CanSee` and `PlayerCanSeeMe` as the original's**](https://github.com/JuggyMcNutty/VibeEngine/commit/c191e4acd085398c3168f3e0238fb68fa1e2a62d) --
  `LineOfSightTo` UT's (reaches by `Visibility`, the enemy's middle,
  0.8 of the height, the cylinder's corners), `CanSee` it with the LOS
  flag, `PlayerCanSeeMe` a player's view within its reach and 60 degrees
  and then its `LineOfSightTo`, where the fork's `PlayerCanSeeMe` never
  saw anything a player looked at; other games keep the fork's; the
  harness's `SightConsole`
  ([implemented, not as the original](NATIVES.md#implemented-not-as-the-original)). **Checked:** `SightConsole`
  85 of 86 lines the original's (64 before); `AIConsole` as before;
  proving runs on three maps clean.
- [**the player's input before its physics**](https://github.com/JuggyMcNutty/VibeEngine/commit/0350c240b0e0004722f741b98b89d368642e67f7) --
  Deus Ex's player runs `PlayerInput` and `PlayerTick` in its own tick
  before its state code and physics, as the original's actor tick does,
  where the fork ran them after and took each move a tick late; other
  games keep the fork's order; `StandConsole` walks with a held key's
  forward axis ([small](NATIVES.md#small)). **Checked:** `StandConsole`
  -- the acceleration the original's from the walk's second tick, the
  walks' ends within a unit and 7; proving runs on three maps clean.
- [**`MoveTo` and `MoveToward` as the original's; a touched node a cylinder's**](https://github.com/JuggyMcNutty/VibeEngine/commit/43541c9d43227fc1aa84ea0bd3e7c1af5cdc2aab) --
  Deus Ex's latent moves step as the original's `moveToward` and its
  polls (a spot reached within 16 units across, the slowdown near it, the
  steering, `AlterDestination` for a pawn walking around what it bumped,
  the first step at once), and a path's node the pawn stands on counts as
  touched by its cylinder, for every game
  ([moving](NATIVES.md#moving-wandering-and-tactical-movement)). **Checked:** `AIConsole` --
  Terrorist15 patrols, within 4 units of the original's at 20 s (374
  before), where it backed off; proving runs on three maps clean.
