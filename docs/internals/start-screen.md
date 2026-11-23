# E3 2018 start screen recon: "Spidey on his phone, press X, jump off into the city" (2026-11-20)

Game build **4.0630.0.0 only**; addresses are `exe+RVA`. Tags: **[RE]** read from the exe or the archive (static; **nothing here was run in game**),
**[I]** inference, **[?]** unknown. Extracted assets were read from a scratch folder and are **not** in the repository (only names and ids below).
Tools used: `tools/assets/extract.py`, `tools/toctool` (`check`, `grepall`), `tools/sigtools` (`xrefs`, `owner`, `disasm`, `rtti`, `funcstrings`, `strrva`).

## Verdict

**Yes, the pieces are still in the shipped game, more than expected.** The phone start-menu scene, its sounds, the demo title-screen option list, the
mission-graph node that drives a demo lobby, and the "button pressed" event are all in the archive and the exe. What is **missing** is the shot where he
jumps off the wall (no asset found), and we do not yet know how the retail game switches the demo path on. Nothing has been tried in game.

| Piece | Status |
|---|---|
| The scene: Spidey with a phone, a lit rooftop set with rain drips and steam | **Present** (section 1; we have not seen it rendered) |
| Phone swipe / button sounds | **Present** |
| A title-screen option list that starts a demo straight in the city | **Present**, a leftover config (section 2) |
| A mission-graph node that runs a demo lobby and reacts to a button press | **Present** (section 3) |
| The "jump off the wall" shot | **Not found**. Only the whoosh/fly *sounds* `start_menu_02` exist (section 4) |
| The switch that enables all this | **Unknown**: a global byte, no static writer found (section 5) |

## 1. The scene

- `cinematics/start_menu/start_menu.cinematic2` (id 8FC486177EE69C03, 11 KB) [RE]. Tracks, read from its strings: `Start_Menu_shot1_Spiderman1`, `Start_Menu_shot1_phone`,
  `Start_Menu_shot1_Camera_igCam_Export`; actors `characters/ui/paperdoll/main_menu_spiderman.actor` (80534AC46C75CB28) and `characters/civilian/civ_att_phone_a.actor`
  (9FE9959A54B24498); atmosphere `atmosphere/atm_ibl_lighting/atm_gi_demo_menu_day.atmosphere`; lights `S01_1_keyBack`, `S01_2_kickHead`, `S01_3_fillAll`...;
  sound events `Play_FOL_Player_Start_Menu_PhoneSwipe` and `Play_FOL_Player_Start_Menu_PhoneButton`. Only **shot 1**. Animclips for the three tracks exist
  (`cinematics/Start_Menu/Start_Menu_shot1_{phone,Spiderman1,Camera_igCam_Export}.animclip`).
- `levels/fast_travel/instance/fast_travel_instance/fast_travel_vig_01/Main_Menu_Cine_01.zone` (BD1530C871A8382C, 9 MB) [RE]: the zone that holds the menu scene
  (strings: `start_menu`, `main_menu_spiderman`, the cinematic2 component). Rain drips, steam vents, a sun flare and `cine_GIDemo_startmenu_smokeBehind.visualeffect`
  ("GI Demo" = Game Informer demo).
- `cinematics/start_menu_03/start_menu_03.cinematic2` (9A05B474457C6AB3) [RE]: a different scene, `FastTravel_Alone_shot1`: Spidey (`hero_Spiderman_Body_Cine`) and a subway car,
  camera config `camera_cinelook_loadscreen_stick`. A fast-travel loading-screen vignette, not the phone scene.
- Related: `material/global/loadscreen_bg_city_2darray/start_menu_bg_2darray.material` (8A0AACBCC22891BC), music `Stop_MUS_UI_Lobby_02_GI_Demo`, ambience `Play_AMB_City_Start_Menu`.
- **The retail game already loads this scene.** `LobbyShellUI@GUI` (vtable `exe+389a448`) function `exe+7a6900` references both `Main_Menu_Cine_01.zone` and
  `main_menu_spiderman`. **[I]** the normal PC main menu may already be a version of this shot. Needs one look in game (section 7, step 1).
- A caution on the archive search: ids in `mods\mod14x` archives are Overstrike-installed copies of base assets, not extra content.

## 2. The demo title-screen option list

`configs/system/system_TitleScreenDemoOptions.config` (B62747351CDAA970, 1.5 KB; the exe loads it from `exe+7add69` / `exe+7ae07c`, called by `TitleUI@GUI`) [RE]:
type `UITitleScreenOptionConfig`, comment "Demo Title Screen", `EnableDebugMenuShortcut: true`, two options of type `UITitleScreenOptionNewOpenWorldGame`:

| Label | Level | Checkpoint | Mission | Other |
|---|---|---|---|---|
| The Raft | `levels/i20_city/i20_city.level` | `CHK_PRISON_RYK_EXIT_CIN` | `GP_A2_PRISON_EXIT_CIN` | `DemoType` "Game Informer Demo", `DemoMode` true, `AllowXP` false |
| Open World | `levels/i20_city/i20_city.level` | `CHK_GAMEINFORMER_DEMO_START` | `GAME_INFORMER_DEMO` | `SaveSlotId` **kSlot1**, `AllowXP` true, `DemoMode` true |

- The option classes in the exe: `UITitleScreenOptionNewOpenWorldGame`, `...ContinueOpenWorldGame`, `...HostCoopGame`, `...JoinCoopGame`, `...DebugFrontend`, `...Entry`, `...Config`.
- **Why it matters:** a demo start does not go "menu level -> loading -> city". It loads the city level at a **checkpoint** with a `MissionOrderList` ("Game Informer Demo")
  and `DemoMode`. That is how the demo got from the start screen into the city without a load in view **[I]**.
- **Side find for Endless Arena:** option 1 is a ready-made "start at the Raft/Ryker's exit with demo rules" entry (`CHK_PRISON_RYK_EXIT_CIN`).
- **Danger:** the second option names `kSlot1`. Starting it could overwrite save slot 1. **Back up saves before any test.**

## 3. The mission graph that runs the demo

- Zone `8EEDAE75B29FBC75` (2 KB, in `g00s008`) [RE]: a Cinematic2 component playing `start_menu_03.cinematic2` on `cinematic2actor_START_MENU`, nodes `Cinematic2Action`,
  `GateAction`, `SignalRelayAction`, `OnMissionObjectiveEventAction`, `AdvanceMissionObjectiveAction`, objectives `TEST_START_MENU` / `TEST_START_MENU_A`, and **`SpecialDemoLobbyAction`**.
  Audio signals inside it: **`MUS_GAME_INFORMER_DEMO_START_BUTTON_PRESSED`**, `MUS_GAME_INFORMER_DEMO_START`, **`E3_AUD_SIGNAL_DEMO_STARTING`**. So this graph is the
  "press start, the demo begins" logic, and the E3 name is still on it.
- Zone `8EA4439E4ED170CB` (55 KB): `IsDemoAction`, `OnMissionAvailableAction`, `CheckMissionAction GAME_INFORMER_DEMO`, `MissionEnabledHelperAction`, a "Game Informer Demo" order list.
- Zones `956375ED418842B0` and `9C481B7730C68030`: `CompleteMissionAction` / `CheckMissionAction` for `GAME_INFORMER_DEMO` and a `UIMissionCompleteAction` (the demo end).
- Config `9A27689105CFFEEE`: `MissionLocks` and `MissionOrderListOverrides` for the order list.
- exe classes: `SpecialDemoLobbyAction` (vtable `exe+38a0638`, first slot `exe+7e4dc0`), `IsDemoAction`, `DemoFullscreenImageAction`, `DemoOverlayTrigger`, `DemoTriggerOnce`,
  `DemoBranchStartNode` / `DemoBranchEndNode`, `DemoStartNodes` / `DemoEndNodes`, `DemoVanityLoadouts`, `DemoBoundaryEdgeModel`, `IsDemoPriority`.
- **The button:** `UIE3DemoButtonPressEvent` (vtable `exe+3bec9e0`, constructor-ish at `exe+14115d0`, handler slot at `exe+140f050`) and the HUD element **`PlayerHudE3Demo`**
  (HUD root offset **+0x1c8**, named `E3Demo` in `docs/internals/ui-recordings.md`'s element map). `HUD/E3Lobby.gfx` (B99CD52DBCEE430D, 31 KB) is a small image-slideshow movie
  (`E3Lobby_I1.tga`..`I4.tga`, frames `intro`, `image_01`, `image_02`, `outro`), managed by `HUD.managers.E3LobbyManager`.
  **[I]** the press-X moment is a HUD button that raises `UIE3DemoButtonPressEvent`, which the graph above relays; the exact wiring is not traced.

## 4. What is not there

- No `start_menu_02` cinematic (`cinematics/start_menu_02/...`, `_exit`, `_jump`, `_start`, `shot2/shot3` animclips: all absent from the toc). The only trace of "02" is audio in a
  soundbank config: `Play_start_menu_02_sfx_whoosh`, `Play_start_menu_02_fly_sm_01/02/03`, and `Play_FOL_Player_Start_Menu_SM_Hands` / `SM_Cloth`. That is what a jump-off-and-swing
  shot would sound like. The picture was cut from the PC release or lives under a name we did not guess **[?]**.
- No separate E3 asset set. The asset names say **Game Informer Demo**; the E3 name survives only in the audio signal and the HUD classes. The E3 show-floor build and the Game Informer
  demo were evidently one demo path **[I]**.

## 5. How the demo path is gated (open)

- `LobbyShellUI` (`exe+7a5f10`) calls the movie's `setDemoMode(flag)` with **a global byte at `exe+7c8becb`**; seven other functions read it (`exe+712495`, `exe+712679`,
  `exe+72b583`, `exe+758329`, `exe+75f9e0`, `exe+7c6816`, `exe+7dda7b`) **[RE]**. No static write was found (scanned `mov [rip+x], imm8` and `mov [rip+x], r8`), so it is set through
  a pointer, a settings loader or a build flag **[?]**.
- `TitleUI@GUI` (vtable `exe+389b0c0`, update at `exe+7a8320`, state at `this+0x370`) calls the option loader `exe+7adc60` when its state goes 1 -> 2 (callers `exe+7a8372`, `exe+7a8494`).
  The loader fetches the config by asset id and hands it to `exe+7ac600`. We have **not** read what `exe+7ac600` does with it, or whether it ignores the list when the flag is 0.
- **Update 2026-11-20 (offline trace, still [RE]).** `exe+7ac600(this, config, index)` is "activate option `index` of the list": it switches on the option's type hash (`0x36e53d84`,
  `0xcf3770d5` = the level/checkpoint/mission entry read from `[rsi+0x30..0x7c]`, `0xd3bc7ad5`, `0xdb57a3d3`, `0xf59ffec1`). `exe+7adc60` calls it with **index 1** ("Open World"), so the demo
  **auto-starts option 1 with no menu** when `TitleUI` (update `exe+7a8320`) reaches `exe+7a8494`: after the start-screen press is handled it checks a zero-initialised byte at
  **`exe+73cf03a`** (read at `exe+7a847f`, `exe+7a8007`, `exe+7ac8e5`) and a per-instance byte at `this+0x16a` (not yet done). So the press-start handler is the E3 "press X and go straight into the
  city" path, gated by `exe+73cf03a`. Both `exe+73cf03a` and `exe+7c8becb` live in `.data` but are zero in the file (set at run time, no static writer found; the `StartDemoGame` string is only
  in a name table at `exe+5fbc250`). The retail menu (user report 2026-11-20): Spider-Man **perched on the side of a pole overlooking the city**, so the phone scene is demo-only.
- The movie callbacks involved: `onPressStartScreen` (string at `exe+389add8`, used at `exe+94874`), `setDemoMode`, `getDemoMode`, `getDemoType`.
- The system menu config (`configs/UIConfig/UISystemMenu.config`) still has `ShowInDemo` / `DemoOnly` item flags and a `PAUSE_OPTION_RESTART_DEMO` entry. These are the demo's pause-menu rules.

## 6. Ways to bring it back

| # | Idea | Cost | What it needs | Honest odds |
|---|---|---|---|---|
| A | **Turn the demo path on.** Set the byte at `exe+7c8becb` early (a script starts with the game) and see whether the title screen shows the "The Raft / Open World" list | Small: one native poke, one look | Whether `exe+7ac600` honours the list; a safe save slot | Good for *seeing* the leftover. The checkpoint and mission data are in the archive; whether they still run on 4.0630 is unknown |
| B | **Data-only: edit `TitleScreenDemoOptions.config`** (Overstrike) to point at our own checkpoints, labels and `SaveSlotId`, e.g. an "Endless Arena" entry at the Raft | Small once A works | Overstrike `.stage`/config replacement; `config2json` / `json2config` exist | Only useful after A shows the list is live |
| C | **A new "Press X to start" screen after load** (script-only): when the save finishes loading, hold the camera on the phone scene (`start_menu.cinematic2` via a Cinematic2 action or a spawned `cinematic2actor`), wait for the button, then release the camera. Works on retail without the demo flag | Medium-large: we have **no cinematic-play native** yet and need the camera/HUD-lock handling | A way to play a cinematic2 asset; hero hidden and invulnerable during it; input capture; the notes on cinematic-safe actors | Realistic. It would not be the demo's seamless start (a load still comes first), but "perched, phone, press X, drop in" is achievable |
| D | **The jump-off.** A short custom shot: hero plays a jump/leap anim from the perch into the swing-start | Large | A new animclip is not possible; reuse an existing traversal state (`HeroState*` leap from a perch) driven from script | Possible as an in-world move, not as a cinematic |

Recommendation: **A first (it answers "is the demo path alive?" and costs almost nothing), then C if the user wants the effect on a normal save.** B and the Raft entry follow from A for free.

## 7. Next steps (one at a time, nothing run yet)

1. ~~User, in game: look at the normal main menu.~~ Done: perched on a pole, no phone, so the phone scene is demo-only.
2. ~~Agent, offline: trace `exe+7ac600`.~~ Done, see section 5. Still open: what `exe+7c8becb` changes beyond `setDemoMode`.
3. **Agent, offline:** trace `UIE3DemoButtonPressEvent` (`exe+140f050`, `exe+14115d0`) and `SpecialDemoLobbyAction` (`exe+7e4dc0`) to see which node and signal consume the press.
4. **User, in game, after backing up saves:** poke the byte (new dev command in the Endless Arena command file) before the title screen, and report what the title screen shows.
5. Decide A/B vs C from steps 1 to 4.

## Facts to remember

- The scene's lighting asset is named `atm_gi_demo_menu_day`; the menu zone also carries rain-drip and steam VFX. How they combine on screen is unknown until it is seen rendered **[?]**.
- The demo option list is the only place the game names `CHK_GAMEINFORMER_DEMO_START`, `GAME_INFORMER_DEMO` and `CHK_PRISON_RYK_EXIT_CIN` so far.
- Do not commit extracted assets (AGENTS.md rule 1). The scratch extractions live outside the repo.

## 8. First in-game test (built 2026-11-20, not yet run)

`poke` in the Endless Arena command file now takes a `u8` type (a u32 write would clobber the neighbouring bytes).

1. Back up saves (the demo option names slot 1).
2. Launch the game, and at the title screen (before pressing start) write to `EndlessMode.cmd`: `hook`, then `poke exe+73cf03a u8 1`. Read `EndlessMode.log` for `poke ... ok`.
3. Press start and report what happens: normal menu, demo options, a load into the city, or a crash.
Whether `hook` installs at the title screen (before a world exists) is itself untested.
