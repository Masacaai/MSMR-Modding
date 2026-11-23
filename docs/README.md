# Documentation

Two kinds of documentation, kept apart:

- **The game reference ("bible")**: [internals/](internals/README.md), how Spider-Man Remastered works inside, by system. Shared by every mod; also mirrored to the GitHub Wiki.
- **Mod documentation**: in each mod's own `docs/` (design, ledger, bugs, release notes). See [Per mod](#per-mod).

## Start here

1. **[GETTING_STARTED.md](GETTING_STARTED.md)**: install the tools, build, install into the game, the command-file dev loop.
2. **[internals/](internals/README.md)**: the knowledge base about the game itself (addresses, structures, UI, crashes). The index is by system.
3. **[REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md)**: how that knowledge was found, with the tools in [`../tools/`](../tools/README.md); how to port to a new build.

## Guides and repository-wide pages

| Doc | What it holds |
|---|---|
| [MAINTAINING_DOCS.md](MAINTAINING_DOCS.md) | **Where everything goes**, the local-only notes rule, how to extend the bible, the checklist before a commit. Read before editing docs or moving code |
| [RELEASING.md](RELEASING.md) | How to cut a release |
| [ROADMAP.md](ROADMAP.md) | Ideas that span several mods (shareable presets, the Settings-tab library) |
| [`../recon/README.md`](../recon/README.md) | Generated reflection dumps of the game (types, enums, events, messages) |
| [`../tools/README.md`](../tools/README.md) | The scripts and programs, by purpose |
| [`../common/README.md`](../common/README.md) | The shared C++ code every mod compiles in |
| [`../AGENTS.md`](../AGENTS.md) | Rules and commands for AI coding agents |

## Per mod

| Mod | Docs |
|---|---|
| Endless Arena | [ARCHITECTURE](../EndlessArena/docs/ARCHITECTURE.md), [HOW_TO](../EndlessArena/docs/HOW_TO.md), [LEDGER](../EndlessArena/docs/LEDGER.md) (design decisions and backlog), [BUGS](../EndlessArena/docs/BUGS.md), [ROGUELIKE](../EndlessArena/docs/ROGUELIKE.md), [SCORING](../EndlessArena/docs/SCORING.md), [REWARD_DESIGN](../EndlessArena/docs/REWARD_DESIGN.md), [ENDLESS_MODE](../EndlessArena/docs/ENDLESS_MODE.md), release notes in [releases/](../EndlessArena/docs/releases/), drafted [issues/](../EndlessArena/docs/issues/) |
| Manhattan Skies | [ROADMAP](../ManhattanSkies/docs/ROADMAP.md), [SKY_LOOKS](../ManhattanSkies/docs/SKY_LOOKS.md), release notes in [releases/](../ManhattanSkies/docs/releases/) |
| Echoes of Time | [README](../EchoesOfTime/README.md), release notes in [releases/](../EchoesOfTime/docs/releases/) |
| Disable Webs | [README](../DisableWebs/README.md), release notes in [releases/](../DisableWebs/docs/releases/) |

## Conventions in these docs

- **Dates** are the day something was found or decided (`2026-10-15`).
- **Confidence tags**: `[verified]` / `[V]` tested in game, `[RE]` read from disassembly only, `[I]` inferred, `[?]` unknown or a guess.
- Addresses are `exe+RVA` for `Spider-Man.exe` **4.0630.0.0**.
- "User" in older docs means the project's owner, who did all the in-game testing.
