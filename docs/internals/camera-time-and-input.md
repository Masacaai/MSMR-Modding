# Time, camera and input

## Time, camera and input

- **Time scale**: the `TimeScaleSystem` static at `exe+621b890` (current scale at `+0x18`). Start a scale change with exe+19bbaa0
  `(sys, channel, target, rate, name, mode)` on channel 5, as `TimeScaleAction` does for scripted slow motion. `timescale::Set`,
  `Freeze` (not exactly 0, to avoid divisions), `Unfreeze`. Used for reward-pick freeze, the abandon prompt, and slow-mo at death. **[verified]**
- **Screen fade**: `ScreenFadeAction` exe+205bb20 -> `MsgSend` exe+2142440 with message types at `exe+7c50c80` (fade out) and
  `exe+7c50c78` (fade in), payload `{float seconds, u8 0}`. `hud::FadeOut/FadeIn`. **[verified]**
- **Camera**: its address was never found directly (`camscan` finds float3 positions by memory scan). Spawn logic uses the hero position plus a
  camera cone of about 78 degrees, taken from the hero heading, to keep spawns out of view. **[?]** for exact FOV.
- **Input**: keyboard via `GetAsyncKeyState` (game focused only), Xbox pads via XInput (also PlayStation pads exposed through Steam Input),
  PlayStation pads over HID (`psinput.cpp`: face buttons `0x10` square, `0x20` cross, `0x40` circle, `0x80` triangle on one byte, D-pad hat in the low nibble).
  `input::Glyph/Name` name the button for the last-used device.

## Input: the game's action layer, for every device (2026-11-15; `[RE]`, read from the exe, not run)

Answers FOF-A1/A2/A3/A5 (partly), EA-F3, MSC-D2/D6. The devices (`nx::Input::GamepadSce`, `GamepadXInput`, `GamepadWindowsInput`, `GamePadSteam`, `NxKeyboardDevice`,
`NxMouseDevice`) feed **device maps** (`PadMap`, `KeyboardMap`, `MouseMap`, `TouchPadMap`; `KeyboardPriorityMask`, `MousePriorityMask`), which feed **named actions**. Game code
never reads a button: it reads an action by name hash from the player's **input controller**.

**Logical buttons.** One enum (type `0x7b87880c`, 121 values) covers every device: 0 kNoPadButton, 1-4 kUp/kDown/kLeft/kRight, 5-8 the face buttons, 9 kL1, 10 kR1, 11 kL2,
12 kR2, **13 kL3**, 14 kR3, 15 kStart, 16 kBack, 17-22 the stick axes and strengths, 23-44 `kPadOnly*`, 45-60 `kMkbOnly*` (57 = kMkbOnlyL3), 61-62 Steam gyro, 63-66
`kMkbLeftStick*`, 67-76 `kMkbCustom00..09`, 77-105 `kKeyboard*`, 106-120 mouse (deltas, wheel, buttons, double clicks). Keyboard and mouse bindings are expressed as these
logical buttons, so "the keyboard equivalent of L3" is whatever the player bound to logical `kL3`: reading the *action* gets it for free. Raw keys are a second enum
(type `0xa832eb64`, 147 values `kKeyNone`..`kKeyMouseRightDoubleClick`). Print either with `fx.py enum "^kNoPadButton$"` / `"^kKeyLeftControl$"`.

**Actions.** Each input context is a map named `<X>BindingsConfig` whose actions are an enum named `<X>Control` (`fx.py enum "Control$"` lists the sets). The main ones:

- `HeroControl` (58): kFire, kReload, kJump, kMelee, kMeleeAndJump, kRoll, kFallDive, kDodge, kSprint, kSwing, kWallCrawl, kWallCrawlCorner, kWallRun, kWebZip, kWebSnap,
  kWebTunnel, kWebZipVent, kWebZipToObject1/2, kWebShooterPellets, kCombatAltFunction, kWebYankBot, kWebYankSpecial, kWebStrike, kWebStruggle, kWallPerch, kBeamAttach,
  kSpiderTracer, kAimMode, kSpiderAssault, kActiveFinisher, kSpiderSuitAbility, kSlottedMelee, kExit, kLook, kInventory, kObjectivePing, **kWeaponWheel, kWeaponWheelAlt**,
  kWeaponPrevious, kWeaponNext, kUseGadget, kEquipGadgetLeft/Right/Up, kHeal, kHeroTouchpadX/Y(+Pinch2), kHostageTalkdown, kHostageAgitate, kPlaceIntelTower, kQTEFire,
  kAccessibilityShaderToggle, kTrickMode, kQTEWebShooterLeft/Right. Also `HeroControlShared` (9), `HeroExternalControl`, `HeroInventoryModeControl`, `HeroTravelMapControl`.
- `HeroQuickSelectControlPrimary`: kWeaponWheelX, kWeaponWheelY, kWeaponWheelMouseDeltaX, kWeaponWheelMouseDeltaY (the gadget wheel's stick, on any device); `...Alt`: kAltWeaponWheelX/Y.
- `CameraInputControl` / `CameraMoveInputControl` (kCameraInputX/Y, kCameraInputGyroX/Y, kCameraMoveX/Y, kCameraRaise/Lower, kCameraFast/Slow, kCameraZoomStick), `kMoveX`/`kMoveY`, `kInteract`.
- `SystemControl`: kSystemPause, kSystemPause2. `ScaleformControl` (43) and `ScaleformRTTControl` (32): menu navigation (kGUIUp/Down/Left/Right, kGUISelectButton, kGUIPopupExit...).
- `PhotomodeControls` (16), `PhotomodeCameraInputControl` (6). `GameControl` (29): kQuit, kHeroMouseToggle and the developers' **free cameras** `kFPSCam*` and `kMayaCam*`;
  `DebugCamera*Control`, `MarketingCameraInputControl` / `MarketingCamOptionInputControl` (a "marketing camera" with 14 options): leads for a mod free camera.
- **`PrototypeControl`** (20): kPrototypeX/Y/A/B, Up/Down/Left/Right, R1/R2/R3, L1/L2/**kPrototypeL3**, Start, Back, MoveX/Y, CameraX/Y, each with its own one-binding map
  (`PrototypeL3BindingsConfig` descriptor `exe+7c16220`, set up at `exe+1f3050`, ...): **generic per-button actions the game ships and does not use**, i.e. a ready action for a
  mod's "hold L3" that is device-agnostic.

`recon/input_actions_4.0630.txt` lists the 118 action hashes game code reads, with the functions that read them.

**Reading an action** (what the gadget wheel does at `exe+792392`):

```
ctrl = q[HeroLocal + 0x50]                      // the hero's input controller (HeroLocal type object exe+6da9d90). The HUD reaches the same kind of object as [owner+0x48]+8.
struct ActionRef { void* ctrl; void* cached = 0; u32 generation = 0; u32 nameHash; } ref = { ctrl, 0, 0, crc32("kWeaponWheelX") };   // hash at +0x14
a = exe+169a690(ctrl, &ref)                     // action state or null (hash map at ctrl+0x258, array [ctrl+0x250], generation ctrl+0x1f0)
value   = a ? exe+1697fa0(ref.ctrl, a) : 0      // float: sum of the bound sources' current values (sticks -1..1, buttons 0/1, triggers 0..1)
pressed = a && exe+1698d10(ref.ctrl, a, null)   // bool: went above the threshold this frame (threshold pointer, default float at exe+6050ec0)
```

A bound source is `{... +0x0c flags (bit 0 = suppressed), +0x10 current float, +0x14 previous float}`.

**Contexts.** `exe+1695a10(ctrl, &mapDescriptor)` adds a map (it builds "Keyboard Device Map", "Mouse Device Map", "Pad Device Map", "TouchPad Device Map" for it);
`exe+1696d20(ctrl, mapId) -> bool` = present; **`exe+169a720(ctrl, mapId, bool enable)`** switches a map on or off. The controller keeps its maps as 0x50-byte records at
`[ctrl+0x218]` (count `+0x214`, map id at `+0x8`). A descriptor's first dword is its id (CRC-32 of the config name). The game locks the hero's input by disabling the hero map:
`exe+83c350(HeroLocal, bool)` calls `exe+169a720(ctrl, d[exe+6da7370], enable)`. The gadget wheel adds and enables `HeroQuickSelectPrimaryBindingsConfig` (`exe+6da94c0`) or
`...Alt...` (`exe+6da95c0`) while open; that, plus the ability lock, is how it takes the stick away from the camera. Config names in the exe: `HeroBindingsConfig`,
`HeroBindingsConfigShared`, `GameBindingsConfig`, `SystemBindingsConfig`, `CameraInputBindingsConfig`, `ScaleformBindingsConfig`, `PhotomodeInputBindingsConfig`,
`HeroTravelMapBindingsConfig`, `HeroInventoryModeBindingsConfig`, `HeroLockWithExitBindingsConfig`, `LoadScreenBindingsConfig`, the 20 `Prototype*BindingsConfig`, ... (62 in all).

**Recipes [not run]:** (1) *Interact on every device, Steam Deck included*: read the action `kInteract` as above instead of raw buttons. (2) *A mod's hold-L3 that is a real
action*: `exe+1695a10(ctrl, exe+7c16220)` once, then poll `kPrototypeL3`; while your UI is open, disable the hero map with `exe+169a720(ctrl, d[exe+6da7370], 0)` (and
re-enable it on close) so the hero does not act on the same press. (3) *Wheel selection with the right stick on any device*: read `kWeaponWheelX/Y` (or `kCameraInputX/Y`).
(4) *Editor cursor*: `kMoveX/Y`, `kCameraInputX/Y`, `kGUIMouseMoveX`.

**Glyphs and device type.** Prompt icons are resolved by `IconMap*` data: `IconMapActionControl{Action}` (glyph of an action), `IconMapPadSpecific` with one child per device
family (**X360, XOne, DualShock, DualSense, SteamDeck, SwitchPro, Generic, Mkb, Steam**; flags `UseGamepadForMkb`, `UsePhysicalGamepadForSteamInput`), `IconMapSingle{IconId}`
(enum `IconId`, 203 glyphs), `IconMapXOSwap`, `IconMapStickSwap`, `IconMapMkbBinding{Button}`, `IconMapSteamBinding{Button, ActionSet {Active, InGame, Menus}}`. Where the active
family is stored was not located. `OSKeyboard` (vtable `exe+3cf8518`) and a Steam `GamepadTextInputDismissed` callback exist: the game can open an on-screen keyboard.

Not examined: the pad backends' axis storage and dead zones, per-user slots in `nx::NxInputImpl` (78 methods; FOF-A4), where bindings are saved.

