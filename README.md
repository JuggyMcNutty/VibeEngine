# VibeEngine

A fork of [Surreal Engine](https://github.com/dpjudas/SurrealEngine), the
open-source Unreal Engine 1 reimplementation, for Deus Ex: a working base for
porting the game to handhelds and consoles. It is the engine of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina), a launcher
and ports for Deus Ex. Made with AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## What it is for

- **Deus Ex as the original plays it.** Where Surreal Engine lacks something
  the game needs, the original's behaviour is read from the game's own DLLs
  and reimplemented: saving and travel, conversations, the AI, the look, the
  sound, and multiplayer with the original's servers and clients. Deus Ex is
  the one game it is for; the other UE1 games Surreal Engine runs are not
  tested here.
- **Handhelds and consoles.** Desktop Linux is the base. The TrimUI Smart Pro
  runs the game, the work on its speed on hold while the engine comes first;
  aarch64 Linux, Android and the Xbox 360 are planned.

Where it stands: [the roadmap](vibe/docs/ROADMAP.md). What it changes from
Surreal Engine, commit by commit:
[what the fork changes](vibe/docs/ENGINE.md#what-the-fork-changes). What it
still lacks of the original: [`NATIVES.md`](vibe/docs/NATIVES.md).

## The fork and Surreal Engine

It does not follow upstream: newer Surreal Engine work is merged in only when
chosen, and nothing goes back. The source keeps upstream's names -- the
`SurrealEngine` executable and its folders -- and [`Docs/`](Docs/) is
upstream's documentation, about Surreal Engine rather than this fork. The
fork's own docs and tools are in [`vibe/`](vibe/), where merging upstream
never reaches them: how it is kept, run and profiled
([`ENGINE.md`](vibe/docs/ENGINE.md)), and how to work on it
([`DEVELOPMENT.md`](vibe/docs/DEVELOPMENT.md)).

## Building and running

Port Ex Machina builds it for each of its ports and starts it;
[running it](vibe/docs/ENGINE.md#running-it) has the command line. On its own
it builds with CMake as [`Docs/Building.md`](Docs/Building.md) says, from a
clone of this repository. Only Linux builds are made and tested here.

## License

Surreal Engine's licences are in [`LICENSE.md`](LICENSE.md); the fork's own
changes are zlib-licensed, as Surreal Engine's own code is. Deus Ex belongs to
its owners; this project is not affiliated with or endorsed by them.
