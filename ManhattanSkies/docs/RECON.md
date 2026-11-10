# Manhattan Skies: recon

> The settled, shared findings (lighting modes, atmosphere pool and slot fields, load-time rule, pause-menu internals) are summarized in [../../docs/GAME_INTERNALS.md](../../docs/GAME_INTERNALS.md). This file is the chronological record with the evidence.

Tags: [V] verified (disassembly or in game), [I] inferred, [?] unknown. Addresses are `Spider-Man.exe` 4.0630.0.0 RVAs.

# Part 1: starting point (2026-11-08)

Most of this came from Uncontained (`Uncontained/docs/RECON.md` Parts 7, 15-19), where it was found for Act 3 weather. Nothing here has been tested for time of day yet.

## Lighting modes (weather lives here)
- Seven modes in the level data: `kDay`, `kSunset`, `kOvercast`, `kOvercastRain`, `kNight`, `kNightRain`, `kBlackout`. [V names from level data]
- The user's Act 3 saves 41 (day rain) and 42 (night rain) start in rain, so the rain modes are used in normal play. [V user]
- `SetLightingModeAction` (exec `exe+bd8640`) looks the mode hash up in a 7-dword table at `exe+5da1280` (`0x952fe93f, 0xe2f80f71, 0x5472474d, 0x8cc8f7d4, 0x8b027fc2, 0x7c9a92aa, 0xc8b42359`) and calls `exe+1f7f9e0(world+0xe10d0, index)`. Which index is which mode: [?] (the names did not hash-match). [V disassembly]
- `exe+1f707e0` is `mov rax,[rip+x]; ret`: the world singleton, no side effects. Current mode = dword at `world+0xe2040`. [V]
- `exe+1f7f9e0` gets an object through `exe+1f03570` and tail-calls the worker `exe+1f058e0(obj, index)`. The worker has no other caller; the wrapper has 7 (`exe+735d17`, `bd867d`, `1f7485b`, `1f7490d`, `1f7ea69`, `1faab88`, `1fae8fa`). So hooking the wrapper sees every mode change. [V xrefs]
- Open [?]: whether rain particles and wet surfaces follow the mode everywhere or only where Act 3 zones listen (`OnLightingModeChangedAction`); whether calling the setter alone is enough or the game also transitions/streams around it (`LightingModeTransition*Event`).

## Atmospheres (looks without a lighting mode: Early Morning, extra environment sets)
- `AtmosphereManager` (vtable `exe+3cf1f18`): slot count `+0x70`, **pointer** to the slot array at `+0x110` (stride `0x8c0`), pointer to a 4-int counter array at `+0x120` (16 bytes per slot). [V]
- **Correction to Uncontained Part 7:** `exe+162f020(mgr, slot, delta, mask, min, max)` is not "activate with a blend time". It finds the slot index, adds `delta` to each of the slot's counters 0-2 selected by mask bits 0-2 and to counter 3 when bit 1 of the shifted mask is set, clamps each to `[min, max]`, then calls `exe+162ff10(mgr)` (recompute). `ActivateAtmosphereAction` passes delta 500, min 0, max 0x7fffffff, so it acts like a weighted reference count; removing the look presumably passes a negative delta. 26 callers. [V disassembly, I meaning]
- Known atmosphere assets: `atmosphere/atm_ibl_lighting/atm_corrupt_truck_chase*.atmosphere`, `atm_nyc_map_2017.atmosphere` (map screen). ALERT parses an `.atmosphere` header with `time_of_day`, sun RGBA/rotation/elevation/radius, ambient RGBA; 133 such assets in MSMR. [V]

## Settings tab
Endless Arena's `menu.cpp` injects a Settings tab (SpidyVR's method, GPL-3.0, credit in THIRD_PARTY_NOTICES). It ships in the v0.2.0 beta. [V]

## S0 recorder (built 2026-11-08)
`mod/src/recorder.cpp` hooks the setter (`ls:`) and `AtmosphereAdjust` (`at:` only when counters change, `as:` first sight of a slot), polls the mode field (`lm:`). The three signatures each have exactly one hit (sigcheck). Installed manually into the game's `scripts\`; not yet run.
Plan: one session, use the game's own time-of-day option to go through each choice, `mark` between them, then `atm`. Expected yield: index for Day/Sunset/Night, which caller the menu uses, and whether a time-of-day change also moves atmosphere slots.

---

# Part 2: what environment mods replace (2026-11-08)

Survey of 23 environment `.smpcmod`s and one `.modular` in the user's Overstrike `Mods Library` (zip listings only, nothing installed). Inventory: `recon/env_mods_inventory.txt` (tool `tools/envassets.py`); every atmosphere path referenced anywhere in the archive: `recon/atmosphere_refs_in_archive.txt` (`tools/atmpaths.py` over a `grepall` scan). Names resolved by CRC64 and checked present in the toc. [V]

## One atmosphere asset per time-of-day slot
| Slot | Atmosphere (all under `atmosphere/atm_ibl_lighting/`) | Its sky texture (named inside the atmosphere) | Mods that replace it |
|---|---|---|---|
| Day | `atm_ibl_lgt_day_02.atmosphere` (882245A8C2B5BD1A) | `textures/hdr/8k_day_sky_miles_cool2.texture` | 5 (BND Day, NYC Clear Day, Raimi Day, Contrail Day, TASM2 Day) |
| Sunset | `atm_ibl_lgt_sunset_02.atmosphere` (98246BE74781784F) | (unresolved) | 6, incl. **Raimi Morning (Sunrise)**, which is a sunset-slot replacement |
| Night | `atm_ibl_lgt_night_02.atmosphere` (9F4F5899144AF4FA) | | 9, incl. all "rainy night" mods |
| Overcast | `atm_ibl_lgt_overcast_02.atmosphere` (8BEDB52FCD0301DF, archive 3) | `material/atmosphere/atm_mv_sky/tempcgsky/overcast/overcast_skybox_hdr_04.texture` | 1 (NYC Rainy Day) |
Each mod also swaps that slot's sky textures (`material/atmosphere/atm_bsm_sky/{day,sunset,night}/8k_*.texture`) and a few shared ids (`textures/effects/generic/fx_flare_radial_01.texture`, material 95E22EB5BAD81B22 in all day/sunset mods). Rainy-night mods add three small ids (textures 8995A4D493702331, 89D5F9DA0876FDB1 and `textures//default_pink_shader//default_pink_shader_g.texture`, 132 bytes); their role is unknown [?].
So a "look" is essentially **one atmosphere asset (header + baked IBL, about 0.5 to 2.7 MB) that names its own sky texture**, plus the sky texture(s). [V file contents, I "essentially"]

## Native looks beyond the three slots [V present in toc]
- **Early Morning:** `atm_ibl_lgt_morning_01_ow.atmosphere` (sky `atmosphere/atm_early_morning/8k_sky_earlymorning_01_ow.texture`), `atm_ibl_lgt_morning_01_fisk.atmosphere`, `atmosphere/peter_apt/atm_peter_apt.atmosphere`. The "BND Early Morning Atmos" `.modular` regrades exactly these (it does not move Early Morning into a slot), which confirms they are the opening mission's look. `_ow` reads as "open world". [I]
- **Night rain:** `atm_ibl_lgt_night_rain_02.atmosphere` (sky `material/atmosphere/atm_bsm_sky/overcast/4k_overcast_hdr_4b_scale.texture`), referenced by a weather-station conduit. There is **no** day-rain atmosphere (`overcast_rain_02`, `rain_02`, `day_rain_02` absent), so kOvercastRain likely = overcast atmosphere + rain effects. [I]
- No `blackout` atmosphere under the obvious names [?].
- Mission/cinematic looks also exist (35 referenced paths), e.g. `atm_ibl_lgt_day_spidey_01`, `atm_ibl_lgt_otto`, `atm_night_biotech`, `atm_ibl_overcast_rhinomiles_02`, `atm_ibl_lgt_vn_scorpion_open_world`. Possible extra presets later.
- Cinematics switch looks with a "Cine Action: <name>.atmosphere" entry, so per-scene atmosphere switching is a normal engine operation. [V strings]

## Consequences for the design [I]
- **Early Morning, night rain, overcast and mission looks need no new assets:** they ship in the game. The job is to load one into an `AtmosphereManager` slot and make it win (S0/S2).
- **More than 3 environment mods:** an `.smpcmod` can only overwrite the slot's asset id, so two Night mods always collide. To keep many, Manhattan Skies must carry each look under **its own new asset path** (e.g. `atmosphere/manhattan_skies/<look>.atmosphere`, with the sky path inside it rewritten to a copied texture), and then load and activate it at runtime like Early Morning. Open [?]: whether Overstrike `.stage` can add assets that are not in the toc (BND's stages replace existing paths only); the format of the in-atmosphere sky reference (string length limits when rewriting); and what the unresolved shared ids (day/sunset material 95E22EB5BAD81B22, extra day textures) are for.
- Redistribution: third-party environment mods are the user's own library; a pack must be built locally from the user's installed mods, not shipped with them.

---

# Part 3: S0 in game, the Settings time-of-day option (2026-11-08)

Log: `recon/s0_menu_tod_day_sunset_night.log`. 3/3 natives OK, both hooks on, ran beside Endless Arena, QuantumBreakRenewed, TrueSwing, WebUpSelection, MainMenuSuits. [V]

## Lighting-mode index [V in game + disassembly]
| Index | Mode | Evidence |
|---|---|---|
| 0 | Day | Settings "Day"; also set twice during save load (callers exe+1faab8f, exe+1f74862) |
| 1 | ? (Night alternative, likely Night Rain [I]) | Settings "Night" picks 5 or 1 with 50/50 odds (below) |
| 2 | Sunset | Settings "Sunset" |
| 3 | ? (Day alternative, likely Overcast or Overcast Rain [I]) | dead branch of Settings "Day" |
| 4, 6 | ? (remaining: probably Overcast/Overcast Rain/Blackout) | |
| 5 | Night | Settings "Night" (this session) |
- The Settings option calls the setter directly (`exe+735d0b..735d19`: `[exe+6f90994] = index; setter(world+0xe10d0, index)`), no reload, instant change. The menu value at `[rbx+0x20]`: 0 = Day -> index 0, or 3 if `rand*2^-24 >= 6.66` (never true, so 3 is a disabled overcast/rain-day chance); 1 = Sunset -> 2; 2 = Night -> 5, or 1 when the RNG's top bit is 0 (50%).
- The setter's worker `exe+1f058e0` does not write the mode: it checks a vtable flag and `byte [obj+0x1179] != index`, then sends a 13-type sync message carrying the index byte (`exe+1985b40`/`1985f80`/`1985e00`). So the mode is applied asynchronously; calls belong on the game thread. [V disassembly, I meaning]

## Atmosphere slots are an overlay layer [V log, I meaning]
- Base time of day does **not** go through `AtmosphereAdjust`: no day_02/sunset_02/night_02 slot appeared. On each change only one local slot was released (-2, caller exe+1a16d8c) and one re-added (+2, exe+1a1b59e): a volume-triggered local atmosphere that has a per-time-of-day variant [I].
- Slot asset id = qword at `slot+8` (CRC64 path). Slot 0 = `atm_nyc_map_2017.atmosphere` (map screen: +1000 from exe+81c3a0 on open, -1000000 from exe+81a6cd on close). Others seen: a black-background atmosphere (`test/bmullen/black_bg_atmo.texture`, load screen), sunset-sky variants with `atm_ibl_sunset_02_bake`. Slots are **reused** (indexes recycled; first-sight bytes go stale) and `+0x70` is not the slot count.
- Weights seen: 2 (local volumes), 50 (exe+1669af1, during load), 1000 (map; exe+7140d6 during load). So a full-screen look like the map uses 1000; an Early Morning overlay would likely need a slot with a similar weight [I].

## S1 build (2026-11-08, not yet run)
`sky <n>` calls the setter on the game thread (lazy GameThreadTick hook, same function Endless Arena hooks: two MinHook copies on one function, worked for Uncontained [?]); `at:`/`atm` now print each slot's asset id; `peek <addr|exe+rva> [len]`.
Plan: `sky 1`, `sky 3`, `sky 4`, `sky 6` one at a time, user describes each (rain? overcast? blackout?), then back to `sky 5`.

---

# Part 4: S1 index sweep (2026-11-08)

Log: `recon/s1_sky_index_sweep.log`. `sky <n>` ran on the game thread every time (lazy GameThreadTick hook beside Endless Arena's: no problem seen). User's installed environment mods during the test: NYC Clear Day (day_02), Brand New Day Sunset (sunset_02), Brand New Day Rainy Night (night_02), NYC Rainy Day (overcast_02).

| Index | Atmosphere loaded (base slot, +2 from exe+1a1b59e) | What the user saw | Mode [I] |
|---|---|---|---|
| 0 | day_02 | Day (Settings) | kDay |
| 1 | `atm_ibl_lgt_night_02` | same as 5 (sub-second hitch on switch) | kNight or kNightRain |
| 2 | sunset_02 | Sunset (Settings) | kSunset |
| 3 | `atm_ibl_lgt_overcast_02` | grey overcast, **rain falling** | kOvercast or kOvercastRain |
| 4 | **`atm_ibl_lgt_morning_01_ow`** | **the opening mission's dawn** [V user] | Early Morning (a mode, not in the Settings list) |
| 5 | night_02 | Night (Settings) | kNight or kNightRain |
| 6 | `atm_ibl_lgt_overcast_02` | same as 3 (rain falling), no difference seen | kOvercast or kOvercastRain |

- **Correction to Part 3:** the base time of day **does** go through `AtmosphereAdjust`. The +2/-2 swap in one slot (release exe+1a16d8c, add exe+1a1b59e) is the base atmosphere; Part 3's ids were stale first-sight bytes from reused slots. [V]
- **Early Morning and rain need no assets and no slot work:** one setter call each. [V]
- Pairs 1/5 and 3/6 share an atmosphere; the difference must be outside it (rain/wetness/audio). Both rain mods the user runs replace the shared atmosphere plus two rain-only textures (8995A4D493702331, 89D5F9DA0876FDB1), so the rain seen in both 3 and 6 may come from the mods. A vanilla pass (those two mods off) settles which index of each pair is the rainy one. [?]
- No blackout mode among 0-6: the level data's 7 names map onto these 7 indexes with Early Morning taking a slot, so either `kBlackout` is index 1 or 6 (with the night/overcast atmosphere) or the name list is not the index order. [?]

---

# Part 5: S2 Settings tab (built 2026-11-08, not yet run)

`mod/src/menu.cpp`: a trimmed copy of Endless Arena's tab injection (SpidyVR's method). Tab id 94, setting numbers 0x400+, one heading and one TIME OF DAY row: DAY 0, EARLY MORNING 4, SUNSET 2, NIGHT 5, NIGHT (VARIANT) 1, OVERCAST 6, OVERCAST (VARIANT) 3. The row's value is read from the live mode field (so it follows the game's own option); a pick is applied by calling the setter inside the Settings callback (game thread, as the game's own option does) and shown as pending for up to 3 s until the mode field catches up. No ini: nothing to persist yet (the save keeps the mode).
Coexistence: Endless Arena hooks the same 10 functions about 1.7 s after start and refuses a target that already starts with a jmp. So when `EndlessArena.dll` is loaded, Manhattan Skies waits (up to 30 s) until all 10 targets carry a jmp, then hooks on top; each layer answers only for its own setting numbers and passes the rest down. Y (RESET ALL) does nothing on our tab (the game ignores tab ids above 10). [I, to test]
Risks: the chained hooks (two MinHook copies, ours over Endless Arena's), and changing the mode while a story mission controls lighting.

---

# Part 6: a real MODS sub-menu (2026-11-08, static; built, not yet run)

User asked for one MODS entry holding every mod's tab, and chose a real sub-menu over a picker row. Static findings [V disassembly]:
- First-page item select: callback `Select` (hash global `exe+6d8ed44`) with the item number -> `exe+7de150`, a switch on 0..0x1d; **item 5 = SETTINGS** (case calls `exe+7dd9f0`). Numbers above 0x1d are ignored. Callback hash globals `exe+6d8ed40..6d8ed90` are set at start-up from their names (initializers around `exe+975e0`): close, Select, NewManualSave, OverwriteManualSave, DeleteManualSave, ApplyWithheldChangedDisplaySettings, RevertPreAppliedChanges, UpdatePCDisplayOption, ExitPCDisplayMenu, EnterPCDisplayMenu, SetIsLastActiveTabGraphics, IsLastActiveTabGraphics, ResetPCDisplayOption, UpdateOption (`6d8ed74`), UpdateColorOption, **CloseOptions (`6d8ed7c`)**, adjustInGameWindowOption, ResetOption, ResetAllOptionsForCurrentOptionsMenu (`6d8ed88`), reloadOnCloseConfirm, reloadOnCloseCancel.
- `exe+7dd9f0` (SETTINGS) loops the config's tabs, builds each with `exe+807740` and pushes it (GFx vtable +0x78) **only if the build returned true**; then invokes Flash `OpenOptions`. `807740` has exactly these 2 callers. So making `807740` return false for the game's tabs while our screen opens yields an options screen with only the tabs we add.
- First page: `exe+7dd7c0` adds the items of the menu at config+0x128 (count +0x38 of it) with `exe+80a220`, returning to `exe+7dd8ae` (SpidyVR's hook point).
Design built: `ModMenu/` (shared by Endless Arena and Manhattan Skies; see ModMenu/README.md). Open [?]: CloseOptions firing on B out of the MODS screen; the screen's title (probably still SETTINGS); whether the first page is rebuilt while the options screen is open (that would end MODS mode early).

---

# Part 7: first run of the MODS menu (2026-11-08)

Logs and the exit-crash dump: `recon/s2_first_run/`.

## Findings [V]
- Host election worked: Manhattan Skies hosted (13 hooks, none chained), Endless Arena joined as a client (tab 95). Both registered, no errors.
- **MODS item did not appear** on the pause menu's first page. The page's 11 items (read from game memory: config `[exe+5d96df0]`, items at config+0x158, count +0x160): item numbers 0, 1, 8, 2, 29, 3, 4, 5, 7, 6, 6; **SETTINGS is number 5** as assumed.
- **Weather follows the lighting mode:** with the user's rain mods off, Night (5 and 1) and Overcast (3, 6) had rain, Day (0) none. The `toc` was compared with `toc.BAK` for the 36 assets the environment mods replace: after Install mods, `night_02` and `overcast_02` were original, `day_02`/`sunset_02` still modded. So the light night rain is not from a leftover mod. The Settings "Night" option picks index 1 or 5 at random (`exe+735ca8..`), 50/50, so 1/5 are probably clear/rainy night, but the user could not tell them apart; a world-state snapshot diff (`snap`/`snapdiff`, built) will find the variable.
- **Exit crash** (18:15:59, the user closed the game): `EXCEPTION_ACCESS_VIOLATION` executing address 0 on the game thread, from `exe+319b6ac` via `31a687a` and `1d2935d`: the game's UI shutdown (`exe+1d29290`) walks the Scaleform movie's live-value list and releases each value through its interface's vtable slot 2 (`call [rax+0x10]`); one value's slot was null. No frame of the fault stack is in our DLLs. The previous close (recorder only) was clean; the Nov 7 dumps were different (Endless Arena respawn frames).
- Hypothesis [I]: the MODS item add faulted inside the game's AddItem (the item number 0x3ff is outside every range our hooks answered, and the game's per-setting getters index records unchecked), my `__except` swallowed it, and a half-built Scaleform value stayed in the movie's list. Fix built: the hooks answer for the MODS item number; faults are captured (code, address, accessed address) and logged; one fault switches the MODS item off for the session.

---

# Part 8: the MODS menu in game (2026-11-08)

- With the `ObjectNumber` fix (hooks answer for the MODS item number 0x3ff): the MODS entry appears under SETTINGS (first page: 11 AddItem calls per build, 1 SETTINGS item found, 0 faults); A on it sends `Select 1023`, the options screen opens with exactly the two mod tabs (MANHATTAN SKIES, ENDLESS ARENA), 0 faults. [V user + log]
- TIME OF DAY row shows the live mode (Early Morning at the time) and picking SUNSET applied mode 2 at once from inside the callback (game thread). [V]
- **The game closes the pause menu on any lighting-mode change**, however it is triggered: a `sky 3` from the command file while the MODS screen was open did the same. So this is the engine, not the menu code. The new sky is in place when the player is back in the world. [V user]
- Not yet tested: the ENDLESS ARENA tab's rows/sections/presets under MODS, backing out of MODS and opening the game's own SETTINGS (should show only the game's tabs), and a clean game exit (the previous exit crashed with a half-built value, fixed by the same item-number change [I]).

---

# Part 9: header and "stay in the menu" (2026-11-08)

## The options-screen header [V disassembly of the movie + log]
- The options screen is the movie asset `9E935C2E8F353065` (class `Options`, root `SystemOverlay`; original in toc.BAK archive 19, the user's UI mod replaces it in `mods\mod153`). A second movie `8E06F92A9F6F4F47` is the title screen's. Both are `GFX` files readable with `tools/sigtools/abcdump.py`.
- **On the tab list** the banner is `_locStrings.PAUSE_OPTION_SETTINGS.<ML>`, a table built once by `Options.OnReady` (`LocalizeArray`) when the pause menu's movie is created, and re-read by `OnLobbyListDataComplete`. No native text lookup happens when the MODS screen opens (28 lookups logged, none for it), so it cannot be intercepted then. Changing it would mean writing the text field through Scaleform's object interface (SetMember is vtable slot 5 and Release slot 2, PushBack slot 15 in this build; the rest unmapped): not done.
- **Inside a tab** `OpenOptionsMenu` uses the selected tab's `menu_header`; the native tab builder (`exe+807740`, around `807b07..807b6a`) sets that member when the tab's heading tag hash (`tab+0x2c`) has text. We point it at the mod's name: confirmed in game (header reads MANHATTAN SKIES).

## Why the pause menu closes on a lighting change [V trace]
`uitrace` with a `sky 3` while the MODS screen was open: `ShowLoadingIcon`, `FadeOut`, **`CloseMenu` (from `exe+7dc06b`)**, `HideText`, `PingPOIs`, `ShowSavingIcon`, `ToggleInGameOverlays`, ..., `FadeIn`, `HideLoadingIcon`, `HideSavingIcon`: a fade-to-black transition (an autosave icon too) with every menu closed on purpose. `exe+7dc06b` is the return address of the pause menu's per-frame update calling Flash `CloseMenu`; the branch (`exe+7dc011..7dc042`) sends it when `world+0xe3cb0 - 0x17 <= 2`, i.e. while the world is in a transition state. Native state is unchanged by that call (Flash reports the close back through callbacks), so swallowing just that one Flash call keeps native and Flash agreeing the menu is open.

## Built, not yet run
- `modmenu::KeepMenuOpen(ms)`: for `ms` after a mod's menu change, while the MODS screen is open, the `CloseMenu` call from `exe+7dc06b` is swallowed (counted in the status line as "closes swallowed").
- Manhattan Skies row **APPLY CHANGES**: INSTANTLY (default; picks apply at once with KeepMenuOpen(6000)) or WHEN I LEAVE THE MENU (the deferred path, tested and working: pick shown in the row, sky changes as the MODS screen closes, which also closes the pause menu).
- Open risks for INSTANTLY [?]: HUD overlays toggled by the transition (`ToggleInGameOverlays`), the fade drawing over the menu, input state after the fade, the autosave icon (does a lighting change autosave?).

---

# Part 10: what the page can hold: atmospheres, live sun edits, sky overrides (2026-11-08)

## Every atmosphere in the game [V]
`recon/atmosphere_catalog.txt` (124 atmospheres: path, sun rotation/elevation/RGBA(intensity), radius, ambient, sky texture; tools `atmpaths_all.py` + `atmcatalog.py`; ids in `mod/src/atm_ids.inc`). They are referenced by zones, cutscenes and missions; families by sky texture: day (painted_2, day_sky_miles_cool2, day_skybox_hdr_02, cityhall, cgskies_0027), sunset (MainSunsetSky, sunset_4k nick, Grand Central, Norman penthouse), morning (EarlyMorning OW/Fisk/Peter's apartment), night (NightSky1, nightcloudy, Sky_Night_3, moon/no moon), overcast (overcast_skybox_hdr_04, 4k_overcast_hdr_4b), strange (Scorpion hallucination blue/yellow/green, 1700s sunset, black backgrounds).

## The lighting setup [V asset]
The global Open_World zone's contents (`98BAD9118100AE64`) names **5 base atmospheres** (day_02, overcast_02, sunset_02, night_02, morning_01_ow) and 5 world "levellights" (day, overcast, sunset, night, **blackout**) plus the sun-flare effect. So mode index 4 = `kBlackout` = morning atmosphere + blackout levellight (Early Morning); the rain modes reuse the night and overcast atmospheres; the levellight (baked world lighting) is a second asset a mode swaps. Atmosphere (sky, IBL, sun) and mode (levellight, rain) are independent layers: Day lighting with the Sunset atmosphere (made with the tools below) was visibly different from real Sunset (yellower, daylight-like streets). [V user]

## Live slot edits [V in game]
The slot keeps a copy of the asset's values: sun RGB at `slot+0x2a8`, intensity `+0x2b4`, rotation `+0x2b8`, elevation `+0x2bc`, ambient `+0x300`, time_of_day float `+0x74`. After writing them, `recompute` (`exe+162ff10(manager)`) makes the renderer use them. Tested: rotation/elevation change sky, shadows and building light; sun RGB red lit everything red; **ambient `+0x300` had no visible effect**; weight 2 on the map atmosphere (slot 0) over Day made no visible change. A color edit left a red patch in cached lighting until the time of day was cycled (reload refreshes).

## Loading any atmosphere [V in game]
`LoadAssetByHash` (`exe+1626380`) with the **AtmosphereManager** (`exe+60430c0`) as manager and the atmosphere's asset id, which is exactly what the game does at a mode change (caller `exe+162f92e`), puts it into a free pool slot (weight 0; the pool's generic insert at `exe+1625100..16251ff` writes the id at slot+8). `AtmosphereAdjust(slot, +2)` on it and `-2` on the old base swaps the active look, as the game does; the game's own next mode change reuses the loaded slot cleanly. `wp set` on slot+8 shows the id written by that insert, `loadtrace` logs `ld:` lines (manager, id, caller, args) for every load.

## Built, not yet run: the sky module (`mod/src/sky.cpp`)
Rows: TIME AND SKY (TIME OF DAY, SKY STYLE, SKY VARIANT) and SUN (SUN DIRECTION 24 steps, SUN HEIGHT, SUN COLOR 10 presets, SUN BRIGHTNESS 7 steps); settings in `Documents\Marvel's Spider-Man Remastered\ManhattanSkies\ManhattanSkies.ini`. 37 looks in 6 styles. The recorder's AtmosphereAdjust detour gets a filter: while an override is on, the game's own base activation (`+2` from `exe+1a1b59e`) is swallowed (the base slot is tracked for restoring AUTO); sun edits are written into the active slot after it has had ~0.5-0.9 s to load and the manager re-blends. Settings are re-applied after every world (re)load. ModMenu `kMaxChoices` raised to 32 for the direction list.
Debug commands: `skystate`, `look <style> <variant>`, `sun <dir> <height> <color> <bright>`, `skyreset`, `atmlist [name]`, `atmload`, `loadtrace`, `adj`, `slotaddr`, `findq`, `pokef`, `poke32`, `recompute`, `wp`.
Risks [?]: long choice lists in the game's Flash row UI (25 entries for direction), odd looks (atmospheres without a sky texture), freed base slots when a look is on for a long time, whether `ModeBaseAtmosphere` is right for the rain variants.

---

# Part 11: the sky module in game; the fog table (2026-11-08, night)

## The sky override works [V in game]
`look` from the command line and the menu both put another atmosphere on top of the lighting mode: Night + the "Cloudy Gold" sunset sky gave a hazy, dream-like sky with deep shadows (user: "looks REALLY cool, like an intentional artstyle"). All 37 curated looks loaded and activated in one pass (36 releases, every one back to 0 references, no give-ups, no faults, pool left with only the map, loading-background and base slots).

## Why the first browse failed after ~13 looks [V]
The pool holds about 16 atmospheres and `LoadAssetByHash` leaves one reference per load that the module never dropped, so the pool filled and every later load was refused (the tracker retried forever). Fix built and verified: `ReleaseSlot` (`recorder.cpp`) decrements the reference count (the header dword at `slot+4`, low 24 bits) and calls `exe+1624e80(manager, slot)`, which appends the slot to the manager's pending-destroy ring; the manager's per-frame update (`exe+1625530`, run for all 24 asset managers from the loop at `exe+1624970`) destroys a queued slot whose count is 0. Also: 3 tries then give up (`g_failed_id`).

## The fog table [V in game]
The slot holds far more than the asset's header block (2240 bytes). A full-slot float diff between the foggy Scorpion Blue and the clear Night showed a fog table: **distance bands at `+0x588..+0x594`** (night 100/900/1800/2400, Scorpion 606/638/660/800) and **six densities at `+0x598..+0x5ac`** (night 0.1, 0.05, 0.01, 0.002, 0.0005, 0; Scorpion 0.9, 0.4, 0.45, 0.6, 0.9, 0.77). Writing the night densities into the Scorpion slot and re-blending cleared the fog at once [V user]; restoring them brought it back. That explains the "Manhattan ends at Spidey's feet" looks. Tried and **not** the control: header `c` at `+0x2c8` (a live 240 -> 20 on Night, 0 -> 800, 5 on the Overcast low-sun look: no visible change), `f3/f4` at `+0x2dc/+0x2e0`. Fog colour: the orange of Scorpion Blue appears three times (`+0x2a8` sun, `+0x56c`, `+0x5b4`); changing `+0x5b4` gave a faint blue hue, `+0x56c` and the sun colour alone left most of the yellow, which must come from the sky texture and baked lighting.
Other slot regions that differ between atmospheres (not yet identified): `+0x388..+0x494`, `+0x534`, `+0x56c..+0x6a4` (90 differing floats in total), candidates for bloom, exposure and tone settings.

## Built, not yet run
- FOG DENSITY row (AUTO, NONE, 25%, 50%, 150%, 300%, 600%): scales the six densities of the active slot (originals kept per atmosphere).
- All 124 atmospheres offered: the 37 hand-labelled looks plus 87 more generated into `mod/src/looks_extra.inc` (`tools/gen_looks_extra.py`), labelled with the real file name, grouped by sky texture into DAY, SUNSET, NIGHT, MORE NIGHT, OVERCAST, INTERIOR (no or black sky); lists capped near 30 entries. Table of all of them: `docs/SKY_LOOKS.md`. 9 more atmospheres exist in the game (ALERT counts 133 in MSMR) but their names are not known.

---

# Part 12: finding the other controls with an automated screenshot probe (2026-11-08, late)

## Method [V]
`tools/fieldprobe.py` (dev tool): switch to Day lighting with no override, then for each float in the active atmosphere slot write a test value, `recompute`, screenshot only the game window (PIL `ImageGrab`, DPI-aware client rectangle), compare with a baseline taken after a recompute (rain and flickering lights move on their own: noise about 1-3 on the diff scale), restore, and record the overall, sky and city mean difference and the brightness change. Validation: sun rotation 60, elevation 43, fog density 24 (all strong), `c` 1.9 (noise). Needs the camera still and the window on top. Screenshots of strong hits are in `recon/probe/`; the numbers in `recon/fieldprobe_*.log|csv`. Limits: a small change (the clouds) scores low, so the thresholds hide it; a contact sheet of the screenshots is what found it.

## What the slot's floats do [V in game, by screenshot]
| Offset | Does | Evidence |
|---|---|---|
| `+0x484` | **exposure bias (stops)** | -4 near black, -2 brightness -68, -1 -32, +1 +29, +2 +53, +4 white-out: smooth and monotonic |
| `+0x668 / 66c / 670` | **RGB colour filter** on the whole image (R, G, B multipliers; 1,1,1 neutral; Scorpion Blue's own grade is 0.71, 0.53, 0.25, the "yellow" the user saw) | G x8 green cast, G x0.05 magenta cast |
| `+0x588 .. 594` | **fog distance bands** (4 floats) | x8 changes sky and city strongly |
| `+0x598 .. 5ac` | fog density, six values | Part 11 |
| `+0x5c0` | **haze**: a white veil that grows with distance | 0.25, 0.5, 1 take the far city to a white-out |
| `+0x81c` | **cloud layer on/off**: any positive value (0.5, 2, 16 look identical) turns clouds on | clear Day sky -> fluffy clouds; probe score only 1.9 |
| `+0x2a8..2bc` | sun | Part 10 |
Not useful or not safe: `+0x47c`, `+0x480` (black or white screen, probably tone-map or adaptation limits: left alone), `+0x494` (small), `+0x1a8..1bc` (six floats; the screenshots look like the baseline, the score was birds and flicker), `+0x300` ambient, `+0x2c8` header `c`, `+0x6a8..0x8c0` (no effect apart from `+0x81c`). Many "weak" scores in `+0x2d0..0x6a4` are within what animation noise produces and were not pursued.

## Built, not yet run
Rows IMAGE: EXPOSURE (AUTO, -3..+3 stops, added to the sky's own), COLOR FILTER (11 presets, absolute multipliers), CLOUDS (AUTO/ON/OFF); and in SUN AND FOG: FOG DISTANCE (25%..600%), HAZE (NONE..THICK, absolute). Originals are read per atmosphere before the first edit and restored when a row goes back to AUTO. `skyset <setting> <value>` command for testing.

---

# Part 13: first look at "other light sources" (windows, streetlights) (2026-11-09)

Goal (user): can the mod also change building lights, streetlights and similar? Result of a first pass: **no simple control found yet**; what is ruled out and what is known.

## Ruled out [V]
- **The per-tile day/night overlay zones are not lights.** Level trigger entries 405 and 407 (`LightingModeOverlay`: day set kDay/kSunset/kOvercast/kOvercastRain; night set kNight/kNightRain/kBlackout) load `<tile>_ground_overlay_act1_2_day.zone` / `_night.zone` (701 tiles each). A night zone is ~1 KB and holds only Pedestrians, SwingVolumes, LedgePaths, Breakables and Static Sound Emitters; a day zone is an empty 136-byte shell. They bring ambience and gameplay data, not lights.
- **The `levellight` assets are headers.** `levels/I20_City/openArea/Open_World/LevelLight/atmosphere_lgt_levellight_{day,overcast,sunset,night,blackout}.levellight` are 164-byte, two-section files; the real baked data is elsewhere (tile zones reference light-grid and `zonelightbin` data).
- **The atmosphere slot's `time_of_day` float (`+0x74`, 80000 Day / 50000 Night) does not light windows.** In Night mode, 50000, 80000, 20000, 4000 and 640000 left the lit windows unchanged (screenshots).
- **The world lighting block has no simple float for it.** 118 plausible floats in `world+0xe1000..0xe4000` (the lighting object at `+0xe10d0`, the mode at `+0xe2040`), each x8 in Night mode, scored 6-9 on the diff scale in a scene whose noise was 5 (real controls scored 24-60). `recon/fieldprobe_world.log`.

## Known
- Streetlights are static models with a companion glow model (`jn_tall_street_light.model` + `jn_tall_street_light_light.model` referenced from a tile zone); whether the glow is switched by a shader parameter or a toggle is unknown.
- The game's material system has an **`Env:` node with `KeyLightIntensity`, `TimeOfDay`, `VFXLightMultiplier`** (also `SunLightScale`, `VolumetricFogSunLightScale`; enum names `kWeatherEnvTimeOfDay`, `kWeatherKeyLightIntensity`, `kWeatherVFXLightMultiplier`, `kWeatherVolumetricFogSunLightScale` in the exe) and **emissive blocker volumes** ("block material emissive output", `m_EmissiveBlockerBits`, `m_EmissiveFadeDistance`). Windows, neon and signs are emissive materials; no archive asset mentions the `kWeather...` names, so they are engine-internal.
- The layers (screenshots, Night/Day mode with the other time's atmosphere on top): the sky, sun, exposure and fog follow the **atmosphere**; the city's lighting follows the **lighting mode**. Lamps and windows belong to the mode layer.

## Next leads (not started)
1. Find the runtime storage of the `Env:` values (the weather environment parameters): xrefs of the `kWeather...` enum strings in the exe, then read the array while switching modes (the snapshot/diff and probe tools work on any address).
2. `LightIntensityAction` / `LightIntensity` (Prius actions that scale lights by tag) and `m_DirectLightScale`: whether a global scale exists.
3. Spawning or toggling an emissive blocker volume to switch emissives off over the whole city.

## What the "NYC Warm Night" mod shows (2026-11-09) [V, file diff against toc.BAK]
Replaces 5 assets: the night atmosphere (`atm_ibl_lgt_night_02`, identical size, **88 bytes changed = 32 values**), the 8k night sky texture (re-encoded, 16 MB against 8 MB) and three baked textures the atmosphere references at file offsets `0x11c` (`BE81B2DDE1B33296`, 8.4 MB), `0x344` (`9EB0AB0DB3FFFF8E`, 131 KB) and `0x44c` (`808D0EA05F873118`, 24.7 KB): the image-based lighting (reflected and bounced light) and a small lookup texture. The 32 changed atmosphere values (file offset; the slot offset is file + 0x174 for the sun block, + 0x1ac for the fog block, + 0x1d4 for the tail): sun colour (0.62, 0.76, 1.0) -> (0.95, 0.95, 0.95), sun intensity 0.005 -> 0.003, sun rotation/elevation 0/40 -> 247/90, header `c` 240 -> 255, four fog densities, **fog colour (0.44, 0.71, 1.0, haze 0.05) -> (0.5, 0.5, 0.45, haze 0.01)**, a second colour triple (0.456, 0.565, 0.638) -> (0.456, 0.455, 0.464) at slot `+0x69c`, and a few tone numbers. So the warmth is atmosphere-level (sun, fog, haze, tint) plus baked IBL: it **does not change windows, lamps or other emissives**, and therefore does not show how they are switched. The other mods in the library that touch the world's look (Cold Night, Rainy Day, Emerald Trees, Contrail, A Dark Rainy Night) are the same kind of atmosphere or material replacements. Fog colour at slot `+0x5b4` (and the haze at `+0x5c0`) are therefore real and already partly exposed (haze row); a FOG COLOR row is possible but its effect was faint in the test.

## Part 14. Stage-0 gate for window and street lights (2026-11-09): not practical
- Diffed the exe's `.data` (34.3 MB) between two Day and two Night dumps: 59,817 words stable inside each mode and different between them, 39,695 float-like, 763 clusters (`tools/datadiff.py`, `recon/datadiff_day_night.csv`).
- The only exact 0 <-> 1 switch is `exe+728390c` (Day 0, Night 0.999897). Written to 0 in Night (write verified by readback, held 2.5 s): lit windows and street lamps unchanged (`recon/probe/S0_base.png` vs `S2_later.png`, only Spider-Man moved). Not the lights switch. `tools/switchtest.py` repeats the test for any address.
- Stop rule applied: no writable value found that changes windows or lamps. Remaining candidates (about 1,500 ratio/zero pairs) would need a blind automated sweep, with crash risk from writing unknown globals.
