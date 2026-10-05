# VibeEngine

A fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), the open-source Unreal
Engine 1 reimplementation, for Deus Ex on the platforms of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina), a launcher and ports for
Deus Ex. Its branch is `deusex`. You need your own copy of Deus Ex (version 1112fm); none of the
game's files are included.

- **Targets**: Linux x86_64 (the base) and the TrimUI Smart Pro; planned: aarch64 Linux,
  Android, Xbox 360 ([where each stands](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#status)).
- **Deus Ex as the original plays it**: where Surreal Engine lacks something the game needs, the
  original's behaviour is read from the game's DLLs
  ([dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info)) and reimplemented. The
  other UE1 games Surreal Engine runs are not tested here.

## The fork and Surreal Engine

upstream's work comes in
only when chosen ([how it is kept](vibe/docs/ENGINE.md#how-it-is-kept)). The source keeps
upstream's names (the `SurrealEngine` executable and its folders), and [`Docs/`](Docs/) is
upstream's documentation, about Surreal Engine rather than this fork. The fork's own docs and
tools are in [`vibe/`](vibe/), where a merge from upstream never reaches.

- [`vibe/docs/ENGINE.md`](vibe/docs/ENGINE.md): how the fork is kept, run and profiled, and
  [what it changes](vibe/docs/ENGINE.md#what-the-fork-changes).
- [`vibe/docs/NATIVES.md`](vibe/docs/NATIVES.md): differences from the original.
- [`vibe/docs/DEVELOPMENT.md`](vibe/docs/DEVELOPMENT.md): scripted runs of both engines,
  debugging a crash, proving a change.

## Building and running

Port Ex Machina builds it for each of its ports and starts it
([quick start](https://github.com/JuggyMcNutty/port-ex-machina#quick-start));
[running it](vibe/docs/ENGINE.md#running-it) has the command line. On its own it builds with
CMake as [`Docs/Building.md`](Docs/Building.md) says. Only Linux builds are made and tested here.

## License

Surreal Engine's licences are in [`LICENSE.md`](LICENSE.md); the fork's own changes are
zlib-licensed, as Surreal Engine's own code is. Deus Ex belongs to its owners; this project is not affiliated with or endorsed by them.
