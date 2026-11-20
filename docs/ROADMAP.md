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
- **Releases.** Manhattan Skies 0.1.0 alpha 1 is out (tag `manhattan-skies-v0.1.0-alpha.1`, 2026-11-10). Endless Arena's beta.1 is out; the ModMenu refactor needs a beta.2 whatever else happens.
- **Manhattan Skies, after the alpha:** the preset save button with a legend glyph (stretch goal); the lighting transition's momentum stop (the cycle only waits for a calm moment, the cause inside the game's transition is not found); the sky style MORNING and the Settings banner naming are unchanged.
- `docs/GETTING_STARTED.md` and `docs/GAME_INTERNALS.md` are still mostly Endless Arena material in their older sections; trim or split them as the other mods grow.

## 3. Smaller open items

- Which of the paired lighting modes (Night / Night variant, Overcast / Overcast variant) is the rainy one.
- Nine atmospheres exist in the game that are not referenced by any name found (ALERT counts 133, 124 are reachable).
- Mixing several third-party environment mods: each replaces one game atmosphere, so two Night mods collide; whether Overstrike can add brand-new assets is unknown. The plan for this is item 4b below.
- Manhattan Skies presets to ship once the preset system exists: a **"Warm Night"** preset (warm sun, warm FOG COLOR, less HAZE, a warm COLOR FILTER) that approximates the NYC Warm Night mod without replacing any texture; also presets for the Day/Sunset/odd looks that users like.
- Manhattan Skies: the new image rows (exposure, color filter, haze, fog distance, clouds) are built and installed but not yet tested in game.

## 4. Next Manhattan Skies release (planned, not started; written 2026-11-10)

### 4a. No loading screen when switching the time of day from the menu
The TIME OF DAY row still uses the game's own lighting transition (black screen, loading icon, about a second), like the game's Settings option. The dynamic cycle already switches without it: `modmenu::SuppressFade` swallows the transition's Flash `FadeOut`/`FadeIn` (see `ManhattanSkies/docs/RECON.md` Part 16). Do the same for the row (`Choose` in `ManhattanSkies/mod/src/menu.cpp`: call `modmenu::SuppressFade(5000)` next to `KeepMenuOpen`).
- Expect the same pop of sky and lights the cycle has. The cycle softens it by blending colors toward the next sky beforehand; a menu pick has no lead time, so the pop stays (or blend quickly after the switch).
- The momentum stop does not matter while the Settings screen is open (the game is paused), but check what happens when the menu closes.
- Check that the pause menu still stays open and the screen does not stay dark.

### 4b. Several atmosphere `.smpcmod`s at once, switchable from TIME OF DAY
Goal: a player puts all the atmosphere mods they like in one folder, and the TIME OF DAY row (or SKY STYLE) lists them, so any can be picked at any time. Today an environment mod replaces one game atmosphere (and its textures) when Overstrike installs it, so two Night mods collide and only one wins.

What is already known:
- An environment mod replaces assets by id (`ModFiles/<span>_<ID>` in the zip): the atmosphere (`atm_ibl_lgt_{day,sunset,night,overcast}_02`), its sky texture (up to 16 MB) and baked lighting textures. Overstrike patches the game's `toc`, so one mod per asset id (RECON Parts 2 and 13; `tools/envassets.py`, `recon/env_mods_inventory.txt`).
- The rain mods change only an atmosphere plus textures; NYC Warm Night changes 32 floats and textures (RECON Parts 13 and 15).
- Our mod can load any of the game's 124 atmospheres into the pool by asset id and edit its values live (sun, fog, haze, exposure, filter, fog color). It cannot yet load an asset that is not in the `toc`.

Approaches to evaluate (all [I] or [?]; none tried):
1. **Values only.** Read each mod's atmosphere from the folder and apply its numbers (sun, fog, haze, exposure, filter) onto a loaded sky. No textures: it would reproduce color-grade style mods (Warm Night) but not their sky or baked lighting.
2. **Re-slot into unused assets.** A converter takes N mods and writes one mod that puts each atmosphere (and the textures it references, with the references rewritten in the atmosphere's bytes) onto a different, otherwise unused game asset id (the game ships many cutscene atmospheres and textures the open world never uses). Our mod then loads those by id, which it already does. Needs: free texture ids of a compatible format and size, and a way to run the converter (Overstrike patches the `toc` only when it installs, so this is a build step for the player, not something the DLL can do at runtime).
3. **Runtime asset injection.** Hook the asset loader so an id missing from the `toc` is served from our own files, and build the textures ourselves. Most work; unknown whether the asset system accepts ids that are not in the `toc`.
4. **Ask Overstrike.** Check `Tkachov/Overstrike` (and the ALERT wiki) for whether a mod can add new asset ids; if it can, a re-id converter is simple.

**Update 2026-11-13 (later): the clean version works.** Carriers are not needed: the mod adds genuinely new assets to the game's in-memory toc (copy span 0 behind the old tables, append ours, repoint the toc object, point span 0 at the copy). A full mod (atmosphere plus its three textures) loaded from loose files under new ids and rendered (`ManhattanSkies/docs/RECON.md` Part 17b). The carrier method below was the first proof and is superseded because it hijacks real assets.

**Status 2026-11-13:** the core is proven. Approach 3 (runtime injection) works through *carrier* assets: the mod rewrites an unused asset's entry in the game's in-memory toc and answers the game's read of it from a loose file (`ManhattanSkies/docs/RECON.md` Part 17, `mod/src/toc.cpp`). A real mod atmosphere was loaded this way. Remaining: textures (own carrier ids and rewritten references), safe carrier choice, reading the zip, the UI.

First steps (done): (a) read Overstrike's source for new-asset support; (b) test whether an `.smpcmod` that replaces an unused cutscene atmosphere loads through `look` / `atmload`; (c) see whether the texture references inside an atmosphere (file offsets around `0x11c`, `0x344`, `0x44c`) can be pointed at other texture ids; (d) design how the player's folder is read and how Overstrike fits (the folder may need to be a conversion step, not a live folder).

Open questions: how to name a mod's looks in the row (file name, or the mod's `SMPCMod.info`); whether to keep the player's installed atmosphere mods working as they do now; what happens when two mods reference the same texture id with different contents.

### 4c. Dynamic cycle follow-ups (user notes, 2026-11-10)
- **Resume where it stopped.** Only a pick in the TIME OF DAY row is saved (`sky::SaveMode`); the cycle's own mode switches are not, so after a restart the game loads the last manual pick (usually Day) and the cycle starts from the start of that phase (`PhaseOfMode`). Save the phase and the overcast flag (every few seconds and on exit) and resume from them, and keep the saved mode in step with the cycle so the game's load-time call matches.
- **Start sooner than 30 seconds after a load.** 30 s (`g_world_up_at` in `sky.cpp`) is a conservative guard from the one hang found when the lighting setter was called 3 s after a load, not a measured need. Use a "gameplay started" signal instead: the hero's position is readable (`GetActor`) and the world timers (`exe+5d6a028`, `5d6a168`, `5d6a2a8`) advance, for a few seconds.
- **Night to Early Morning looks wrong** (red, overhead, too bright). Cause [V from the code and the catalog]: the morning sky's own sun is red (RGB 1.0, 0.41, 0.41, intensity 0.75, elevation 29); in the last seconds of night the blend mixes that color and intensity into the night sky's light (the moon sits 40 degrees up and is not steered), so a bright red light shows overhead; once the morning phase starts the steered sun is low but still red and bright. Plan: do not blend sun color or intensity before a steered phase; fill the sky with ambient instead (exposure, haze, fog color, filter), then in the morning phase start the sun below the horizon (about -3 degrees), orange (about 1.0, 0.6, 0.3), intensity ramping up from about 10%, easing to the sky's own color and intensity as it rises.
- **Ambient light: no control found.** The atmosphere header's ambient value (`slot+0x300`) did nothing visible (RECON Part 10, user test). The sky's ambient light comes mostly from the baked IBL textures. Candidate fakes: exposure, haze, fog color, color filter. Unexplored: the weak candidates in `slot+0x2d0..0x6a4` (the image probe crashed the game around `+0x320`), and the material nodes `KeyLightIntensity` / `SunLightScale` (RECON Part 13).

### 4d. Atmosphere mods from a folder: built, follow-ups (2026-11-13)
Built and in the mod: the `Atmospheres` folder (Documents\Marvel's Spider-Man Remastered\ManhattanSkies\), `mod/src/atmpacks.cpp` (reads `.smpcmod` zips, finds which of the five open-world atmospheres each replaces, gives atmosphere and shipped textures new ids and rewrites the references, adds them to the game's in-memory asset table via `toc.cpp`), an ATMOSPHERE row under TIME OF DAY (one choice kept per time of day, also used by the dynamic cycle), start at launch, textures unpacked only when a mod is first picked. How it works: `ManhattanSkies/docs/RECON.md` Part 17 and 17b. Not yet tested over a long session; not released.
Follow-ups noted by the maintainer and Claude:
- **Built 2026-11-13, installed, not yet played:** CYCLE ATMOSPHERES toggle (pool = game's own + listed skies + the time of day's atmosphere mods; the ATMOSPHERE pick still wins); `.modular` support (below; the BND Early Morning file is in the folder); TIME OF DAY row swallows the black fade (4a); the cycle resumes from the saved phase (ini `[cycle]`, written every 5 s; the saved time of day follows the cycle's switches) and starts once the hero is readable and the world timers advance for 4 s, 8 s after a load at the earliest (4c); Night to Early Morning: no sun blend toward morning, the dawn sun rises from -3 degrees, orange and faint, easing to the sky's own (4c).
- **Built later 2026-11-13 (untested):** SUN BRIGHTNESS OFF; no sun blending in any transition; the cycle freezes (no clock, no switch) while `modmenu::PlayerBusy()` (HUD calls seen by a hook on exe+1d1cee0: ShowCrimeScanner / ShowCounter / ShowMissionObjective / ActivateProgressBar start, HideCounter / HideMission / DeactivateProgressBar end with 5 s grace, SetComboCounter = a fight for 12 s); per-phase random brightness, fog density, fog distance and haze in the cycle; ModMenu keeps old tab strings and item memory alive (crash hardening, cause unproven).
- **Still open, not built (needs the game running to verify):** a placeholder archive for players with no Overstrike asset mod (needs an unused archive entry and a writable name in the in-memory toc); shared helper assets (sun flare texture, one material) while a mod is selected; ambient light control.
- **`.modular` support (done, see above; notes kept):** the format is an outer zip with `info.json` (a layout of modules with options) plus nested `.stage` zips whose entries are named by game path (`<span>/atmosphere/atm_ibl_lighting/atm_ibl_lgt_morning_01_ow.atmosphere`); hash each path to its asset id (the same CRC64 as ALERT) and treat each option as a version. Many Early Morning mods ship this way.
- **No Overstrike asset mod installed:** the new assets borrow an Overstrike raw archive (`mods\modN`), which only asset mods create. Fix: find an archive entry no asset uses, rename it in memory to a small placeholder file of our own, and borrow that.
- **Shared helper assets a mod ships but its atmosphere does not reference** (the sun flare texture, one material) are skipped, so their look stays vanilla. Could be supported by swapping those two table entries while the mod's version is selected.
- **Done (CYCLE ATMOSPHERES): let the dynamic cycle rotate through the atmosphere mods (maintainer request, 2026-11-13).** Add a toggle under DYNAMIC CYCLE (e.g. "INCLUDE ATMOSPHERES", saved in the ini, shown only when atmosphere mods were found). When on, each time of day's pool of variations in the cycle also contains that time of day's atmosphere mods, each used as the mod ships it: default lighting values and parameters for everything else (no sun/fog/exposure/filter/cloud overrides on top, no random-sky blending). The pick is a random choice among the pool, like the existing sky pick (`PickLook` in `sky.cpp`; `atmpacks::Labels/IdFor` give the pool). Decided: a version the player picked in the ATMOSPHERE row overrides the random pick for that time of day (the same rule as the manual TIME OF DAY pick overriding the cycle); the random pool is used only where the row is on DEFAULT. Remember `atmpacks::Prepare(id)` before loading a picked mod's atmosphere (the cycle's target prep already does this).
- **Menu limit:** a row holds 32 choices (31 mods per time of day); the asset table has room for 4096 added assets.
- **Release notes / Nexus text** must describe the Atmospheres folder, the ATMOSPHERE row, the cache folder and the Overstrike-asset-mod requirement before the next release.

## 5. Manhattan Skies: play-test checklist for the builds of 2026-11-19 (everything below is built and installed, none of it run)

Install state: both mods' packages in `Mods Library` and the DLLs in `scripts\` are the 2026-11-19 builds. Logs: `scripts\ManhattanSkies.log`; dev commands: `scripts\ManhattanSkies.cmd`.
1. **Seamless switches are now always on** (the SWITCH STYLE row was removed 2026-11-20; `seamless 0` in the command file turns the hook off for a test). Change the time of day while swinging, then during a crime. Expect no loading icon, no stop, the crime survives. Log: `seamless: skipped the load screen ... A -> B`. If the world pops badly or something breaks, set it back to GAME'S (or `seamless 0` in the command file).
2. **AMBIENT LIGHT / AMBIENT COLOR** (IMAGE section). Step 1..7 in a shaded alley and on a night rooftop; report which step is the first visible one and which washes out. The steps are a guess (vanilla night rain is 0.003).
3. **no_swallow.txt**: create it in `Documents\Marvel's Spider-Man Remastered\ManhattanSkies` for one long Settings-heavy session. If the `exe+31e9582` crash does not come back, swallowing was the cause. (With seamless on there is no fade to swallow anyway.)
4. **`fsinfo`** in the command file once in a loaded world: tells which toc object layout is right and how many archives are open (needed to add an archive entry without another Overstrike mod).
5. Still untested from before: the colour-amount rows, the activity freeze in a real crime, the hold at a phase boundary, `use_extras.txt` (off by default).
