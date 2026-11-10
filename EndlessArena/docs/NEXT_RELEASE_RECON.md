# Next release recon: dynamic music and in-game customization (started 2026-10-18)

Goals (from the maintainer, see [LEDGER.md](LEDGER.md), "Next release"): (1) dynamic music for EndlessArena using the game's own tracks; (2) user
customization (blacklist factions, set faction order, choose bosses...) with a way to control the mod **inside the game**. Confidence tags as in
[GAME_INTERNALS.md](../../docs/GAME_INTERNALS.md): **[verified]**, **[RE]** (read from the exe, not run), **[?]** (unknown).

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

## 3. Prior art found: SpidyVR (read 2026-10-19)

A VR mod for the same game build (4.0630.0.0, GPL-3.0; https://github.com/IlyaMez/SpidyVR, cloned to `SpidyVR/`, gitignored, not ours). Its
`src/game_menu.cpp` + `include/spidy/game_menu.hpp` + `docs/REFERENCE.md` ("SPIDY VR tab in the game's Settings") are a working, in-game-verified
implementation of **option 1**: an extra tab in the game's own Settings, built by the game's own row code. Read, not run here. Same build as ours; its
`exe+16d5e80` (Spider-Sense sound) is our SendEvent, which cross-checks both projects' addresses.

How it works (all RVAs for 4.0630.0.0):
- Settings system (static) `5d96dd0`; `+0x20` the config (`UISystemMenuConfig`); tabs array at config `+0x168` (0x40 bytes each), count at `+0x170`.
- Tab (`UISystemMenu`, vtable `3a7da68`): `+8` id, `+0x10` name tag, `+0x20` heading tag, `+0x30` items, `+0x38` count. Item (`UISystemMenuItem`, vtable
  `3a7d988`, 0x88 bytes): `+8` setting number, `+0x10` title tag, `+0x20` help tag, `+0x48` option type (setting `3a7d6e8` / heading `3a7d838`),
  `+0x70` choices (0x50 bytes each: `+8` name tag, `+0x18` help, `+0x28` preview), `+0x78` count. Tags are {char*, length, CRC32}.
- It **copies** a real tab, setting and heading from the loaded config into its own memory, edits the copies (setting numbers 0x200+, its own text tags), and
  at the moment the game calls Flash `OpenOptions` (via the UI Invoke wrapper `1d1cf30`, name literal `389f4c0`) builds the tab's Flash object with the
  game's builder `807740` (out value, movie from `1d29690`, tab) and pushes it onto the tabs array (GFx array push = vtable slot 15).
- Hooks for the setting numbers it owns: kind `72d640` (0 list, 1-2 slider, 3 colour, 4 command, 5 heading), value `72d650`, active `72e1d0`, default
  `72d5f0`, default choice `72d610`, set `730ee0`, reset `730ae0`; plus the Flash callback dispatcher `7dc400` (UpdateOption / ResetOption /
  ResetAllOptionsForCurrentOptionsMenu), and the text lookup `1749970` / `1749ae0` (answers its own CRC32 tags only while building its tab).
- Also adds a button to the pause menu's first page (`7dd7c0`, item loop returns to `7dd8ae`) - a way to put an "Endless Arena" entry on the front page.
- Lists with up to 9 choices, ON/OFF switches, section headings, per-row help text, X RESET and Y RESET ALL all work. Sliders (kinds 1-2) are known to
  the type hook but not used by Spidy.

What this changes for us: option 1 is now low-risk and mostly mechanical. A faction blacklist = ON/OFF switch per faction; order = a list row per slot
(choices = factions); boss choice = list rows; all with the game's look and controller navigation. Limits to design around: 9 choices per list as written
(the buffers are fixed-size and could be enlarged), values are choice indices or floats, no free text.

Also in the repo (possible later use, none needed now): audio through Windows MCI (not the game's Wwise), per-system `RVA` tables for the TimeScaleSystem
(`19bb430`), hero/dodge managers, bot/allegiance RTTI lists (`SimpleAllegiance` kBlueTeam/kPolice/kFisk/kMaggia/kSableEnemy), Havok/body details,
and a tools/ folder of in-game probe scripts (the same command-file style as ours). Music: the sound posted through SendEvent with hash `0x4b5f5740` hints
that sounds may be posted as events through the Event system - to confirm with the recorder.

## 4. Findings of 2026-10-19 (static, not yet run in game)

**Music: the exe exports the Wwise API.** `Spider-Man.exe` has a PE export table of 381 entries and they are the Wwise SDK's C++ names
(`?PostEvent@SoundEngine@AK@@...`, `SetState`, `SetSwitch`, `SetRTPCValue`, `ExecuteActionOnEvent`, `StopPlayingID`, `PostTrigger`, `StopAll`...), so
`GetProcAddress` finds them; no signatures needed. `src/audio.cpp` hooks them as a recorder (`audio on` / `audio off` / `audio mark <text>`), writing
`EndlessMode_audio.txt` (ids, game object, caller) to the data folder, and can post (`audio post <event> [obj]`, `audio state <group> <state>`,
`audio switch`, `audio rtpc`). Wwise ids are FNV-1 of the lower-cased name (checked: 2167 of the first 3000 names in `Events.wwiselookup` have their id in the file);
`tools/assets/wwise_names.py` turns a recording back into names. Needed from the maintainer: record a crime and a hideout, then play the Arena briefly.
Open: which game object the music plays on, whether the bank for a cue is loaded in the arena (the lookup auto-loads banks when its events are referenced), the
intensity states and RTPCs the combat music reacts to.

**Customization: SpidyVR** (section 3) is the way in; built as `menu.cpp` + `settings.cpp`: a PRESET row, run, difficulty, rewards, curses, you, enemy mix, factions
(switches, order, slots, budget), bosses (per faction), Horde. Boons and curses are fully configurable; pool lists can also be edited in `EndlessArena_pools.txt`.

**Allegiance (allies, no infighting): dropped 2026-10-20** - it would tamper with the survive-as-long-as-you-can fantasy and need more settings. (Notes kept for the record: every fighter has a `SimpleAllegiance` component, team int at `+0x48`, set by `exe+2117e00`; the enum order is at `exe+39470d8`.)

**Boss phases (Kingpin):** see docs/GAME_INTERNALS.md. The director now steps Kingpin through his health phases.
