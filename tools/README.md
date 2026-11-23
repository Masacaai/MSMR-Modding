# tools: reusable scripts and programs

Everything here works on a copy of the game you own; outputs that contain game data are git-ignored and never committed.
Python tools need Python 3.12 and, per tool, `pip install minidump pefile capstone numpy`. Third-party clones (`tools/ALERT`, from Tkachov/ALERT) are fetched locally, see
[GETTING_STARTED.md](../docs/GETTING_STARTED.md). Method and examples: [REVERSE_ENGINEERING.md](../docs/REVERSE_ENGINEERING.md).

| Folder | What |
|---|---|
| `sigtools/` | Reading the executable and crash dumps: `fx.py` (disassembly with names, RTTI, xrefs, reflection tables), `sigscan`, `rtti`, `xrefs`, dump/stack readers, hash tools. Header of each file says usage |
| `assets/` | The asset archive: `extract.py`, `deps.py` (writes `common/game/preload_deps.inc`), `archives.py`, `tocinfo.py`, `vanilla_vs_mod.py`, Wwise names |
| `toctool/` | C++ offline checker for asset paths against the `toc` (needs vcpkg zlib and lz4) |
| `memory/` | Diffing memory dumps taken by the mods' dev commands (`datadiff.py`, `pausediff.py`) |
| `roster/` | Mines enemy actor files into Endless Arena's roster (`gen_roster.py` writes `EndlessArena/src/roster_data.inc`) |
| `runviewer/` | Browser viewer for the telemetry JSON each Endless Arena run writes |
| `docs/` | Repository maintenance: `check_links.py`, `gen_internals_index.py`, `rewrite_links.py` (see [MAINTAINING_DOCS.md](../docs/MAINTAINING_DOCS.md)) |
| `wiki/` | `sync.py`: builds (and pushes) the generated GitHub Wiki |
| `github/` | `create_issues.ps1`: opens the drafted issues of `EndlessArena/docs/issues/` |

Mod-specific scripts that only make sense for one mod's data stay in that mod's `tools/` (for example `ManhattanSkies/tools/`: atmosphere catalogs and look tables).
