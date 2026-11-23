# UI: Scaleform, the pause menu and mod tabs

## UI (Scaleform)

The HUD is a Scaleform (Flash/ActionScript) movie. We drive it from code the way the game's own script actions do.
Full chronology, with every recording: [ui-recordings.md](ui-recordings.md). Summary of what works:

### HUD root and elements
- Root for player i: `qword [exe+7c2b5a0 + i*8]` (a static `GUI::PlayerHUD`). Elements hang off it at fixed offsets:
  `0x188 Ammo, 0x190 Weapon, 0x198 Reticule, 0x1a0 Message, 0x1a8 POI, 0x1b0 ProgressBar, 0x1b8 QuickSelect, 0x1c0 Health (hosts the activity panel), 0x1d8 InteractButton, 0x1e0 SuitPower, 0x1e8 Activities (don't call blind), 0x208 CountDown, 0x210 Mission, 0x218 Counter, 0x240 Communicator`.
- Calling ActionScript: `exe+1d1cee0 Invoke(movie, &as_object, "Method", result, args, count)`. We address a method as
  "root offset : AS object offset" (e.g. `1c0:6e8` Refresh). `ui::Call` / `gfx::Invoke`.
- The movie wrapper is `[[root+0x1A0]+0x300]+8`; `+0x18` of that is the GFx Movie. `gfx::Ready()` self-checks before any call.

### Building GFx values from code (`gfx.cpp`)
- A value is 0x30 bytes: `+0x00/+0x08` links of the movie's list of live managed values (the game initialises them to all-ones),
  `+0x10` interface, `+0x18` type (2 bool, 3 int, 4 uint, 5 number, 6 string, 8 object, 9 array; `0x40` flag = managed), `+0x20` data.
- Create an object with exe+319f7d0, a string with exe+319f7e0, set fields via the object interface `SetMember` (vtable +0x28), push to
  arrays via the array interface, then pass the object as the single argument of a method. `gfx::Node` builds trees of these.
- **The movie's teardown (exe+319b340) walks the managed-value list and releases every node.** A Value you registered and then
  destroyed without releasing leaves a dangling node and crashes the game on the next HUD reload (crash at death, 2026-10-15,
  fixed by keeping captured values on the heap and never freeing them).
- `SetMember` hook at exe+3529e30 (prologue `40 55 53 56 57 41 54 41`): used by `uilog` to record data objects and by the text
  capture to learn which object holds which localized strings.
- **Text capture**: the HUD loads its fixed texts (button labels, titles) into per-panel objects at load time. `gfx::InstallTextCapture`
  remembers them (call it before the HUD loads), and `SetText`/`RestoreText` change one by key, e.g. relabelling RETRY to START
  (`CHALLENGE_GEN_BTN_TRYAGAIN`). `gfx caps` lists captured objects. Stale captures are detected when the managed flag is gone.

### Panels and messages that work
- **Notification box**: `exe+7793d0 Show(hud, slot, text, seconds, 0,0,0,1)`, `exe+77a1e0 Clear`. Slot 0 is the general box.
  It renders button glyph tokens like `[BTN_Y]` and `<span class='emphasis'>`. `hud::Show`. **[verified]**
- **Medium message** (top banner): `exe+77db70(element, type, text, data, 1, 0, 0.0f)` on the Message element. Type 7 = custom text;
  21 = wave banner `{icon, currentWave, maxWave}` (the movie prints "WAVE n", "FINAL WAVE" when current == max);
  13 = base-complete `{icon}` + label; 6 = level up. `hud::Banner`, `gfx::Message`. **[verified]**
- **Big countdown** `208:30 ShowBigCountDown(int)` / `HideBigCountDown`; **counter** `218:30 ShowCounter("ENEMIES", cur, max, ...)` / `HideCounter`. **[verified]**
- **Crime pop-up** `210:38 ShowCrimeScanner(title, subtitle, 2, 5, array)`; hide with `HideMission()`. Used for boss entrances. **[verified]**
- **Activity panel** `1c0:6e8` with `Refresh(obj)` / `HideActivity(obj)`; `obj = {activity, panelType, panelData}`:
  - activity 3 (hideout): panelType 0 = **preview on the circle** (label, icon, time, enemiesDefeated, subObjectives `{label, currentCount, maxCount, reward}`, rewards); 1 = in progress; 2 = completed.
  - activity 1 (challenge): panelType 0 preview; 1 = **live score** `{type 2, label, icon, score, scoreItems}`; 2 = **results** `{label, icon, score, highScore, summaryData[{label,value}], rewards, noRewards, awardedRewards, rewardTiers, taskMaster...}`.
  - A completed subObjective has `currentCount == maxCount`. `HideActivity` must use the same activity as the panel you showed (we hid the wrong one once).
  - The panels animate on game time. Freezing the world mid-animation leaves them translucent; freeze only after they have faded in.
  - **Results panel data shape (found by disassembling the panel's ActionScript, 2026-10-16, `tools/sigtools/abcdump.py`):** `rewards` must be an
    **array of arrays**, one inner array per tier (`rewards[tier][0].value` is read, `rewards[tier][0].icon` shown). A flat list makes
    `ActivityChallengeAfterAction.Refresh` throw (`undefined.value`) as soon as `highScore > score` and `highScore >= rewardTiers[0]`.
    A throw aborts the rest of Refresh silently, so the tier "min score" labels keep their placeholder 500, the `intro` animation never plays
    (the panel stays on the hidden end frame: invisible from the second time on, flashing only when `HideActivity` plays the outro) and
    the delayed `ShowBtnHint` (which sets the Continue and Retry hints) is never scheduled. Other fields it reads: `label`, `icon`, `summaryData`,
    `score`, `highScore`, `rewardTiers` (3 numbers), `rewards`, `noRewards`, `awardedRewards` (flat list shown in the reward list when non-empty).
    The Taskmaster progress fields are not read by Refresh. `ShowBtnHint` always sets both a Continue (bottom face button) and a Retry (top) hint.
- Localization strings the HUD uses: `HUD_WAVE`, `HUD_TASK_COMPLETED`, `CHALLENGE_GEN_BTN_CONTINUE`, `CHALLENGE_GEN_BTN_TRYAGAIN`,
  `HUD_TASK_COMPLETE_REWARDS`, `HUD_TASK_COMPLETE_BONUSOBJECTIVES`, `CHALLENGE_GEN_RANK_*`, and more (full list in UI_HUNT).

### Not found / dead ends
- Map icons: icons come from `UIMinimapConfig` (element Type -> icon) and static tables; no UI call adds them. Abandoned.
- `ShowActivityMessage` and `ShowAwardTrackerToast` don't exist in any HUD movie (leftovers). `ShowAlert` needs menu state.
- A near-fullscreen pop-up for reward picks (to show the suit menu's video/picture) has not been found.

## The pause menu and mod tabs in Settings (2026-11-08; `common/modmenu/`, `docs/internals/atmosphere-and-menu-recon-log.md` Parts 5-9)

- The Settings tab method is SpidyVR's (see `THIRD_PARTY_NOTICES.md`): `OpenOptions` `exe+7dd9f0`, tab builder `exe+807740`, AddItem `exe+80a220`, callback dispatcher
  `exe+7dc400`, text lookups `exe+1749970` / `exe+1749ae0`. `common/modmenu/` appends one tab per mod after the game's own tabs on the Settings screen (13 hooks, host chosen through a named shared object). A separate MODS entry on the pause menu's first page was built and verified too, then dropped on 2026-11-09 in favour of Settings tabs (`kModsPageItem` in `modmenu.cpp`).
- The Options movie takes the tab-list banner from `_locStrings.PAUSE_OPTION_SETTINGS` (it stays "SETTINGS") and the in-tab header from the tab's `menu_header`.
- The pause menu **closes itself on a lighting change** by sending Flash `CloseMenu` from `exe+7dc06b`; swallowing that call for a few seconds keeps the menu open.
- Scaleform `GfxValue` / ObjectInterface vtable slots: 2 Release, 5 SetMember, 15 PushBack.

