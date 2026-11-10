# Roadmap and ideas

Ideas the maintainer wants kept somewhere. Nothing here is decided or started unless it says so.

## 1. ModSettings: dropped (decided 2026-11-09)

The idea was to promote the shared menu code to a standalone mod called **ModSettings**, like the MSM2 script of that name. **Decided against:** the maintainer
does not want to release something modelled on an existing modder's MSM2 work, and most of the method comes from SpidyVR's author. `ModMenu/` stays as an internal
library compiled into the maintainer's own mods (cleaner than three copies of the menu hooks); it is not offered as a base for other mods, and no stable public API is planned.

What stays open from that idea (small, internal):
- **The tab-list banner** still says SETTINGS (baked into the pause menu's Flash at creation); a fix would live in `ModMenu/`.
- **Credit.** The method is SpidyVR's (see THIRD_PARTY_NOTICES.md); keep that.
- **Other mods that also touch the pause menu.** `ModMenu` hooks the game's menu functions itself and shares one set of Settings tabs only among our own DLLs; it does not detect or defer to a third-party menu mod. If one ever appears, check for duplicate tabs and id clashes (tab id 94+, setting numbers 0x400+) before shipping alongside it.

## 1b. Shareable presets (idea, 2026-11-08)

Endless Arena and Manhattan Skies should ship with **preset folders they read from disk**, so people can share presets.

- A preset row at the **top** of the mod's menu cycles through the presets found on disk (Endless Arena already has a PRESET row of its own: `settings.cpp`, `ApplyPreset`).
- **Saving** is not a UI row: it is a controller or keyboard button (Square or similar) whose glyph shows in the legend at the bottom of the screen, and the legend entry appears only while there are custom, unsaved settings in play.
- No text input in MSMR's UI is expected, so a saved preset is named **Custom1, Custom2, ...**; the player renames the file on disk and the mod shows the file name in the menu, because it reads the folder.

Things to settle (raised by Claude, not decided):
- **Where.** Overstrike wipes `scripts\` on every Install mods, so shipped presets need a copy-on-first-run into the data folder (`Documents\Marvel's Spider-Man Remastered\<Mod>\Presets\`, like the ini today), and the player's own presets must live there, not in `scripts\`.
- **Format.** One small ini or JSON file per preset, keyed by setting name (not by row index), so a preset keeps working when rows are added; unknown keys ignored, missing keys left at their current value.
- **Knowing a preset is "custom/unsaved".** Compare the live settings with the loaded preset (a dirty flag); show the legend entry only then.
- **The legend glyph is the hard part.** The Settings screen's button legend is part of its Flash movie (the `BTN_*` hint strings such as reset-option and reset-all in the `Options` class); adding an entry needs reverse engineering that hint list. Fallbacks: reuse an existing legend slot, or accept the keyboard/controller button without a glyph. Endless Arena already reads the PlayStation controller (`psinput.cpp`).
- **Rescanning.** Re-read the preset folder each time the menu opens, so renames and shared files appear without a restart.
- This is a natural feature for **ModMenu**: provide the preset row, the folder scan and the save button once, so every mod on it gets presets without rewriting them.

## 2. One repository for all the mods (idea 2026-11-08; folders and docs restructured 2026-11-09, local, not committed)

Done:
- Layout: `EndlessArena/` (renamed from `EndlessMode/`), `ManhattanSkies/`, `Uncontained/`, `ModMenu/`, shared `docs/`, `recon/`, `tools/`. A new root `README.md` introduces the mods.
- Endless Arena's design, ledger, bugs, scoring and release notes moved to `EndlessArena/docs/`; shared knowledge (getting started, game internals, reverse-engineering method, UI notes, releasing, this roadmap) stays in `docs/`. Relative links were rewritten and checked.
- `ManhattanSkies/` is no longer ignored as a whole: only its `recon/` and `mod/build/` folders (game-derived material, build output) are. `Uncontained/` stays fully ignored until it is ready for release; only release-ready mods leave the ignore list.
- Runtime names kept on purpose so existing installs and records keep working: `EndlessMode.log/.ini/.cmd`, `EndlessMode_runs`, and the Documents data folder named EndlessMode.

Also done 2026-11-09:
- **Shared `common/` layer**: `log`, `memory`, `scan`, `gamethread`, `watchpoint` live once in `common/`; each mod's `build.cmd` compiles it and adds its own `src/` to the include path (`gamethread.cpp` needs the mod's `natives.h`). All mods rebuilt.
- **Docs pass**: root README, `AGENTS.md`, `CONTRIBUTING.md`, the bug-report template and `GETTING_STARTED.md` now speak for all mods; the shared lighting, atmosphere and pause-menu knowledge from Manhattan Skies is summarized in `docs/GAME_INTERNALS.md`.
- **Uncontained stays fully git-ignored** until it is ready for release; only release-ready mods leave the ignore list.

Still open:
- ~~Renaming the GitHub repository~~ done 2026-11-09: `Masacaai/MSMR-Modding` (GitHub redirects the old `Spider-Man-Endless-Mode` URLs). Still to do by hand: the Nexus page and any pinned links elsewhere.
- **Releases.** Endless Arena's beta.1 is out; the ModMenu refactor needs a beta.2 whatever else happens.
- `docs/GETTING_STARTED.md` and `docs/GAME_INTERNALS.md` are still mostly Endless Arena material in their older sections; trim or split them as the other mods grow.

## 3. Smaller open items

- Which of the paired lighting modes (Night / Night variant, Overcast / Overcast variant) is the rainy one.
- Nine atmospheres exist in the game that are not referenced by any name found (ALERT counts 133, 124 are reachable).
- Mixing several third-party environment mods: each replaces one game atmosphere, so two Night mods collide; whether Overstrike can add brand-new assets is unknown.
- Manhattan Skies presets to ship once the preset system exists: a **"Warm Night"** preset (warm sun, warm FOG COLOR, less HAZE, a warm COLOR FILTER) that approximates the NYC Warm Night mod without replacing any texture; also presets for the Day/Sunset/odd looks that users like.
- Manhattan Skies: the new image rows (exposure, color filter, haze, fog distance, clouds) are built and installed but not yet tested in game.
