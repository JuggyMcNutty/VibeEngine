# VibeEngine

A fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), the open-source Unreal
Engine 1 reimplementation, for Deus Ex on the platforms of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina), a launcher and ports for
Deus Ex. Its branch is `deusex`. You need your own copy of Deus Ex (version 1112fm); none of the
game's files are included.

- **Targets**: Port Ex Machina's ports, Linux x86_64 (the base) and the TrimUI Smart Pro first
  ([the ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#ports)).
- **Deus Ex as the original plays it**: where Surreal Engine lacks something the game needs, the
  original's behaviour is read from the game's DLLs
  ([dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info)) and reimplemented. It
  runs no other UE1 game ([how it is kept](vibe/docs/ENGINE.md#how-it-is-kept)).

## The fork and Surreal Engine

Upstream's work does not come in: the two differ at the core
([how it is kept](vibe/docs/ENGINE.md#how-it-is-kept)). The source keeps upstream's names (the
`SurrealEngine` executable and its folders), and [`Docs/PackageFormat.md`](Docs/PackageFormat.md)
is upstream's documentation, about Surreal Engine rather than this fork. The fork's own docs and
tools are in [`vibe/`](vibe/).

- [`vibe/docs/ENGINE.md`](vibe/docs/ENGINE.md): how the fork is kept, run and profiled, and
  [what it changes](vibe/docs/ENGINE.md#what-the-fork-changes).
- [`vibe/docs/NATIVES.md`](vibe/docs/NATIVES.md): differences from the original.
- [`vibe/docs/DEVELOPMENT.md`](vibe/docs/DEVELOPMENT.md): scripted runs of both engines,
  debugging a crash, proving a change.

## Building and running

Port Ex Machina builds it for each of its ports and starts it
([quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start));
[running it](vibe/docs/ENGINE.md#running-it) has the command line. On its own it builds with
CMake, `cmake -S . -B build && cmake --build build --target SurrealEngine`, with the packages
[Port Ex Machina's dependencies](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)
list. Only Linux builds are made and tested here. Kept, but neither built nor tested here: the
D3D11 device and the Win32 code it needs (SurrealWidgets' Win32 backend, `Win32Resources/`, the
natvis, the vendored OpenAL Soft, the crash reporter's Windows part), and the SDL3 backend.

## License

Surreal Engine's licences are in [`LICENSE.md`](LICENSE.md); the fork's own changes are
zlib-licensed, as Surreal Engine's own code is. Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.
