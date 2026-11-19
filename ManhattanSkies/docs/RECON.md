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

## Part 15. Rain experiment (2026-11-09): dropped
- The three rain mods (NYC Rainy Day, Brand New Day Rainy Night, A Dark Rainy Night) replace only atmosphere and texture assets. Diffing their atmospheres against vanilla (`tools/atmdiff.py`, against `toc.BAK`) shows only look values (sun, fog, ambient, IBL); a non-rain mod (NYC Warm Night) changes almost the same fields. So no rain value lives in the atmosphere asset; with the mods off the game already rains in Night and Overcast and not in Day, so rain follows the lighting mode.
- `.data` dumps in Day, Overcast, Night, Sunset and Early Morning (`tools/raincapture.py`, `tools/raindiff.py`): 7 words are 0 in Day and nonzero in Night and Overcast. Two clusters look like rain levels (`exe+7a2e8ec/8f0/8f8` and `exe+5d6a94c/950`: Overcast 0.8 / Night 0.4, Day, Sunset and Early Morning 0).
- A hardware write watch on `exe+7a2e8ec` shows the block is cleared every frame by the game's memset (`exe+363e6e9`, from `exe+18968a7`) and refilled by a copy (`exe+363e27e`, from `exe+1886f23`, `1bbe951`, `18963bd`), so a single write is overwritten within a frame. Holding all five values from a 1 ms thread for 15 s in Day (`holdf` command) made no rain or wet surfaces appear (only a texture flicker). Conclusion: they are outputs, not the controls. Not pursued further: rain in Day would look wrong without clouds anyway.
- Image-controls probe (`tools/imgprobe.py`, 2026-11-09 night): vignette, saturation, contrast, highlight and sharpness metrics over slot `+0x2d0..+0x6a8`. Writing 1.0 into the zero floats around `+0x320..+0x330` left the image distorted (every later test showed the same shifts) and the game crashed a few writes later. Abandoned: the graphics settings already offer exposure and similar controls, so the maintainer did not want more image rows.

## Part 16. The dynamic cycle: what the game let us do (2026-11-09/10)
- **Atmosphere weights do not blend** [V]: moving weight from one atmosphere to another in 50 steps (`adj`) made the sky swap at once at some point, not fade. So skies cannot be cross-faded through the pool. What can be blended are the slot's own values (sun color and intensity, fog, haze, exposure, filter, fog color): the cycle loads the next sky quietly (weight 0), reads its values and blends the current slot toward them over the last seconds before a switch (`ReadAtm`, `Cycle`, `ApplySun` in `sky.cpp`).
- **The lighting transition's fade** [V]: the Flash calls `FadeOut` (arg 0 s) and `FadeIn` (arg 0.2 s double) come from the transition; the black is mostly the transition itself (about 0.6 s). `modmenu::SuppressFade` swallows both for a few seconds: the transition still completes, with no black screen (a pop of sky and lights only).
- **The transition is a mini load** [V, user]: a lighting-mode change always stops Spider-Man's momentum in mid-air, even between modes with identical sky and lights (3 to 6, 5 to 1), so it is the transition process, not what it loads. `uitrace` during a switch: `ShowLoadingIcon`, `FadeOut`, `ClearPOI`/`UpdatePOI`/`AddPOI` (the map markers are rebuilt), `HideText`, `HideLoadingIcon`, `FadeIn`. The setter `exe+1f7f9e0` forwards to `exe+1f058e0`; the state machine is `exe+1f7fa00` (state byte at `+0x7cc`). Not reverse engineered further. Mitigation: the cycle samples Spider-Man's speed (HeroSystem `exe+5d9dd00` handle at `+0x1c`, `GetActor`, transform `+0x30`) and waits up to 45 s of game time for him to drop below 3 m/s before switching.
- **Pause detection** [V]: the world time scale (`exe+621b890+0x18`) does not change in the pause menu. Three timers in `.data` (`exe+5d6a028`, `5d6a168`, `5d6a2a8`; found by diffing dumps taken running, paused and running again, `tools/pausediff.py`) advance every frame and stand still while paused; the cycle's clock stops when none moved for 90 ms.
- Random skies: `kPickNight/Sunset/Day/Overcast` in `sky.cpp` (user-chosen lists, the game's own sky is one more choice in each). The user compared the near-similar ones by eye and found them all distinct.

## Part 17. Loading an atmosphere from a loose file at run time (2026-11-13): the core works
Goal: several atmosphere `.smpcmod`s usable at once (ROADMAP 4b). Overstrike only redirects *existing* asset ids and writes one mod per id, so the runtime has to supply the data itself.
- **Mod format** [V, Overstrike source]: an `.smpcmod` is a zip of `ModFiles/<span>_<ASSETID>` entries (deflate-compressed in the zip; the data is raw DAT1 bytes). `Install mods` writes each mod into a new raw archive file `asset_archive\mods\mod<N>` and rewrites the `toc` (kept as `toc.BAK`, restored at each install) so the replaced asset's entry points at (archive index, offset, size) in that file. The `.smpcmod` installer cannot add new asset ids (other installers, for suits, do).
- **The toc in memory** [V]: a 21 MB DAT1 buffer in private heap memory (the address changes every launch; `toclocate` finds it by its DAT1 header in about a second). Sections: ids (8 bytes each, tag 506d7b8a), offsets (8 bytes: archive index u32, offset u32; tag dcd720b5), sizes (12 bytes: 1, size u32, index u32; tag 65bcf461), archive names (72 bytes each: 8 bytes of metadata then a 64-byte name; tag 398abff0; 210 archives). Entry `i` of each table describes the asset at id index `i`. It is plain read-write memory, so entries can be changed (`tools/tocinfo.py` prints the same entries from the file; the table changes with every Overstrike install, never hard-code an index).
- **Reads** [V, `iotrace`]: the game imports `ReadFile` and reads assets with single overlapped `ReadFile` calls from one call site (`exe+1d1bfb8` for raw mod archives). A raw asset is read at exactly (its archive file, offset, size) from the toc; a sky then triggers reads of the textures it references. The game's own archives (`g00sNNN`, tens of GB, memory-mapped) are compressed in blocks and read in many pieces.
- **Carrier method** [V]: pick an existing asset the game does not need (the carrier). Point its toc entry at the archive file of a raw mod archive the game uses (here `mods\mod166`, archive index 209) with a made-up offset (`0x7e000000`, past the end of the file) and the size of our own file. Hook `ReadFile`; when the game asks for that (file, offset, length), set the OVERLAPPED offset to 0 and read from our own handle instead (same OVERLAPPED, so completion is the game's own). Test: the unused sky `atm_ibl_lgt_peterscook` was loaded through `atmload` and the pool slot then held the NYC Rainy Day overcast values (sun 1, 1, 1 at 0.01, rotation 180, elevation 90), not Peter's own. Commands: `toclocate`, `tocfind`, `iotrace`, `carrier <id> <archive index> <file>`, `uncarrier`.
- **Still to do for the feature**: (1) textures: a mod's sky and baked-lighting textures use the ids of the textures they replace, so two mods collide; each needs its own carrier texture id, with the id references inside the atmosphere bytes rewritten (offsets such as `0x11c`, `0x344`, `0x44c`); (2) choosing carrier assets that are really never used (a wrong choice breaks whatever uses them), and hiding them in the SKY rows; (3) reading the `.smpcmod` zip at run time (deflate: the game folder ships `libdeflate.dll`); (4) needing a raw mod archive that the game has open (at least one Overstrike mod installed; this install has about 185); (5) UI: one row listing the mods found in a folder; (6) timing: carriers must be set up before the game first loads the carrier asset, and restored on exit.

### Part 17b: brand-new assets work (2026-11-13), no carriers needed
- **How the game finds an asset** [V, read-watch on the toc object + disassembly]: `exe+16d7990(tocview, id, group, kind)` binary-searches ONE span of the id table and returns a pointer to the matching 12-byte entry of the sizes table. `tocview` is the toc object + 8 (`+0` spans pointer, `+8` ids pointer, `+0x20` sizes pointer); the span index is `group*8 + kind`, taken from the spans table (first index u32, count u32). The caller `exe+16dd090` tries the current group, then another group, then group 0, always with the same kind. The toc object (found as the triple "DAT1 base, spans, ids" pointers) is: `+0x00` base, `+0x08` spans, `+0x10` ids, `+0x18` key assets, `+0x28` sizes, `+0x30` offsets, `+0x38` archives, `+0x40` key-asset count, `+0x44` archive count (210), `+0x48` asset count (u64).
- **All atmospheres and the textures they use live in span 0** (group 0, kind 0; about 214,000 entries). A new asset in span 255 is never searched (first try: the game created a pool slot for the new id but never read the file).
- **The method that works** [V]: allocate bigger copies of the ids, offsets and sizes tables; keep the old tables whole at the start (every old asset keeps its index), then copy span 0's entries behind them and append the new assets (ids above span 0's last id, ascending); write the three table pointers and the asset count into the toc object; finally point span 0 at the copy with one 8-byte store (`spans[0] = {first = old count, count = old span count + added}`). The ReadFile redirect from Part 17 serves the reads. Test: the Raimi (2002) Morning Atmosphere (Sunrise) mod, split offline (`tools/buildslot2.py`: new ids for the atmosphere and each texture it ships, references inside the atmosphere rewritten) loaded through `atmload` under the new id: the game read the atmosphere (2,770,688 bytes) and its three textures (16,777,344, 131,200 and 24,704 bytes) from loose files, and swapping the pool slot in showed the warm, orange sunrise look. Nothing of the game was replaced, and nothing is hijacked.
- Commands: `toclocate`, `addasset <id> <archive index> <file>`, `tocptrs`, `wp setr` (read/write watch). The scan for the toc object takes about 17 s the first time; the first call blocks the command loop meanwhile.
- **Still to build**: unzip `.smpcmod` (deflate) at run time; assign ids and rewrite the texture references in C++ (what `buildslot2.py` does); a folder of mods and a row to pick them; make the look system accept dynamic ids; an archive file to borrow (today an Overstrike raw mod archive such as `mods\mod166`, archive index 209, must exist and be open: a player with no mods needs a placeholder); the look needs the right lighting mode (a sunrise sky under night lighting is blown out); restore on exit.

## Part 17c. `.modular` atmosphere mods and the cycle's atmosphere pool (2026-11-13, built, not yet played)

- **Format** [V, read from `BND Early Morning Atmos.modular`]: an outer zip with `info.json` (`layout`: `["module", "<name>", [["", "<option label>", "modules/<module>/<n>_<x>.stage"], ...]]`) and one `.stage` zip per option (deflate inside the outer deflate). A stage's entries are game paths prefixed by the span: `0/atmosphere/atm_ibl_lighting/atm_ibl_lgt_sunset_02.atmosphere`, plus `info.json`. An entry's asset id is the CRC64 of the path without the span prefix (ALERT's `dat1lib/crc64.py`: reflected, polynomial and start value `C96C5795D7870F42`, lower case, slashes collapsed, then `>> 2 | 1<<63`); `atmpacks.cpp` `PathId` reproduces the known ids of the Day, Early Morning and Sunset atmospheres [V].
- **Use:** each stage is unpacked once into `cache\stage_<hash>.zip` and then handled like an `.smpcmod` (its entries keyed by id): one choice per option, named `<FILE> - <OPTION>`. The sample has three options (Sunset, Early Morning, Day), each also shipping the sun-flare texture (skipped, shared) and, for Early Morning, two more atmospheres that are not among the five.
- **Cycle pool:** `PickLook` (sky.cpp) draws from the game's own sky for the phase, the hand-picked sky list and, with CYCLE ATMOSPHERES on, the time of day's atmosphere mods; an ATMOSPHERE chosen in the row wins. Early Morning has no hand-picked list, so with the toggle on it draws among its own sky and the mods.
- **Not built:** a placeholder archive for players with no Overstrike asset mod (the redirect only needs an archive *name* and a made-up offset, but a vanilla archive may be read through a mapped view rather than ReadFile, so borrowing one is unsafe untested); swapping shared helper assets while a mod is selected.

## Part 17d. Black sky after a cycle switch to an atmosphere mod (2026-11-13, fixed, tested)

- **Symptom:** the dynamic cycle switched to a mod's atmosphere (Aurora Over Manhattan at night, a BND Early Morning option at dawn): the world lighting changed, the city stayed lit, the sky was black.
- **Log evidence [V]:** the cycle loads the next phase's atmosphere quietly (weight 0) about 12 s before the switch, to read its fog/haze values for the blend. For a mod's atmosphere only the atmosphere file was read from our redirect; none of its skybox textures ever were, not at the quiet load and not after the switch gave the slot weight 2. A menu pick (load and switch on within 2 ms) was followed by texture reads about 0.7 s later. Toggling the active slot's weight off and on again (`adj`) requested nothing either.
- **Cause [I, consistent with all of the above and the fix]:** the game requests an atmosphere's textures when the atmosphere finishes loading with a weight above 0, not when a weight is raised later. A quiet load finishes at 0, so the skybox was never requested and the sky renderer drew black.
- **Fix:** `ApplyLook` (sky.cpp): when the wanted look is an atmosphere mod that the cycle loaded quietly (`g_cyc_hold`), release that load and load it again the way a menu pick does; the game's own base sky shows for about a third of a second inside the lighting transition. The early load is still used for the fog/haze blend. Tested by the maintainer: textures load.

## Part 17e. Which archive the new assets borrow, and the placeholder idea (2026-11-14)

- **What is needed [V]:** the made-up toc entries must name an archive the game reads as *plain* data. In the toc's archive list (`tools/archives.py`) the vanilla archives (`g00sNNN`) are compressed (`DSAR`) and have a chunk map (entry field `chunkmap` nonzero); Overstrike's `mods\modN` archives come in two kinds: 74 with chunk map 0 (raw) and 42 with a chunk map in the maintainer's install. Only chunk map 0 archives are borrowed (`PickRawArchive` checks it).
- **Which mods make one [I]:** `.smpcmod` mods that replace game files give raw archives (the one in use, mod166, held 5 assets of an atmosphere `.smpcmod`). Which other formats (`.modular`, `.stage`, `.suit`) give raw or chunk-mapped archives was not worked out.
- **Placeholder tried and dropped:** rewriting an unused language archive entry (`a00s035.fr` ...) in memory into a raw archive of ours. Not shipped: whether the game opens archive files lazily by that name was never tested, and a player can have every language archive installed. Decision (maintainer): require that the player has at least one raw Overstrike archive, and say so in the mod: the ATMOSPHERE row shows "NEEDS AN OVERSTRIKE MOD" with an explanation when mods are in the folder and no archive exists.
- **Companion mod idea (not built):** a tiny `.smpcmod` shipped with the script would create the archive, but it must replace a real game asset; shipping the original bytes is redistribution and shipping our own bytes risks breaking an asset that is used.

## Part 17f. Files an atmosphere mod ships that the loader does not use (2026-11-14)

`tools/skipped.py` lists, per `.smpcmod`, the files that are neither one of the five base atmospheres nor a texture the atmosphere references. In the maintainer's folder: `A6F92F8761456EE3` (349,632 B, span 0, a 35mm star flare texture) and `95E22EB5BAD81B22` (371,348 B, span 2) in 10 mods; `89D5F9DA0876FDB1` (65,664 B, span 2) in 5 mods (the MM Overcast trio and the two rainy mods); `B0D1CA3D73401F93` (132 B) in 2. 65,664 is exactly 64x64x16 plus a 128-byte header: probably a color-grading lookup texture, which would explain a pink cast on the MM Overcast (Heavy Snow) sky (reported, not confirmed). They stay the game's own while a mod is on. Possible fix, not built: point the game's toc entry for that id at the mod's file while the mod is selected (the carrier method, `toc::AddCarrier`/`RestoreCarriers`), and put it back afterwards; the asset may need to be reloaded to take effect.

---

# Fable session, 2026-11-15 (answers to `docs/FABLE_REQUESTS.md`, MSK-*)

Everything in Parts 18-22 was **read from the exe and from two atmosphere assets on disk; nothing was run in game**. `[V]` here means "read in the
code/data" (static), `[I]` inference, `[?]` unknown. Live checks are listed at the end of each part. Tool: `tools/sigtools/fx.py`
(`fx.py ddl <Type>`, `fx.py ddlflat AtmosphereDef`, `fx.py d <rva>`); shared facts are in `docs/GAME_INTERNALS.md` ("Fable session 2026-11-15").

## Part 18. The lighting modes by name, and why a switch is a "mini-load" (MSK-A3, MSK-C2, MSK-E1 partly)

**Names [V]** (enum `LightingModeGame`, descriptor `exe+393c090`): **0 kDay, 1 kNight, 2 kSunset, 3 kOvercast, 4 kBlackout, 5 kNightRain,
6 kOvercastRain**. This corrects Part 4 (1 is the plain night, 5 is the rain night, 6 the rain overcast). A second enum, `LightingConditions`
{kDay, kOvercast, kSunset, kNight, kBlackout}, is what assets are tagged with (`LightingConditionAtmosphere{Atmosphere, LightingCondition}`,
`LightingConditionLL{File, LightingCondition}` = the levellight per condition, `LightingConditionBitSet{Day, Overcast, Sunset, Night, Blackout}`):
**the two rain modes have no condition of their own**; they reuse the Night/Overcast assets and differ by the atmosphere they activate [I].

**The setter [V]**: `exe+1f7f9e0(world+0xe10d0, mode)` = `X = exe+1f03570(arg)`; `exe+1f058e0(X, mode)`: if `X->vtable[+0x30]()` and
`mode != (int8)X[+0x1179]`, it writes **synced state field 0xd** (1 byte) of the sync object at `X+0x18` (`exe+1985b40` begin, `exe+1985f80(buf, 0xd, &mode, 1)`,
`exe+1985e00` commit). The mode is a replicated state variable; the transition is the reaction to the state change, not the setter itself.

**Callers of the setter [V]**: `exe+735d19` (`GUI::MenuRTTUpdater` slot 18: the Settings option), `exe+bd867f` (`SetLightingModeAction` node, execute `exe+bd8640`),
`exe+1f7485d` and `exe+1f7490f` (inside the game tick `exe+1f745f0`: the load-time apply), `exe+1faab8a` (fn `exe+1faa9e0`), `exe+1fae8fc` (fn `exe+1fae8b0`).

**What makes it a mini-load [V]**: the transition sends `LightingModeTransitionStartedEvent` (id `0x4d116e75`, sender `exe+1f7eb20`; payload
`+0x138` from-mode, `+0x13c` to-mode) and later `LightingModeTransitionFinishedEvent` (`0x8dd5dcd8`, `exe+1f7e9b0`,
"LightingZoneManager::DispatchTransitionFinished"). Four systems handle "Started":

| handler | system | what |
|---|---|---|
| **`exe+710970`** | **`DynamicLoadScreenSystem`** | **starts a load screen of type `kLightingTransition`** (the enum is {kInitialLoad, kCheckpointWarp, kLightingTransition}) |
| `exe+20550e0` | `SaveFile` (vtable `+0xa8`) | save bookkeeping |
| `exe+de64e0` | `ParkedCarSystem` | re-keys parked cars |
| `exe+e1efb0` | `VehicleSystem` | re-keys traffic |

`DynamicLoadScreenSystem::OnLightingTransitionStarted` (`exe+710970`), read line by line:

```
if (this[+0x18]) return;                                   // busy/disabled
cfg = this[+0x228]  (DynamicLoadScreenConfig, asset "configs/system/system_DynamicLoadScreen.config")
for e in cfg.LightingModeMissionScreens  (ptr cfg+0x138, count cfg+0x140; {StartMissionOrObjective, EndMissionOrObjective, SkipLoadScreen +0x28, WantsDoppelganger +0x29, Screen}):
    if the mission window matches (mission system exe+7c3ef70) -> override = e;  if e.SkipLoadScreen -> return
for e in cfg.LightingModeTransitionScreens (ptr cfg+0x128, count cfg+0x130, 0x48 bytes each; {StartingLightingModes[], EndingLightingModes[], SkipLoadScreen +0x28, WantsDoppelganger +0x29, Screen}):
    if from in e.Starting and to in e.Ending -> candidate (random pick among matches);   if picked.SkipLoadScreen -> return
this[+0x28] = 2 (kLightingTransition); this[+0x4d] = WantsDoppelganger
if (this[+0x2c]) exe+713b90(this, 0)                       // sends FastTravelNoVignetteEvent, touches the atmosphere manager and the zone manager
exe+713720(this, 0.0)                                      // "Dynamic Load Screen PreWarpFadeOut": the fade, the icon, the warp-style freeze
```

So the loading icon, the fade, the frozen hero and (through the fast-travel-style "no vignette" path, which the crime and mission systems treat as a
world transition [I]) the ended crime all come from **this one handler**, and the game already has a rule for seamless changes: **`SkipLoadScreen`**.

**Recipes for a seamless switch [I: built from the code above, not run]**, cheapest first:

1. **Hook `exe+710970` and return at once while our own switch is in flight** (exactly what the function does itself for a `SkipLoadScreen` entry). No
   instance pointer is needed.
2. **Patch the parsed config in memory**: `cfg = [DynamicLoadScreenSystem + 0x228]`; for each of the `[cfg+0x130]` entries at `[cfg+0x128] + i*0x48` set byte `+0x28` to 1.
   A pair with no entry at all still gets the load screen, so the hook is safer.
3. **Data-only**: ship an edited `configs/system/system_DynamicLoadScreen.config` (`LightingModeTransitionScreens[*].SkipLoadScreen = true`) as a `.stage`.

Unknown until tried: whether the levellight/zone swap pops in visibly without the fade (the game uses skip for scripted moments, so it must be acceptable for
some pairs), and whether the vehicle and parked-car re-key (headlights) still hitches. The hang of Part 5 (setter called 3 s after a load) is consistent
with a second load screen requested while `kInitialLoad` was still up [I]: with the hook that request never happens.

*Live check:* `sky <mode>` with the hook on while swinging and during a crime; watch for `DynamicLoadScreen*Event`s (none expected) and `LightingModeTransitionFinishedEvent`.

## Part 19. The atmosphere asset, fully named (`AtmosphereDef`), and how it maps into the 0x8c0 slot (MSK-A2, A1, A6, A7, A8, A10, A11, B6)

**The schema [V]**: the exe holds the reflection descriptor of the authoring struct `AtmosphereDef` (0xea8 bytes, descriptor `exe+6a98aa0`): **299 named
fields**. Print it with `python tools/sigtools/fx.py ddlflat AtmosphereDef`. Groups, in order: `SkySettings` (SkyObjectsScale, SkyObjects[], SkyBoxCubeMap),
`AssetSwap`, **`EnvLighting`** (KeyLightColor, KeyLightIntensity, KeyLightAzimuth, KeyLightElevation, **SunDiskOffsetAzimuth, SunDiskOffsetElevation**,
ShadowDrawDist, CsmLodCount, ImpostorCaching, TimeOfDay, **SunDiameter**, FillLightCubeMap, LightGridIntensity, LightGridSkyMultiplier, LightGridAORadius,
LightGridModConfig, KeylightMaterial, KeylightMaterialScale, CasterRangeLower, CasterRangeUpper, VFXLightMultiplier, **AmbientFillColor, AmbientFillIntensity**),
`ToneMapSettings` (LuminanceKeyValue, ExposureBias, AdaptationStrength/Stops/Anchor/Range/Bias, AutoAdjustTime), `BloomSettings` (Intensity, Threshold,
GaussianStrength0-4, BloomDirtiness(+Asset), LensFlare*: LumMin/Max/Intensity, Ghosts, FallOff, Intensity, Distortion, HaloRadius, HaloIntensity, GhostDispersal),
`ColorCorrectionSettings` (**SourcePath**: the colour-grade lookup), `VignetteSettings` (Intensity, CenterClear), `ChromaticAbSettings`, **`FogSettings`**
(ColorMapAsset, ColorMapDepthRange/Scale, Color, LuminanceScale, KeyLightInfluence, NearRadius, Height0..4 with Opacity0..4, RemapStrength, RemapKeyOpacity),
**`VolumetricFogSettings`** (Color, Opacity, BottomHeight, TopHeight, NearDistance, FarDistance, SunLightScale, LightsScale, GIScale, NoiseVSpeed, NoiseSpeed, NoiseScale),
`MotionBlurSettings`, `DepthOfFieldSettings`, `FilmGrainSettings`, `ScatteringSettings` (RayleighDensity, MieDensity, MieAlbedo), `LightShaftsSettings`
(Range, Weight, ConeAngle, SortDistance, Color), `ClipSettings`, `ProjectCustomData` (ValueA..D), `AOSettings` (..., SunlightFactor, LocalLightFactor,
AmbientShadowDrawDist, AmbientShadowIntensity), `AASettings`, `SSRSettings`, **`Weather`** (WeatherConstant; `WetSurfaces`: Porosity/Streak/Splash textures, **Wetness**,
GlossLimit, VFXSplashRange, VFXSplashRate; **`Particles`**: Texture, NearFadeDistance, MaxDrawDistance, WindInfluence, Turbulence, **Density, Speed, Size, Alpha**, Color,
ExposureDuration, MotionStretchMax(+Far), RainBloomInfluence; **`Wind`**: Intensity, Azimuth, gust Intensity/Frequency/Duration min-max), `Ocean` (ReflectionMap, Foam*,
WaveAmplitude, ChopScale, WindSpeed, TimeScale, Darkening, Depth, Color), **`VolumetricClouds`** (ray marching; modeling: CloudsLayerStart, LayerThickness, CloudSize,
CloudDensity, CloudCoverageModifier, CloudTypeModifier, WeatherMap...; wind: WindAzimuth, WindIntensity, rotation; lighting: CustomColor, Contrast, Precipitation,
SilverIntensity, CloudBrightness, FogBlendFactor, BackgroundBlendCubemap; shadow: EnabledShadow ...).

This answers several "does it exist" questions at once: **rain, wet streets and wind are atmosphere data** (`Weather.Particles`, `Weather.WetSurfaces.Wetness`, `Weather.Wind`:
that is why atmosphere mods change the rain and why it cannot be told apart from the mode), there is a **procedural sun disk** with its own size and an offset from the key light
(`SunDiameter`, `SunDiskOffsetAzimuth/Elevation`), the **colour grade is a file reference per atmosphere** (`ColorCorrectionSettings.SourcePath`), the fog is a **5-point
height/opacity curve plus a separate volumetric (froxel) fog**, light shafts, SSR, AO and film grain are per atmosphere, the ocean look follows the atmosphere, and the
atmosphere has an **`AmbientFill` colour and intensity**. `RainIntensityData` (`LightThreshold, MediumThreshold, HeavyThreshold, AsRainParticleAlpha, AsRainParticleSpeed,
AsRainParticleSize, AsRainWetness`) is the table other systems use to classify the current rain as light/medium/heavy from those particle values [I from names].

**The file [V]** (two assets read: `atm_2018_sunset`, `atm_bank_shocker`): a DAT1 with four sections: `0x02f06d4e` the compiled settings (**0x628 bytes**; the parser demands
`kBuiltAtmosphereBaseVersion 219`, check `exe+1621af0(slot, data, 0x39f27e27, versionString)`), `0x72f28658` strings (texture paths), `0x71c168b4` (0x7ffec bytes,
"Light Grid Atmosphere ... Gen"), `0xe7997256` (**0x224010 bytes: the baked IBL**). The size variants in the request (2,770,xxx versus 526,xxx bytes) are with and without the
big sections [I].

**The parser [V]**: `AtmosphereManager` vtable `exe+3cf1f18`; slot 0 `Init(this, capacity)` (slots = `this+0x110`, capacity `this+0x118`, 16 bytes of per-slot data at
`this+0x120`), slot 20 `OnLoaded(this, slot, data)` -> **`exe+162cc00(slot, data)`** then `Recompute`, slot 22 `OnUnloaded` -> `exe+162d570(slot)`; `exe+162dc80(slot)` writes the
"Default Atmosphere". `exe+162cc00` copies the compiled section into the slot in blocks (**slot = section + delta**):

| section bytes | slot bytes | delta | content |
|---|---|---|---|
| `0x40..0x57`, `0x58..0x6f` | `0x288..0x29f`, `0x310..0x327` | | two asset references (24 bytes: u64 id, ...): sky box cube map and fill-light cube map [I which is which] |
| `0x34`, `0x38`, `0x3c` (offsets of strings) | `0x2a0`, `0x328`, `0x2e8` (pointers) | | "Light Grid Atmosphere" name, "Atmosphere LightGridModConfig", "CloudCover" texture |
| **`0x70..0xaf`** | **`0x2a8..0x2e7`** | 0x238 | **EnvLighting scalars** (table below) |
| `0xb0..0xcf` | `0x2f0..0x30f` | 0x240 | EnvLighting, second part (**ambient fill**) |
| `0xd0..0x127` | `0x340..0x397` | 0x270 | (zero in both samples; ends 0.8, 12) |
| `0x128..0x21b` | `0x398..0x48b` | 0x270 | tone map / bloom / lens flare region (**exposure `slot+0x484` = section `+0x214`**) |
| `0x220..0x417` | `0x490..0x687` | 0x270 (0x26c for `0x3a8..0x3b7`) | colour correction reference, vignette, chromatic aberration, **fog** (`slot+0x584..0x5c0`), volumetric fog, DoF, grain, scattering, shafts |
| `0x418..0x627` | `0x6b0..0x8bf` | 0x298 | AO, AA, SSR, **weather (wet surfaces, particles, wind), ocean, volumetric clouds** (`slot+0x81c` = section `+0x584`) |
| IBL section | `slot+0x650`, `+0x658`, `+0x660` (texture objects), `+0x668..0x677` | | see Part 20 |

`slot+0x674` is preset to 1.0; `slot+0x70/0x74/0x78` are counts from section `+0x20/+0x24/+0x28`, `slot+0x30..` an array of sky-object handles.

**EnvLighting in the slot** (field order checked against the values of both assets: 1,1,1,0.19 / 297.4 / 40.4 ... and 1,1,1,3 / 220 / 25 ...):

| slot | field | sample (sunset / bank) | tag |
|---|---|---|---|
| `+0x2a8/2ac/2b0` | KeyLightColor RGB | 1,1,1 | [V] (matches Part 10) |
| `+0x2b4` | KeyLightIntensity | 0.1875 / 3 | [V] |
| `+0x2b8` | KeyLightAzimuth (degrees) | 297.4 / 220 | [V] |
| `+0x2bc` | KeyLightElevation (degrees) | 40.4 / 25 | [V] |
| `+0x2c0`, `+0x2c4` | **SunDiskOffsetAzimuth, SunDiskOffsetElevation** | 0, 0 | [I: declared order] |
| `+0x2c8` | ShadowDrawDist | 90 / 100 | [I] |
| `+0x2cc` | CsmLodCount (int) | 4 | [I] |
| `+0x2d0` | TimeOfDay | 0.75 / 0.75 | [I] |
| `+0x2d4` | **SunDiameter** | 0.5 / 1.0 | [I] |
| `+0x2d8`, `+0x2dc`, `+0x2e0`, `+0x2e4` | KeylightMaterialScale?, CasterRangeLower, CasterRangeUpper, (int flag) | 100, -10, 400 / 40, 0 / 1 | [I: by value] |
| `+0x2f0`, `+0x2f4`, `+0x2f8`, `+0x2fc` | LightGridIntensity, LightGridSkyMultiplier, LightGridAORadius, VFXLightMultiplier | 1, 1, 15, 1 | [I: by value] |
| **`+0x300/304/308`** | **AmbientFillColor RGB** | 0.0003 x3 / 1,1,1 | [I, strong] |
| **`+0x30c`** | **AmbientFillIntensity** | **0 / 0** | [I, strong] |

**The ambient lead (MSK-A1):** Part 10 poked `slot+0x300` and saw nothing. `+0x300` is only the red channel of `AmbientFillColor`, and the **intensity at `+0x30c` is 0 in both
vanilla atmospheres read**, so a colour change multiplies by zero. *Live check, first thing to try:* `holdf <slot>+0x30c 0.5` (then 1, 2, 5) with `+0x300..0x308` set to 1,1,1,
`recompute`, and screenshot a shaded alley. If it works it is exactly the row asked for (a fill light independent of the sun). The other ambient knobs in the same block:
`LightGridIntensity` (`+0x2f0`) and `LightGridSkyMultiplier` (`+0x2f4`) scale the baked GI (light grid) and its sky term; `AOSettings.AmbientShadowIntensity` is in the last block.

**Fog in the slot** (values 400, 800, 1200, 2400 and 0.002, 0.001, 0.005, 0.001, 0.0001 in the sunset asset): `slot+0x584` Height0 (-500), **`+0x588..0x594` Height1..4**
(Part 10's "fog distance" is the *height* of the curve points), **`+0x598..0x5a8` Opacity0..4** (the "density"), `+0x5ac` RemapStrength [I], `+0x5b0` RemapKeyOpacity [I],
`+0x5b4/5b8/5bc` Color [V matches Part 10], `+0x5c0` = LuminanceScale or KeyLightInfluence (the "haze", sun in-scatter) [I]. The volumetric fog block follows.

The remaining blocks are not labelled field by field: the compiled order is **not** the declared order (exposure sits at section `+0x214`, after a bloom-like block),
and the tool that compiles a `Def` into the 0x628 block is not in the retail exe (searched for a converter by the Def offsets: none). Fastest way to finish the map: the
`fieldprobe` with the named candidates of each block (the names say which values are safe: `Enabled` flags are ints, texture references are pointers). **The probe crash near
`+0x320` was the fill-light cube map reference at `+0x310..0x327` and the string pointer at `+0x328`: never write `+0x288..0x2a7`, `+0x2e8`, `+0x310..0x33f`, `+0x650..0x667`.**
`recon/atm_parser_map.txt` (local) lists every copy the parser makes.

## Part 20. The baked lighting of an atmosphere, and runtime textures (MSK-A11, G1; FOF-B3)

[V] Section `0xe7997256` (0x224010 bytes) is three raw **RGBA16F** images plus 16 bytes, turned into textures by the parser (only when `byte [exe+7a89150]` is set):

| blob offset | size | created with | slot |
|---|---|---|---|
| `+0x000000` | 0x20000 = 128 x 128 x 8 bytes | `exe+19b1500(TextureManager exe+621b1a0, 128, 128, fmt 0xa, name, pixels, 0)` | `+0x650` |
| `+0x020000` | 0x200000 = 128 x 128 x 16 x 8 | `exe+19b1280(mgr, 128, 128, 16, fmt 0xa, name, pixels, 0)` ("Runtime Texture (3D)") | `+0x658` |
| `+0x220000` | 0x4000 = 128 x 16 x 8 | `exe+19b1500(mgr, 128, 16, fmt 0xa, name, pixels, 0)` | `+0x660` |
| `+0x224000` | 16 bytes (4 floats) | copied | **`+0x668..0x677`: Part 13's "colour filter" is the tail of the IBL blob** |

What each image is (irradiance, prefiltered specular, sky-view lookup) is [?]; the names are formatted as "`<asset name> Gen <n>`". Two consequences: (1) **the engine can make a
texture from memory** (`exe+19b1500`: width, height, format, name, pixel pointer), which is the route for a sky or an icon without the toc (MSK-G1, Friend or Foe FOF-B3); using
such a texture object as the *sky box* still needs the `SkyBoxCubeMap` reference path (`slot+0x288`, resolver `exe+1631d80`) [?]. (2) Custom ambient = rewrite these three images
(same sizes) in a loose atmosphere; no light probes need recapturing.

## Part 21. Cross-fades exist in the engine: `AtmosphereBlender` and `ActivateAtmosphereAction` (MSK-A5, A12, B4)

- [V] Component **`AtmosphereBlender`** (`AtmosphereBlenderPrius`: `StartActive`, `SourceAtmosphere`, `DestinationAtmosphere`, **`BlendFieldCompletePercent`**, `ForceSourceAtmosphere`):
  the game's own two-atmosphere blend driven by a 0..1 value. Type object and methods: `recon/component_types_4.0630.txt` (`AtmosphereBlender`); not disassembled yet [?].
- [V] Node `ActivateAtmosphereAction` (`Atmosphere` file, `DisableOnDestroy`), execute `exe+204fb10`, calls `AtmosphereAdjust` (`exe+204fc81`, `exe+204ff6d`).
- [V] Cinematics have atmosphere tracks: `Cinematic2AtmosphereTrackDef`, `Cinematic2GlobalAtmosphereTrackDef`, `Cinematic2AtmosphereModifierTrackDef`, `Cinematic2AtmosphereActionDef`.
- [V] `AtmosphereSyncComponent` (type `exe+745f760`) is read by `Recompute` (`exe+162ff10`): the weights are synced state too.
- **All 26 callers of `AtmosphereAdjust` (`exe+162f020`)** [V]: fast travel / load screen (`exe+712aac`, `712bd6`, `713c5c`, `7140d1`, `714435`, `7154dd`, `715b5f` in fns
  `7129c0`, `713b90`, `713ea0`, `714400`, `715460`, `7156c0`), menus and 3D UI object viewers (`813526`, `81661a` `GUI::UIObjectDisplayer`, `816676`, `81a6c8`, `81c39b`, `81e183`,
  `81e67e`, `821950`, `822ebf`: the map/suit/gadget screens' +1000), engine (`16605a5`, `166216a`, `1669aec`, `166b050`), the lighting object (`1a16d87`, `1a17bf0`, `1a19ed3`,
  `1a1b599`: the base-atmosphere switch already known), `ActivateAtmosphereAction` (`204fc81`, `204ff6d`).

No continuous clock was found in the reflection data: `EnvLighting.TimeOfDay` is a per-atmosphere constant (0.75 in both samples) [V]; the "time of day" of the world is the
discrete mode. A continuous cycle therefore means blending atmospheres (the blender above) and steering `KeyLightAzimuth/Elevation`, as the mod already does.

## Part 22. Nodes and conditions that set or read the lighting (MSK-C6, A12)

[V] `SetLightingModeAction` (execute `exe+bd8640`, calls the setter), `GetLightingModeAction`, `OnLightingModeChangedAction` (`StartActive`, `TriggerOnRegionLoad`, `MaxTimesToFire`),
`MissionLightingHelperAction`, `SetBypassDistantLightingAction`, `LightingModeOverlayTrigger` (overlay entries keyed by the mode: Streets Part 9), `PedestrianLoadConditionWeather`,
`PedestrianRainToggleEvent`, `RainDropEvent`, `AtmosphereChangeFullscreenEvent`, `UIWeatherStationAction` + `WeatherStationAccept/DeclineEvent` (an in-game weather station prompt).
To keep the player's time of day inside a mission, hook `SetLightingModeAction`'s execute (`exe+bd8640`) rather than the setter (the load-time apply and the menu keep working).

## Part 23. The toc object without a scan, by-id reads, and evicting a texture (MSK-B2, B3, B7; shared with Streets MST-A1/A2)

[V, read from the exe, not run] The game's file system object is the global `q[exe+74a6a00]` (`FileSystem::ArchiveFileSystem`, vtable `exe+3cf9920`): **the toc object is at
`fs+0x18`**, the toc DAT1 buffer pointer at `fs+0x10`, the 8-byte location table every read uses at `q[fs+0x40]` (= toc object `+0x28`, section `0xdcd720b5`: low dword archive
index, high dword offset), the per-archive open table at `q[fs+0x68]`, capacity 0x200 archives. Reads are requested **by asset id and kind** (`exe+16d83e0(request, assetId, kind)`
-> `fs->vtable[+0x28]` = `exe+16da4c0`) and the location is looked up in the toc at that moment (`exe+16d7990(toc, id, languageGroup, kind)`), so nothing holds stale entry
pointers across requests: repointing the tables is safe between requests, and `toclocate` can be replaced by reading `q[exe+74a6a00]`.
Texture pixel streams: `TextureStream` static `exe+621b4e0`; `exe+19b5e70(stream, record, 0)` evicts one texture's high mips and `exe+19b5d70(stream, record, 0)` requests them;
records are 0xc0 bytes at `q[exe+621b2b0]`. The stream is read into a mapped staging buffer (`exe+19b7f30`) and copied later by `exe+19b5a20`, which is a place to substitute
pixels with no archive at all. Details, the field table and the caveats are in `ManhattanStreets/docs/RECON.md` Part 33 (local); the same text is summarised in
`docs/GAME_INTERNALS.md`.

## Part 24. The Settings-screen crash at `exe+31e9582`: what the faulting code is (MSK-D1, partial: not root-caused)

[V from the three dumps with `tools/sigtools/dumpctx.py`, and from the disassembly] The dumps' own exception context (the old tool printed the handler's) gives:
`Rip = exe+31e9582`, read of `0x90`, `Rax = 0`, `R15 = 0x90`, `Rsi` = the object passed in, `Rbp = 1`, `R14 = 0`. Corrected chain (the `exe+31f9422` entry of the old
stack is a stale slot, not a caller): `31e9582` <- **`32c2745`** <- `32c0db6` / `32261e9` (the ActionScript VM, recursing) <- `3224a31` <- `3224d14` <- `3411ee7` <- `34d5d1d`
<- `34d8a04` <- `34f511c` <- `1d294d3` (the movie's per-frame advance).

- `exe+31e9540(asObject, index, count)` removes `count` entries starting at `index` from an array kept in the object's **side record** (`exe+31c5070(asObject, 0x200)` finds
  the record through the 4 KiB page header and the 56-byte cell table; the array is at `record+0x90`), clearing each entry's back pointer (`+0x20`) and dropping its reference.
  It crashed because the side record was **null**: the cell of `asObject` is free.
- Its caller `exe+32c2550(displayList, container, index)` is the **display-list child removal** of a `DisplayObjectContainer`: `asObject = container->vtable[+0x228]()` is the
  container's ActionScript instance, and the call at `32c2740` is "remove 1 child reference at index `d[entry+8] + 1`" from that instance.

So the event is: **during a frame advance, script code removed a child from a container whose ActionScript instance had already been freed** (the native display object was still
in the display list). It is a use-after-free of a *container's* script object, not of an array of ours. That fits "2 to 7 s after leaving the tab": a timeline or transition
animation that outlives the screen runs `removeChild` on a clip whose script object lost its last reference early.

What can drop that reference early [I, ranked]:

1. **A call the game's Flash expects and never gets.** `ModMenu` swallows `CloseMenu`, `FadeOut` and `FadeIn` in `InvokeHook` for a few seconds around a lighting change. If the
   native side has already released its reference to the options clip (it believes the menu is closing) while the clip stays on the timeline because `CloseMenu` never
   ran, the next transition removes children of a clip whose instance is gone. The crashes follow sessions with many row changes, i.e. many swallowed calls.
2. **An unbalanced release of a value that references a display object.** `ReleaseValue` calls `ObjectInterface::ObjectRelease` (vtable slot 2) on the tab value after `PushBack`:
   that is correct for a value *we created* (`g_build_tab` output). It would be one release too many for any value that was *borrowed* (an argument of a hooked `Invoke` or of
   the callback): check that no code path releases `args[i]`.
3. The rebuild itself (`OpenOptions` re-entered from inside a Flash callback while the previous build's clips are mid-transition).

Next steps that would settle it [not done]: log every swallowed `Invoke` name with a counter and compare with crash sessions; stop swallowing `CloseMenu` (let the menu close
and reopen it) for one play session; in the fault handler, also dump `Rsi` (the freed instance), the container (`[rsp+0x70]` in the frame of `exe+32c2550`) and its word at
`+0x6a` (display-object flags) so the clip can be identified by its class name.

## Part 25. Offline checks of Fable's maps, and what was built from them (2026-11-19)

**Offline slot-map check [V, tools/atmslot.py]**: the compiled settings block (DAT1 section `0x02f06d4e`, 0x628 bytes) of any vanilla atmosphere can be read from `toc.BAK` and mapped to slot offsets with Part 19's delta table.
`python tools/atmslot.py <path suffix> [...] [--diff]` prints each word as hex and float. Checks against known values: the **haze** (`slot+0x5c0`) of `atm_ibl_lgt_night_rain_02` is 0.15 against 0.05 in
`night_02` (matches the rain look), **exposure** `+0x484` -1 / -0.5, the key light `+0x2b4..0x2bc` (0.015, 234, 70 in the rain night), the fog table `+0x58c..0x5a8` and the fog colour `+0x5b4..0x5b8`. The mapping holds.
- **Ambient fill [V]**: `+0x30c` (AmbientFillIntensity) is **0 in `night_02` and 0.003 in `night_rain_02`**; the fill color `+0x300..0x308` does not differ between them. So the scale of real values is 0.001-0.01.
  (Part 19 found 0 in two other atmospheres.) The AMBIENT LIGHT row writes `+0x300..0x30c` (color white unless AMBIENT COLOR is set, intensity 0.001 .. 0.06) and restores the sky's own values when set back to AUTO.
- **Rain is not in the compiled Weather block [V negative]**: the NYC Rainy Day and MM Heavy Snow mods change the key light, fog heights/opacities, haze, exposure, bloom/tone values and the clouds switch, but **nothing in `slot+0x6b0..0x8bf`
  except two words at `+0x780/+0x784` (hash-like in vanilla, 0 in the mod) and `+0x81c`**; `night_rain_02` differs from `night_02` in the same way (no weather-block change). The fields Part 19 names (`Weather.Particles`, `Wetness`) are not what makes
  rain, or live outside the 0x628 block. Rain stays tied to the lighting mode (names `kNightRain`, `kOvercastRain`); a WEATHER row needs the rain-mode path found in the setter's reaction, not atmosphere values.

**Built from Fable's Parts (all untested in game; each is off or AUTO by default)**:
1. **SWITCH STYLE (seamless)**, `mod/src/seamless.cpp`: hooks `exe+710970` (`DynamicLoadScreenSystem::OnLightingTransitionStarted`, bytes `40 53 48 83 ec 70 80 79 18 00` verified against the exe) and returns at once for a
   transition this mod started (`recorder::ApplyMode` arms it for 5 s before calling the setter): Part 18 recipe 1. Row SWITCH STYLE: GAME'S / SEAMLESS; ini `[sky] seamless`; dev command `seamless [0|1]`. The log says
   `seamless: skipped the load screen of a lighting transition A -> B` per use. **What to watch**: does the hero keep moving, does a crime survive, does the world pop (levellight/zone swap), do the parked cars/traffic hitch.
2. **AMBIENT LIGHT / AMBIENT COLOR** rows (above).
3. **`no_swallow.txt`** (in `Documents\...\ManhattanSkies`): the host does not swallow `CloseMenu`, `FadeOut`, `FadeIn` for that session; the test of Part 24's first-ranked cause of the Settings crash. The fault handler also dumps 32 qwords of stack (the container
   of `exe+32c2550` is at `[rsp+0x70]` in that frame).
4. **`fsinfo`** dev command: prints the file system object `q[exe+74a6a00]`, the toc object at `fs+0x18`, the DAT1 pointer, the location and open tables, and compares them with the heap-scan result, to settle which toc object layout is right
   (Part 23's toc-object offsets differ from the ones our grow code uses: `+0x10 ids, +0x28 sizes, +0x30 offsets, +0x48 count`) and whether new archive entries (open table capacity 0x200, about 210 used) can be added without any Overstrike archive.
