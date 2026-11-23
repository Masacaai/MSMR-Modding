# Roadmap and ideas (repository-wide)

Ideas that touch more than one mod. Per-mod plans live in `<Mod>/docs/ROADMAP.md`. Nothing here is decided or started unless it says so.

## ModSettings: dropped (decided 2026-11-09)

The idea was to promote the shared menu code to a standalone mod called **ModSettings**, like the MSM2 script of that name. **Decided against:** the maintainer
does not want to release something modelled on an existing modder's MSM2 work, and most of the method comes from SpidyVR's author. `common/modmenu/` stays as an internal
library compiled into the maintainer's own mods (cleaner than three copies of the menu hooks); it is not offered as a base for other mods, and no stable public API is planned.

What stays open from that idea (small, internal):
- **The tab-list banner** still says SETTINGS (baked into the pause menu's Flash at creation); a fix would live in `common/modmenu/`.
- **Credit.** The method is SpidyVR's (see THIRD_PARTY_NOTICES.md); keep that.
- **Other mods that also touch the pause menu.** `ModMenu` hooks the game's menu functions itself and shares one set of Settings tabs only among our own DLLs; it does not detect or defer to a third-party menu mod. If one ever appears, check for duplicate tabs and id clashes (tab id 94+, setting numbers 0x400+) before shipping alongside it.


## Shareable presets (idea, 2026-11-08)

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

