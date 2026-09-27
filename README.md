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
  runs the game, with work on its speed under way; aarch64 Linux, Android and
  the Xbox 360 are planned.

Where it stands: [the roadmap](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/ROADMAP.md).
What it changes from Surreal Engine, commit by commit:
[what the fork changes](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/ENGINE.md#what-the-fork-changes).

## The fork and Surreal Engine

It does not follow upstream: newer Surreal Engine work is merged in only when
chosen, and nothing goes back. The source keeps upstream's names -- the
`SurrealEngine` executable and its folders -- and [`Docs/`](Docs/) is
upstream's documentation, about Surreal Engine rather than this fork.

## Building and running

Port Ex Machina builds it for each of its ports and starts it; its
[engine doc](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/ENGINE.md#running-it)
has the command line. On its own it builds with CMake as
[`Docs/Building.md`](Docs/Building.md) says, from a clone of this repository.
Only Linux builds are made and tested here.

## License

Surreal Engine's licences are in [`LICENSE.md`](LICENSE.md); the fork's own
changes are zlib-licensed, as Surreal Engine's own code is. Deus Ex belongs to
its owners; this project is not affiliated with or endorsed by them.
