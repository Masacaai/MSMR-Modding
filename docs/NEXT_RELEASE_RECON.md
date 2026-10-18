# Next release recon: dynamic music and in-game customization (started 2026-10-18)

Goals (from the maintainer, see [LEDGER.md](LEDGER.md), "Next release"): (1) dynamic music for EndlessArena using the game's own tracks; (2) user
customization (blacklist factions, set faction order, choose bosses...) with a way to control the mod **inside the game**. Confidence tags as in
[GAME_INTERNALS.md](GAME_INTERNALS.md): **[verified]**, **[RE]** (read from the exe, not run), **[?]** (unknown).

## 1. Music

### What the game uses
- **Wwise.** The exe wraps it in named engine functions `PostEvent`, `SetState`, `SetSwitch`, `SetRTPCValue` (also `PostEventDelay`, `SetRTPCInternal`,
  `SetRTPCValueByPlayingID`), with data types `WwiseEvent`, `WwiseState`, `WwiseGameSyncAction`, `WWiseSyncComponent`. **[RE]**
- **Names are resolved through a lookup asset:** `sound/banks/Events.wwiselookup` (asset id `A81AB0A616889CC2`; the exe loads it through
  `WwiseLookupAssetManager` and refuses to start sound without it). It is 3.5 MB and lists every event name the game can post (57,000 strings).
  Banks are `sound/banks/*.soundbank` (`Init.soundbank`, `DCOM.soundbank`, ...). **[verified: extracted and read]**
- **The game's own music-intensity system:** `MusicIntensityIncreasedEvent` / `...ReducedEvent`, mission actions `SetMusicIntensityOverrideAction`,
  `ClearMusicIntensityOverrideAction`, `ResetMusicIntensityAction`, `QueryMusicIntensityStateAction`, `MusicCueListenerAction`, configs `MusicConfig`,
  `HeroMusicConfig`, `MusicConfigBotValueElement`, and `MusicVolume`. **[RE]**
- Extract the name table yourself: `python tools/assets/extract.py out "sound/banks/Events.wwiselookup"`, then take the printable strings (names only;
  don't commit the file).

### Candidate tracks (event / cue names from the lookup, 216 music-style names)
Mission scores are `I20_MUS_A1_*`, `A2_*`, `A3_*` (story), `CTNS_*` (DLC), `OM_*` (open-world content), `VN_*` (villain fights), plus generic
`Play_MUS_*` events. Most useful for an arena:

| Use | Names |
|---|---|
| Boss / fight themes | `I20_MUS_VN_Tombstone_Fight`, `..._Tombstone_Track`, `..._Tombstone_Track3`, `I20_MUS_VN_Electro_Fight`, `I20_MUS_VN_Rhino_Fight`, `I20_MUS_A3_050_OttoFight`, `I20_MUS_V10_Shocker_Chase`, `I20_MUS_OM_Taskmaster_Boss_Intro`, `I20_MUS_OM_Taskmaster_Boss_Finale`, `I20_MUS_CTNS_HH_080_Finale` (Hammerhead), `I20_MUS_CTNS_SS_070_Finale` (Sable) |
| Combat | `I20_MUS_OM_Taskmaster_Combat`, `I20_MUS_CTNS_BC_Combat`, `I20_MUS_Combat_Test`, `Play_MUS_combat_stinger` |
| Faction / base themes | `I20_MUS_OM_Bases_Demons`, `Play_MUS_I20_MUS_OM_Bases_Demons`, `I20_MUS_A3_015_PrisonerBases`, `I20_MUS_CTNS_HH_Bases`, `I20_MUS_CTNS_SS_Bases` |
| Results / sting | `Play_MUS_Mission_Failed` / `Stop_MUS_Mission_Failed`, `Play_MUS_stealth_action_stinger`, other `Stinger` / `death` / `title` names |

How a cue is actually posted (event vs state vs switch), how it fades, and what stops it: **[?] not yet known.** That is what the recorder is for.

### Plan
1. Build an audio recorder in EndlessArena's dev tools (like `uilog`): hook the engine's PostEvent / SetState / SetSwitch / SetRTPCValue wrappers and the
   music-intensity events, and log each call with the name (resolved through the lookup) and a timestamp.
2. The maintainer plays a crime and a hideout with music on while it records. The log shows how the game starts a score, raises intensity during a fight,
   fades out, and what it does at the end.
3. Drive the same calls from the director: intensity by wave, a boss theme on boss waves, a sting on the results panel; stop everything when the run ends.
   (The seconds between waves need nothing special.)

## 2. In-game customization

### What exists for MSM2 (and not for MSMR)
- **ModSettings** (Nexus, Spider-Man 2 page) is an Overstrike **script** (`modsettings.script`) that other scripts depend on. It adds a **MODS** entry to the
  game's own lobby and pause menus; mods register menus that use Header / Button / Toggle / Option / Slider / submenu / dropdown rows, saved through a callback.
  Its public API header is in `hbgda/SM2ScriptTemplate` (`ModSettings.h`): it works on the game's `UISystemMenu` / `UISystemMenuItem` (0x30-byte items)
  and `ItemDisplayData` (0x88 bytes: label, description, image, icon). The implementation is not published (only a `.lib`). It is MSM2-only; nothing like it
  was found online for MSMR. **[verified: read the template]**

### What MSMR has
- The exe has the **same menu class family**: `UISystemMenu`, `UISystemMenuItem`, `UISystemMenuItemBase`, `UISystemMenuOptionTypeBase / Command / Setting /
  SettingList / ColorSetting / SubMenu / Header`, `UISystemMenuConfig`, `UISystemMenuCampaign`, and menu events `UIMenuToggleEvent`, `UIMenuSliderEvent`,
  `UIMenuUpdatedOptionEvent`, `UIMenuSelectEvent`, `UIMenuMoveEvent`. Those are data (reflection) classes; the structure is defined by the config
  `configs/UIConfig/UISystemMenu.config`. **[RE]**
- The **screen that shows it is `ui/export/popups/SystemOverlay.gfx`** (the system/settings menu: Options, Saves, Controller map), with `Options`,
  `OptionsListItem`, `SystemList`, `SystemMenuData`. Each option item the ActionScript reads has: `option_type`, `label`, `value`, `minimum`, `maximum`,
  `value_default`, `options` (list), `locked`, `child_level` (indentation), `prominent`, `disable_reset`, `fidelity`, `secondary_txt`... **[verified: disassembled
  with `tools/sigtools/abcdump.py`]**
- The pause shell (`PauseShell.gfx`) has tabs (Map, Missions, Gadgets, Gear, Skills, Inventory, Gallery, Moves list, Benchmarks, CTNS), a different movie.
- The **C++ that builds those item objects** is a cluster at `exe+806cd0 ... exe+80c6e8`: the function at `exe+808830` fills one option item (strings
  `option_type`, `minimum`, `value_default`, `prominent`, `fidelity`) and is called from nine sites, one per option kind; `exe+80a220` handles `child_level`;
  `exe+80b950` builds the descriptor with `option_type`, `previewImage`, `warnMessage`, `warnIsForReload`, `active`. **[RE]**

### Options, ranked
| # | Approach | Pros | Cons | Effort |
|---|---|---|---|---|
| 1 | **MSM2-style injection into the system menu**: hook the item builder, append our own rows (a MODS / ENDLESS ARENA submenu with toggles, lists, sliders), hook the toggle / slider / update events for our rows | Native look and controller navigation, same as MSM2 mods | Needs the runtime menu object layout and a safe way to add rows; the most reverse-engineering | High |
| 2 | **An in-game overlay** (ImGui over the D3D12 swap chain) | Full control of the UI; Script Hook for MSMR exposed `AddImGuiMenu`, so it was done before | Not native; gamepad use is clunky; D3D12 hooks can clash with ReShade (installed here) | Medium |
| 3 | **Our own screens built from game panels** (the dock circle panel, notification box, choice buttons like the boon picks) | Uses code we already have; no new RE | Cramped; fine for a handful of toggles, poor for long faction lists | Low |
| 4 | **EndlessArena as a `.modular` file** (options in Overstrike, as in QuantumBreak Renewed) | Reliable, no RE | Customization lives outside the game; least wanted | Low |

### Next steps for 1
1. Record the system menu with the existing `uilog` (`uilog on`, open Pause > System/Options, walk through a few settings and sliders): shows the exact
   objects and ActionScript calls the C++ makes, and which events come back when a value changes.
2. From the recording, find the call that sends the items to `SystemOverlay` and the function that creates them; check whether a row can be appended
   (and how its changes are reported) without touching the config asset.
3. If that is a dead end, fall back to option 3 for a first release and revisit option 1 afterwards.
