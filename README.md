## VibeEngine

A fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), the open-source Unreal
Engine 1 reimplementation.

Targeting only Deus Ex.

### You need your own copy of Deus Ex (version 1112fm); none of the game's files are included!

- **Build Targets**: Linux on x86_64 (X11, Wayland, SDL2) and on aarch64 with any GPU
  (Snapdragon, Mali, PowerVR). So far it runs on x86_64 and on the Smart Pro's PowerVR
  ([the ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports)).
- **Deus Ex as the original plays it**: Our goal is a playable build as close to 1:1 with the
  original engine, its scripting and the rest as possible.

## The fork and Surreal Engine

The two differ at the core ([how it is kept](vibe/docs/ENGINE.md#how-it-is-kept)).

The fork's own docs and tools are in [`vibe/`](vibe/).

- [`vibe/docs/ENGINE.md`](vibe/docs/ENGINE.md): how the fork is kept, run and profiled, and
  [what it changes](vibe/docs/ENGINE.md#what-the-fork-changes).
- [`vibe/docs/NATIVES.md`](vibe/docs/NATIVES.md): differences from the original.
- [`vibe/docs/DEVELOPMENT.md`](vibe/docs/DEVELOPMENT.md): scripted runs of both engines,
  debugging a crash, proving a change.

## Building and running

Only Linux builds are made and tested here.

### The easy way: Port Ex Machina

[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina) fetches the engine, builds it
with the launcher for each of its ports, and stages, runs or deploys the result: follow
[its quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).

### Building it on its own

You need:

- a C++20 compiler (GCC 10 or later), CMake 3.15 or later and pkg-config;
- the development files of OpenAL, SDL2 and X11 (with Xi);
- optionally Wayland's (waylandpp, xkbcommon), EGL, dbus, GLib and fontconfig, used when found.

On Arch these are among the packages
[Port Ex Machina's dependencies](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)
list. Then:

```sh
git clone https://github.com/JuggyMcNutty/VibeEngine.git
cd VibeEngine
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_SDL3=OFF
cmake --build build --target SurrealEngine -j"$(nproc)"
```

`-DENABLE_SDL3=OFF` keeps the SDL2 window backend, the only one with the fork's gamepad support
([the gamepad](vibe/docs/ENGINE.md#running-on-our-devices)). The build leaves two files in
`build/` that go together:

- `SurrealEngine`, the engine: the executable keeps upstream's name;
- `SurrealEngine.pk3`, its resources (the UI's fonts among them), found beside the executable.

### Running it

Give it the game's folder, or start it from the game's `System` folder:

```sh
build/SurrealEngine /path/to/deusex
```

It draws with Vulkan unless its settings name another device, so it needs the GPU's Vulkan
driver. Where to go next:

- [Running it](vibe/docs/ENGINE.md#running-it): the rest of the command line -- the log on
  stderr, a map to start on, a dedicated server, the inis.
- [Settings and environment](vibe/docs/ENGINE.md#settings-and-environment): the settings file
  and the variables the engine reads, the window backend a pad needs among them.

### For advanced users

The build's own switches:

| Switch | Default | What it does |
|---|---|---|
| `ENABLE_SDL2` | `ON` | The SDL2 window backend, the only one with the fork's gamepad support. |
| `ENABLE_SDL3` | `ON` | The SDL3 backend: when SDL3 is found, it is built in place of SDL2's. |
| `ENABLE_X11`, `ENABLE_WAYLAND` | `ON` | The desktop's window backends; off for a device with neither. |
| `VIBE_SDL2_BORDERLESS_FULLSCREEN` | `OFF` | SDL2's fullscreen as a borderless window, for a vendor SDL2 that deadlocks in `SDL_SetWindowFullscreen`. |
| `ZIPDIR_EXECUTABLE` | unset | A host-built `zipdir` to pack `SurrealEngine.pk3` with when cross-compiling. |

- **A port's build** is an initial cache (`cmake -C`), `ports/<id>/engine.cmake` on the port's
  launcher branch: the desktop's switches
  ([linux-x86_64's](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/engine.cmake)),
  or a cross build's toolchain and switches
  ([trimui-smartpro's](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/engine.cmake)).
  Port Ex Machina's `scripts/engine.sh build <port>` builds with it
  ([a port's files](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#what-a-port-is)).
- **Debugging**: a build with symbols, or with AddressSanitizer
  ([crashes](vibe/docs/DEVELOPMENT.md#crashes)).
- **Profiling and validation**: [measuring it](vibe/docs/ENGINE.md#measuring-it).
- **Kept, but neither built nor tested here**: the D3D11 device and the Win32 code it needs
  (SurrealWidgets' Win32 backend, `Win32Resources/`, the natvis, the vendored OpenAL Soft, the
  crash reporter's Windows part), and the SDL3 backend.

## License

Surreal Engine's licences are in [`LICENSE.md`](LICENSE.md); the fork's own changes are
zlib-licensed, as Surreal Engine's own code is. Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.
