# Instructions for AI coding agents

This project is **AI-friendly**: it was built with an AI assistant (Claude Code), the docs are written so an agent can pick the work up
cold, and AI-assisted contributions are welcome. This file is the entry point for agents (Claude Code, Codex, Cursor, Copilot, Gemini, and
others that read `AGENTS.md`). Humans should read [CONTRIBUTING.md](CONTRIBUTING.md) first; everything here applies to them too.

> Claude Code reads `CLAUDE.md`, not `AGENTS.md`. If you use it, create a local `CLAUDE.md` containing the single line `@AGENTS.md`
> (the repo's `.gitignore` keeps `CLAUDE.md` out of commits so each person can add their own preferences).

## What this is

A repository of Overstrike `.script` mods (C++20) for **Marvel's Spider-Man Remastered, build 4.0630.0.0 only**, and the reverse-engineering
behind them. Layout and the mods are in the root [README.md](README.md):

- `EndlessArena/`: an endless-survival mode at Ryker's Island. Map of the code: [EndlessArena/docs/ARCHITECTURE.md](EndlessArena/docs/ARCHITECTURE.md).
- `ManhattanSkies/`: time of day, sky, sun, fog and image controls. Start at [ManhattanSkies/README.md](ManhattanSkies/README.md) and its `docs/RECON.md`.
- `ModMenu/`: the shared code that puts each mod's tab in the game's Settings screen. `common/`: the code every mod compiles in (`log`, `memory`, `scan`, `gamethread`, `watchpoint`).

Each mod finds game functions by signature, hooks the game's per-frame function and acts on the game thread. The reverse-engineered knowledge
is in [docs/GAME_INTERNALS.md](docs/GAME_INTERNALS.md). **Read it and the mod's own docs before changing code.**

## Commands

```
EndlessArena\build.cmd                  build -> EndlessArena\build\EndlessArena.dll / .script   (prints "BUILD OK"; build this first, it installs MinHook)
ManhattanSkies\mod\build.cmd            build -> ManhattanSkies\mod\build\ManhattanSkies.dll / .script
EndlessArena\tests\run_plan.cmd 20      print the wave plan offline (no game)
EndlessArena\tests\run_offline.cmd      resolve every game native against Spider-Man.exe on disk (no game running)
```

Installing into the game and testing needs a human with the game: copy `EndlessArena\build\EndlessArena.dll` to `<game>\scripts\` with the game
closed, launch the game (the maintainer uses Steam), and drive it with the command file (`<game>\scripts\EndlessMode.cmd`, output in `EndlessMode.log`). See
[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md). If you have shell access to a machine with the game, you can write commands to the
command file and read the log yourself; a lot of debugging works that way.

## Rules that matter

1. **Never commit game material**: no `.actor`/`.config`/textures/models, no extracted JSON of them, no memory dumps (`*.dmp`, `*.mdmp`), no saves.
   Facts (an address, an offset, a field name, an asset *path*) belong in the docs; the bytes don't. Check `git status` before committing.
2. **Be honest about what you tested.** You usually cannot run the game. Say "compiled, not tested in game" in your summary and the PR.
   Mark new knowledge in the docs as `[verified]`, `[RE]` (read from disassembly only) or `[?]` (unknown). Never invent an address, offset or
   API; if you don't know, write "unknown".
3. **Game state changes go through the game thread** (`gamethread::Run/Post`) and every game function through `natives.cpp` with a signature and an
   **expected RVA**. Reads of game memory use `common/memory.h`. Code shared by all mods lives in `common/`; change it there, never in a copy. Never block the game thread.
4. **Don't make it up as you go**: a one-off oddity (a single hang, one stuck enemy) is recorded in [EndlessArena/docs/BUGS.md](EndlessArena/docs/BUGS.md) and not "fixed".
   Act on problems that recur. Don't add speculative workarounds.
5. **Design decisions are in [EndlessArena/docs/LEDGER.md](EndlessArena/docs/LEDGER.md)** (no health sponges; boons never touch Spider-Man's skills; every curse has an upside;
   Horde shows no levels). Don't reverse one without asking the maintainer. The pool-curation list is the maintainer's call.
6. **Match the surrounding code**: comment density, naming, idiom, and the 4-space/same-line-brace style. Comments say *why*, with the evidence
   (a log line, an address, a date). Never hard-code a button name in a prompt: use `input::Glyph/Name`.
7. **Generated files**: `roster_data.inc` (from `tools/roster/gen_roster.py`) and `preload_deps.inc` (from `tools/assets/deps.py`) are generated.
   Change the generator and regenerate; don't hand-edit. Regenerating needs a local game install (see [EndlessArena/docs/HOW_TO.md](EndlessArena/docs/HOW_TO.md)).
8. **Keep the docs true.** If your change makes a doc wrong, update it in the same change. New game knowledge goes in GAME_INTERNALS.md.
9. **Licensing**: the project is GPL-3.0-or-later. Don't paste code from other projects unless its license is compatible, and say where it came from.

## Working on this with an agent: practical notes

- The in-game loop is slow and manual. Batch your changes, build once, and give the human a short checklist of what to look at in the game and in the log
  (the format used in [EndlessArena/docs/NEXT_SESSION_TESTS.md](EndlessArena/docs/NEXT_SESSION_TESTS.md)).
- The log is the best debugger: add one `LOG(...)` line per event you'd want to see.
- For crashes and hangs use the tools in `tools/sigtools` ([docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md)). They need Python with
  `pip install minidump pefile capstone`.
- Do not run commands that modify the user's game folder or saves (other than copying the built DLL when asked) and do not close or kill the game unprompted.
- The shell on the maintainer's machine is Windows (PowerShell/Git Bash); build scripts are `.cmd`/`.ps1`.

## Disclosure

If an AI wrote a substantial part of a change, say so in the PR description (tool and model are enough). A human is accountable for what gets merged:
read the diff, build it, and test it in game where you can.
