# Manhattan Skies

Time of day, sky, sun and fog control for Marvel's Spider-Man Remastered (Steam, `Spider-Man.exe` 4.0630.0.0 only). A native `.script` for Overstrike's script support. Part of the [MSMR-Modding](../README.md) repository; its `recon/` folder (game-derived catalogs, dumps, screenshots) is git-ignored and never committed.

## What it does
A **MANHATTAN SKIES** tab in the game's **pause menu > SETTINGS** (added by `../ModMenu/`, which Endless Arena uses too):

| Row | What it does |
|---|---|
| PRESET | Cycles saved looks read from `Documents\Marvel's Spider-Man Remastered\ManhattanSkies\Presets\*.ini` (one small ini per preset; copy a file to share it). Ships Warm Night, Golden Hour and Dreamy Haze. CUSTOM is your own unsaved settings: they are remembered when you switch to a preset and come back when you pick CUSTOM (kept in `custom.ini`). A preset sets the rows it names and returns the rest to AUTO. |
| TIME OF DAY | The game's seven lighting modes, including Early Morning (the game's `kBlackout`) and the overcast looks its own option never offers. Applies at once; the screen fades briefly and the pause menu stays open. Remembered across launches and applied to the game's own load-time choice (so story missions use it too); the game's own selector is used until you pick one here. |
| DYNAMIC CYCLE | Plays a full day over and over (2 to 60 minutes): Early Morning, Day (one day in four is overcast), Sunset, Night. The sun travels smoothly; each phase may use a random sky from a curated list (or the game's own). No fade to black: the colors shift toward the next sky shortly before it arrives. It follows the game's pause (it stops with the pause menu) and waits for Spider-Man to slow down before a lighting switch, because the game's lighting change stops him in mid-air. Starts about 30 seconds after a world loads. |
| SKY STYLE / SKY VARIANT | Puts any of the game's **124 atmospheres** (sky, sun, reflected light) on top of the time of day, so you can mix, e.g. Day world lighting under a Sunset sky. Styles: DAY, SUNSET, MORNING, NIGHT, MORE NIGHT, OVERCAST, STRANGE, INTERIOR. Table of every look and its real file: `docs/SKY_LOOKS.md`. |
| SUN DIRECTION, SUN HEIGHT | Turn and raise the sun (live: changes sky, shadows and building lighting). |
| SUN COLOR, SUN BRIGHTNESS | Tint and scale the sunlight. |
| FOG DENSITY, FOG DISTANCE, FOG COLOR, HAZE | Thin or thicken the fog, move where it starts, change its color, and add or remove the white distance veil. Some cutscene skies are very foggy. |
| EXPOSURE, COLOR FILTER, CLOUDS | The whole picture's brightness (in stops), a color tint over everything, and the cloud layer on or off. Found with `tools/fieldprobe.py`. |

Y (reset all) resets the sky rows to AUTO; X resets one row. Settings are saved to `Documents\Marvel's Spider-Man Remastered\ManhattanSkies\ManhattanSkies.ini` and applied again when a world loads. "AUTO" always means the game's own value.

## Layout
- `mod/`: source. `mod\build.cmd` builds `mod\build\ManhattanSkies.dll` and `.script`; it installs nothing. MinHook comes from `EndlessArena\vcpkg_installed` (build Endless Arena once first). `log`, `memory`, `scan`, `gamethread`, `watchpoint` come from the shared [`../common/`](../common/README.md).
  - `sky.cpp`: the settings, the look tables, the override logic (load an atmosphere, swap weights, swallow the game's own base activation, release it again), sun and fog edits.
  - `menu.cpp`: the tab's rows. `recorder.cpp`: hooks, slot access, dev tools. `looks_extra.inc`: generated look table.
- `docs/RECON.md`: what is known, with [V] verified / [I] inferred / [?] unknown tags (Parts 1-12).
- `docs/SKY_LOOKS.md`: every look, its label (an educated guess from the file name) and its real file.
- `recon/`: catalogs and logs copied out of the game. `tools/`: Python helpers (atmosphere catalog, label table, sweep).

## Dev control
Write a line to `<game>\scripts\ManhattanSkies.cmd`; results go to `<game>\scripts\ManhattanSkies.log`. `sky [n]` sets a lighting mode; `look <style> <variant>`, `sun <dir> <height> <color> <bright> [fog]` (indices), `skystate`, `skyreset`, `atm`, `atmlist`, `atmload`, `atmrelease`, `adj`, `slotaddr`, `peek`, `pokef`, `poke32`, `recompute`, `findq`, `snap`/`snapdiff`, `loadtrace`, `uitrace`, `wp`, `menu`, `mark`. `tools/sweep.sh` cycles every curated look.

## Install (manual, dev)
Copy `ManhattanSkies.dll` into `<game>\scripts\` and add `ManhattanSkies.dll` as a line in `<game>\scripts.txt` (backup: `scripts.txt.bak-before-manhattanskies`). Overstrike's "Install mods" rewrites `scripts\` and `scripts.txt`, so redo this after it.

## Status
Everything above is verified in game (2026-11-09): the rows, 124 looks, live edits, persistence across launches, clean exit. Not released; the plan for packaging it (shared repo, presets) is in `../docs/ROADMAP.md`.

## Known limits
- A sun color edit can leave tinted patches in cached lighting until the time of day is changed.
- Some atmospheres come from cutscenes and look odd or foggy in the open world. That is why they are offered, not hidden.
- Which of the paired lighting modes (Night/Night variant, Overcast/Overcast variant) is the rainy one is not settled.

## Credits
The Settings-tab method is SpidyVR's (IlyaMezerowsky, https://github.com/IlyaMez/SpidyVR, GPL-3.0), via Endless Arena's re-implementation and `ModMenu/`. The log/memory/scan/gamethread layers come from Uncontained/Endless Arena. ALERT (Tkachov, GPL-3.0) documents the asset formats used by the catalog tools.
