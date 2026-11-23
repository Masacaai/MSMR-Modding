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
| [**Echoes of Time**](EchoesOfTime/README.md) | Two time-based abilities from Spider-Man: Edge of Time: Hyper-Sense (slow motion with a trail of after-images) and Accelerated Decoy (one decoy that enemies prioritize), all in the Settings screen | 1.0.0 |
| [**Disable Webs**](DisableWebs/README.md) | F7 turns off swinging, zipping, point launch, the web shooter and the web-assisted wall-run moves | 1.0.0 |

## Layout

| Path | What |
|---|---|
| `EndlessArena/`, `ManhattanSkies/`, `EchoesOfTime/`, `DisableWebs/` | One folder per mod, holding only what is specific to it: `src/`, `docs/` (design, bugs, release notes), `mod.json`, `build.cmd` |
| [`common/`](common/README.md) | Code every mod compiles in: engine layer, `game/` (natives, spawning), `modmenu/` (the Settings tabs), `build/` (toolchain, packing) |
| [`docs/internals/`](docs/internals/README.md) | **How the game works inside**: the reverse-engineering reference, organized by system (also on the Wiki) |
| [`docs/`](docs/README.md) | Guides: [getting started](docs/GETTING_STARTED.md), [reverse-engineering method](docs/REVERSE_ENGINEERING.md), [releasing](docs/RELEASING.md), [roadmap](docs/ROADMAP.md), [maintaining the docs](docs/MAINTAINING_DOCS.md) |
| [`recon/`](recon/README.md) | Generated dumps of the game's reflection data (types, enums, events, messages) |
| [`tools/`](tools/README.md) | The Python/C++ helpers (crash dumps, signatures, asset extraction) |
| `AGENTS.md`, `CONTRIBUTING.md`, `THIRD_PARTY_NOTICES.md` | Agent rules, how to contribute, credits and licenses |

Anything extracted from the game (catalogs, memory dumps, screenshots) stays in git-ignored local folders and is never committed.

## Requirements (all mods)

1. Game build **4.0630.0.0** exactly. The mods find game functions by signature and check each one against an expected address, so on any
   other build they refuse to call into the game instead of crashing it.
2. Overstrike 1.8.1 with **"Enable .script support"** and **"Add '-scripts' to commandline.txt"** on; see
   [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md#2-overstrike-and-the-scripts-proxy).

## Building

Each mod has a `build.cmd` (Visual Studio 2022 C++ workload). The first build installs MinHook through vcpkg for all mods. Details in
[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md).

## Legal

GPL-3.0, see [LICENSE](LICENSE). Credits and third-party licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
