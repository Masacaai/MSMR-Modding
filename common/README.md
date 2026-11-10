# common

Source shared by every mod, compiled into each mod's DLL (the mods are built separately; there is no shared library at runtime).

| File | What |
|---|---|
| `log.*` | `LOG(...)` to a file next to the DLL's data, truncated each launch |
| `memory.*` | SEH-guarded reads and writes of game memory |
| `scan.*` | Signature scanning of `Spider-Man.exe` and RVA helpers |
| `gamethread.*` | Run a function on the game's main thread (hooked per-frame tick) |
| `watchpoint.*` | Hardware write watchpoints for the dev tools |

`gamethread.cpp` includes `natives.h`, which each mod supplies in its own `src/` (the mod's list of resolved game functions; it must provide
`natives::GameThreadTick`). Each mod's `build.cmd` adds its `src/` and this folder to the include path.
