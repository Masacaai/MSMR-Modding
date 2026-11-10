# Shared documentation

Knowledge that applies to every mod in this repository. Each mod keeps its own design notes, ledger and bug list in its own folder:
[Endless Arena](../EndlessArena/docs/) and [Manhattan Skies](../ManhattanSkies/docs/).

If you are new, read these in order:

1. **[GETTING_STARTED.md](GETTING_STARTED.md)**: install the tools, build, install into the game, the command-file dev loop.
2. **[GAME_INTERNALS.md](GAME_INTERNALS.md)**: the knowledge base about the game itself (addresses, structures, UI, crashes).
3. **[REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md)**: how that knowledge was found, with the tools in `tools/`; how to port to a new build.

## Reference

| Doc | What it holds |
|---|---|
| [UI_HUNT.md](UI_HUNT.md) | The chronology of finding and driving the game's HUD and menus: every recording, what each panel accepts |
| [RELEASING.md](RELEASING.md) | How to cut a release |
| [ROADMAP.md](ROADMAP.md) | Ideas and plans across the repository: shareable presets, the shared `common/` layer |
| [`../recon/M1_natives_4.0630.md`](../recon/M1_natives_4.0630.md) | The first reverse-engineering pass: how the natives were decoded |
| [`../ModMenu/README.md`](../ModMenu/README.md) | The shared code that adds each mod's tab to Settings |
| [`../AGENTS.md`](../AGENTS.md) | Rules and commands for AI coding agents |

## Per mod

| Mod | Docs |
|---|---|
| Endless Arena | [ARCHITECTURE](../EndlessArena/docs/ARCHITECTURE.md), [HOW_TO](../EndlessArena/docs/HOW_TO.md), [LEDGER](../EndlessArena/docs/LEDGER.md) (design decisions and backlog), [BUGS](../EndlessArena/docs/BUGS.md), [ROGUELIKE](../EndlessArena/docs/ROGUELIKE.md), [SCORING](../EndlessArena/docs/SCORING.md), [REWARD_DESIGN](../EndlessArena/docs/REWARD_DESIGN.md), [ENDLESS_MODE](../EndlessArena/docs/ENDLESS_MODE.md), release notes in [releases/](../EndlessArena/docs/releases/), drafted [issues/](../EndlessArena/docs/issues/) |
| Manhattan Skies | [RECON](../ManhattanSkies/docs/RECON.md), [SKY_LOOKS](../ManhattanSkies/docs/SKY_LOOKS.md) |

## Conventions in these docs

- **Dates** are the day something was found or decided (`2026-10-15`).
- **Confidence tags** in GAME_INTERNALS: `[verified]` tested in game, `[RE]` read from disassembly only, `[?]` unknown or a guess.
- Addresses are `exe+RVA` for `Spider-Man.exe` **4.0630.0.0**.
- "User" in older docs means the project's owner, who did all the in-game testing.
