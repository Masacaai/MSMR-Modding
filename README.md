# MSMR-Modding

Native mods for the PC version of *Marvel's Spider-Man Remastered* (Steam, `Spider-Man.exe` **4.0630.0.0** only), built as `.script`
mods for [Overstrike](https://www.nexusmods.com/marvelsspidermanremastered/mods/4199), plus the reverse-engineering notes and tools behind them.

> **Unofficial fan project.** Not affiliated with or endorsed by Insomniac Games, Sony or Marvel. It ships **no game assets**; you need
> your own legitimate copy of the game.

## The mods

| Mod | What it is | Status |
|---|---|---|
| [**Endless Arena**](EndlessArena/README.md) | Waves of enemies, bosses, boons and curses on Ryker's Island, until you go down | Beta released |
| [**Manhattan Skies**](ManhattanSkies/README.md) | Time of day, sky, sun, fog and image controls from the pause menu: 124 skies, presets, Early Morning | Alpha released |
| [**ModMenu**](ModMenu/README.md) | The shared code that puts the mods' tabs in the game's Settings screen (a library compiled into each mod) | Working |

## Layout

| Path | What |
|---|---|
| `EndlessArena/`, `ManhattanSkies/` | One folder per mod: source (`src/` or `mod/src/`), its own `docs/`, and its own tools |
| `ModMenu/` | Shared Settings-tab library |
| `docs/` | **Shared knowledge**: [getting started](docs/GETTING_STARTED.md), [game internals](docs/GAME_INTERNALS.md), [reverse-engineering method](docs/REVERSE_ENGINEERING.md), [UI notes](docs/UI_HUNT.md), [releasing](docs/RELEASING.md), [roadmap](docs/ROADMAP.md). Start at [docs/README.md](docs/README.md) |
| `recon/`, `tools/` | Reverse-engineering notes and the Python/C++ helpers (crash dumps, signatures, asset extraction) |
| `AGENTS.md`, `CONTRIBUTING.md`, `THIRD_PARTY_NOTICES.md` | Agent rules, how to contribute, credits and licenses |

Anything extracted from the game (catalogs, memory dumps, screenshots) stays in each mod's git-ignored `recon/` folder and is never committed.

## Requirements (all mods)

1. Game build **4.0630.0.0** exactly. The mods find game functions by signature and check each one against an expected address, so on any
   other build they refuse to call into the game instead of crashing it.
2. Overstrike 1.8.1 with **"Enable .script support"** and **"Add '-scripts' to commandline.txt"** on; see
   [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md#2-overstrike-and-the-scripts-proxy).

## Building

Each mod has a `build.cmd` (Visual Studio 2022 C++ workload). Build **Endless Arena first**: it installs MinHook through vcpkg, and the other
mods borrow that copy. Details in [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md).

## Legal

GPL-3.0, see [LICENSE](LICENSE). Credits and third-party licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
