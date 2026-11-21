# Requests for Fable: reverse-engineering wish list (opened 2026-11-14)

The maintainer plans a session with Fable (the heavy reverse-engineering model) on **2026-11-15**. Each agent working in this repository adds **one section** below with
its goals and everything it wants discovered and documented. Fable works through the sections, in priority order, and writes the answers down.

**Game build: Spider-Man.exe 4.0630.0.0 only.** All `exe+RVA` below are for that build. Tools in the repo: `tools/sigtools` (rtti, disasm, owner, xrefs, funcstrings, vt2name,
dumpq, sigscan, findimm, strings), `tools/ALERT` (asset formats), `tools/toctool` (`hash`, `check`, `grepall`), `tools/assets/extract.py`, the mods' command-file protocol
(write lines to `scripts\<Mod>.cmd`, read `scripts\<Mod>.log`), `vtfind`, `snap`/`snapdiff`, `wp` (hardware write watchpoints) in the Friend or Foe mod, and Endless Arena's
`uilog` (records every ActionScript call the HUD makes). The user plays the game when an in-game step is needed.

## How to read this file, how to answer

- Each request has an id (`FOF-A1`...), a priority (**P0** blocks the next milestone, **P1** big quality win, **P2** needed for later milestones, **P3** long shot), why it matters,
  what is already known (with where it was read), and what to find. **Do not limit yourself to the wording**: if you see a better question while digging, answer that too.
- **Verified means read from the exe or seen live.** Mark every answer `[V]` (verified from code or live memory), `[I]` (inference) or `[?]` (unknown). Give RVAs, struct offsets,
  signatures (one hit in `.text`; the mods use `common/scan`) and how you checked.
- Write answers as numbered sections in the requesting agent's own recon file, and put a one-line pointer under the request here (`Answer: <file> section <n>`). For Friend or Foe
  that file is `FriendOrFoe/docs/RECON.md` (kept local and out of git until the mod is ready; it holds game-derived addresses).
- Do not use leaked Insomniac material (see `docs/REVERSE_ENGINEERING.md`). Public formats and our own analysis only.
- Other agents: copy the template below at the end of this file.

```
## <Agent / mod name>
### Goals
### Current state (what works, what is proven, pointers)
### Requests
#### <ID>-<n> <title>  (P?)
Why / Known / Find / Verify
```

---

## Friend or Foe (companion allies, ally wheel, character switching, co-op)

### Goals

**Friend or Foe** is a companion mod for Marvel's Spider-Man Remastered, built on Overstrike `.script` support (native DLL, `common/` layer, no Script Hook). Goals, in the maintainer's order:

1. **An ally wheel**, native-looking, opened by **holding L3** (PlayStation; the equivalent on Xbox and on keyboard and mouse), built on the game's own input system so it works on every
   device. It summons allies (Silver Sable first; then Black Cat and others) and sends them away. Summoning an ally who is already out must not clone her: teleport or respawn.
2. **Allies that behave like real companions:** follow the player (walk, run, silently teleport when far or stuck), fight on their own when the player fights (Sable already does),
   can be directed at targets, have idle and locomotion animations, react with voice lines (Spidey and the ally banter, no uncanny silence), survive cutscenes/zone changes sensibly.
   The maintainer wants Sable to keep her aggressive personality (she charges in, never takes orders as a soldier would).
3. **Switch to and play as an ally** (Lego-games style character switching). Needs custom input maps for all the character's actions and heavy testing.
4. **Split-screen co-op:** two players at once on one machine, different characters, either free to switch to any character not in use; possibly two Spider-Men if the game allows it.

### Current state (2026-11-14), so you do not redo it

Everything below is in `FriendOrFoe/docs/RECON.md` (sections in parentheses). **Proven in game:**

- `ally_silver_sable.actor` (`character\enemy\enm_riflethug\`, asset id AAA8F8C69CB49BE5) spawns with Endless Arena's spawn code, is `kBlueTeam`/`IsAlly`/`kImmortal`, and fights enemies on her own outside any mission; with no enemy she stands in a static pose and does not follow (1 to 10).
- **Bot commands work.** The game's `BotCommandQueueGame` (needs the three job hosts Sable has) accepts a `BotCommandGoto` pushed with `exe+1d966d0`; Sable walks to the point (13, 16, 17). Fields: destination vec3 at command `+0x5c`, **arrival radius float at `+0x70` (uninitialized = the command ends at once)**, link id at `+0x50`. A running Goto copies its destination at start; rewriting it does nothing (24).
- Her movement mode byte (`movement +0xa0`, 0..6 = index of the style in her `SilverSableMovementConfig`: Walk, Search, Run, Strafe, Panicked, Wounded, Jog) is set to **2 (Run) by the game itself** when she is far; our loop's forcing it back to 0 was the visible twitching (23). `BotCommandChaseRunTo` moves her without animation ("glides") and never finishes (19).
- Silent teleport by writing the transform (`[entity]->+0x30`) works; facing is rows 0 and 2 of the transform (row 2 = forward, row 0 = (fz,0,-fx)) (12, 22).
- **Input:** all gamepad backends call one shared function `exe+1d0d5e0(device, pressed, state, id)` with ids `0x190..0x19d` (L3 = `0x196`, R3 = `0x197`, verified on a DualSense); hooked, it gives hold-L3 on every pad (27, 27b). Keyboard/mouse is a separate path, not done.
- **Wheel UI:** the game's gadget wheel is Flash at HUD root+0x1b0 (AS object +0x4f8; the C++ object is `PlayerHudQuickSelect` at root+0x1b8, vtable exe+3894040). Recorded: `SetQuickSelect(array of {guidLeft, guidRight, codeName, name, iconName, currentAmmo, maxAmmo, rechargeAmmo})` once at HUD load, then `OpenQuickselect(n)`, `HandleWheelInput(n)`, `CloseQuickselect()` (28). Our made-up entries were ignored by Flash; build 12 borrows real gadget ids and icons (28b). Not yet tested.
- Still broken: following chases old positions (re-targeting did not visibly work), a Goto stalls when the target is off the navmesh (a command can hang forever), finished commands pile up as components on the entity (21 seen) (25).
- Cut Insomniac co-op is **still in the game**: exe `HeroCoopSpawnPoint`, `CoopWarpAction`, `UITitleScreenOptionHost/JoinCoopGame`, `CoopHeroCharacter`, `MaxHPCoop`, `kCoopOnly`..., data `levels/I20_City/overlay/MultiplayerMission/COOP_BC_*`, checkpoints `CHK_COOP_*` (8).

### Requests

Priority legend: **P0** = blocks the wheel milestone, **P1** = big quality win for allies, **P2** = needed for switching/co-op, **P3** = long shot.

#### A. Input system, device-agnostic, and the wheel's controls

**FOF-A1 (P0) Keyboard and mouse path.** *Why:* the wheel must open on KB/M with the "equivalent of L3". *Known:* RTTI `nx::Input::NxKeyboardDevice` (vtable exe+4f444e0, one live object), `NxMouseDevice`; keys are `kKey*` enums (`kKeyLeftControl`, `kKeyMouseLeft`...). *Find:* the keyboard/mouse equivalent of the shared pad setter `exe+1d0d5e0` (the function every key event goes through, its state array layout, "held/pressed/released" bits like the pad's), the key ids, **which key the game already maps to L3's action (its sprint/focus-style control)** and which keys are free; how raw input (`RegisterRawInputDevices`, `GetRawInputData`) reaches it; mouse buttons and wheel; the device-switch logic (the game changes button glyphs when the player switches device: where, which flag).
*Verify:* hook it, log keys.
Answer (2026-11-16 update): docs/GAME_INTERNALS.md 'Input: the game's action layer' (logical buttons cover keyboard/mouse; read actions, not keys).

**FOF-A2 (P0) The action layer above the buttons.** *Why:* we want our hold-L3 to be a proper game *action* (shown in the Controls menu, rebindable, swallowing L3 so the hero does not also act on it) instead of a side hook. *Known:* action names in the exe (`ACTION_WEB_YANK`, `WEAPON_WHEEL`, `WEAPON_WHEEL_ALT`, `SUIT_POWER` = L3+R3, `PING`...), `kInputAction(Prius)`, `InputSystemGame` (static at exe+5da0bf0, getter exe+b9d170, 5 callers: `HeroCameraManager` acquire/release `exe+1f2c3f0`/`1f2c500`, `GameModeControl`), `HeroButtonPressListenerAction`, `OnButtonPressedAction`, `InteractButtonListener`. *Find:* how a button becomes an action (tables, config assets: `configs/...`/pad maps, `!MISSING_PADMAP_ICON!` hints at pad-map assets), where the hero reads its actions per frame (the function that answers "is action X held"), how to **register a new action with default bindings for every device** and show it in the Controls/Settings menu (Spidy and ModMenu already add rows to Settings: `ModMenu/`), how to **consume** a button so the game ignores it while our wheel is open, how the real gadget wheel blocks other input and slows time while open (which systems: time scale channel, input context, camera).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 36 (leads only: HeroButtonCache, input lock; not answered).
Answer (2026-11-16 update): docs/GAME_INTERNALS.md 'Input: the game's action layer' (action read calls, map enable/disable, unused kPrototypeL3 action).

**FOF-A3 (P0) Sticks and triggers, all backends.** *Why:* wheel selection by the right stick. *Known:* XInput axis function `exe+1d16230(device, idx, x, y)` stores processed floats in the pad object (idx 0/1 triggers `+0x48/+0x4c`, idx 2 stick A `+0x50/+0x54`, idx 3 stick B `+0x58/+0x5c`); the DualSense backend calls `exe+1d0df50(device, idx, ...)` with bytes (0..255). Not checked live. *Find:* verify which stick is which and the sign of y on XInput, DualSense, Steam Input, Windows.Gaming.Input; where the DualSense backend stores its processed values (same base offsets?); dead zones and curves; the game-level "right stick" read (camera) so we can read exactly what the camera reads. Also the **WGI backend** (`GamepadWindowsInput`, vtable exe+4f3c148; no live object seen) and `GamePadSteam`: which one is active on which setup (the game "prefers Windows.Gaming.Input"; ArkWeb notes a registry switch).

**FOF-A4 (P1) Multi-user input.** *Why:* the co-op goal needs a second pad driving a second hero. *Known:* four `GamepadSce` and four `GamepadXInput` objects live; `NxInputImpl` (vtable exe+4f3c1b8, `INxInput`, about 40 methods) has per-slot tables (20 entries of 20 bytes at `this+0x690`, accessors exe+1ce0930/0950/0970, 1ce1e30/1e50/1e70) and getters at `this+0x81c`/`0x820`; HUD root table `[exe+7c2b5a0 + i*8]` suggests per-player HUDs. *Find:* the full `INxInput` method table with names/meaning, how users, devices and slots are assigned (and who calls it), how the second pad would be bound to a second "user", per-user action state, rumble/light-bar per pad, what the title-screen's co-op join does with input, whether an actor can be told "read input user 1".

**FOF-A5 (P1) Device type and prompt glyphs.** *Why:* the wheel prompt should show the right icon. *Known:* movies know `DEVICE_DUALSENSE/DUALSHOCK/MKB/XBOXONE/STEAMINPUT/STEAMDECK/XBOX360/SWITCHPRO/GENERIC`, `[BTN_L3]` tokens render the active device's glyph. *Find:* where the active device type is stored/queried (C++), how it changes with last-used input.

Answer (2026-11-16 update): docs/GAME_INTERNALS.md 'Input' (partial: icon maps per device family; active-device storage not found).
**FOF-A6 (P2) Controls menu entries and saved bindings.** Where bindings are saved/loaded; format; how a mod adds an entry safely.

#### B. The wheel UI (Flash `HUD/QuickSelect.gfx`)

**FOF-B1 (P0) Full API of the quick-select clip.** *Known:* methods seen: `SetQuickSelect(array)`, `OpenQuickselect(n)` (n = 0, 2, 5 seen: unknown meaning), `HandleWheelInput(4/5/6)`, `CloseQuickselect()`, `SetSpinnerRotation`, `GetSpinnerPositioning`, `SetAspectRatio`; `GadgetWheelPiece`, `wheel_piece_01..`, events `WHEEL_INTRO/SELECT/EQUIP` in asset BAFACEA48ACDC71C (the wheel's `.gfx`, extractable with ALERT; identifiers in `recon/ui/ui_export_hud_quickselect.gfx.identifiers.txt`). *Find (decompile the ActionScript with a SWF/GFx decompiler on the extracted movie):* the exact meaning of every `SetQuickSelect` field and its **validation** (why entries with unknown `guid*`/`codeName`/`iconName` are ignored and the old list stays; what keys into what), the meaning of `OpenQuickselect`'s argument, the codes of `HandleWheelInput` (slot index vs direction vs state), slot count and geometry (angle of each slot, max entries, how 1 to 8 entries are laid out), what `HandleWheelInput` highlights, how the movie reports the selected slot back (ExternalInterface/fscommand names) and how the C++ listens (`UIQuickSelectWheelEquip/Intro/Select` events), and the ammo/charge display fields.

**FOF-B2 (P0) The C++ side `PlayerHudQuickSelect` (vtable exe+3894040).** *Known:* `exe+793290` cycles up to 8 slots (`this+0x3b8 + i*8`, current index `this+0x450`), `exe+793320` builds entries from the inventory (items type 3, up to 16, per-entry builder `exe+7914a0`) and sends `SetQuickSelect` unless the count is unchanged, `exe+793730` is `Open`, `exe+793da0` sends `HandleWheelInput`. *Find:* a clean way to **add custom entries natively**: hook point(s) where we can inject entries and receive the selection, so the real stick logic, time slowdown, sounds and icons all work for free; the meaning of the `bpl` flag of `exe+793320` (we call it with 1 to restore the real list: confirm it forces a resend), the struct layout of the element (counts, slot tables), the open/close state, what the wheel does to time scale (`TimeScaleSystem` channels) and input.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 33 and EndlessArena/docs/RECON.md section 7.

**FOF-B3 (P1) Custom icons/portraits.** *Why:* allies need their own icons (we borrow gadget icons now). *Known:* `art/GadgetIcons.gfx` clips by `iconName`; HUD texture lists in `configs/UIConfig/UIHUD.config`; the movie has an `AssetLoader`/`AttachImages`/`CacheIcon`. *Find:* how to put a new image into a HUD clip at runtime (texture asset path, loader call, texture format), whether the game can load a texture from a loose file or only from the archives; how Overstrike `.stage`/`.smpcmod` can ship a new icon asset; the cheapest way to show a portrait of Sable and Black Cat in the wheel.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 33 (partial: runtime texture creation call).

**FOF-B4 (P1) Other HUD elements for allies.** An ally health bar or marker: `PlayerHudBossHealth`, `PlayerHudCommunicator` (portrait + text, `ShowCommunicator(name html, portrait path, 1)`), `PlayerHudTargetLock`, POI/`POI` map icons (`AddPOI`), `AllyXRayManager` (ally outline: `kAllyNoOutline`, `kAllyDark`, `AllyXRayTargetParameters`). Document which fits an on-screen ally name/health and a map marker, with the calls and argument shapes.

**FOF-B5 (P2) Banner/toast and button-prompt text** for "Silver Sable joined", with button glyph tokens (`[BTN_L3]`): the cheapest call that shows icons. (Endless Arena found the notification box and medium message; list the best call per use.)

#### C. Spawning, despawning, lifecycle

**FOF-C1 (P0) Despawn and the actor lifecycle.** *Why:* "send home" and "no clone on summon". *Known:* Endless Arena's `spawn::SpawnActor` works (steps: LoadAsset, SpawnRaw, CreateSyncObject, register handle, attach, activate; see `EndlessArena/src/spawn.cpp`, natives table there) and `DespawnActor` exists but fails on actors already gone (`despawn: FAILED` for dead/unloaded entities). *Find:* the engine's proper destroy path (names, args), what happens to the sync object, area/group membership and the 4,000+ slot sync table, reference counts of loaded assets, how to tell the actor is still alive (handle validation: `GetActor` semantics, handle reuse, generation bits), what the engine does to our spawned actor on tile unload, fast travel, mission start, checkpoint reload, death/respawn of the hero and cutscenes (does it vanish, freeze, stay behind?), and how to make an ally **persistent** (follows across zone loads) or at least cleanly re-summonable.

**FOF-C2 (P1) Spawn parameters.** Team/allegiance at spawn time, spawn flags (the log shows `flags=0a`), spawn-engaged vs not, health, initial state (stand/idle), spawn onto the navmesh, a placement helper "behind the camera, on the ground, out of view" (camera forward is available as the `FollowCamera` actor: `entity::Camera()` in Endless Arena), and the **dependency preload chain** for ally actors (weapon items, jetpack, heads: `tools/assets/deps.py` does it for enemies; check `ally_silver_sable` has no white cubes; produce the list for Black Cat and others).

**FOF-C3 (P1) Warp done right.** *Why:* our teleport is a raw transform write (works, no glitch, but the mover/nav state is not told). *Known:* `BotWarpProcessor` (component on bots), `ActorWarpProcessor`, `ActorWarpAction`, `CoopWarpAction` (the cut partner catch-up), `HeroWarpTarget`, `BehaviorTaskmasterTeleport`. *Find:* the game's own bot warp API (arguments, effects: clears path, resets mover, plays no fade), and whether it can place on the navmesh/ground; compare with the raw write.

**FOF-C4 (P2) Entity internals.** The `Entity` struct beyond `+0x00` transform, `+0x68` component array, `+0x70` count: ids, flags, `+0x58`/`+0x80` alternate component containers (seen in `FindComponent` helpers at exe+16799a0/167ab40), handle <-> entity table `[exe+7a44380]` (0xC0 per entity), a safe component lookup by type, component add/remove lifecycle (`exe+159cb80` AddComponent: what registers the component with its system; what removing does), and why finished `BotCommand*` components are left on the entity (a leak: 21 seen).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 29 (partial: lookup and add by type object).

#### D. Ally AI, commands, movement, animation

**FOF-D1 (P0) A reliable follow.** *Why:* the Goto-based follow stalls and chases stale points. *Known:* the Goto behavior builds its request at `exe+20de94e` (request: dest `+0x30`, radius `+0x60`, `+0x68` 0.5, `+0x6c` 10.0, `+0x70` 0.1, `+0x74` 2.0, flags `+0x78` 0x210 for AI gotos vs 0x216 from our command), `BotStateGotoLocal` init `exe+1dd6660`, update `exe+1dd6ad0` (switch on mode byte), the `NavPath` component, events `BehaviorGotoNavQuerySucceeded/FailedEvent`, the `BehaviorGotoGame` component. Standish and Layton (`AllyBotCompanionData`, non-combat, immortal) are the game's own followers; `AllyBotGuideData`, `AllyBotDefenderData`, `AllyBotInteractiveData`, `BotGuideArrived/DestChanged` events, `HeroAllyTracker`, `BotCommandFollowPathScriptAction`. *Find:* **how the game makes Sable follow Spider-Man in her Silver Lining missions** (which behavior, data and events: guide/companion/defender), the complete request struct meaning (every field, the flag bits 0x216 vs 0x210), a way to **retarget a live Goto** or a "follow actor X" command that tracks the hero continuously, the arrival criteria (why she stopped about 4 m away although the radius was 1.5), why commands hang when the destination is off the navmesh and how to detect it (event), **navmesh queries**: nearest-navmesh-point and path-exists calls so we can pick reachable targets (and when the hero is on a roof or in the air, a sensible ground point or a teleport trigger).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 30 (the companion decider, goto request with an actor target).

**FOF-D2 (P0) Movement style and speed rules.** *Known:* `movement +0xa0` mode byte (0..6) in the movement object returned by `exe+20de270(botComponent)` (captured as pairs of (component, movement) by a hook; for Sable the component that works is one the game passes, found by capture); the game sets 2 (Run) for far Gotos; `BotConfigMovement` styles with `NormalSpeed/MaxSpeed/Accel/UseAnimNormalSpeed` (Sable: Walk 1.5 m/s, run style 10 m/s, Jog 3.75). *Find:* **who sets the mode byte and by what rule** (distance? the hero's speed? a timer? a field of the request?), whether the hero's speed matters (the user saw her run when he ran), how to ask for Jog for a calm follow, how to set speed multipliers, how acceleration/turn rates are applied, and the animation side: how the mode byte maps to the locomotion animations (why `BotCommandChaseRunTo` glides: which state it skips), and the complete reference of **`BotCommandChaseRunTo`**: fields, completion condition, why it never ends, whether it can be made to animate.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md sections 30 and 31.

**FOF-D3 (P1) The whole BotCommand family.** For each class (`Goto`, `ChaseRunTo`, `ChaseJumpTo`, `JumpTo`, `FollowPath`, `ShootAt`, `ShootOverRailing`, `Face`, `Idle`, `PlayAnim`, `FaceAndPlayPartial`, `SetAnimFilters`, `Combo`, `Queue`, `ConfigBehavior`, `PileOn`, `TrafficPath`, `CarryItem`, `Carry`/`Collector`, `ComboSwitchConfig`, `BlackCatCommandPerchAttack`, `BlackCatThrowGrenadeScriptAction`): the **component type object RVA** (like `BotCommandGoto` exe+7bf6840, `BotCommandChaseRunTo` exe+6c7a510), field layout of the command component (what the script action's maker writes: see the makers via the script action vtables, e.g. Goto's `exe+1d97d20`, ChaseRunTo's `exe+331f80`), what executes it (the behavior that pops the queue: `exe+1d96680` pops, `exe+1d96120` is `BotCommand::Finish`), completion events, the result codes, how to cancel cleanly (we use `Finish(cmd, 0)`), how components are freed (the leak). Priority verbs for us: **ShootAt, Combo, Face, PlayAnim, Idle, FollowPath, JumpTo**.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md sections 29 and 31 (partial: type objects and node data).

**FOF-D4 (P1) Directing allies: the targeting internals.** *Known:* `AllyTieredAggroTargeting(Prius)` component (config `allysilversabletargetingconfig`: three tiers with `Range/AggroAtRange/RangeY`, `TargetTypes` `Bit9`/`kHero`/`kBot|kAlly`, `AllyData.HeroTargetDistanceModifiers` score curve over distance to the hero, `StartAggro 15`, `StopAggro 10`, `SwitchAggro 5`, `AggroMax 100`), live offsets in the component: `+0x78` target handle (an output, recomputed each frame), `+0x64` aggro float, `+0x5c/+0x6c` state numbers. *Find:* the per-candidate score storage and how to bias it so she picks a chosen enemy (or how to force a target), what **`TargetTypes Bit9`** is, how to give an ally a "protect the hero" or "stay" behavior without breaking her charge-in personality, how `HeroAllyTracker` and the awareness (`OmniAwarenessPrius`, range 1000) feed it; **what makes her stand still with no enemies** and how to add idle/guard/follow behaviors (the behavior map: `GameBehaviorMap`, `BehaviorScripted`, `BehaviorSingleState`, `JobHostModeControl`, the job hosts: Skirmish/Investigate/Patrol/Combat/RangedCombat).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 32 (partial: Bit9 = kDecoy, config fields).

**FOF-D5 (P1) Co-op takedowns and combos.** `AllySilverSableData` (`GuestTakedownDriver React_Attack_Grab_Thug`, `HostTakedownDriver Melee_Attack_Grab_Thug`, `LeapAttackComboName "Leap Attack"`), `SilverSableBehaviorTakedown`, `SilverSableTakedownStartedEvent`, Black Cat's `BlackCatAutomaticTeamTakedownEvent`, `BlackCatTakedownDriverSet`, finale takedown events (`...TakedownBCOnlyEvent`, `...KillStealEvent`, `...StartEvent`, `...ReminderEvent`), `kBlackCatTakedown`. *Find:* how to trigger a team takedown on demand (events, payload, preconditions, which side initiates), how the ally decides to take down on her own, the hero-side states (`HeroState*` for being the guest/host), and the contract for animations (drivers by name).

**FOF-D6 (P1) Allegiance and turning any actor into an ally.** *Known:* `SimpleAllegiance` component, team int at `+0x48`, set by `exe+2117e00`, enum order at `exe+39470d8` (kNeutral, kBlueTeam, kPolice, kFisk, kMaggia, kSableEnemy ...), `TargetData.Allegiance`, `TrackingSenseTarget.Type kAlly`, `IsAlly`, `HealthFlags kImmortal/kInvulnerable`, `SilverSableAllyDamageModifier`. Enemy bots' targeting configs list only `kHero`. *Find:* the full list of components/fields that make an actor friendly, how the **enemies** decide to attack an ally (do thugs shoot Sable? the user saw no infighting issue but did not test), friendly-fire rules, which bot classes (`SilverSable`, `BlackCatBot`, `ThugBot`, `Taskmaster`...) honor ally awareness/targeting/melee/combo/firing configs, and a **runtime recipe** to flip an ordinary actor (a thug, Black Cat's chase actor, Mister Negative, Taskmaster) into a working ally: which components to add/replace, which configs to swap (`BotBaseData.TargetingConfig/AwarenessConfig/MeleeConfig`, `BotConfigData.ComboConfig`, `HumanBotData.FiringConfig`) and whether swapping live is possible.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 32 (partial).

**FOF-D7 (P1) Idle animation, facing, look-at.** *Known:* Sable stands in a static pose when idle; actor has `AnimControllerComponent` (anim sets `characters/npc/npc_silversable/npc_silversable.animset` plus thug sets), `StrandState`, `LookAtHero` (on the Black Cat chase actor), `Dialog`, `PedestrianPOI`. *Find:* why there is no idle animation (anim graph state? missing idle state because the ally relies on a mission script to start one), how to start the idle/locomotion graph, how a bot faces a target (command `Face`), idle variations, weapon holster/draw states, and how to play a named animation on an ally (`BotCommandPlayAnim` fields).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 30 (partial).

**FOF-D8 (P0 for Black Cat) Black Cat as an ally.** *Known:* only `enm_blackcat_chase.actor` exists by name (BlackCatBot, health 100, not flagged ally, `BlackCatChaseController`); the exe has a rich Black Cat companion set (`BlackCatBot`, `BlackCatDeciderTree`, `BlackCatAttackController`, `BehaviorBlackCatPerchAttack`, `BlackCatCommandPerchAttack` (+ script action), `BlackCatPerchHunter`, `BlackCatGotoBehavior`, `BlackCatThrowGrenadeScriptAction`, `BlackCatFlashbangShot`, `BlackCatFinale*`, distraction grenades, `BlackCatPedestrianDemoter`), data `npc_blackcat.model`, `Actor_BlackCat` inside the gallery combat zone 9B7130D1BAE859E1, the cut co-op zones, `bot_emergent_vo_blackcat.config` (voice `BlackCat`, tag `BCAT`). *Find:* **every actor/config/zone asset that contains Black Cat** (scan the toc with class-name needles: `BlackCatBot`, `Actor_BlackCat`, `npc_blackcat`, `blackcat`; ignore texture/suit hits), her real Heist companion setup (how she fights beside Spider-Man in the Heist missions, which actor/spawn, which configs), the actor(s) usable for her as an ally with their dependency chain, how her **command-driven perch attacks and grenade throws** work and can be invoked on demand, her stealth-mode companion behavior, and the minimum to spawn her as a following, fighting ally.

**FOF-D9 (P1) Other allies.** Enumerate every actor in the archives that could serve as a friend: Standish and Layton (`AllyBotCompanionData`, found via `npc_standish_head`, `npc_layton_head`), Miles (`kMiles`; missions with Miles NPC), Yuri Watanabe, Mary Jane and Aunt May (stealth/NPC sections), Screwball, Taskmaster and others used in co-op fights (`npc_taskmaster`, `npc_imposter_spiderman*`), the cut co-op heroes; per actor: path, bot class, health, what it can do (melee/ranged/stealth), whether `IsAlly` exists, dependency chain. Include actors in DLC archives and test levels (`CHK_P35_CHARACTER_TEST_AREA`, `character_test*`).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 30 (partial: AllyBot data types).

**FOF-D10 (P1) Voice and banter.** *Known:* `EmergentVOManagerBotPrius` with per-bot `bot_emergent_vo_<name>.config` (Black Cat: `EntityType BlackCat`, `DialogTag BCAT`), `Dialog` component (`LocatorName`), hero VO systems, the audio recorder in Endless Arena (`audio post <event>`, Wwise exported API), Wwise ids are FNV-1 of the lower-cased name (`tools/assets/wwise_names.py`). *Find:* how an ally's barks are triggered (events that make `EmergentVO` speak: spot enemy, take damage, takedown, idle chatter), a **list of Wwise events (and banks) for Sable and Black Cat lines with their text** (subtitle keys, localization), how Spider-Man's lines respond (hero VO events), how to post a line by name on a given actor, conversation throttling/priorities, and what it takes to **add new voice lines** (bank format; can Overstrike replace a bank; are subtitles tied to events).

**FOF-D11 (P2) Ally health, death and downed states.** Sable is immortal; Black Cat is not necessarily; `MaxHPCoop`, `InjuredHPCoop`, `RegenDelayCoop` (co-op health values) suggest revive states: document the downed/revive mechanics if any exist, hit reactions (`HitReactConfig`), knockdown (`SilverSableBehaviorKnockedDown`), and what happens when an ally falls off the map (we saw a spawned Sable at y = -1259 after spawning over a void).

#### E. Playing as an ally (character switching)

**FOF-E1 (P1) Who is the hero, structurally.** *Known:* the hero system singleton `exe+5d9dd00` (hero actor handle at `+0x1c`; `Endless Arena` and `ArkWeb` read it), `Hero::HeroLocalPrius`, `HeroCameraManager` (vtable exe+38b1dd0), `HeroMoverManager`, `HeroIKManager`, `HeroWeaponManagerLocal`, `HeroSpiderAssaultManager`, `GameEquipInventory` (type exe+6e09180), `HeroSpawnStateData`, remote states (`HeroStateWebLinePullReactRemote`, `HeroStateHideInPlainSightRemote`), `CoopHeroCharacter`/`CharacterNameCoopPlayer`, `kMiles`/`kPeterParker`. *Find:* the **inventory of hero-owned systems and singletons** (everything that assumes one local hero: list each with address and how it finds "the hero"), the hero actor definition (`.actor` path, which components make a hero a hero), how the game spawns/despawns/replaces it (checkpoint, mission start, suit change: `exe`-side functions), and **whether the hero actor can be swapped at runtime** for another actor class (Miles, MJ, Black Cat, Sable) or only its model/suit.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 35.

**FOF-E2 (P1) Miles and Mary Jane sections.** The game has playable Miles and MJ missions: document exactly how: which actor, which controller (a hero actor with a restricted state set? bot driven by the player?), input mapping, camera, HUD and abilities, and the call(s) that **start and end** them. These sections are the template for "play as Sable/Black Cat".
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 35 (character records and SetHeroCharacterAction; not run).

**FOF-E3 (P2) The cut co-op hero (Black Cat).** `HeroCoopSpawnPoint`, `CoopHeroCharacter` (appears only in the main level asset 93243FC0D3FE0498 as a string), the `_Heroes`/`_Players` groups in mission zones, the co-op Black Cat actor (a player-controlled hero version?), her traversal and attack states, and what remains of it (animations, states, abilities, HUD): is a player-controllable Black Cat still functional in the data? Find all her assets (hero actor, anim sets, configs) and what `HeroState*` classes handle her.

**FOF-E4 (P2) Moveset feasibility for Sable and Black Cat.** What animation sets and states exist for each (locomotion, jump, climb, swing/grapple for Black Cat?), whether the game's generic `HeroState*` machine can drive them, and what is missing (a short feasibility report).

**FOF-E5 (P2) Swapping control mechanically.** Camera target change, HUD owner, input routing, health bars, gadgets, suit powers, saved state (abilities and the save: will swapping corrupt anything), and hiding or parking the previous character.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 35 (partial).

#### F. Co-op and split-screen

**FOF-F1 (P1) The cut co-op, fully documented.** *Known:* exe `HeroCoopSpawnPoint(Prius)` + actor `actors/HeroCoopSpawnPoint/HeroCoopSpawnPoint.actor` (asset B375D8485746D556; 19 assets reference it, `HeroSinglePoint`/`herosinglespawnpoint.actor` also exists), `CoopWarpAction`, `CoopWarpLocation`, `kCoopOnly`/`kSingleAndCoop` (config enums: find their config type and which assets set them), `AllowInCoop`, `MaxHPCoop`/`InjuredHPCoop`/`RegenDelayCoop`, `UITitleScreenOptionHostCoopGame/JoinCoopGame`, zones `COOP_BC_01_GALLERY`, `COOP_BC_06_CONSTRUCTION`, `Coop_BC_Gallery`, checkpoints `CHK_COOP_BC_04_DRUGLAB`, `CHK_COOP_BC_06_*`, `CHK_COOP_BC_08_CASINO`, `CHK_COOP_CHASE_DEMO`, `CHK_COOP_CRIME_DEMO`, `CHK_COOP_PATROL_*`, `CHK_COOP_RACE_*`, `CHK_T37_COOP_PATROL`, `enm_coopchase_enemy_movement`, `coop_bc_08_enm_heavy`. *Find:* the whole co-op architecture that is left: local vs remote hero classes, what is replicated (sync objects), whether it can run **offline with two local heroes**, what each of the above does when triggered, what the title-screen host/join options call, and what is dead code vs working.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 34 (partial: what is left in the exe).

**FOF-F2 (P0 for any co-op/test) A checkpoint and level loader.** *Why:* we have none, and several tests need `CHK_*` checkpoints (the old Script Hook Debug Menu had one). *Find:* the game's call that loads a checkpoint/mission/level by name or id (the `CheckpointLoaded` event id `0x414AD37A` is known: `LevelLoaded` etc. in `EndlessArena/src/events.cpp`), its arguments, preconditions (save coupling, which level the hub world needs), the mission system (`exe+7c4bbb0`, ids = Insomniac CRC32), `MissionStartedEvent` etc., a safe way to load `CHK_COOP_BC_01_*` (the gallery) and `CHK_P35_CHARACTER_TEST_AREA` from a mod, and what state it leaves.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 34 and docs/MISC_RECON.md section 2 (GameManager::LoadLevel; not run).

**FOF-F3 (P1) Cameras, viewports and rendering for two views.** *Known:* `ViewportUpdateSystem`, `ViewportUpdateComponent`, `hkViewport`, `AttachActorToViewport`, `HeroCameraManager`, `ViewportDraw` (a component on actors), the HUD root table indexed per player, D3D12 backend (`D3D12` folder, DLSS/FSR/XeSS libs), `FollowCamera` actor (`entity::Camera()`). *Find:* how the renderer supports views (is there a notion of a second player view or a render target per view; scene constants, culling, streaming centers, LOD origins, the HUD's per-player roots), what streaming uses as its center (the hero? the camera?), what would be needed for **true split-screen** and what a **shared-camera two-hero** (Lego-style) needs; where the cost sits; risk list. Also: how the pause menu and cinematics assume one player.

**FOF-F4 (P1) A second hero actor.** Singleton audit: every global that stores "the hero" or "the player" (HeroSystem exe+5d9dd00, `GetPlayerHero`, the hero handle at `+0x1c`, `HeroAllyTracker`, camera, input acquire at `exe+1f2c3f0`, HUD per player, damage/health systems, mission scripts), and what spawning a **second Spider-Man** (same `.actor`) breaks. Try it if possible: does the engine accept two actors with a `HeroLocal`? What if the second has `CoopHeroCharacter` set?

**FOF-F5 (P2) Player index everywhere.** Find fields that mean "player index/user" in components (`TargetData`, `HeroInput`-like components, `kPlayer*` enums, `_Players` groups) and per-player tables.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 34 (partial).

**FOF-F6 (P3) Online replication, if reusable.** `SyncObject`/`SyncStaticStateMachine`, `*Remote` state classes, `BotStateGotoRemote` (we saw `BotStateGotoLocal` serialize its state through hashed field names at `exe+1dd6e00`): the replication protocol, and whether a loopback second client could drive a remote hero (the route hbgda's Patreon multiplayer mod presumably took).

#### G. World and game-state events

**FOF-G1 (P1) Event table.** A table (hash -> name -> sender -> payload layout -> when it fires) for the events an ally manager must react to: level loaded, checkpoint loaded/saved, mission started/completed/failed, objective changes, hero death and respawn, fast travel start/end, loading screen show/hide, pause/unpause, cutscene start/end (`Cinematic2*`), combat start/end, crime/hideout start, zone/tile load and unload, `HeroSpawnedEvent`, `HeroWarp*Event`, time-scale changes. We have `SendEvent` (`events.cpp`, known hashes: `MissionStarted 0x6E588F6D`, `ObjectiveStarted 0xD358823E`, `MissionComplete 0xCC580B11`, `CheckpointLoaded 0x414AD37A`, ...) and the name hash is CRC-32 **non-normalized** (checked on three names). Provide the full set, plus how to **subscribe** to events from a mod (a listener registration, not only a hook on `SendEvent`).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 29 (recon/events_4.0630.txt, tools/sigtools/events.py).

**FOF-G2 (P1) Cutscenes and scripted moments.** What happens to unknown actors during cinematics (hidden, frozen, destroyed, moved), whether an actor can be made cinematic-safe, how to detect "cinematic playing" and "hero not controllable" so the ally manager can hide or park allies and teleport them back after.

**FOF-G3 (P2) World streaming.** The tile/zone streaming center, when actors in unloaded tiles are destroyed, how far from the hero our spawned actors stay valid, interiors vs open world (the hero in an interior while the ally is outside), the `GetActor`/zone relationship.

**FOF-G4 (P2) Time and pause.** `TimeScaleSystem` (exe+19bb430, channels, priorities) and what the pause menu does to actors and `GameTime` (so an ally's loop does not run during the real wheel's slow-motion or pause).

#### H. Documentation and tooling asks (cheap, valuable)

**FOF-H1 (P1) A symbol map.** Name and annotate the functions we meet constantly, in one table: RTTI class -> vtable -> slot names for `Bot*`, `Ally*`, `BotCommand*`, `BotState*`, `Behavior*`, `Hero*Manager`, `PlayerHud*`, `Nx*Input*`; and the exe+ functions touched so far (spawn natives in `EndlessArena/src/natives.cpp`, the 40 natives we use) with signatures, arguments and returns.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 29 (partial: generated tables).

**FOF-H2 (P1) The "gotolog"-style tracing recipes.** Where useful, add tracing hooks that dump arguments (we have `gotolog`, `padlog`, `mvhook`, `uilog`) for: the behavior-map dispatch (which behavior runs on an ally and why), `BotCommand::Finish` (who ends commands and with what result), the bot warp, the targeting score update, and the hero's per-frame action query. Leave the hooks in `FriendOrFoe/mod` behind commands.

**FOF-H3 (P2) Config schemas.** Config types we touch (`BotConfigTargeting`, `BotConfigAwareness`, `BotConfigMovement`, `BotConfigMeleeGame`, `BotConfigFiring`, `GameBotConfigCombo`, `BotConfigEmergentVO`, `UIHUD.config` and the pad-map configs): field names, types, defaults, and the runtime object each is parsed into (offsets), so configs can be edited as data (`.stage`) instead of patched live.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 29 (recon/ddl_types_4.0630.txt).

**FOF-H4 (P2) Overstrike integration notes.** What an Overstrike `.script` can ship besides a DLL (asset replacement, new assets: Manhattan Skies adds assets to the in-memory TOC, `ManhattanSkies/docs/RECON.md` Part 17), so we can ship ally icons/portraits, new actor definitions (e.g. an ally Black Cat `.actor` cloned from the chase actor with ally configs) and voice banks with the mod. Document the cleanest route for a **new `.actor` asset** (clone and edit with ALERT, hash the path, make the game find it).

**FOF-H5 (P3) Quality-of-life engine facts:** the game thread tick (`exe+1f745f0`) rate and what runs after it; safe places for per-frame logic; what blocks `gamethread::Run` during loads; crash patterns we hit (null `MoverStandard` in `BotStateGotoLocal` init, missing component in `exe+3303e0` when a bot starts command mode: the `BotCommandQueueGame` requirement).
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 37 (partial).

### Priority summary (what unblocks what)

- **To finish the ally wheel:** A1, A2, A3, B1, B2 (then B3 for portraits), C1 (despawn without clones), C2.
- **To make allies pleasant companions:** D1, D2, D4, D7, then D3, D5, D10, C3, G1, G2.
- **To add Black Cat (and others):** D8, D6, D9, C2, D10.
- **To play as allies:** E1, E2, E3, E4, E5, F2.
- **To get to co-op:** F1, F2, F3, F4, A4, F5, F6.

### Where to put the answers

Number them in `FriendOrFoe/docs/RECON.md` after section 28 (that file is the running notebook of this mod and stays local), add a one-line pointer under each request above, and
mention anything that the **other mods** can use in `docs/GAME_INTERNALS.md`. New findings that change the plan: also update `FriendOrFoe/docs/ROADMAP.md`.


---

## Manhattan Streets (street-level options: street life, dressing, landmarks, crowd, traffic, wildlife, music; the companion of Manhattan Skies)

### Goals

**Manhattan Streets** is the street-level companion to Manhattan Skies (`ManhattanSkies/`): where Skies lets the player change the sky, Streets lets the player change what is **on the ground**, as rows in the game's Settings screen (through `ModMenu/`), real time, mixable, in any save including after the credits. Like Skies it reuses **only content the story missions already ship** and makes it available after the story; "AUTO" always means the game's own state. Native `.script` DLL, `common/` layer, no Script Hook. In the maintainer's order:

1. **Split the Act 3 street bundle into independent switches.** One command (`act3 on`) already brings back trash, homeless camps, FEMA tents, smoke, the Act 3 music and the Act 3 pedestrians in a post-credits save; the player should be able to pick each one (and more) separately.
2. **Street life by act, per district:** the 54 district vignette types (yoga, picnics, buskers, preacher 1/2/3, protests, Sable and SWAT shutdowns, Maggia arrests, accidents, vehicle fires, FEMA tents ...) as choices: everyday / unrest / outbreak / martial law / gang war.
3. **Landmark states:** City Hall (rally set, pristine, destroyed), Fisk Tower, the Harlem Rhino fight area, the Hammerhead lockup, the docks office ... pristine, damaged, or as the story left it.
4. **Crowd and traffic:** pedestrian style (Act 1/2/3 wardrobes), density, walking pace, sick or masked pedestrians; vehicle set and amount (fewer cars, emergency vehicles).
5. **Wildlife, music and ambience:** critter and bird densities, street music of any story state, district ambience, helicopters.
6. **Parked, but wanted: seasons.** Vegetation and road textures that follow Skies' dynamic cycle (snow sky = snow on roads and trees; the base game is autumn). Parked on 2026-11-14 because textures cannot be changed live (see section A); it comes back if section A finds a live reload.
7. **An idea to check, not promised ("Chaos Mode"):** more Negative-corrupted citizens ambushing in the streets.

**Scope.** Out of scope as *features*: crimes (the crime director, `CrimeType` events) and faction bases (Sable, Demon-prisoner, Hammerhead/Maggia base overlays), and sky, sun, fog, weather and time of day (Manhattan Skies). **Documenting how those work is welcome** (section B7): we may reopen them. The folder `ManhattanStreets/` is git-ignored until release (it holds game-derived addresses and dumps), so answers go there.

### Current state (2026-11-14), so you do not redo it

Everything is in `ManhattanStreets/docs/RECON.md` (Parts in parentheses; tags [V]/[I]/[?] there). Dumps and logs are in `ManhattanStreets/recon/` (`overlays_dump_*.log`, `zoneloads_*.log`, `act3_real_diff.json`, `level_overlay_entries.txt`, `street_families.txt`, `texwatch_run1.log`, `texsub_run1.log`, `city_zone_paths.txt` = all 12,337 zone paths of `i20_city.level`).

- **Overlay system [V]** (5, 9, 10-14): static `CustomOverlaySystem` at `exe+62a4e60` (entries array `+0x18`, count `+0x20` = 382, stride `0x28`: `+0x00` trigger object, `+0x10` u64 payload, `+0x18` dword = running index 800..1843 (the gap to the next entry = the entry's zone count: 295 entries have 1, the largest 63), `+0x1c` load state (0 off, 1 loading, 2 unloading, 3 loaded), `+0x20` trigger state). The game calls the trigger's `IsTriggered()` (vtable slot 11) in `UpdateOverlay(sys, idx)` = `exe+1eaf700`, which loads (`exe+1f73580` -> `1f87490`) or unloads (`exe+1f735a0` -> `1f88af0`). **It is event-driven, not per frame** (nothing changes unless something prods the entry). The table index equals the order of the `OverlayTrigger` entries in section `339C970E` of `i20_city.level` (784 entries; ALERT's parser needs a lenient string decoder). Triggers are boolean trees (And/Or/Not over MissionComplete/MissionState/BetweenMissionObjectives/DuringMissionObjectives/MissionCampaign/LightingMode/FactionBase/NoMissionsActive); fully decoded in `level_overlay_entries.txt`. **Our override works:** a private vtable copy with slot 11 returning a constant, then `UpdateOverlay` (`act3 on` patches 30 entries; `ent <i> on|off|auto` any one entry).
- **Not known:** which zones each entry loads (the payload is not a CRC64 of any name we tried; a check on City Hall's entries did not reproduce the folder sizes), what prods `UpdateOverlay`, the leaf trigger classes' field layouts, what the u64 payload is.
- **Zone loading [V]** (6, 24): `ZoneAssetManager` (`exe+621d900`), `LoadAssetByHash exe+1626380`, `ConvertStringToHash exe+1bb25d0`. A loaded zone's **static art appears** (City Hall's `ART_CITYHALL_pristine.zone` brought back the Osborn rally stage, podium and posters) but **its script-driven actors do not start** (`GP_A1_CITYHALL_Civilians.zone`: no crowd; the Negative-corruption zones: no corruption). No zone unload is known (`AssetRelease exe+1626940`, ref count at `asset+4`, exists in Endless Arena's natives, untested on zones).
- **Crowd [V]** (16-19): `PedestrianLoadSystem` object `exe+5dbb670` (concrete vtable `exe+390fd50`), per-frame `Update exe+da3da0` calls the getter (vtable slot 19, `exe+da1d00`), compares with the stored key (`+0x20/+0x24/+0x28`), stores it and sets a changed flag (`+0x40`). Key = three dwords `{?, ?, Act}`: Act is 0 after the credits and 2 in a real Act 3 save; hooking the getter to return Act 2 makes the pedestrians Act 3 pedestrians (wardrobes `civ_*_common_act3`, homeless variants, rules of `PedestrianSystemConfig 984F58B916E2E1FC`: dynamic cap 10 at night or Act 3, `QuarantineSettings`, density modifiers, `OverrideEntries`). Key dword 0 is unexplained (4, 6, 7, 8 seen). The Act condition is evaluated by the type-hash switch at `exe+dd4230` (Act branch `exe+dd4513`).
- **Traffic [?]** (16, 18-19): `VehicleLoadSystem` (vtable `exe+39138f0`), getter `exe+e0a4d0`, key = one dword (forced when `this+0x24 != 0`, else looked up from `this+0x218` via `exe+e1e650`); three keys cycle in a post-credits save (`0xb616ba7b`, `0xc13fe1ff`, `0xc5affc49`), the Act 3 values were never captured; config classes `VehicleSetConfig`, `VehicleDensityListEntry`. The Act 3 override did **not** thin the traffic.
- **Zone-load diff real Act 3 versus post-credits [V]** (15): Act 3 loads `GLOBAL_ACT3_FX` (smoke), `GLOBAL_ART_ACT_3`, `GLOBAL_ACT3_TRAFFICNETWORKBLOCKERS`, `GLOBAL_CRITTERSMOD_ACT3`, `GLOBAL_ART_ACT2_3`, `I20_Global_Act3_Helos`, `I20_MUSIC_ACT3`, `ART_K34/L34/L47_ACT3_PEDS`, `Vig_ProtestersAct3`, `Vig_Preacher3`, `AbandonedAccident`, `VehicleFire`, `GP_A3_NYPD_NON_MISSION_*`; post-credits loads `GLOBAL_ART_ACT1/ACT1_2`, `I20_MUSIC_ACT1`, the city-life vignettes and the police scenes.
- **Textures [V]** (21-26): a texture is two toc entries under one id (header in span 0 with the low mips, pixel stream in span kind 1); the live toc (773,410 assets, 207 archives, a 21 MB DAT1 buffer in the heap) can be found and edited, **but editing it does not change where the game reads texture streams**; the game reads a pixel stream through one overlapped `ReadFile` (call site `exe+1d1bfb8`) at the entry's original `(archive, offset, length)`; **read-level substitution works** (answer that exact read from a same-size loose file; needs the entry to live in a raw `mods\modN` archive, i.e. an installed Overstrike mod) and turned the icy road to asphalt after a world load. Streams are re-read at world or checkpoint reload and in area batches, not on a timer, not after a time-of-day change; **headers are read once at boot** (not again after leaving to the main menu). Texture Quality toggling untested.
- **Other [V]:** weather = lighting mode (setter `exe+1f7f9e0`, world singleton `exe+1f707e0`, mode field `+0xe2040`); `loadlog` (hook on `LoadAssetByHash`, logs zone hashes; hash = CRC64 of the lowercased path); Skies' toc growth and carrier method (`ManhattanSkies/docs/RECON.md` Part 17).
- **Tools in our mod** (`ManhattanStreets/mod/README.md`; command file `scripts\Uncontained.cmd`, log `.log`; a `scripts\Uncontained.startup` file runs commands at launch, `wait <s>`): `act3 on|off`, `ent <i> on|off|auto`, `overlays`, `watch on`, `mark <name>` (zone-load segments), `zone <path>`, `keys act|veh`, `ped`, `peek`, `wp set|dump`, `toclocate`, `texinfo <id>`, `texwatch <list>`, `texsub <id> <0|1> <file>`. Saves for diffs: Slot 1 Save 41 (real Act 3, day rain), Save 42 (night rain), the normal post-credits save; ask the maintainer for saves at other story points.

### Requests

Priority legend: **P0** = blocks the next milestone (splitting the bundle, vehicle/crowd control, live textures), **P1** = big quality win, **P2** = needed for later rows, **P3** = long shot. **Do not limit yourself to the wording.**

#### A. Textures: streaming, headers, live reload (decides whether seasons ever come back)

#### MST-A1 (P0) The texture streaming architecture.

*Why:* editing the toc did not change where texture streams are read; we need the real mechanism. *Known:* every stream read comes through one overlapped `ReadFile` call site (`exe+1d1bfb8`); pixel streams are re-read at world/checkpoint reload and in area batches; streams were not re-read after a lighting transition or 150 s of flight; the toc in memory (found by heap scan, 21 MB DAT1) is plain read-write. *Find:* the call chain from that call site up to the texture streamer; the structure that caches each texture's `(archive, offset, size)` (why the toc edit was ignored; is the toc object's section pointer still the buffer we edited? Skies' `FindObject` pattern `{base, spans, ids}` was never used in Streets); per-texture streaming state (resident mips, requested, evicted), the pool and budget (and what the Texture Quality setting changes), the eviction policy and the functions for stream-in and evict; threads involved. *Verify:* the `texwatch`/`texsub` setup (`recon/texwatch.txt` from `tools/texmods.py`) with a hook on the request function logging texture ids.
Answer (2026-11-16 update): ManhattanStreets/docs/RECON.md Part 33 (file system object, by-id reads, streamer functions).

#### MST-A2 (P0) A live "reload this texture" call.

*Why:* the whole seasonal idea needs a texture to be re-requested on demand. *Find:* a function or recipe that evicts one texture id (header and stream, or only the stream) so the next use reads it again, and/or a "flush all streamed textures"; what the game does when the Texture Quality or any graphics setting changes (does it rebuild the pool? is that callable?); and the alternative: replace a resident texture's contents in place (same dimensions and format) through the GPU upload path (document the upload function and the D3D12 resource per texture). *Verify:* with `texsub` armed for the road (`AC46315D1DCC7AD5`, entries in `mods\mod163`), call the reload and see the road change within seconds without leaving the world.
Answer (2026-11-16 update): ManhattanStreets/docs/RECON.md Part 33 (evict/request one texture; not run).

#### MST-A3 (P0) Headers at boot.

*Why:* a header holds the low mips (the distant look); with the stream swapped the far roads stayed icy while near ones became asphalt. *Find:* when and where headers are read (first read, call site, archive, `ReadFile` or mapped view), what keeps them resident, whether a header can be re-read live, and the earliest point at which our DLL can arm a substitution before the first read (our DLL loads at the proxy's start; the toc scan took 26 s in a fresh process, too late for headers). *Find also:* a way to know every entry's location at DLL load without scanning the heap (a static pointer to the toc object, or a parse of the toc file with the zlib wrapper it uses).
Answer (2026-11-16 update): ManhattanStreets/docs/RECON.md Part 33 (partial).

#### MST-A4 (P1) What actually shades a leaf and a road.

*Why:* if foliage colour is driven by a ramp texture plus a parameter, a season could be a parameter, not a texture swap. *Known:* `textures/cperrella/misc/cp_leafgradient_fall_c.texture` (`AFCA485A06446D50`, 1,152 bytes, an authors' "fall gradient"; NYC Emerald Trees replaces it; Snowy Vegetation replaces it and 19 other vegetation textures with one 43,808-byte placeholder, header and stream alike, so a different size and format is accepted). *Find:* which material graphs and shaders sample the gradient and the Central Park / banana-leaf / maple textures; every dynamic input of the foliage, ground and road shaders (wetness, puddles, snow cover, dirt, leaf-fall amount, tint, wind) and where each is set (a global constant bus? per-frame from the lighting mode? per material?); whether the game has any "season", "fall" or "snow" scalar. *Verify:* set a found parameter live and screenshot.

#### MST-A5 (P1) Texture and material identification.

*Known:* three ids we cannot name: `95A7C14BE6DCC62B`, `8AD71A3ABA8EEF0C` (tree textures replaced by "natural green trees" and "NYC Emerald Trees") and `AC46315D1DCC7AD5` (the road replaced by "Icy roads"). *Find:* their paths; the materials and models that use them; and a **list of the texture ids per surface class** across Manhattan (asphalt, sidewalk, park grass, tree leaves by species, park paths, rooftops, water edges) so season packs can target them. *Also:* a **hash-to-path dictionary** for textures, materials, models, zones, configs and actors, harvested from the path strings inside materials, models, zones and configs (ids are CRC64 of the lowercased path), as a tool plus a method note (the file itself stays local).

#### MST-A6 (P2) The vanilla read path.

*Why:* substitution only works for entries that already live in a raw `mods\modN` archive (an installed Overstrike mod); a player without such a mod, or a texture no mod touches, cannot be answered. *Find:* how the vanilla compressed archives (`g00sNNN`, `DSAR`, chunk-mapped) are read: which call (one `ReadFile` per chunk? mapped views? DirectStorage?), the chunk map format use, and where a hook could answer a *vanilla* entry from a loose file, so no companion mod is needed.
Answer (2026-11-16 update): ManhattanStreets/docs/RECON.md Part 33 (substitute after the archive layer; not run).

#### MST-A7 (P2) Overstrike formats, precisely.

Why `natural green trees.smpcmod` ships a pixel-stream-sized file labelled `0_95A7...` (what the `<n>_` label means: span, archive index or span kind); how Overstrike writes each file kind into `mods\modN` and rewrites the toc (including entries whose size changes, e.g. the 43,808-byte placeholders); how `.stage` and `.modular` differ; what makes an archive raw versus chunk-mapped.

#### B. The overlay system and the story-state logic

#### MST-B1 (P0) The complete entry-to-zone map.

*Why:* every row that is "turn this on and off" needs to know which entry carries which zones; today we find out by flipping entries one at a time. *Known:* see above (the payload is not decoded). *Find:* what `payload+0x10` (u64) and `+0x18` (the 800..1843 index) refer to; where the zone name or id lists are stored; how entries are created from section `339C970E` and the other level sections; how tile suffix zones (`_overlay_act3.zone` over 700+ tiles) and custom overlay folders expand. **Deliver a table**: all 382 entries -> trigger expression -> zone paths (names), plus the sections of `i20_city.level` in a readable schema (the 15 sections; today only `339C970E` is understood; `07C75341` is 15 MB). *Verify:* `ent <i> on` plus `mark` and `watch on` shows exactly those zones loading.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 27 (partial: the overlay record table and how to dump it; the 382-entry table not produced).

#### MST-B2 (P0) What prods `UpdateOverlay` and what loading and unloading really do.

*Find:* all callers of `exe+1eaf700` and `exe+1eaf7d0` and the event or timer behind each (mission state change events? region streaming? checkpoint load?); the load/unload natives (`exe+1f73580`, `exe+1f735a0` -> `exe+1f87490`, `exe+1f88af0`): arguments, the payload structure they take, the world-streaming manager they call on, and a **direct call recipe** to load and unload an overlay's zones by index without a trigger; what happens to a zone's actors, scripts and variables on unload (is the state kept, lost, or restarted on the next load?); the load-state values at `entry+0x1c` and `+0x20`.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 27.

#### MST-B3 (P0) Unloading a zone loaded by path.

*Why:* zone loads (class B) are one-way today. *Find:* a safe way to undo `LoadAssetByHash(ZoneAssetManager, hash)` (the release call and ref counts at `asset+4`, `AssetRelease exe+1626940`; the difference between an asset loaded and a zone instantiated and active; what the world streamer must be told), so any zone can be toggled on and off at run time. *Verify:* load `ART_CITYHALL_pristine.zone`, see the rally set, unload, see it go.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 27 (partial: overlays unload by id; zones loaded by path not examined).

#### MST-B4 (P1) Trigger classes, mission-state queries and scoped spoofing.

*Find:* the field layout of every runtime trigger class (And/Or/Not children storage, `MissionCompleteTrigger` mission hash at `+0x64`/name at `+0x58`?, `MissionStateTrigger`, `BetweenMissionObjectivesTrigger`, `DuringMissionObjectivesTrigger`, `MissionObjectiveCompleteTrigger`, `MissionCampaignOverlayTrigger` (cached value at `+0x50` against a global), `LightingModeOverlayTrigger`, `FactionBaseOverlayTrigger`, `NoMissionsActiveTrigger`, `DemoOverlayTrigger`, `CheckpointOverlayTrigger`); the mission database they query (`exe+1fa7380`, `exe+1f71af0`, `exe+1f70a20`, `exe+201f640/800`, `exe+1fa74f0`, `exe+1bcf3e0` appear in these and in the pedestrian key getter) and how Active/Complete/Cleanup/Available/Inactive/Failed map to bits; a **read-only query API** a mod can call safely (to show "your story progress" and to drive logic) and a note on the write path (documentation only). *Why:* we want to answer story queries differently **only for named consumers** (the overlay system, the pedestrian system) instead of patching 30 trigger objects.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 28.

#### MST-B5 (P1) Region-streamed custom overlays.

*Known:* class names `CustomOverlayRegionData`, `AutoGenerateFromRegionOverlayData`, `CustomOverlayRegisteredEvent`, `CustomOverlaySystem`; district vignette zones load by player position (the real-Act-3 save loaded all 9 districts' vignettes within about 3 s of the load, our override did not bring them in during 14 s). *Find:* how region overlays are described (bounds, act filters, priority), registered and streamed in and out; which of the 382 triggers gate them; why the override loaded the Act 3 tile dressing but not the vignettes; how to get a chosen vignette set to load in every district.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 27 (partial).

#### MST-B6 (P1) Every consumer of "Act".

*Known:* `PedestrianLoadConditionAct{Act:Act3}`, the key's Act dword (0 or 2), `GLOBAL_ART_ACT1/ACT1_2/ACT2/ACT2_3/ACT_3` marker zones (they fire `GLOBAL_ART_ACTn_LOADED` signals), `I20_MUSIC_ACTn`, the per-act critter, vignette (`preacher`, `preacher2`, `preacher3`) and wardrobe variants. *Find:* whether one central campaign/act value feeds most of these; every consumer of it (callers, what each does when it changes); the narrowest single place to set it; then **enumerate what changes** when Act is forced to 2 in a post-credits save (zone-load diff plus visuals), and the same for 1.

#### MST-B7 (P2, documentation only) Faction bases, crimes and the Negative-corrupted-citizen ambushes.

Out of scope as features, but we want to understand them: `FactionBaseOverlayTrigger` (state source, `ActiveOrComplete`/`Complete`/`IncludeCleanupStateAsComplete`, where base progress lives), the 47 base-keyed entries that differ between a real Act 3 save and a finished game, the crime director (`CrimeOverlay`, 50 `CrimeType`s incl. `kRiot`, `kMassArrest`, `kCheckpoint`, `kAPCPatrol`, `kPoliceAmbush`, `kJetPackAmbush`, `kDemon*Ambush/Assault`, `kCTNSFactionCarChase`; `crime_blockers` in `GLOBAL_ART_ACT_3`): how crimes are scheduled and placed, rates, cooldowns, radii, per-district gating, how to enable or disable one type; **which system makes corrupted citizens (Inner Demons / Negative's) ambush the player in the streets** and its tunables (maintainer idea "Chaos Mode"); what the Act 3 "harassers" are (`COL_Harassers` 8 zones, `HELP_ACT3_HARASSERS`, `MUS_Act3Harassers_*`).
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 31 and docs/MISC_RECON.md section 1.

#### C. Making loaded zones act (scripts, events, actors)

#### MST-C1 (P0) Starting a zone's script without the mission.

*Why:* a loaded overlay's actors do not start (no rally crowd; no Negative corruption in Part 6). *Known:* zone scripts are Prius action graphs (`OnZoneLoadedAction`, `SignalRelayAction`, `OnMissionObjectiveEventAction`, `CheckMissionObjectiveAction`, `DelayAction`, `SetBooleanAction`, `Var*`/`*_LOADED` names) that wait for mission-objective events and signals (`GP_A2_TRUCK_CORRUPTION`, ...); `SendEvent exe+16d5e80` and the event system (type hash = CRC32 of the class name). *Find:* how a zone's script runs (the graph interpreter, which action classes, when `OnZoneLoadedAction` fires, how the objective listener subscribes), the exact events/signals the City Hall rally set (`GP_A1_CITYHALL_PRISTINE/*`, `GP_A1_CITYHALL_PRE`) needs, and a **recipe to start it** (fire the event, or call the action) without changing mission state or the save; do the same for one other overlay (Fisk fires, `GP_A1_FISK_NEWBOMB_FIRES`). *Verify:* load the zone, fire the recipe, the crowd appears.

#### MST-C2 (P1) The script variable and signal bus.

How `Var_*` and `SetBooleanAction` variables are stored (zone-local or global), how signals (`SignalRelayAction`, `*_LOADED`) are routed, a way to read and write both from native code, and a **list of the variables and signals used by the street overlays** (Act 3 FX, FEMA, hobo, peds, vignettes, music).

#### MST-C3 (P1) Zone actor instantiation and per-actor vetoes.

*Why:* the Act 3 tile overlay (`Tile_XNN_ground_overlay_act3`, 713 tiles, 267 MB) is one bundle of trash, camps, sick pedestrians, traffic removers, rat locators and sounds; we want to switch the kinds apart. *Known:* a zone holds a "Zone Actor Names" section (offsets into the string table), inner assets, material overrides (267 per tile). *Find:* the call chain that creates entities from a zone's instance data, and the earliest point where an actor can be **vetoed by name hash, asset or type** (so `prop_os_trash*`, `gbl_prop_homeless_tent_*`, `ped_sick_*`, `traffic_remover` can each be dropped while the rest of the overlay loads), plus the cost per zone; how `Zone Material Overrides` apply.

#### MST-C4 (P2) What is in the Act 3 overlay, decoded.

For the per-tile Act 3 overlays and the custom Act 3 overlays (`ART_I14/J18_ACT3_HOBO`, `ART_I17_ACT3_FEMA_TENTS`, `ART_K34/L34/L47_ACT3_PEDS`, `GLOBAL_ACT3_FX`, `I20_Global_Act3_Sable` (17 zones), `I20_Global_Act3_Helos`): per-zone actor lists by class and count (our survey only has the names), the semantics of `traffic_remover`, `vol_trafficremover`, `vol_trafficnetworkblocker` (hard or soft, what they do to the traffic graph), `vol_rat_locator`, `loot_trash_spawner`, `aud_emit_trash`, `modifynavvolume`, `vol_ped_sit_on_bench_act*`; where the smoke VFX are anchored; what `I20_Global_Act3_Sable` is (checkpoints? barricades? patrols?) and whether it is street presence or base content.

#### D. Population: crowd, traffic, animals

#### MST-D1 (P0) The vehicle system in full.

*Find:* what the `VehicleLoadKey` dword is (a hash of a set name? an index?); `VehicleSetConfig` for every key (vehicle lists, densities, emergency vehicles, Sable APCs, ambulances, taxis, buses); the lookup chain (`this+0x218`, `exe+e1e650`, what is at `[entry+0x14]`); the names of the three post-credits keys and the Act 3 keys (derive them from the configs and the conditions, no save needed; the real-Act-3 save can confirm: capture with `keys`); `VehicleDensityListEntry` (density by district or time of day); the spawner and its control points (a **live density multiplier**, forcing a vehicle set, removing all traffic, emergency vehicles only, pause traffic); `i20_vehicle_traffic.zone` (a node graph `MID_TN_*`); whether the `traffic_remover` volumes and `GLOBAL_ACT3_TRAFFICNETWORKBLOCKERS` (16 volumes) explain why Act 3 traffic is thin and why our override did not thin it.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 30 (partial: the set/density model; key names not confirmed).

#### MST-D2 (P0) The pedestrian system in full.

*Known:* see "Crowd" above. *Find:* every field of `PedestrianLoadKey` (dword 0: 4, 6, 7, 8 seen at different times in one session, 6 in a real Act 3 save: time of day? weather bucket? lighting mode?), dword 1 (always 0); all `PedestrianLoadCondition*` classes (TimeOfDay, Weather, Act, Mission, MissionObjective, Or ...) and what each compares against; `PedestrianSystemConfig` and `PlacedPedestrianSystemConfig` schemas and every value (`OverrideEntries`, `DynamicPedestrianMaxOverrides`, `MaxNumPedestrians` 250, `EnableDynamicPedestrians` false, `QuarantineSettings`, move settings, pools and actor reservations, `PedestrianVariantsConfigs` per type); **live controls**: a crowd density multiplier, the max count, walking pace, choosing the wardrobe set independently of Act; how sick, panhandler and coughing pedestrians (`ped_sick_panhandler`, `ped_vign_sick_pedestrian_*`, EMTs) are spawned and animated, and whether masks, coughing audio and stumbling are configurable; and **where the "streets filled with hostile groups that jump Spider-Man" of Act 3 come from** (pedestrian system, crime events, harassers, vignettes?).
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 29 (key = District, Time, Act).

#### MST-D3 (P1) The key machinery.

`PedestrianLoadSystem::Update` (`exe+da3da0`): what consumes the changed flag (`+0x40`) and reloads, the world queries the getter makes (`exe+1fa7380(&static, id, 0)` against `this+0x160/+0x164`: which missions decide Act 2 and 3), the mode byte `+0x16c`; the same for `VehicleLoadSystem` (`this+0x24`, `+0x218`). A table "key -> what reloads -> how long it takes".
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 29 (partial).

#### MST-D4 (P1) Animals and ambient world life.

*Find:* the implementation of `ChangeCritterDensityAction`, `ChangeFlockDensityAction`, `ChangeLandingBirdDensityAction`, `ChangeLedgeBirdDensityAction` as callable natives with a density config id; the schemas and **all** `CritterDensityConfig`/`FlockDensityConfig`/`LandingBirdDensityConfig`/`LedgeBirdDensityConfig` assets (names, ids, values: low, default, none); `GLOBAL_CRITTERSMOD_ACT3` and `_ACT1_2_NIGHT`; the global zones `I20_Global_Birds`, `_Fish`, `_Boats`, `_Planes`, `_Trains`, `_Foodcarts`, `_Photo`, `_Borders`, `_Manually_Placed_Peds`, `i20_pedestrian_traffic`: what each contains and runs, and how to switch each off or tone it down at run time (or by skipping its load at world start; judge the risk).
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 32 (partial: types only).

#### E. Landmarks, street life, dressing

#### MST-E1 (P1) Landmark state mechanics.

*Known:* names only: City Hall (`GP_A1_CITYHALL_PRISTINE/DESTROYED/DRAG/PRE_MISSION/POST_MISSION`, `instance/Cityhall_Destroyed/*`, entries 5 (12 zones), 8, 228 touch City Hall), Fisk Tower (`GP_A1_FISK_PRISTINE_ART`, `_CIVS_FIRES`, `_NEWBOMB_FIRES`, `_EXT_FACADE`, tile G20), Harlem Rhino fights (`VN_RHINO_FIGHT_PRISTINE/DESTROYED/QUARANTINE`, `VN_RHINO_MILES_PRISTINE/DESTROYED`), Hammerhead (`CTNS_HH_LOCKUP_ENV_PRISTINE/DESTROYED`, `CTNS_HH_INTRO_EXTERIOR_PRISTINE_ACT1_2/ACT3`, `CTNS_HH_LASTSTAND_EXTERIOR_PRISTINE`), the CTNS docks office (pristine/struggle), Li's shelter office, `ART_O27_PRISTINE`, the Act 2 construction site and crash (`ART_I36_ACT2_CONSTRUCTION/CRASH`), the Shipyard. *Find, per landmark:* the entries (index and expression) that load each state, what is baked into the base tile art versus the overlay, whether the destroyed overlay must be unloaded for the pristine one to show (z-fighting if both), the lighting companions (`*_Lgt`, `_light_grid`, `_Atm`) and their interplay with Manhattan Skies. **Deliver a table:** landmark -> entries -> zones -> what changes -> how to force each state.

#### MST-E2 (P1) Street-life vignettes.

The 54 vignette types x 9 districts (`I20_Vig_<District>_<Type>.zone` plus `CustomOverlay/Vig_*`): how each is gated (act, region, time of day, weather?), which entry or region loads each, per-type behaviour and scripts (preacher text, chess, SWAT and Sable shutdowns, `PoliceBlockedStreet`, `PedAccident`, `AbandonedAccident`, `VehicleFire`, `HomelessCamp`, `FEMAMedicTent`, `GraveMourner`), whether any starts gameplay (a crime, a chase) and whether vignettes of different acts can coexist; a table "type -> acts available -> zones -> entry".

#### MST-E3 (P2) Campaign offices, ads and signage.

`ART_*_CAMPAIGN_OFFICE` (10 tiles, Act 1-2 and `_CTNS_` versions), `TILE_P43_ACT1_2_PROPS`, Osborn banners and posters; any dynamic billboard or ad system (what "Real Brands and Ads Revamp" replaces, Daily Bugle and shop signs) and how its textures are chosen: an AD STYLE or SIGNAGE row.

#### MST-E4 (P2) Decals, graffiti, stains, fires, damage.

The shared decal materials in the act overlays (graffiti stickers `jn_gbl_arch_decal_color_change_stickers_01`, drip stains, grime): how act-dependent, the "color change stickers" mechanism, any global dirt or grime parameter, the smoke columns (`gbl_Act3_Smoke_*`) and fire anchors, "post-mission" damage layers; what `Zone Material Overrides` do (267 in one tile).

#### MST-E5 (P2) Sable street presence, helicopters and the Act 3 side zones.

`I20_Global_Act3_Sable` (17 zones), `I20_Global_Act3_Helos` (flight paths, spotlights, scripts), `GP_A3_NYPD_NON_MISSION_*`, `GP_A3_NORMANAPT_PRE`, the `OM_*_PRE` side-activity preloads, `TILE_L39_NYPD_PARKEDHELICOPTER`: contents, whether each can be started and stopped on its own, interplay with crimes.

#### F. Music and ambience

#### MST-F1 (P1) Music.

*Known:* `I20_MUSIC_ACT1/2/3`, `_CITYHALL`, `_FISK`, `_PRISON`, `_CTNS1-3` zones (Act 3's changed the music in game when loaded by the override), Wwise state `BCJ_MIX_STATE_02`, `MUS_Act3Harassers_*`, the soundbanks `AMB_I20_City_Global`, `dcom`. *Find:* what each music zone runs (scripts, Wwise states, switches, RTPCs, `WwiseGameSyncAction`), how the dynamic music chooses between traversal, combat and ambient sets, and a **live API to set a music state or switch**: the Wwise engine object, `PostEvent`, `SetState`, `SetSwitch`, `SetRTPCValue` call signatures and how names are hashed; the full list of states, switch groups and RTPCs the street and music systems use, and what each does.

#### MST-F2 (P2) Ambience.

`AUD_*` district zones (9), `aud_emit_*` (trash, boombox, rain), `aud_walla_*` (crowd chatter, `aud_walla_fema_tent_ped_male`), `static_soundemitter`: how emitter classes can be muted or scaled at run time (bus volumes, RTPCs, emitter groups).

#### G. Cross-cutting world-state facts

#### MST-G1 (P1) The world singleton and a "does the game know seasons" search.

Struct map of the world singleton at `exe+1f707e0()` (the lighting object at `+0xe10d0`, mode at `+0xe2040`, and every other interesting field: time of day, weather, wetness, anything season-like), the hero system `exe+5d9dd00`, `TimeScaleSystem`. **Search the exe, the level, materials and shaders for** snow, season, fall, autumn, wet, puddle, ice, frost, foliage, leaf, rain-intensity, temperature, and report each hit with its referencing code.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 32 (partial: no season scalar; weather is atmosphere data).

#### MST-G2 (P1) `i20_city.level` and the other level assets. A readable schema for all 15 sections of `i20_city.level` (tags `07C75341` (15 MB), `2236C47A`, `2BA33702`, `3395AEC1`, `339C970E`, `396F9418`, `4130D903`, `41887FB3`, `4E023760`, `611F490D`, `6251A0BF`, `7077E5F5`, `7CA7267D`, `95F91E24`, `C30D92B6`) and the tile and `.zone` section schemas; extend `ManhattanStreets/tools/level_triggers.py` into a full JSON dump (local, git-ignored). ALERT's parser fails on a non-ASCII string in this level (we patched it in a script).

#### MST-G3 (P1) The event catalogue and subscriptions.

A table type hash -> class name -> sender -> payload layout -> when it fires for the mission, objective, checkpoint, overlay, zone-loaded, lighting-transition (`LightingModeTransitionStarted/FinishedEvent`), `MissionCampaignChangedEvent`, `MissionStateChangedEvent` events, and how to **subscribe** from a mod (so we observe changes instead of polling the overlay table).
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 32 (recon/events_4.0630.txt).

#### MST-G4 (P1) Saves and progress, and a safety audit.

Where story progress lives in memory and in the save (`slotN-s.save`), which fields are Act and mission states, a safe read-only access, and an audit that our forced overlay triggers, key hooks and zone loads **cannot be written into a save** (what writes saves, at which events, and whether any state we force is captured by a checkpoint).

#### MST-G5 (P2) Live introspection of the managers.

Layouts of the zone, actor, atmosphere, material, texture and config asset managers (slot arrays, state byte at `asset+1`, ref count at `asset+4`, hash), so a tool can print "what is loaded now" with names; with MST-A5's dictionary it makes every recorder readable.

#### MST-G6 (P2) Hook coexistence.

Endless Arena, Manhattan Skies, Streets and Friend or Foe each hook `GameThreadTick` (`exe+1f745f0`), and several hook `ReadFile` and `LoadAssetByHash` with separate MinHook instances; Skies also grows the in-memory toc while Streets reads and substitutes. Document the hazards (chain order, removal, double hooks, two mods growing the toc) and recommend a **single shared hook hub and toc owner in `common/`**, with a test plan.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 32 (recommendation only).

#### MST-G7 (P3) Timing facts.

What blocks `gamethread::Run` during loads; why `toclocate` takes 26 s in a fresh process and 80 s late in a session (heap size), and a deterministic way to find the toc object; load-time facts that matter for startup-armed substitutions.
Answer (2026-11-16 update): ManhattanStreets/docs/RECON.md Part 33 (toc object at q[exe+74a6a00]+0x18, no scan).

#### H. Longer shots that would unlock features

#### MST-H1 (P2) Street wetness and rain on the ground.

Skies found rain follows the lighting mode and the rain mods only change atmospheres and textures. *Find:* whether wet roads, puddles and rain splashes are driven by a scalar separate from the lighting mode (a shader constant, a global parameter, a particle system), and how to set it, so a WET STREETS row (or a snow-cover one) exists without textures.
Answer (2026-11-15): ManhattanStreets/docs/RECON.md Part 32 and ManhattanSkies/docs/RECON.md Part 19.
#### MST-H2 (P3) Windows, shop signs and street lights.

Skies' stage-0 gate found no simple switch (`ManhattanSkies/docs/RECON.md` Part 13-14). Only if it turns out cheap: where emissive parameters of building, window and sign materials come from.
#### MST-H3 (P3) Anything else you notice that would make a good street-level row (events that exist but never trigger in the story, cut content in the level data such as unused vignettes or overlays, debug flags that expose street states).

### Priority summary (what unblocks what)

- **Split the Act 3 bundle into switches:** B1, B2, C3 (per-actor vetoes), C4, B5, then B6.
- **Traffic and crowd rows:** D1, D2, D3, then D4.
- **Landmark and street-life rows:** E1, E2, B3 (unload), C1 (start scripts), then E3-E5.
- **Seasons come back only if:** A1, A2, A3 (and A4 for a parameter-driven route), then A5-A7.
- **Music and ambience rows:** F1, F2.
- **Safety and coexistence (do early, cheap):** G4, G6, G1.

### Where to put the answers

Number them as Parts 27 onward in `ManhattanStreets/docs/RECON.md` (local, git-ignored), add a one-line pointer under each request above (`Answer: ManhattanStreets/docs/RECON.md Part <n>`), put anything the **other mods** can use (hook points, event catalogue, Wwise API, struct maps) in `docs/GAME_INTERNALS.md`, and update `ManhattanStreets/README.md` (the "More rows" table and its status column) when a row becomes possible. The maintainer can play on request: real Act 3 saves (Slot 1 Saves 41 and 42), a post-credits save, and other story points on ask; prefer **a play-through with the recorders on** (`mark`, `watch on`, `texwatch`) to guessing.


---

## Manhattan Skies (time of day, sky, sun, fog, weather: the sky half of the pair with Manhattan Streets)

### Goals

**Manhattan Skies** (`ManhattanSkies/`, alpha 0.1.0 released, 1.0.0 in preparation) lets the player change **how the sky and light of Manhattan look**, as a tab in the game's Settings screen (through `ModMenu/`), live, in any save, **using only what the game's own files already contain** (124 atmospheres) plus the atmosphere mods the player already owns. "AUTO" always means the game's own state. Native `.script` DLL, `common/` layer, no Script Hook. In the maintainer's order:

1. **Full control of how the world is lit, with a smooth, native-feeling time of day.** Any time of day at any moment; a continuous **dynamic day-night cycle** (built: four phases, random skies, sun path, pauses with the game) **without the loading-screen stop** every lighting change causes today, and with **lit windows and street lamps that follow it** (they do not today).
2. **Ambient light.** The one thing no row controls yet: how bright and what colour the *shadows and unlit surfaces* are (the player's top request). Plus sun, fog, haze, exposure, colour grade and clouds, which work today.
3. **Weather as its own choice:** rain, snow, wet streets, lightning, wind, fog banks, independent of the lighting mode (today rain is tied to the mode; the mods "MM Overcast (Heavy Snow)" etc. show snow exists).
4. **Atmosphere mods from a folder, several at once, reliable, with no dependency on another Overstrike mod** (built: runtime "new assets"; the fragile parts are listed below).
5. **Be a good citizen:** never end a crime or mission (a lighting switch does), never crash the Settings screen (three crashes so far), work with every other mod that uses `ModMenu`.
6. **Stretch:** seasons with Manhattan Streets, per-district weather, a preset save button in the tab, lightning storms, custom sky textures made from ordinary images.

### Current state (2026-11-14), so you do not redo it

Everything is in `ManhattanSkies/docs/RECON.md` (Parts in parentheses; tags [V]/[I]/[?] there) and `docs/GAME_INTERNALS.md` (section "Time, lighting"). Logs, dumps and probes are in `ManhattanSkies/recon/`. Tools in `ManhattanSkies/tools/` (`atmdiff`, `atmpair`, `atmvals`, `fieldprobe`, `imgprobe`, `tocinfo`, `archives`, `skipped`, `buildslot*`, `pausediff`, `raincapture`, `switchtest`, `datadiff`, `genpresets`). Dev commands in `ManhattanSkies/mod/src/main.cpp` (command file `scripts\ManhattanSkies.cmd`, log `scripts\ManhattanSkies.log`): `sky <mode>`, `skyset <n> <v>`, `look`, `atmload`, `atmlist`, `adj <slot> <delta>`, `slotaddr`, `peek`, `poke32`, `pokef`, `holdf`, `wp set|setr|dump|clear` (hardware write/read watchpoints), `snap`/`snapdiff`, `recompute`, `iotrace`, `toclocate`, `addasset`, `presets`, `preset <n>`, `mark`.

- **Lighting modes [V]** (Parts 4, 5, 8): seven modes (0 Day, 2 Sunset, 5 Night, 1 Night variant, 3 and 6 Overcast, 4 Early Morning/`kBlackout`); setter `exe+1f7f9e0(world+0xe10d0, index)`, world getter `exe+1f707e0`, mode field `world+0xe2040`. The game sets the mode once per load with the field still `-1`; swapping the index inside that call works with no transition; **calling the setter 3 s after a load hung the loading screen for good**. A setter call later is a "mini-load": `ShowLoadingIcon`, Flash `FadeOut`, a point-of-interest rebuild, `FadeIn`, Spider-Man's momentum stops, **and a crime or mission in progress is ended** (reported twice).
- **Atmosphere pool [V]** (Parts 6-10): manager `exe+60430c0`; `AtmosphereAdjust exe+162f020(mgr, slot, delta, mask, min, max)` (base weight 2, the map screen adds 1000), recompute `exe+162ff10`, load `exe+1626380`, release `exe+1624e80`; ~16 slots of 0x8c0 bytes. Slot fields [V] (Parts 10, 12, 13): refcount `+4`, asset id `+8`, sun block `+0x2a8..0x2bc` (RGB+intensity, rotation, elevation), exposure `+0x484`, fog distance `+0x588..0x594`, fog density `+0x598..0x5ac`, fog colour `+0x5b4`, haze `+0x5c0` (absolute 0..1; 0.5 is a white-out), colour filter `+0x668/66c/670`, clouds `+0x81c`; `+0x47c/+0x480` give black or white screens. **Weights do not cross-fade: a swap is sudden.** The lighting object's own activation (`caller exe+1a1b59e`) must be swallowed while a sky override is on.
- **Ambient [V negative]** (Part 10): the header "ambient" `slot+0x300` does nothing visible; the baked IBL textures (three, referenced at asset offsets `0x11c/0x344/0x44c`) carry the sky light. Unprobed: `slot+0x2d0..0x6a4` weak candidates (the image probe crashed the game around `+0x320`), and the material nodes `KeyLightIntensity`/`SunLightScale` (Part 13). **Windows and street lamps [V negative]** (Part 14): nothing we tried (overlay zones, levellight headers, atmosphere fields, a Day/Night 0/1 `.data` switch at `exe+728390c`) changes them alone; they follow the mode.
- **Rain [?]** (Part 15, dropped): not located; names `kOvercastRain`, `kNightRain`; atmosphere mods change it, so pairs cannot be told apart.
- **Runtime-added assets [V]** (Parts 17-17f): the toc (a DAT1 buffer in the heap, found by scan; toc object pointers `+0x10 ids`, `+0x28 sizes`, `+0x30 offsets`, `+0x48 count`; span 0 = group 0 kind 0 holds atmospheres and their textures) can be copied and grown with new asset ids; reads of made-up offsets (>= `0x7E000000`) on a **raw Overstrike `mods\modN` archive** (archive entry chunk map 0; entries are 72 bytes: bucket u32, chunkmap u32, name 64) are answered from loose files by a `ReadFile` hook. 74 of 116 mod archives in the maintainer's install are raw, 42 have a chunk map. Vanilla archives (`g00sNNN`, magic `DSAR`) are compressed with chunk maps; `a00s035.fr` ... `a00s045.la` are language archives whose files are missing on an English install.
- **Texture request behaviour [V log, I cause]** (Part 17d): an atmosphere mod loaded **quietly at weight 0 sometimes got its skybox textures requested and sometimes not** (Aurora, BND Early Morning, MM Heavy Snow); releasing and reloading it was also unreliable. Fixed by loading once at the switch, like a menu pick. The rule is unknown.
- **Shipped-but-skipped files** (Part 17f): atmosphere mods ship files their atmosphere does not reference (a 65,664-byte file = 64x64x16 + 128 = a colour-grade lookup volume; a 349,632-byte star-flare texture; a 371,348-byte asset; a 132-byte config). **Last built: swap the toc entry of the vanilla asset while the mod is on**; untested whether the game re-reads a resident asset.
- **The crashes**: the Settings-screen crash `exe+31e9582`, three dumps: `...2026-11-11-20-07-09.mdmp` (at `31e8dcc`), `...2026-11-13-18-37-28.mdmp`, `...2026-11-14-11-25-24.mdmp` in `Documents\Marvel's Spider-Man Remastered\`; first log `ManhattanSkies/recon/crash_2026-11-11_20-07_ManhattanSkies.log`; see section D. `ModMenu/modmenu.cpp` has had a forensic fault handler for Scaleform code since 2026-11-14 11:28 (registers, object memory, last 96 menu log lines); no crash has happened since, so it is unexercised.
- **Game-state signals we use today**: world timers `exe+5d6a028/5d6a168/5d6a2a8` (freeze when paused), time scale `exe+621b890+0x18`, hero position through `GetActor exe+15a0560` (handle at `exe+5D9DD00+0x1C`, transform `+0x30`), and, for crimes/hideouts/fights, **names of HUD method calls** seen by a hook on `exe+1d1cee0` (starts: `ShowCrimeScanner`, `ShowCounter`, `ShowMissionObjective`, `ActivateProgressBar`, `SetComboCounter`; ends: `HideCounter`, `HideMission`, `DeactivateProgressBar`, `HideActivity`). These are a stand-in; the real states are wanted (section C).
- **What the mod needs from you most** (details in the priority summary): ambient light (A1), a lighting switch that does not stop the hero or end a crime (A3, C2), the real signals for combat/crime/mission/cutscene (C1), the texture/asset lifecycle (B1-B3), the Scaleform crash root cause (D1).

### Requests

Priority legend: **P0** = blocks the next release or is the player's top request, **P1** = big quality win, **P2** = needed for later features, **P3** = long shot. **Do not limit yourself to the wording**; dump any structure you meet while digging (field maps are as valuable as answers).

#### A. How the world is lit: ambient, the sun, the clock, windows, weather

#### MSK-A1 (P0) Ambient light control.

*Why:* the top missing row: brightness and tint of shadows and unlit surfaces ("mood"), independent of the sun. We can fake it with exposure, haze, fog colour and a colour filter, but they affect everything. *Known:* see "Ambient" above; the sky light comes from baked IBL textures; the header ambient value does nothing visible. *Find:* where the renderer reads ambient/IBL/sky-light intensity and tint for **diffuse GI and specular reflections** (the per-frame constant buffer or lighting-pass inputs; the function that fills them from the active atmosphere slot or from the blended result that `Recompute exe+162ff10` writes); which slot fields (or fields of that blended result) feed it; any global multiplier (`SunLightScale`, `KeyLightIntensity` material nodes, a post-process volume); the **fields we never probed** (`slot+0x2d0..0x6a4`, and `+0x6a8..0x8c0` beyond the clouds); per-material or per-zone light-probe intensity; and a live recipe (offset + value range + the right call to re-apply) that scales ambient up/down and tints it. *Verify:* write it live with `pokef`/`holdf`, then compare screenshots of a shaded alley, a roof at night and an interior door; report units and safe ranges.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (lead, not run: AmbientFillIntensity at slot+0x30c is 0 in vanilla).

#### MSK-A2 (P0) The complete annotated atmosphere slot (0x8c0 bytes) and the blended result.

*Why:* we know ~14 of the fields; every unknown one may be a row (sun disc size, glow, moon, stars, god rays, volumetric fog, cloud speed/density, horizon colour...). *Find:* a full struct map: for each field its offset, type, unit, safe range, who reads it (xrefs) and what it does visually; how the **asset's DAT1 sections** map into the slot (the parse/copy function; the section tags; `tools/ALERT` has no atmosphere type); the **blended result struct** (the sum over active slots that the renderer actually reads) and where it lives; what `+0x47c/+0x480` really are. Deliver a C struct and a markdown table. *Known:* `fieldprobe_scan*.log`, `fieldprobe_world.log` in `ManhattanSkies/recon/` (the probe crashed the game around `+0x320`, so some regions were skipped: find out why: a field that must not be written).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (all 299 field names; slot map by block, partly by field).

#### MSK-A3 (P0) A lighting switch without the mini-load.

*Why:* every time-of-day change stops Spider-Man in mid-air, shows a loading icon, rebuilds the map markers and **ends a crime/mission in progress** (two reports on 2026-11-14). The dynamic cycle needs hundreds of switches. *Known:* setter `exe+1f7f9e0` (+ what it calls: `ShowLoadingIcon`, Flash `FadeOut` / `FadeIn`, a POI rebuild); the Flash fade can be swallowed (done); the stop of momentum and the mission side effect cannot. *Find:* a **line-by-line account of everything the setter does** (call tree: the levellight swap, overlay/zone loads, streaming flushes, the POI rebuild, the hero freeze, mission/crime notifications) and which parts are *necessary for the visuals* versus *side effects of the mode change being a "level state" change*; then the smallest set of calls that changes the visuals (sky, sun, levellight, lit windows/lamps, weather) with **no side effects**, or a flag/state that makes the game treat it as a seamless change, or a way to prevent the hero freeze and the mission teardown while the setter runs. If it cannot be avoided, document exactly what is torn down so we can predict it. *Verify:* a call recipe tested while swinging and during a crime (a Maggia pursuit, a hideout wave, the Sable APC mission).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 18 (the load-screen handler and its SkipLoadScreen rule; recipes not run).

#### MSK-A4 (P0) Lit windows, street lamps and everything that follows "night".

*Why:* "night" must be a *fraction*, not a mode: today the city's windows, lamps, signs, car headlights and neon only follow the lighting mode, so a dusk sky over a lit or unlit city is impossible, and a smooth cycle pops. *Known:* Part 14 (negatives). *Find:* the system(s) that decide, per object or per zone, whether emissive windows/lamps/signs/headlights are on; what input they read (the mode? a "night" scalar? the levellight asset? a time-of-day bit in a zone?); the exact data (levellight asset format, per-zone lists, material emissive parameters, instance flags); and a **call/field that sets "lights on" independently** (instant, or as a 0..1 ramp). Also street-lamp glare, light shafts and cookies; building window light colours and variety; whether lit windows are baked into textures plus a mask. If it is per-zone data streamed in at load, say how to override it.

#### MSK-A5 (P0) Is there a continuous time-of-day inside the engine?

*Why:* the cycle is built from seven discrete modes and a hand-steered sun; if the engine has a real parameterised path (a clock, a "time of day" float, a sun-path curve, blended atmosphere keyframes used by cutscenes or DLC), we should drive that instead. *Find:* any time-of-day or clock variable (story clock, mission clocks, loading-screen day), any code that interpolates between atmospheres or key lights (cutscene lighting keyframes, camera-tied lights), and the atmosphere **"blend" machinery in the manager**: why weights do not cross-fade (is the sum used only for sun/fog/exposure scalars while the sky texture is "the top weight wins"? is there a second sky-texture slot for a blend?) and whether a true two-sky crossfade is possible (a shader input for a blend factor?). *Known:* Parts 9, 16; moving 50 weight steps made the sky swap at some point.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 21.

#### MSK-A6 (P1) Weather: rain, snow, wetness, lightning, wind, fog banks.

*Why:* a "WEATHER" row independent of the lighting mode. *Known:* Part 15 (dropped), mode names, the snow mods. *Find:* what draws rain (particle effect asset/config, a "precipitation" system, post effects like lens droplets), what turns it on (a mode-dependent flag? an atmosphere field? a level/zone state? a Wwise rain state?), its intensity parameters, **wet-surface response** (wetness/puddles/roughness: a global scalar? a material parameter collection? tied to the same flag?), lightning flashes (do they exist?), thunder audio, wind (cloth, trees, flags, rain direction), snow (what "MM Overcast Heavy Snow" changes: which assets, especially `89D5F9DA0876FDB1` and the 132-byte `B0D1CA3D73401F93`) and a recipe to switch rain/snow/wetness/wind separately from the mode and at a chosen intensity. Also whether weather is per-district.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (rain, wetness, wind are atmosphere fields).

#### MSK-A7 (P1) Fog and volumetrics.

*Why:* fog rows work but we only know the slot's fog table (distance 4 floats, density 6 floats). *Find:* the fog model (analytic height fog? volumetric froxels? the meaning of each of the 4+6 floats: near, far, start, height, falloff, max opacity, scattering colour, sun in-scatter), distance haze vs fog, light shafts/god rays (a toggle and intensity), volumetric lighting through windows, **sun-in-fog glow**, fog colour by direction (sun side), and the fog's interaction with the sky (horizon blend). Document where they are applied (the post pass) and the field that scales each.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (partial).

#### MSK-A8 (P1) Sun, moon, stars, sky dome, clouds.

*Why:* steering the sun moves the light and shadows but, we believe, **not the sun painted in the sky texture**; rows for moon, stars and clouds are wanted. *Find:* how the sky dome is drawn (one HDR texture? a cube? is there a **sky rotation / texture offset** parameter, and where?), whether a sun disc/halo is drawn procedurally (size, colour, glow fields) or only painted, whether the moon and stars are separate (textures? fields?), the cloud layer (what `+0x81c` switches; the layer's parameters: coverage, speed, height, shadows on the city), and the relation between the slot's sun rotation/elevation and the painted sun's position per atmosphere (a table of "painted sun azimuth/elevation" for the 124 skies so we can rotate the sky to match any sun path).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (partial: sun disk fields).

#### MSK-A9 (P1) Shadows.

*Why:* low sun, night and overcast shadows decide the mood. *Find:* the shadow system's inputs from the atmosphere (sun direction, cascade distances, softness/penumbra size, bias), whether a **moon shadow** exists at night and what enables it (the night sun intensity is 0 in the catalog), shadow draw distance and resolution settings, contact shadows, and a way to force the main directional light on/off or to choose which light (sun/moon) casts shadows.

#### MSK-A10 (P1) Exposure, tonemap, bloom, the colour grade (lookup), post-process.

*Why:* we have exposure bias, a colour filter (RGB multipliers) and a clouds switch; "colour grade" should be a real choice. *Find:* the auto-exposure model (eye adaptation speed, min/max, metering), tonemapper, bloom, vignette, film grain, chromatic aberration, lens dirt, sharpen, DoF, where each lives (slot fields, a global post-process volume, a camera/level config), **the colour-grading lookup: asset type and format, which atmosphere/level/zone selects it (the mods ship a 64x64x16 volume `89D5F9DA0876FDB1`), how to select another lookup by id live** (and the 128-byte header), plus any per-district grade. *Verify:* swap the active lookup for another vanilla lookup and show the colours change.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (partial: field names).

#### MSK-A11 (P2) The baked lighting set (IBL / GI).

*Why:* a sky swap without its baked lighting leaves a mismatch the player notices (the first test: "the streets stayed in night mode"). *Find:* what each of the three baked textures an atmosphere references (`0x11c`, `0x344`, `0x44c`) is (diffuse irradiance cube/SH, specular prefiltered cube, sky-light?), their formats and sizes, whether the lighting also uses per-zone **light probes / lightmaps** with day variants (and how the game chooses), and whether the IBL can be **regenerated at runtime** from a sky texture (a prefilter pass we could call) so custom skies get matching ambient.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 20.

#### MSK-A12 (P2) Interiors and special areas; who overrides us.

*Why:* our cycle must not touch interiors, and mission skies must keep their own light. *Find:* how interiors choose their lighting (the `INTERIOR` atmospheres, per-zone atmosphere overrides), the code that applies an atmosphere per level/zone/district (do districts have their own fog/atmosphere?), which systems push their own atmosphere (the map screen `+1000`, cutscenes, hideouts, the suit/gadget menus) with a table of **callers of `AtmosphereAdjust` and of the setter `exe+1f7f9e0`** (RVA, meaning, when), so we know what overrides us and when.
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Parts 21 and 22.

#### B. Assets: the atmosphere pool, textures, the toc, a safer way to add our own files

#### MSK-B1 (P0) When does an atmosphere request its textures? (the black-sky mystery)

*Why:* a load at weight 0 sometimes requested the skybox/IBL textures, sometimes not; re-activating did not request them; reload was unreliable. We need the rule and a **reliable call "make this slot's textures resident now"** (and "release them"). *Known:* Part 17d log evidence (the `atmpacks` reads for Aurora, BND, MM Heavy Snow). *Find:* the loader of an atmosphere asset (the callback on load completion), what it does with the weight, where texture ids are read from the asset and requested, the texture asset manager call used, what keeps them resident and what frees them (the pool's ~16 slots, the texture streaming pool), what "release slot" does to the textures, and why a quick release+load of the same id hit a stale state.

#### MSK-B2 (P0) A safe way to serve our own bytes for any asset id, without borrowing an Overstrike archive.

*Why:* new assets need a raw archive that exists only if the player has another Overstrike asset mod; rewriting an unused language-archive entry was rejected (the player may have every language). *Find:* (a) how and when the game **opens archive files** (all at startup? lazily? a handle table sized by archive count? DirectStorage or `ReadFile`?), the per-archive state struct (handle, chunk-map pointer, compression flag; how "raw" is decided: file magic or the entry's chunk map), (b) whether a **new archive entry can be appended at runtime** (grow the archive table and the handle table), (c) the **central "read asset bytes" function above `ReadFile`** (the dispatcher keyed by `(archive, offset, size)` and the asset request/completion objects) so we can complete a request from memory for a chosen asset id with no archive at all, and (d) the **header/stream split** (Streets found a texture is two toc entries and the stream is re-read at its original location). Deliver a hook point and a recipe that works on a stock install with no `mods\modN`.
Answer (2026-11-16 update): ManhattanSkies/docs/RECON.md Part 23.

#### MSK-B3 (P0) Replace, reload or evict a resident asset by id.

*Why:* swapping the toc entry of a vanilla asset (the colour-grade lookup, the sun flare) only works if the game loads it after the swap. *Find:* the table of **loaded assets** (id to object, refcount; `AssetRelease exe+1626940`, ref count at `asset+4` are known to Endless Arena), the function to evict or reload one id (and its dependants), what happens to GPU resources, and a recipe to **replace a resident texture's contents in place** (same size/format) through the upload path. (Coordinate with Manhattan Streets MST-A1/A2: same machinery; answer once and cross-reference.)
Answer (2026-11-16 update): ManhattanSkies/docs/RECON.md Part 23.

#### MSK-B4 (P1) Everything about `AtmosphereAdjust`, the manager and the load queue.

*Find:* a table of every caller of `exe+162f020` and `exe+1626380` (RVA, who, when, delta, mask), the manager's full struct (pool capacity, the slot free-list, per-slot flags, the "base" slot concept, the map screen's +1000), what `Recompute` produces and who reads the result, the threading rules (we run `AtmosphereAdjust` on the game thread through a posted job; is that required?), and the failure modes of a full pool (the first browse failed after ~13 looks: Part 12).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 21 (partial: callers table).

#### MSK-B5 (P1) Who loads which atmosphere.

*Why:* so mod skies and our overrides do not break missions, and so we can say each sky's origin honestly. *Find:* for each of the 124 atmospheres (and the five base ones) the levels, zones, nodegraphs, cutscenes or scripts that load it (a reverse index over the toc/zone/config data), and what the "STRANGE"/"INTERIOR" skies need (interiors, special lights).

#### MSK-B6 (P2) The `.atmosphere` and sky `.texture` formats, fully.

*Why:* offline tools to make or convert skies, and a custom-sky feature (the player drops a PNG/EXR). *Find:* the DAT1 section map of an atmosphere (tags, sizes, fields to slot offsets), the texture reference fields (types, alignment: we scan 4-byte-aligned u64s today), the asset versions we met (**2,770,688 / 2,770,768 / 2,770,784 / 526,080 / 526,112 bytes**: what differs?), sky texture encoding (16,777,344 = 8k BC6H? and the 8,388,736 / 8,386,688 / 24,704 / 131,200 / 143,876-byte companions), the header vs mip-stream split for skies, and a written spec so an encoder can be built (BC6H HDR from an equirect image).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Parts 19 and 20 (partial).

#### MSK-B7 (P2) Toc and archive structure for everyone.

*Find:* the live toc object's complete struct (we know ids/sizes/offsets/count and the spans array), the span semantics (lookup `exe+16d7990`, caller `exe+16dd090`, group*8+kind), the "dupe metadata" offsets section (`0xDCD720B5`, 8 bytes per entry) and the per-language duplicates (`a00s0xx`), the install-bucket and chunk-map fields, the file-handle table, and the checks on `toc` (`toc.sha1`). Put it in `docs/GAME_INTERNALS.md`.
Answer (2026-11-16 update): ManhattanSkies/docs/RECON.md Part 23 and docs/GAME_INTERNALS.md.

#### C. Game-state signals (so we never switch at the wrong moment)

#### MSK-C1 (P0) Real flags for combat, crime, hideout, mission, cutscene, loading, pause, photo mode, map.

*Why:* today we infer these from HUD method names (`exe+1d1cee0`), which missed the end of the Sable APC mission (our "idle" fired mid-crime) and cannot see cutscenes. *Find:* for each state the underlying game object and a **cheap read from our worker thread**: the mission/activity manager (what is active: story mission id and phase, crime type and phase, hideout/base wave, challenge, tail, collectible), the combat/alert state (hero "in combat", highest enemy alert level, number of enemies aware of the hero; `HeroStateRunLocal`, `CombatCamera`, bot awareness), cutscene/cinematic and "HUD hidden" states, loading and streaming idle, pause menu / photo mode / map screen / fast travel, the options screen. Give addresses, struct offsets, value meanings, and a test recipe for each (a recorded transition log). Include the **crime types** and their start/end events (`ShowCrimeScanner` args; the Maggia, Sable, Demon and Hammerhead families).
Answer (2026-11-15): docs/MISC_RECON.md section 6 (partial: mission states, loading, hero flags).

#### MSK-C2 (P1) What exactly does a lighting switch end, and when is it harmless?

*Why:* a switch ended a crime and a story-mission segment. *Find:* the code path from the setter to the mission/crime teardown (the POI rebuild? a zone reload? the loading-icon "world transition" state that missions treat as an abort?), which activities survive, and a **predicate "a switch is harmless now"** we can evaluate. (If A3 finds a clean switch this is moot; otherwise it is the safety net.)
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 18.

#### MSK-C3 (P1) The authoritative game clock and pause flag.

*Why:* the cycle follows three timers (`exe+5d6a028/168/2a8`) and the time scale (`exe+621b890+0x18`); they work but are guesses. *Find:* the real "gameplay time" counter and its scale (slow-mo from focus and finishers: our `ts` dropped to 0.2 for moments), the pause state (pause menu, photo mode, cutscene, loading) in one place, and whether the game has any **in-world clock** (a story time, a "world age").

#### MSK-C4 (P1) Hero state, position and place.

*Find:* stable getters for position, velocity/speed, ground/air/swinging/zipping/wall-running/gadget/takedown states (the state machine and its enumeration), whether an interior is entered, **the district/neighbourhood name and zone the hero is in** (for per-district weather/time), the current level/overlay set, and a handle-to-actor resolver that works every frame without the game thread (we use `exe+15a0560` through a posted job).

#### MSK-C5 (P1) Catalogue of HUD calls per activity.

*Find:* from `uilog` or by reading the HUD's ActionScript, the start/update/end calls (name, owner, args) of every activity type (crimes, hideout waves and completion, challenges, tails, collectibles, boss entrances, story objectives, tutorials, `ShowMissionOption`), so the stand-in detector can be exact until C1 is answered.

#### MSK-C6 (P2) Mission lighting control.

*Find:* every script/mission that forces a lighting mode or a specific atmosphere (the opening mission's `kBlackout` dawn; "missions reset the time of day" as the maintainer saw), how (setter callers, nodegraph nodes that set lighting, overlay entries), the names and ids of those nodes/configs, and a way to suppress or intercept them (so the player's time of day can stay inside missions that do not need their own).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 22.

#### D. The Settings screen: crashes, rebuilds, better rows

#### MSK-D1 (P0) Root cause of the Scaleform crash at `exe+31e9582` (and `31e8dcc`).

*Why:* three crashes in four days, always 2-7 s after leaving the mod tab, always the same Flash call chain. *Known:* the faulting function (`exe+31e9540`) asks `31c5070(this, 0x200)` for an object's storage record and gets **null** (the 56-byte-cell page table says the cell is free), then reads `[null+0x90]`: **a use-after-free of an ActionScript array/object**. Stack (innermost first): `31f9422` -> `32c2745` -> `32c0db6`/`32261e9` (the AS interpreter, recursing) -> `3224a31` -> `3224d14` -> `3411ee7` -> `34d5d1d` -> `34d8a04` -> `34f511c` -> `1d294d3` (the movie's per-frame advance) -> `1d23088`. 42 and 48 options-screen openings preceded the last two crashes (every row change rebuilds the whole screen, ours included); keeping our native strings/items alive for 32 builds did **not** help; our code is not on the stack. *Find:* which AS function runs (use `tools/sigtools/abcdump.py` on the options/pause menu movie: what `push`/array method is at that point?), which array it is and who holds the reference, **why its refcount went to zero** (a managed-value list node of ours? the tabs array we `PushBack` into `args[0]` of `OpenOptions`; the Value we build with `movie = kMovie()` (`exe+1d29690`): is that the *same movie* as the options screen's? are the `FadeOut`/`FadeIn`/`CloseMenu` calls we swallow expected to release or consume something? does the game's own caller free or reuse the args after our `PushBack`?), and a **correct, safe procedure to add tabs** (ownership, AddRef/Release, GC rules, which thread, which movie) so `ModMenu/modmenu.cpp` can be fixed. If you can reproduce it (stress-open the screen 100 times, a page-heap build), do.
Answer (2026-11-15): not answered; real registers of the three dumps via tools/sigtools/dumpctx.py (EndlessArena/docs/RECON.md section 9).
Answer (2026-11-16 update): ManhattanSkies/docs/RECON.md Part 24 (faulting code identified: a freed container's script object during child removal; root cause not confirmed).

#### MSK-D2 (P1) Rows without rebuilding the screen; sliders; more choices; buttons.

*Why:* every row change re-sends the whole options array; a row holds at most 32 choices (`kMaxChoices`); the maintainer wants **sliders** (colour and fog strength, ambient), a save-preset button with a legend glyph, and richer headings. *Find:* the native **slider setting type** (brightness, volume): item layout and value API, how to make a continuous row with min/max/step; how a single row's value or labels are updated in place (the Flash call and its args) without `OpenOptions`; the per-choice help/preview image fields; how to add a **button row** (run an action) and a **legend glyph**; the maximum rows/choices and why; how `Select`/`Set`/`Reset` callbacks are routed and what the callback hashes we log are (`0x0d3daa00`, `0xf337b658`, `0xf5185638`, `0xd649ae01`, `0x30216bfe`, `0x84ce74e7`).

#### MSK-D3 (P2) Menu state signals.

*Find:* the flag for "options screen open / which tab / which row", "pause menu open" (and which page), photo mode, map; the cleanest hook for "screen closed" (we use the first-page build in `AddItemHook`); and a way to read the game's own **Settings** (its time-of-day option: handler and storage, so our row can replace it and the two cannot disagree).

#### E. The game's own time-of-day option, the save, and the load

#### MSK-E1 (P1) The game's Settings "time of day" and what is saved.

*Find:* the Day/Sunset/Night option's handler (Select to `exe+1f7f9e0`?), where its value is stored (settings file? save game? memory only), whether a **save game stores the lighting mode** and what a load does with it, the exact semantics of "the setter is called once per load with the mode field -1" (which caller is `exe+1faab8f` and which `1f74862`), and how to make our chosen time of day apply at load **without** replacing the argument inside that call (a cleaner, documented route).
Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 18 (partial: setter callers).

#### MSK-E2 (P1) A safe-to-switch readiness signal after a load.

*Why:* a setter call 3 s after a load hung the loading screen; we wait 8 s + a readable hero + advancing timers (a heuristic, untested at its limits). *Find:* what state the hung call collided with (reproduce it if you can; `tools/sigtools/dumps/hang_20261015_1941.dmp` is an Endless Arena hang, a different one), and one reliable "world fully loaded, streaming idle, safe to change lighting" predicate with an address.

#### F. Threading and safety

#### MSK-F1 (P2) Which calls need the game thread?

*Find:* the engine's job/thread model: which of our calls (`AtmosphereAdjust`, `Recompute`, `LoadAtmosphere`, the setter, slot field writes, `ReleaseSlot`) are safe from a worker thread, which need the game thread (we post them via a hook at `exe+1f745f0`), and what a safe cross-thread slot write looks like (the renderer reads slots on another thread: tearing risks).

#### MSK-F2 (P2) Is anything else watching the in-memory toc?

*Find:* whether the game validates the toc (checksum, `toc.sha1`), caches pointers into its sections (the grow-and-repoint trick works: Part 17b), and anything that would make a repointed table unsafe over a long session or across a checkpoint reload (do the asset system or the streamer hold stale index/entry pointers?).

#### G. Ideas to check (long shots; answer what is cheap)

- **MSK-G1 (P2) Sky from our own texture, no toc.** Create a texture in memory and bind it as a slot's sky texture (custom skies from images, no archives).
  Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 20 (partial: runtime texture creation).
- **MSK-G2 (P3) Lightning and storms.** Does a lightning system exist (flashes, thunder, cloud illumination)? If not, what would drive a flash (a brief exposure/ambient pulse) and how does thunder audio hook in (Wwise events; coordinate with Streets F).
- **MSK-G3 (P3) Seasons.** With Manhattan Streets: what must change for snow (road and vegetation textures: MST-A; wet/snow material parameters; snow particles; breath puffs; NPC winter clothing) and which of those "MM Overcast Heavy Snow" already does, so the sky half can be built first.
  Answer (2026-11-15): ManhattanSkies/docs/RECON.md Part 19 (partial: what an atmosphere carries).
- **MSK-G4 (P3) Water.** Does water colour/waves follow the atmosphere or weather? A global water tint/choppiness parameter (rain choppiness, storm waves), sky reflections in water.
- **MSK-G5 (P3) Reflections.** SSR / reflection probes: do they update live with the sky? A call that re-captures probes after a sky change (stale reflections are a risk).
- **MSK-G6 (P3) City light colours.** A global scale or tint for emissive windows, street lamps, neon, billboards, car and vehicle lights (follows A4).
- **MSK-G7 (P3) What follows the time of day?** Does anything (crowd density, traffic, music, ambience, shop lights) follow the time of day, through which variable? (Coordinate with Streets D/F.)
- **MSK-G8 (P3) Photo mode.** Hooks for lighting/time controls in photo mode; does it freeze our cycle properly; can it expose time-of-day sliders (the maintainer may want them there).

### Priority summary (what unblocks what)

- **Ambient light row and a "mood" slider:** A1, A2, then A10 (grade), A11 (IBL), A8 (sky rotation and the painted sun).
- **A cycle with no loading stop and lights that follow it:** A3, A4, A5, C2, E2.
- **Never ending a crime or mission:** C1, C2, A3, C5.
- **No-dependency atmosphere mods, no black skies, mods' shipped extras:** B1, B2, B3, B6, B4.
- **Stop the Settings-screen crashes and make the tab nicer (sliders, no rebuilds):** D1 (first), D2, D3.
- **Weather as its own row, seasons later:** A6, A7, A9, G2, G3 (with Streets).
- **Cheap, high value, do early:** D1, C1, B1, A2, C3, C4.

### Where to put the answers

Number them as **Parts 18 onward in `ManhattanSkies/docs/RECON.md`** (the file already holds Parts 1-17f; it is committed, so keep it to RVAs, offsets and findings, no extracted game bytes), add a one-line pointer under each request above (`Answer: ManhattanSkies/docs/RECON.md Part <n>`), and put anything **other mods** can use (hook points, the callers table of `AtmosphereAdjust` and the setter, struct maps, the toc/archive description, the Scaleform ownership rules) in `docs/GAME_INTERNALS.md`. Where an answer is also useful to Manhattan Streets (textures, reload, overlays) or Friend or Foe (hero/combat states), cross-reference its request id instead of repeating it. Please leave any test helpers you write (`tools/...`) in the repo, with a way to re-run each check. The maintainer can launch the game, load any save (Slot 1 Saves 41/42 are Streets' real Act 3 saves; ask for a crime/hideout save), and run `scripts\ManhattanSkies.cmd` commands for you; the mod's own tab and log are the quickest probes.

---

## Endless Arena (Campaign and Horde at Ryker's Island: roguelike rewards and curses, gadgets, bosses, waves, HUD)

### Goals

**Endless Arena** (`EndlessArena/`, public repo, released as **0.2.0 beta 1** on 2026-11-07) is an endless-survival mode on Overstrike `.script`. The player takes a Manhattan dock circle to Ryker's Island
and fights waves: **Campaign** (chapters of 4 waves, one faction each, a boss every 4th wave, boons and curses as roguelike rewards) and **Horde** (one nonstop riot with a style meter).
Design rules we hold to: **no health sponges** (difficulty comes from numbers, mix and behaviour, never extra enemy health), boons never touch Spider-Man's skills, every curse has an upside, and
the mod only uses the game's own states and screens where it can. Goals, in the maintainer's order:

1. **Finish the roguelike layer.** Three curses are designed and blocked on engine levers: the **web-fluid curse** (Peter loses every web-based gadget, the Web Shooter, throwables and web-swinging for
   the run, and gains Focus faster), **Street Brawler** (melee deals +150-200 percent, health drains while airborne, can kill the player) and **The Last Hunt** (an undying hunter who chases the player
   through the whole run, even between waves, and cannot be killed). We also want custom perks beyond the game's items, and to offer powers and mods the player has not unlocked.
2. **Make gadgets a first-class reward** without crashes: unlock, show, refill and swap them cleanly inside a run, and show locked versus empty correctly in the wheel.
3. **Stability.** Four game crashes and two hangs traced to our gadget calls, the death/respawn flow and asset release. We want to know exactly what the game's code expects, so we stop poking it blind.
4. **Enemy behaviour and bosses.** Bosses and units that stand still, heavies that never attack, bodies that stand up, bosses that cannot be killed or end in a frozen pose, stuck units.
5. **Native-feeling UI and input on every device** (a Steam Deck player reports the Interact press does nothing), a real choice screen with icons, a panel for the current boons and curses.
6. **Music** (the game's own Wwise states, per wave and per boss) and other arenas beyond Ryker's.

### Current state (2026-11-14), so you do not redo it

All of this is in `EndlessArena/` (source `EndlessArena/src`, design `EndlessArena/docs/REWARD_DESIGN.md`, `LEDGER.md`, `UI_HUNT.md`; the table of every native we already use, with signatures, is
`EndlessArena/src/natives.cpp`). Mark: `[V]` verified live or from code.

- **Gadget ids** [V]: `EquipGadget(this, int id, int slot, bool force)` is **exe+2162300** (prologue `48 8b c4 48 89 58 10 48 89 68 18 48 89 70 20 57 41 56 41 57 48 81 ec b0`, hooked with MinHook in
  `gadgets.cpp`). Ids: 9C82 Electric Web, 9C83 Impact Web, 9C84 Web Bomb, 9C85 Trip Mine, 9C86 Concussive Blast, 9C87 Spider Drone, 9C88 Suspension Matrix, 9C89 Web Shooter. Also seen: 9C43 and 9C92
  (in the table), C350/C351 (equipped by the game itself right after a gadget equip, slots 0/1), and **id 0**.
- **Gadget table** [V]: `this` is the hero's `HeroWeaponManagerLocal` component. Table entries are 0x18 bytes at `this+0x1B8`, count at `this+0x628` (10 here: 9C43, 9C82..9C89, 9C92), each entry starts
  with the u32 id. Lookup `exe+2163500(this, id)` returns the record (`this+0x1a8+idx*0x18`) or null (first calls vtable `+0x98`); `exe+2163570` reads the byte at the record; `exe+2163360(this, slot)`
  reads the id at `this+0x6c+slot*40` and returns the gadget config (tail call `exe+15a0560`) or **null**. Equipped slot 0 = id at `this+0x6c`.
- **Hiding trick** [V]: writing 0 into an entry's id makes the wheel show that gadget red with ammo 0/0 and unusable; writing the id back makes it selectable again with its old ammo. Ammo survives
  hide/show (Web Bomb 3/4 stayed 3/4). A gadget unhidden **inside a run** keeps showing red 0/0 until the *player* equips it; equipping from our code (even equip + put back) did not clear it. In free
  roam and in our debug sandbox the same unhide is blue at once, and our equip works.
- **Crashes** [V]: four, all in one call tree: `exe+1ce9e67` (after `call exe+35ae765`) into the code that evaluates the equipped gadget; one was a null deref at **exe+374237** right after
  `exe+2163360` returned null for slot 0 (we had refused the game's own equips of id 0 and C350); three were an execute-at-address-0 (`access type 8`) at the moment of our `EquipGadget` call or table
  rewrite during a run. Minidumps show the crash *handler's* context, so the real faulting frame is lost (see EA-J2). In a **real run slot 0 holds id 0** and the game calls `EquipGadget(0, slot 0..7)`
  in bursts (wave starts); wheel selection of the Web Shooter sends `EquipGadget(0, ...)` which returns 1 but leaves our force-equipped gadget in slot 0.
- **Ammo** [?]: not found. `GadgetItemAmmoManager` (hero component) holds no gadget ids in its first 0x800 bytes; the weapon manager's first 0x800 bytes do not change when a gadget is fired; `inv watch`
  (inventory add/remove hooks) logs nothing on firing. Other relevant components on the hero: `GadgetResupplyPerk`, `MaxGadgetAmmoPerk`, `HeroWebShooterManager`, `HeroWebZipManager`,
  `SwingPointHunter`, `MeleeSwingPointHunter`, `HeroTraversalTracker`, `HeroTraversalTargeting`, `JobManagerThrowProjectile`, `ActorAbilityHistory`, `HeroAbilityLock`, `ItemLoadoutComponent`. In the
  arena the gadgets are the **bare, un-upgraded** versions (cause unknown: probably our run-loadout reset).
- **HeroAbilityLock** [V partly]: component vtable exe+38AA530. 128 abilities x 4 lock layers; the lock counter for (layer, ability) is the byte at `comp+0x1C8 + layer*0x80 + ability`; an ability is locked
  when any layer's byte is > 0 (or a timer from `exe+1E8AD60` is positive). Methods: `exe+1E8A640` (decrement a counter), `exe+1E8A6D0` (IsLocked), `exe+1E8A730` (debug draw, names through vtable+0x88).
  By bisecting layer 0 live: abilities **0-3** = attacks and shooting (punch, web shoot), **4** = ground movement (walk; shooting also failed), **5** probably web-swinging (4+5 locked blocked swinging,
  4 alone did not; 5 alone not confirmed). The maintainer recalls the Harry's Research Station mission taking the webs away for a moment [I, unverified].
- **Focus** [V]: `HeroSpiderAssaultManager` hero component: segment fills (float) at `+0x110/+0x118/+0x120`, caps `+0x114/+0x11c/+0x124` (100 each), count `+0x128` (3), active flag `+0x148`. We scale the
  per-tick change (50 ms) instead of hooking the gain function.
- **Damage and health** [V]: our `stats.cpp` hooks the hero's damage handler; the damage message amount is at `msg+0x144`; the hero health component has hp `+0x88`, max `+0x80`. `DmgCreate/DmgCommit`
  (natives.cpp) create and send damage. Boss health components: `BossHealth` (vtable exe+3839d38; floats at +0x80 max, +0x88 current, +0x90 and +0x98 unknown; has phases and fractions;
  `HealthVolleyTracker` vt exe+38a6ce0 on some), `BotHealth` (vt exe+3839e98). **Setting a boss's health to 0 through `HealthSet` reads back 0 but does not kill it** (two bosses kept fighting while the mod
  counted them dead). Outside their story mission bosses stop taking damage at a gate (1 HP, or a fraction: Hammerhead 55 percent) and Kingpin takes no damage outside his story phases (issue #18).
- **Engagement** [V partly]: units spawn with the engaged flag (`BotData` `SpawnEngagedWithHero` 0 to 1, `ThugBot`, `MrNegativeSwordsman`, `SilverSable` classes). **A boss spawned off-camera (behind the
  camera, 25-67 m) stood still for minutes and ignored "told about the hero"; the same boss spawned in view 20-35 m ahead, or by our `boss` command at 20 m or even 60 m, fights at once.** Ordinary goons
  spawned off-camera do fight. Heavies and miniguns often stand around (issue #1). Some units are flagged "calm" (spawn without the engaged flag).
- **Death and respawn** [V]: after `hero_down` the game reloads; usually back in about 7 s, **once an endless load** (force close needed; the mod's asset release 30 s after the run landed in the middle
  of it). A one-off hang inside `TargetQuerySystem::ProcessJob` (issue #4, dump in the repo notes).
- **Spawning** [V]: `spawn.cpp` loads the actor asset by hash, `SpawnRaw`, `CreateSyncObject` (slot numbers 28, 29...), registers and attaches; limits seen: body pool 150, "model budget" in unit types
  (white cubes, floating heads and invisible enemies appear when the game runs out of model room; a released asset that is requested again can come back broken); decision: no asset releases during a run.
- **Input** [V]: `EndlessArena/src/input.cpp` reads the keyboard with `GetAsyncKeyState`, Xbox/Steam Input pads with XInput (Y = Interact), PlayStation pads with raw HID (`psinput`), and ignores everything
  unless `GetForegroundWindow()` belongs to the game process. A Steam Deck player reports the Interact press does nothing.
- **Music** [V]: `music.cpp` drives the game's Wwise state group `I20_MUS_OW_A1` through the engine's exported Wwise API (`audio.cpp`): start event, then a ladder of states per wave tier, a sting per
  boss, a cooldown, end states. The state ids have no names in the lookup; roles come from when the game set them (`NEXT_RELEASE_RECON.md`).
- **UI** [V]: `gfx::Invoke` calls into the HUD movies (e.g. `ShowCrimeScanner(title, subtitle, 2, 5, [])` on movie slot 0x210 for boss pop-ups, the hideout/Taskmaster panels, results panels);
  `uilog` records every ActionScript call. The reward screen is only a notification-box message plus button polling (no icons).
- **Perks** [V]: 119 `PerkItemConfig` assets (`tools/assets/perk_ids.txt`, display text in `tools/assets/perk_text.json`); category 1 = suit power, 2 = suit mod, from `inventory::Info`.

### Requests

Priority legend: **P0** = blocks the next milestone (the three curses and the stability work), **P1** = big quality win, **P2** = needed later, **P3** = long shot.
Some requests overlap Friend or Foe (input, actions, spawn/despawn) and Manhattan Streets/Skies (music, mission and crime state); where so, answer once and cross-reference.

#### A. Gadgets: equip, wheel, ammo, upgrades

**EA-A1 (P0) What exactly does a safe equip need, and what does the player's own equip do that ours does not?** *Why:* four crashes and a red-until-equipped wheel come from this. *Known:* see the gadget bullets
above. *Find:* decompile `EquipGadget` (exe+2162300) completely: every field it touches, every call, the "force" flag, what it creates (gadget instance, weapon actor, UI notify, ammo registration), and
the code at `exe+1ce9e67`, `exe+35ae765`, `exe+374237` and `exe+1cd0e4f`/`exe+382cdf9` (the callers on the crash stacks): what the per-frame equipped-gadget evaluation expects (`this+0x6c+slot*40`
non-zero? a live instance object? a config pointer from `exe+2163360`?), and which of those our mid-run equip violates. Then: **the UI path.** What does wheel selection call (`UIQuickSelectWheelEquip`,
`kEquipGadgetUp/Left/Right`), in what order, on which thread, and what extra it does (the event that makes a granted gadget stop being red). Give a call recipe that equips a gadget in a live run
without crashing, or say why it cannot. *Verify:* `gadgets::Equip` from the command file in a forced run.
Answer (2026-11-15): EndlessArena/docs/RECON.md sections 5 and 9 (decompiled; the crashes were a freed weapon manager and a slot id missing from the table).

**EA-A2 (P0) Who writes id 0 into the gadget slots, and what does "id 0 = Web Shooter" mean?** *Why:* in a real run slot 0 holds 0 and the game calls `EquipGadget(0, slot 0..7)` in bursts at wave
start; wheel select of the Web Shooter sends equip(0) that "succeeds" without effect while a force-equipped gadget sits in slot 0. We need the model: what are the slots (Up/Left/Right on the wheel? three,
eight?), what 9C43 and 9C92 and C350/C351 are, and how the Web Shooter is represented (id 0, 9C89, or a separate manager `HeroWebShooterManager`). *Find:* the slot count and layout of the equipped array at
`this+0x6c` (stride 40: what are the other 36 bytes), who calls with id 0 and why (a mission/loadout config? our own `runloadout` unequips? the combat-encounter script?), and a way to find "which gadget is
selected right now" reliably. *Verify:* dump slots 0-7 at rest and after each wheel pick.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 5 (id 0 = no weapon; slot and table layout).

**EA-A3 (P0) Where is gadget ammo?** *Why:* a granted gadget must start with a chosen ammo count; ammo is the design's leash (refills from kills, `Web Fluid Cartridges` boon, Resupply). *Known:* see above (not
found in the first 0x800 bytes of two components; inventory hooks silent). *Find:* current and max ammo per gadget (the HUD's `0/4`), where max comes from (config, `MaxGadgetAmmoPerk`, upgrade tier),
the functions that spend one shot, that refill (`GadgetResupplyPerk`, finisher refills, `Scavenger`, `Metabolic Converter` perks, crates), and a setter/getter pair usable from a mod. Also what the
wheel reads to decide red/blue and `0/0` versus `0/4` (`0/0` = "not in the table"; `0/4` = empty). *Verify:* set ammo, fire, watch.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 6 (ammo = inventory item count; max and refill functions). Not run in game.

**EA-A4 (P1) Gadget upgrades and the bare versions in the arena.** *Why:* the arena shows un-upgraded gadgets (Impact Web, Web Bomb, Trip Mine radius and ammo upgrades missing); we dropped upgrades from the
reward pool, but want to know why and whether the player's saved upgrades could simply apply. *Find:* how upgrade tiers are stored (inventory items "blueprints"? the profile?), how they are applied to a
gadget at equip (config overrides, `HeroGadgetConfig`, `GadgetConfig`, `MasterWeaponListConfig`), and what in our `runloadout` reset (or in the arena level) clears them.

**EA-A5 (P1) How does the wheel build its list, and when does it refresh?** *Why:* it shows gadgets red until the player equips; a user's custom 9th gadget ("Web Up", added by another mod) disappeared from the
wheel in the yard and came back after firing the web shooter. *Find:* the wheel's data source and refresh triggers (function, event, message), so we can force one after changing the table, and the
rules for what is listed, hidden, red, or greyed (unlock flag vs ammo vs table presence). How a modded extra entry gets in, and what dropped it.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 7.

**EA-A6 (P2) "Locked" as the game shows it early in the story.** *Why:* the user remembers locked gadgets simply not appearing in the wheel in the early game, which is what we want for un-owned gadgets.
*Find:* the unlock state per gadget (profile/inventory flag), and what the wheel does with a locked one; a safe way to toggle that state for the run and restore it (also at crash recovery).
Answer (2026-11-15): EndlessArena/docs/RECON.md section 7 (locked = item absent from the inventory).

#### B. Web abilities and the ability lock (the web-fluid curse)

**EA-B1 (P0) The full ability table of `HeroAbilityLock`.** *Why:* the curse must lock: Web Shooter, web-swinging, web zip/point launch, web strike, web throw (throwables) and the gadget wheel's web gadgets,
while leaving walking, dodging, punching and the non-web gadgets. *Known:* layout and bisect results above. *Find:* the **name and index of every ability 0-127** (the debug-draw function at `exe+1E8A730`
prints names through vtable+0x88; the name table is behind `[rip+0x55c5f49]` from `exe+84b4a7`; dump it), what each locks (animation states? input actions?), what the four **layers** mean (mission,
cutscene, scripted tutorial, dev?), which API the game's own script calls to lock/unlock (`exe+1E8A640` is the decrement; find the increment and the layer-aware entry points and the event or message that
requests a lock, e.g. from a mission node), how long locks persist and whether a death or reload clears them, and a recipe that locks only the web abilities. *Verify:* lock one ability, test in game.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 1 (all 40 abilities, layers, calls).

**EA-B2 (P0) How the game removes the webs in the Harry's Research Station mission.** *Why:* a ready-made, tested "no webs" state to copy. *Find:* the mission data (zone/nodegraph/config) and the actions it
fires (lock layer, ability ids, HUD changes, gadget wheel changes, swing-point disabling), and how it restores them. Give the exact calls.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 2 (partial: the nodes and calls; the mission's own graph was not opened).

**EA-B3 (P1) Throwables.** *Why:* the curse removes throwables too. *Find:* how "throw an object" is gated: the state or ability, the component (`JobManagerThrowProjectile`, `SpiderSenseHeroThrowDirectionEffect`,
`HeroThrow*`), the input action, and the lock index (EA-B1) or a direct flag; also what happens to an object already held.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 1.5 (partial: combat moves use HeroCombatActionLocker; its ids not dumped).

**EA-B4 (P1) The Web Shooter and web-swinging as systems.** *Find:* what `HeroWebShooterManager`, `HeroWebZipManager`, `SwingPointHunter` and `HeroTraversalTargeting` own; flags that mean "swinging", "zipping", "web-striking", "wall-running"; the cheapest reliable "is the hero airborne / swinging / on the ground" signals (also for EA-C1).
Answer (2026-11-15): EndlessArena/docs/RECON.md section 8 (partial).

**EA-B5 (P2) "Out of web fluid" feedback.** The HUD pieces for the web shooter's ammo bar and gadget `0/0`: how to show the web shooter as empty without the lock (for a softer curse).

#### C. Combat signals for curses and perks

**EA-C1 (P0) Airborne detection.** *Why:* Street Brawler drains health while airborne and needs it to be correct (not a swing, a jump, a ragdoll...). *Find:* one cheap per-frame signal: hero mover/state
machine state ids (`HeroState*`, `StateMapHeroWeapon`, `HeroTraversalTracker`), "feet on ground" from the mover/physics, swinging and zipping flags; give the field and the values, and which states count as
airborne, combat-airborne, ragdolled and wall-running. *Verify:* log it live while swinging, jumping, falling and fighting.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 8 (HeroMacroState flags).

**EA-C2 (P0) Classifying damage and hits.** *Why:* Street Brawler's +150-200 percent applies to **melee only** (gadgets, webs, suit powers, environmental, takedowns and finishers excluded);
perks like Vampire/Black Suit need clean "kill by the hero" attribution. *Known:* our damage hook sees the message amount at `+0x144` and the damage hash. *Find:* the whole damage message struct
(`DmgCreate`/`DmgCommit`: attacker, victim, amount, damage type/category, weapon id, hit kind: light/heavy/aerial/dodge counter/takedown/finisher/gadget/power/projectile/environment/enemy-on-enemy),
how to tell a hero melee hit, a hero gadget hit and an enemy-on-enemy hit, and the "killed by" and "kill credit" fields. Also a **safe way to apply damage-over-time** to the hero (Street Brawler drain,
Rent Is Due) that bypasses hit reactions and cannot trigger a death animation spuriously, and how to leave the hero at 1 HP instead of dying.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 3 (DamageRequest layout).

**EA-C3 (P1) Events worth listening to.** Perfect dodge, parry/counter, takedown, finisher, web zip, gadget used, focus spent, hero hit, enemy killed (with killer), suit power activated and ended:
for each, the event/message id (`SendEvent`/`EventSys`), payload, and the function we can hook. The game already has most (`Discharge Power Cells` reads perfect dodges, `Threat Sensors` slows time after one).
Answer (2026-11-15): EndlessArena/docs/RECON.md section 4.

**EA-C4 (P1) The Focus meter, fully.** *Known:* offsets above. *Find:* the gain function (what feeds it per hit, per dodge, per web hit, per perk: `Kinetic Power Cells`, `Webbed Striker`, `Battle Tech`),
the spend function (heal from a Focus bar, finishers, suit powers), drain rules, how `First Aid` and `Life Support` are applied, and the active flag at `+0x148`. A hook point where a multiplier can be
applied cleanly would replace our per-tick diff.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 4 (partial: the gain event).

**EA-C5 (P1) Health: heal, max, damage chain.** The functions behind `HealthSet`, healing, max-health changes (`Life Support`, suit mods), the hero's damage-taken chain in order (so a "reduce incoming damage"
perk can hook once), invulnerability flags (Peter Tingle's "first hit does nothing" is a hack: can a proper "absorb next hit" flag be set?), regeneration.
Answer (2026-11-16 update): EndlessArena/docs/RECON.md section 10 (partial: damage flags kPreventKill, kNoReact; health sync layout).

**EA-C6 (P2) Time dilation.** `Threat Sensors`, the game's slow-motion, the pause/freeze we do with the time-scale (0.001 at reward screens): the owning system, whether enemies' AI, audio and effects
all respect it, and what leaks.

#### D. Enemy engagement, bosses, bodies

**EA-D1 (P0) The engagement model.** *Why:* we do not understand why a boss spawned behind the camera never moves while the same boss spawned in view does; goons spawned the same way do fight; heavies
stand around. *Find:* the full pipeline from spawn to attack: `BotData`/`SpawnEngagedWithHero`, the AI tree, alert levels, the **target query system** (`TargetQuerySystem::ProcessJob`), attack tokens and
slots (how many attackers at once; per enemy class), AI LOD/culling or "sleep until seen/near/streamed" rules (distances, visibility, camera, nav-mesh residency), perception, and every way to set an
actor hostile **now** (`GroupAlert`, `SendEvent`, writing the state) with a verified recipe that works on bosses and on heavies/miniguns. Include why "told about the hero" (our call after 15 s still)
had no effect.

**EA-D2 (P0) Killing and finishing a boss properly.** *Why:* writing health 0 does not kill a boss; the finisher at the story gate leaves bodies frozen in a static pose; Kingpin takes no damage outside his
story phases (#18). *Known:* `BossHealth` and `BotHealth` layouts above, `SetPhase` calls we make, "gate" fractions (1 HP for most, Hammerhead 55 percent). *Find:* what actually kills a boss (the death
message, the function that starts the death state, the animation/ragdoll hand-off), what the phase machine and the "gate" are (which flag blocks damage, who releases it: mission script?), a recipe to run a
boss through all phases standalone and defeat it with a proper death, and a clean "set boss to dead" for a stuck boss.
Answer (2026-11-16 update): EndlessArena/docs/RECON.md section 10 (Health::Kill exe+1f16590 with kForceKill; not run).

**EA-D3 (P1) Heavies and miniguns stand around (#1) and units that never move.** The stationary reasons: no path, no token, calm flag, waiting for an animation or a perch, wrong nav area. A
diagnostic accessor (path status, current goal, current state) for any actor, so our "stationary" rule can read the real cause instead of timing out.

**EA-D4 (P1) Bodies stand back up into a fight pose (#5), static-pose corpses, and cleanup.** The death state machine (ragdoll, "get back up" for stunned/downed units), corpse lifetime, which despawn
path is safe (our `spawn::Despawn`: sync object delete then entity destroy), leaks, and why "enemies killed by other enemies" lose their heads.

**EA-D5 (P0) An undying hunter: The Last Hunt.** *Why:* a curse where one enemy constantly chases the player, even between waves, and **cannot die**. *Find:* the best existing actor and AI for a relentless
stalker (any chase behaviour the game has: the Black Cat chase, Scorpion chase, Shocker chase, Taskmaster hunting, a "hunter" bot?), how to make an actor **unkillable** (invulnerable flag, health floor,
hit-reaction-only) while still being hittable and staggering, how to keep an AI in "chase" mode across our wave transitions (cleanup and spawn passes), how to make it ignore our `cleanup` and bodies logic,
and its animations when "knocked down" (so it gets up). *Verify:* spawn one with the `boss` command and survive 5 minutes.
Answer (2026-11-16 update): EndlessArena/docs/RECON.md section 10 (partial: immortal flag recipe).

**EA-D6 (P2) Elite and variant markers.** How the game gives an enemy "elite" traits (armour, dosed Demons, shields, glowing outlines, resistance flags, speed), so we can apply them to any unit as
**behaviour** instead of health (design rule: no health sponges). A list of per-unit behaviour knobs (attack rate, aggression, accuracy, damage multiplier, move speed, token cost).

**EA-D7 (P2) Roster facts.** For every enemy actor we spawn (`EndlessArena/src/roster_data.inc`): role, weapon(s), projectile and jetpack/head dependencies, base health, attack config, faction, and which
need a head actor loaded (else a headless crash). A machine-readable dependency list for each `.actor` would let preloading be exact instead of guessed.

#### E. Spawning, streaming, capacity, hangs, death and respawn

**EA-E1 (P0) The endless load after death, and the TargetQuerySystem hang.** *Why:* the one hang we saw (#4) and one endless loading screen after a Horde death (2026-11-07) both force a restart. *Find:* the full
**hero death to respawn** sequence: the state machine, who triggers the reload (checkpoint, mission, a `GameModeControl` event), which threads wait on what (main thread parked in a wait with other threads streaming),
what could deadlock with a mod-side asset release or entity cleanup, the load's end event, and whether the load can be **skipped**: see EA-J1 (intercepting death). Also decompile `TargetQuerySystem::ProcessJob`
and say what makes it hang (a dead or dangling target, a despawned entity still referenced).

**EA-E2 (P1) The model-capacity limit (#3).** *Why:* "white cubes, floating heads, invisible enemies" appear when the game runs out of model room; we guess at a unit-type budget (40) and it is wrong or
incomplete. *Find:* the real pools: model/mesh instance pools, streaming memory budget, asset slot tables, actor and sync-object limits (we see `CreateSyncObject slot 28` and a body pool of 150), what
happens on exhaustion (the placeholder cube), and a query "is this asset fully streamed and usable" per asset and per spawned actor (`spawn::Ready`), plus the number free in each pool, so we can budget exactly.
Why does an asset that was released and then requested again come back broken?

**EA-E3 (P1) The spawn path, verified.** The complete correct sequence to spawn any actor: load by hash, dependencies (weapons, projectiles, jetpacks, heads, ammo items), `SpawnRaw`, `CreateSyncObject`,
registration, activation, and the reverse (`DeleteSyncObject`, destroy); what `SpawnEngagedWithHero` really does; what can crash it (we saw a crash at `exe+15f9bc9` for a fresh Olympus spawn); thread rules.
Answer (2026-11-16 update): EndlessArena/docs/RECON.md section 13 (partial: ActorDeleteAction -> exe+1f65260).

**EA-E4 (P2) Safe long teleports and level streaming.** How the game streams the world around the hero after a teleport (we teleport to Ryker's), the camera snap, and what must be waited for before
spawning (nav mesh ready?). Also the hero-actor lifetime: when the `HeroSystem` actor handle changes (respawn), so our cached pointers (manager, health) are never stale (a stale weapon manager crashed us).
Answer (2026-11-15): EndlessArena/docs/RECON.md section 9 (partial: cached hero pointers go stale at respawn).

**EA-E5 (P2) Rooftop spawns and nav-mesh queries.** A snipers-on-roofs test spawned units that fell or were teleported. *Find:* the navigation query API (nearest nav point, is-point-on-navmesh, ground height at x,z,
raycast) and how units placed on roofs path off them; and the designer placements for roof snipers in the mission zones (the prison zone `GP_A2_PRISON_EXT_GAMEPLAY.zone` gave us 139 ground points).

#### F. HUD, choice screens, input on every device, audio

**EA-F1 (P1) A real reward-choice screen.** *Why:* we show 3 options as a notification-box message and read button presses; the user wants a bigger, native screen with item icons and descriptions (#15).
*Find:* which game screens already present a few choices with icons and text (suit-power/mod pickers, the hideout/crime reward, the mission-complete rewards, "Choose your suit", the Taskmaster panel) and
how to open one from code with our own entries (movie, ActionScript calls, data structure), returning the chosen index; also the item-media API (icons, preview video) by asset id. `uilog` data and
`UI_HUNT.md` already list many calls; map them to screens.

**EA-F2 (P1) A persistent panel for the current boons, curses, power and mods (#16).** Which HUD element can show several lines of free text (objective list, notification box, the crime scanner pop-up,
the activity panel), how to keep it on screen, refresh it, and show it on a button press.

**EA-F3 (P0) Input that works on every device, including Steam Deck.** *Why:* a Steam Deck player says our Interact (Y/Triangle at the dock and exit circle) does nothing. *Known:* `input.cpp` (XInput, HID,
`GetAsyncKeyState`, and a `GetForegroundWindow()`-belongs-to-us gate). *Find:* how the game itself reads its devices on the Deck (Windows.Gaming.Input vs XInput vs SDL/Steam Input); whether
`GetForegroundWindow` is trustworthy under Proton and gamescope; and, better, the **game's own action layer** (`InputSystemGame`, action names, `OnButtonPressedAction`, `InteractButtonListener`;
see **FOF-A1, FOF-A2, FOF-A5**) so we can poll or hook "the Interact action" instead of raw buttons, with the right glyph. Give the recipe, and the device-type query for prompts.
Answer (2026-11-15): FriendOrFoe/docs/RECON.md section 36 (leads only, not answered).
Answer (2026-11-16 update): docs/GAME_INTERNALS.md 'Input' and EndlessArena/docs/RECON.md section 12 (poll kInteract through the game's controller).

**EA-F4 (P2) Results and score panels.** The Retry prompt that always sits next to Continue (can it be hidden, or relabelled), the Horde tier label "500" that we cannot explain, how a panel ticks a row
that reached its max, and how to keep a panel open across a respawn.

**EA-F5 (P2) Map icons and waypoints for the docks.** How `MapElement` entries get registered with the map (spawning a fast-travel actor did not create an icon), and the custom-waypoint action
(`UIUpdateCustomWaypointAction`), so the two docks can appear on the map like game activities.

**EA-F6 (P2) Pause and resume a run (#12) and pause menu hooks.** Can the whole game state (AI, spawns, scripts, audio) be frozen cleanly for a pause screen of our own, and what does the game's own
pause freeze?

**EA-F7 (P1) Music, per wave and per boss, with names.** *Why:* we step a tier ladder through state ids with no names; bosses have their own tracks (`I20_MUS_VN_Tombstone_Fight` and others). *Find:* the
Wwise state/switch/event names behind our ids (hash dictionary from the soundbanks: dump every `I20_MUS_*` event, state group, state and switch with its 32-bit id), what the game sets and when for hideouts,
boss fights (each villain), victory and defeat, and the stop/cool-down rules; the master volume, ducking and bus controls. A table "role -> event/state id" would let us pick a boss track per
boss and a Horde ladder cleanly.

**EA-F8 (P3) Localized text.** How to add or override strings in the localization table at runtime (the HUD takes plain text now), and the fonts for symbols.

#### G. Arena and world

**EA-G1 (P1) Other arenas.** *Why:* Ryker's is the only arena; the design wants more (hideouts, rooftops, interiors). *Find:* a generic recipe for any location: bounds and walls from the level/zone data, ground and
navmesh, designer spawn points from `.zone` gameplay files (what we did for Ryker's), which interiors can be loaded additively or streamed in the open world, the safe teleport, and **how the real prison
mission level could be loaded** (mission-flow data edit vs runtime checkpoint call; `GP_A2_PRISON`), including save-state coupling.
Answer (2026-11-15): docs/MISC_RECON.md section 2 (the level/checkpoint loader; partial).

**EA-G2 (P1) The real camera (#21).** We guess a camera cone to keep spawns out of view. *Find:* the camera object, position, forward vector and FOV getters (and the photo-mode camera), and whether a
simple "is this point on screen/occluded" query exists.
Answer (2026-11-16 update): docs/MISC_RECON.md section 10 (camera = view 0 record, forward +0x20, position +0x30).

**EA-G3 (P2) The mission/crime/combat states that block us.** The flags that say "a mission is active", "the player is in combat", "a crime is running", "hideout active" (we have `MissionBlocks`);
a reliable set and where they live, so we never start a run over a story beat.
Answer (2026-11-15): docs/MISC_RECON.md section 6 (mission state queries).

#### H. Loadout, progression, perks, saves

**EA-H1 (P0) Autosave and checkpoints (#11).** *Why:* a run must never overwrite the player's save with the stripped loadout. *Find:* every autosave and checkpoint trigger (Ryker's mission zones, level
transitions, deaths), the save-write function, a clean "suppress saving" switch, and what a crash mid-run leaves on disk (our loadout recovery relies on a snapshot at next launch). Also confirm where the
suit power, mods and gadgets are persisted.
Answer (2026-11-16 update): EndlessArena/docs/RECON.md section 11 (checkpoint save writer and the game's AutoSaveOn/Off switch; not run).

**EA-H2 (P1) Grant items the player has not unlocked (#13).** What unlock/owned flags exist in the inventory (`inventory::Info`: found, owned, locked, category), a **temporary** grant that can be removed at the
run's end without trace, and the effect on XP, skill points, tokens and the profile (we do not want permanent progression changes).

**EA-H3 (P1) Custom perks (#14).** How a `PerkItemConfig` defines its effect (the effect list and parameter types: health, damage in/out with filters by damage or weapon type, status durations, focus
segments/drain/fill, gadget ammo/resupply/recharge, velocity damage, vengeance, low gravity, holo decoy, Spider-Bro, storm bolt, electric blast, invisibility, Noir, loot rate), and whether we can **create a
perk config in memory** (asset-swap/injection path) with chosen numbers, so new perks use the game's own effect system and show in the suit menu. A worked example with a "+10 percent damage" perk.
Answer (2026-11-15): partial: perk data types are in recon/ddl_types_4.0630.txt (fx.py ddl "Perk"); creating one in memory not examined.

**EA-H4 (P2) Perk and mod activation rules in the arena.** Which perks need a gadget, a stealth state, a scanner or XP (we cut seven as useless) and a table of each perk's trigger and effect from the
configs (names `perk_ids.txt`), so the pool filter is grounded in data and not in menu text.

#### I. Difficulty levers that are not health

**EA-I1 (P1) What can make crowds meaner without sponges?** Enemy damage multiplier, attack rate/cooldowns, accuracy and fire rate of ranged units, aggression and token counts (how many may attack at once),
move speed, aim assist-style tracking, perception range: where each lives (per `.actor`, per faction, per difficulty config such as the "Harder Game Difficulty" mods edit, or runtime) and how to change it at
runtime for one actor or all.

**EA-I2 (P2) Enemy damage numbers.** The damage each enemy type deals to the hero (per weapon and melee), so we can balance from the game's own data, and how armour/blocks/parry windows are tuned.

#### J. Resilience and tooling

**EA-J1 (P1) Intercept the hero's death.** *Why:* a death triggers a reload with its load time and its hang risk; the run is over anyway. *Find:* the function that kills the hero and the state machine of the
death flow; whether we can hold the hero at 1 HP "down" (knocked out animation), show our results panel and teleport out, **without** the reload, then restore normal state. Also the "you died" UI and fade.
Answer (2026-11-15): docs/MISC_RECON.md section 7 (death -> HeroRespawn chain and interception points; not run).

**EA-J2 (P1) Faulting-context recovery from minidumps.** Our dumps show the game's crash handler context (ntdll, same registers every time) not the faulting frame. *Find:* where the original exception
`CONTEXT` is on the stack/in the dump and extend `tools/sigtools/dumpstack.py` to print the faulting RIP, registers and the real call chain. Every agent benefits.
Answer (2026-11-15): EndlessArena/docs/RECON.md section 9 (tools/sigtools/dumpctx.py, dumpindex.py).

**EA-J3 (P1) Update resilience (#19).** For each game function and static the mods use (our list is `EndlessArena/src/natives.cpp`; the other mods have theirs), produce signatures that are unique in `.text`
and survive small code changes, and a script that reports which natives moved after a game update.

**EA-J4 (P3) A function-name map.** Anything you can learn about names: the exe has RTTI and `funcstrings`; a growing `exe+RVA -> name` file for the systems above (inventory, hero state, AI, damage, UI,
audio) would help every future session.
Answer (2026-11-15): docs/GAME_INTERNALS.md, 'Fable session 2026-11-15' (generated tables in recon/; tools/sigtools/fx.py).

### Priority summary (what unblocks what)

- **The three curses:** EA-B1, EA-B2, EA-B3 (web-fluid); EA-C1, EA-C2 (Street Brawler); EA-D5, EA-D2 (The Last Hunt); EA-C5 for safe drain.
- **Stop the crashes and hangs:** EA-A1, EA-A2, EA-E1, EA-J1, EA-J2, EA-E3.
- **Make gadgets feel right (not red, with ammo):** EA-A3, EA-A5, EA-A2, EA-A6, then EA-A4.
- **Bosses and enemies that fight:** EA-D1, EA-D2, EA-D3, EA-D4, EA-D6, EA-I1.
- **Input everywhere (Steam Deck):** EA-F3 first (shared with FOF-A1/A2/A5).
- **A better reward screen and panel:** EA-F1, EA-F2.
- **Music and arenas:** EA-F7, EA-G1, EA-G2, EA-G3.
- **Saves and progression:** EA-H1, EA-H2, EA-H3, EA-H4.
- **Cheap, high value, do early:** EA-J2 (tooling for everyone), EA-B1 (dump the name table), EA-F7 (dump the audio names), EA-A1 (read the crash tree).

### Where to put the answers

Number them in a **new file `EndlessArena/docs/RECON.md`** (the repository is public; keep it to RVAs, offsets and findings, no extracted game bytes), add a one-line pointer under each request above
(`Answer: EndlessArena/docs/RECON.md section <n>`), and mention anything the **other mods** can use in `docs/GAME_INTERNALS.md`. Findings that change the plan: update `EndlessArena/docs/REWARD_DESIGN.md`
(curses and gadgets) and `EndlessArena/docs/LEDGER.md` (bugs).



---

## Miscellaneous (new-mod ideas: Villain Chases as crimes, Race Editor, Combat Tour Editor, and the shared engine facts they need)

### Goals

The "Miscellaneous" agent owns three mod ideas that are **not started yet** (opened 2026-11-14). All three would be native `.script` DLLs on the `common/` layer and `ModMenu/`, no Script Hook, playable after the story, reusing the
game's own content and UI wherever possible. In the maintainer's words, with my reading of each:

1. **Villain Chases (working name): chases as post-game crimes.** Bring back the chase set-pieces the story had and offer them as repeatable crimes after the credits. The maintainer remembers **Shocker** (and maybe **Vulture**)
   in the main game, **Black Cat** in *The Heist*, **Screwball** in *Silver Lining*. A chase should start from the crime system (or an equivalent trigger), run (target flees, hero follows, catch/lose rules, HUD, music) and end
   cleanly (success/fail, reward, no story-flag or save damage), and be replayable. The maintainer's memory may be incomplete: MSC-B1 asks for the real inventory.
2. **Race Editor.** An in-game editor to build a **race course** (start, ordered gates/checkpoints, finish, rules, par times), save it as a small shareable file, load other people's, and race it (countdown, timer, best time, restart,
   maybe a ghost or AI rivals), in the game's own look (rings, beams, markers, HUD). Must work on every input device (the maintainer also plays on a Steam Deck).
3. **Combat Tour Editor.** An in-game editor to build a **combat tour**: an ordered list of stops around the city, each with an encounter (enemy mix, waves, rules/bonus objectives) and a score, with a route between stops, savable
   and shareable like races. Endless Arena already spawns and fights enemies in the arena; the new part is **doing it anywhere in the open city**, with civilians, police and streaming around.

What the three share, and where I most need you: **(a)** how the game runs scripted activities (mission graphs, objectives, HUD, rewards, completion) and how a mod starts one on demand or builds its own; **(b)** the crime director;
**(c)** a world cursor and free camera for placing things; **(d)** world markers and 3D labels; **(e)** reading what the hero is doing (swing, zip, run, fight) and the world under it (ground, roof, navmesh, district);
**(f)** safe persistence (our own file, never the save).

**Be liberal.** Anything you learn about these topics is wanted even if no request names it. A short "what exists, where, how it is wired" map beats a perfect answer to one narrow question.
**If time is short, my top three are MSC-A1 (crime director), MSC-B1 (chase inventory and anatomy), MSC-C1 (mission/nodegraph runtime and starting one on demand).**

### Current state (2026-11-14)

Nothing for these three mods is built or probed. What the repository already says (read from this file, `docs/UI_HUNT.md`, `docs/GAME_INTERNALS.md`; **none of it was checked for chases, races or tours specifically**):

- **Crime UI is known:** `ShowCrimeScanner(title, subtitle, 2, 5, array)` on the HUD movie, then `ActivateProgressBar`, `UpdateProgressBar`, `OnTargeted`; end with `DeactivateProgressBar`, `OnUnTargeted`, `HideActivity` (the "failed"
  screen) (`UI_HUNT.md`). Example seen: title `"Maggia Pursuit: Sycamore "`, subtitle `"Chase the vehicle!"`, so **a vehicle-pursuit crime exists in the base game**. `CrimeType` events and "the crime director" are named in
  Manhattan Streets (MST-B7) and are **not yet documented**.
- **Cut co-op content that looks like chases, races, crimes and patrols** (Friend or Foe F1): checkpoints `CHK_COOP_CHASE_DEMO`, `CHK_COOP_CRIME_DEMO`, `CHK_COOP_RACE_*`, `CHK_COOP_PATROL_*`, `CHK_T37_COOP_PATROL`, and
  `enm_coopchase_enemy_movement`. These may be prototypes of exactly what I want.
- **Chase-related names seen:** actor `enm_blackcat_chase.actor` (`BlackCatBot`, health 100, `BlackCatChaseController`, `LookAtHero`); bot commands `BotCommandChaseRunTo` (moves without animation, never finishes), `BotCommandChaseJumpTo`,
  `BotCommandFollowPathScriptAction`, `BotCommandTrafficPath`; Debug Menu ids `V10_SHOCKER_CHASE` and `V10_SHOCKER_BANK` (prefix meaning is a guess from `CLAUDE.md`).
- **Challenges:** `SparkChallengeManager` (Screwball), `ShowGoldenSparkBonus`, `ChallengeOverlayData` (Taskmaster); map icons come from a `MapElement` component whose Type picks the icon (`kChallengeCombat`, `kBaseFisk`,
  `kCustomWaypoint`). `UI_HUNT.md` plan (b) is "run a copy of a challenge template node at the dock", which is the same idea as a tour stop.
- **Map/waypoints:** `UIMapPOIAction` (exe+7f0e20) and `UIUpdateCustomWaypointAction` (exe+7efd10) are graph nodes that only pass pins on.
- **Solved elsewhere:** spawning (Endless Arena `spawn.cpp`; standalone bosses verified: Taskmaster, Sable, Tombstone, Mister Negative, Hammerhead), hero transform, `FollowCamera` actor, `SendEvent`, time-scale system
  (`exe+19bb430`), game-thread hook `exe+1f745f0`, `loadlog`, `uilog`, the command-file protocol.
- **Mission system:** singleton `exe+7c4bbb0` (ids are Insomniac CRC32); events `MissionStarted 0x6E588F6D`, `ObjectiveStarted 0xD358823E`, `MissionComplete 0xCC580B11`, `CheckpointLoaded 0x414AD37A`. `.nodegraph` assets drive
  mission flow; ALERT's viewer lacks subgraph support. We have **no checkpoint/mission loader** (FOF-F2 asks for one).

**Not known (the heart of my requests):** how the crime director chooses and starts crimes; which chases exist and how each is built; how a mission/objective is started and ended outside the story; the HUD calls for chase,
race and tour screens; the physics/world-query calls (raycast, ground, navmesh); what the engine offers for debug drawing and world markers; what the hero state machine exposes; how to snapshot a save safely.

### Conventions

- Tags `[V]` verified, `[I]` inference, `[?]` unknown. Give RVAs, struct offsets, a signature (one hit in `.text`) and how you checked.
- Answers go in **`docs/MISC_RECON.md`** (new file, public repo: RVAs, offsets, findings, **no extracted game bytes**). Add one line under each request here: `Answer: docs/MISC_RECON.md section <n>`.
- Where a request overlaps another agent's, answer once and cross-reference (overlaps are noted).

### Requests

Priority: **P0** blocks the first milestone of a mod, **P1** big quality win, **P2** needed for later milestones, **P3** long shot. **Do not limit yourself to the wording.**

#### A. The crime director (the trigger for Villain Chases; also useful to Streets and Skies)

**MSC-A1 (P0) Anatomy of the crime director.** *Why:* the cleanest way to make chases "post-game crimes" is to be one more crime type the director offers, or to ride on an existing one. *Find:* every `Crime*` RTTI class and config type
(director, spawner, district tables, lifetimes, cooldowns, radii, time-of-day weights); the object that runs the director (static address, tick function, state machine); how a crime is **chosen** (inputs: district, act, hero
position, active missions, time since the last crime, crimes already active), **placed** (locators/spawn points: in zone data? configs?), **started** (the call that makes a crime exist, with map icon and HUD), **updated**,
**completed/failed/abandoned** (leash distance, timeouts, hero death) and **rewarded** (tokens, XP, district crime-rate counters). Give a table of **all crime types** (names, ids, act or DLC, which are removed after the credits and
why: a flag, a config, a mission condition) and how the director behaves in a **post-credits save**. *Verify:* hook start/end, log while the maintainer walks around (`crimelog`).
Answer (2026-11-15): docs/MISC_RECON.md section 1 (partial: object, data model, 67 types; the tick not read).

**MSC-A2 (P0) Force-start a crime on demand.** The exact function(s) and arguments to start crime type X at position P as if the director had chosen it (including the Maggia Pursuit vehicle chase), a clean **end/cancel** (no leftover
actors, no stuck HUD, no stuck music, no failure penalty) and what state it leaves. Can two crimes run at once? What happens if the hero is in a mission, an interior or on a roof? What world conditions are needed (streamed tiles,
pedestrian and vehicle systems)?
Answer (2026-11-15): docs/MISC_RECON.md section 1 (recipe from the game's own calls; not run).

**MSC-A3 (P1) Crime data format and extensibility.** Which assets define a crime (configs, nodegraphs, zones, actors, level overlay entries); whether a **new crime type** can be added by **data only** (Overstrike `.modular` or
`.stage`) or needs code; where the director's registry of types lives (so a script DLL could add one in memory). A reverse index: crime type -> assets it loads.
Answer (2026-11-15): docs/MISC_RECON.md section 1.

**MSC-A4 (P1) The pursuit crime in detail (Maggia Pursuit).** The vehicle chase is the nearest thing to a chase in the base game and a template for mine: the target vehicle's actor, route (traffic path, spline, navmesh?), speed and
rubber-banding rules, catch/lose conditions (what does the progress bar mean: distance? health?), how the hero "catches" (web the car? takedown? stop it?), reward and end flow. Even if it is vehicle-only, the architecture
(controller component, state machine, events) is what a foot or rooftop chase would reuse.

**MSC-A5 (P2) Crime presentation.** The tiered UI flow (detect, show, track, resolve), audio (dispatch lines, stingers), minimap and world icon, and how a mod shows a **custom** crime entry with its own title, subtitle and progress.
(Overlaps EA-F2, MSK-C1.)

**MSC-A6 (P2) Crime director knobs.** Frequency, max concurrent, per-district weights, crime-rate meters: runtime changes so chases appear more or less often, or "only these types".
Answer (2026-11-15): docs/MISC_RECON.md section 1.

#### B. The villain chases themselves

**MSC-B1 (P0) Inventory and anatomy of every chase and chase-like segment, DLCs included.** *Why:* I remember three and may be wrong or missing some. *Find:* search nodegraphs, zones, actors, configs, checkpoint names, Wwise events and
strings for chase, pursuit, flee, escape, tail, follow, getaway, rooftop, vehicle chase, and **report a table**: chase name, context (main game, Heist, Turf Wars, Silver Lining), mission/checkpoint id (`GP_A*`, `V10_SHOCKER_*`, `VN_*`,
`CTNS_*`), target actor and controller (e.g. `BlackCatChaseController`), movement mode (foot parkour, rooftops, vehicle, swing, flying), **where the route data lives** (asset and format), scripted beats (obstacles, QTEs, cutscene
hand-offs, dialogue triggers), player-side rules (gadgets disabled? time limit?), HUD (gauges, markers), win/lose conditions, music and VO, tiles it needs loaded, checkpoints. Cover **Vulture** (is there a chase at all?), **Shocker**
(`V10_SHOCKER_CHASE`: vehicle or foot? and `_BANK`), **Black Cat** (Heist), **Screwball** (Silver Lining), and anything else (Electro, Scorpion, Rhino, Mister Negative, Taskmaster, Doc Ock, the Demon prisoners). Mark which are
**reusable standalone** (target spawns, runs, can be caught, in free roam).

**MSC-B2 (P0) The chase target controller.** The components and state machines that make the target flee along a route and react to the hero (speed matching, waiting for the player, catch-up and slow-down rubber banding, jumps,
climbs, doors and rails, traffic): `BlackCatChaseController`, `enm_coopchase_enemy_movement`, `BotCommandChaseRunTo/JumpTo`, `BotCommandFollowPath`, and the Shocker/Screwball equivalents. Fields, tuning configs (defaults, runtime
offsets) and **the route representation** (spline? locators annotated with jump/climb? navmesh goals chosen at runtime?). *The practical question:* **can a target be given an arbitrary, mod-generated route in the open city
(ground and rooftop)**, or does it need pre-authored traversal data (jump links)? Cheapest way to make it run a long route without falling off, getting stuck or losing animation. (General bot movement is FOF-D1/D2/D3: do not duplicate,
extend with the chase-specific parts.)
Answer (2026-11-15): docs/MISC_RECON.md section 3 (the chase system's types and nodes).

**MSC-B3 (P1) Standalone spawnability of chase targets.** For each target actor from B1: does it spawn and behave standalone? Dependency chain (`tools/assets/deps.py`), attacks while fleeing (does Shocker shoot back? does Black Cat
throw?), invulnerability and health (meant to be fought or just caught?), the **ending** of each chase (web-pull, zip, QTE, a boss fight) and how to trigger the ending or a fight phase on demand.

**MSC-B4 (P1) The chase HUD and feedback.** Distance or escape meter widgets (movie, calls, argument shapes; record with `uilog`), objective marker on the target with distance read-out, "target lost" messages, camera behavior,
music and stingers (how the music state is driven; shared with EA-F7), VO barks during chases. A **call table** (name, owner, args) for every UI call used by every chase in B1.
Answer (2026-11-15): docs/MISC_RECON.md section 3 (partial).

**MSC-B5 (P1) Starting a chase in free roam.** Which parts of each chase mission depend on story state (act, previous missions, zone overlays, NPC placement) and the minimum world setup to run it after the credits; how story
hides and unhides the tiles; checkpoint ids and their loaders (FOF-F2). *Alternative:* if running the story mission is impractical, list the **actors, configs and graph nodes** needed to rebuild a chase in our own code (target, route,
meter, end) with the least data.

**MSC-B6 (P2) Rewards, repeatability, accounting.** Which "mission complete" side effects must be avoided (unlocks, story flags, trophies, token grants)? How can a mod pay a reward and record "completed" without touching story state?
(See MSC-C3, MSC-F1.)

**MSC-B7 (P2) Cut or alternate chase content.** `CHK_COOP_CHASE_DEMO`, `enm_coopchase_enemy_movement`, `coop_bc_*`: what they are, whether they work in free roam, whether **one can be a generic chase template** for any target.
Same for `CHK_COOP_CRIME_DEMO` and any `*_DEMO` or `*_TEST` chase checkpoints.
Answer (2026-11-15): docs/MISC_RECON.md section 3 (partial).

**MSC-B8 (P3) Variant targets.** Can a different enemy (a thug, Taskmaster, Mister Negative) be made the chase target by swapping the controller and movement config? What differs per enemy class (animations, parkour)?

#### C. Mission, nodegraph and objective runtime (shared by all three mods)

**MSC-C1 (P0) The mission/nodegraph runtime and starting one on demand.** *Why:* chases are mission graphs; races and tours are tiny missions. *Find:* (1) the **interpreter** that executes a `.nodegraph` (class, node base class and vtable,
node-type registry -> execute function RVAs, pin and value types, subgraphs, waits and triggers, update cadence); (2) a **catalog of node types** (name, pins, meaning), at least for objectives, HUD calls, timers, triggers and volumes,
spawn/despawn, waits, branches, music, VO, checkpoints, rewards, mission end; (3) the call that **starts a mission or graph by id** with its preconditions, and the call to **end or abort** it cleanly; (4) whether a mod can **build a graph in
memory** or run a **shipped** `.nodegraph` (an Overstrike-added asset) and how a by-hash asset is found by the runtime; (5) a **graph-to-text dumper** in `tools/` with subgraph support. *Verify:* dump a short mission (a chase) and
match it to the HUD calls recorded by `uilog`. (Overlaps FOF-F2, EA-G3, MSK-C1: answer once.)
Answer (2026-11-15): docs/MISC_RECON.md sections 2 and 5 (partial: loader, node runtime, catalogue of 1,032 nodes; no graph dumper).

**MSC-C2 (P0) Objective and activity HUD API.** The calls that show and update: objective text, objective marker (world beacon with distance), counters ("3/8"), timers (countdown and stopwatch), progress bar, score, start banner,
result and "mission complete" screen, failed/retry screen (the Taskmaster panel has `ShowBtnHint` Continue/Retry), and the **Screwball/Spark challenge** UI (`SparkChallengeManager`): which clip, which calls, argument shapes,
**how strings are localized** (can a mod show its own text, can it register new strings?), and how each element is hidden. A **call table** per widget with examples. Single most valuable doc for the Race and Tour editors.

**MSC-C3 (P1) Rewards and progression bookkeeping.** How the game grants tokens, XP and currencies, "activity completed" counters, district clear stats, trophies and challenge state (`LoadoutRewardConfig` etc.); the call to grant X
without a mission context; whether that state lands in the main save; a safe way to detect first-time versus repeat completion.

**MSC-C4 (P1) Triggers and volumes.** How the game detects "hero entered a volume" or "passed a gate": trigger component classes (box, sphere, capsule, ring), fields, the events they raise, update cadence, and how to **spawn a trigger at
runtime** (an actor with a trigger component). Our own overlap test is fine, but the native one gives events and debug drawing for free. Also a "crossed a plane at speed" test for race gates.

**MSC-C5 (P2) Zones and streaming for far-away content.** What a chase, race or tour needs loaded far from the hero; what happens when a route leaves the loaded area (does the target vanish? freeze?); how story overlays
(`CustomOverlaySystem exe+62a4e60`, see Streets) affect crimes and chases after the credits.

**MSC-C6 (P2) State predicates in one place.** "A mission is active", "cutscene", "hero controllable", "loading", "in combat", "interior", "in a crime or hideout", "photo mode", read each frame (shared with MSK-C1, EA-G3, FOF-G2).
Answer (2026-11-15): docs/MISC_RECON.md section 6.

#### D. World cursor, placement and the editors' tools

**MSC-D1 (P0 for both editors) World raycast and ground query.** The physics calls (Havok `hknp` world `[exe+78939e8]`, `TerrainPhysics`): **ray cast** from the camera through the screen center (or any ray) returning position, normal,
**surface type or material**, the hit **entity or collider**, **filter masks** (static world only, no characters or props); **ground height** at (x, z); sphere and capsule overlap; "inside geometry, interior or water" tests. Which thread and
phase to call from, locks. *Verify:* a `ray` command that prints what the crosshair hits.
Answer (2026-11-16 update): docs/MISC_RECON.md section 10 (camera raycast recipe with collision profiles; not run).

**MSC-D2 (P0) Free camera and a placement cursor.** *Known:* `FollowCamera` actor, `HeroCameraManager` (vtable exe+38b1dd0), photo mode exists. *Find:* how **photo mode** works (free camera object, input, bounds, HUD hiding, **world pause**);
whether a mod can enter and leave a free-cam state, set camera position, rotation and FOV directly, take over for a fly-through and **give the camera back**; how to **freeze hero and world** (time-scale channel 0) while keeping a cursor
alive; which input the free cam reads. A top-down "map editor" camera if cheap. (Overlaps MSK-G8, FOF-A2.)
Answer (2026-11-16 update): docs/MISC_RECON.md section 12 (partial: photo mode classes and switch).

**MSC-D3 (P0) World markers: props, beams, rings, labels.** *Why:* gates, start/finish, tour stops, previews. *Find, in order of preference:* (1) **existing assets** that look right (rings, checkpoint beams, holographic gates, Screwball
spark pickups, Taskmaster challenge markers, tutorial swing rings, objective beacons, Fisk-base capture markers, golden sparks): paths, dependency lists, spawn recipe, scale/colour/animation parameters, how to switch them on,
off or "collected"; (2) a **VFX spawn API** (name -> effect asset, attach to a transform, lifetime); (3) **debug draw** (lines, spheres, text: compiled into release? how to enable; signature; per-frame or persistent);
(4) **world-to-screen projection** and **screen-text drawing** for our own labels (font, size, outline); (5) 3D text in the world. Also the **objective marker widget** pointed at an arbitrary position, and the **map POI route** for a
**custom icon and label** (`MapElement` types, kCustomWaypoint, `UIUpdateCustomWaypointAction`, the "set waypoint" button's call; shared with UI_HUNT plan (a) and EA dock icons).
Answer (2026-11-16 update): docs/MISC_RECON.md section 11 (partial: debug draw compiled out; draw-text target node).

**MSC-D4 (P1) Props and static things.** Spawn a simple mesh or prop by hash; a **catalog of cheap visible markers** (cubes, rings, cones, arrows, signs, holo blocks), their collision settings (non-colliding ghosts for race gates),
how to delete them cleanly. (Lifecycle overlaps FOF-C1/C2.)

**MSC-D5 (P1) Placement validity: navmesh, roofs, districts, interiors.** Nearest navmesh point, "is point on navmesh", path exists and length; the district at a position (names, ids, localization: `playerDistrict` and `districtData` are
seen on the map); roof detection (is this surface a roof, height above street); **interior versus open world** and the interiors list; water or void check. A `place` command that reports all of it at the cursor.
(Overlaps FOF-D1, EA-D3.)

**MSC-D6 (P1) Editor input without breaking the game.** Which actions an editor mode can **swallow** (so the hero does not swing while the player moves a cursor), device-agnostic reads of buttons, sticks, triggers and mouse with the right
glyphs (FOF-A1..A5 are the shared answers; add the Steam Deck cases), and a safe way to suspend and restore hero control.
Answer (2026-11-16 update): docs/GAME_INTERNALS.md 'Input: the game's action layer'.

**MSC-D7 (P2) Text entry.** Does the game have any text input (naming a save, photo-mode captions, Steam Deck on-screen keyboard)? If not, which route is least bad: ModMenu presets, a Windows or Steam virtual-keyboard overlay, a
gamepad letter-picker on HUD clips? Needed for course and tour names and share codes.

**MSC-D8 (P2) Selection highlight.** Is there a per-actor outline or reticle (`AllyXRayManager`, `PlayerHudTargetLock`) a mod can drive to highlight an editor selection in the world?

#### E. Race specifics

**MSC-E1 (P0) Hero movement state and speed.** The hero state machine's **current state id and name** (swing, web zip, point launch, zip-to-point, wall run, run, sprint, air dive, ledge, combat, takedown, interact), where it is stored
(`Hero::HeroLocalPrius`, `HeroState*`, `HeroMoverManager`), the **velocity vector**, grounded flag, swing anchor, swing and zip counters, airborne time, wall-run flag: readable every frame. For race rules: how to **disable** zip-to-point,
point launch, fast travel, gadgets and takedowns (whole traversal categories) for the duration of a race, and **speed modifiers** (swing speed, sprint) so a race class (swing-only, no-zip) can be defined. (Shared with EA-A*.)
Answer (2026-11-15): docs/MISC_RECON.md section 6 (partial).

**MSC-E2 (P1) Teleport, restart, streaming.** A safe "place hero at X facing Y" (zero velocity, reset swing state and camera, no fall-through while tiles stream) and a way to **pre-stream** the start area so a restart or a distant start is
instant. Do tiles ahead load in time at swing speed? How pedestrian and vehicle density behave at speed. (Shared with FOF-C3, FOF-G3.)

**MSC-E3 (P1) Countdown, timer, results.** The C2 calls specialised for races: "3-2-1-GO", stopwatch and par-time bar, checkpoint counter, splits, a **results screen** that can show times (can the Taskmaster or Screwball results panel
show times and medals? how are its fields set?). **Audio:** the Wwise event names for countdown beeps, checkpoint chimes and finish stingers (`audio post <event>` exists).

**MSC-E4 (P1) Ghost and rival racers.** (1) How to **record the hero's pose** per frame compactly (transform plus animation state and phase from `AnimControllerComponent`) and **play it back on a spawned actor** (a second Spider-Man,
`npc_imposter_spiderman*`, a hero-model actor) with a ghost material and **no collision**, and the costs; (2) whether a **bot can swing** (any AI that web-swings: imposter Spider-Man content, Black Cat in the Heist, Screwball, Taskmaster?)
and how a bot is told to follow a path with swing animations; (3) fallback: a **kinematic follower** that plays a looping run or swing animation along a spline (`BotCommandFollowPath`): fields, whether it works on any actor, how to hide
foot sliding.

**MSC-E5 (P2) Run validity.** What detects teleport, fast travel, loading screens or time-scale changes during a course (events such as `HeroWarp*Event`, time-scale channel changes; shared with FOF-G1/G4) so a run can be voided; which clock
to time with (`GameTime` versus real time; slow-mo from focus and finishers).

**MSC-E6 (P2) Existing race-like content.** What is in the cut `CHK_COOP_RACE_*` checkpoints (gates? timers? scripts?), and any parkour, swing-through-rings, "reach it in time" or drone-chase activities in the base game or DLCs: node
templates and assets I can reuse for **gates, timers and checkpoints**.
Answer (2026-11-15): docs/MISC_RECON.md section 4.

**MSC-E7 (P3) Guidance ribbon.** Can a mod draw a **guide line or ribbon** along a list of points (the objective trail the game shows toward a story marker)? Useful for races, chases and tours.

#### F. Persistence and sharing

**MSC-F1 (P1) Save-file internals and safety.** Location, format (container, compression, checksums or signature, versioning), slots and autosaves (`Documents\Marvel's Spider-Man Remastered\`, Steam Cloud), behavior with unknown
extra data, how a mod **detects a save or load** (events: `CheckpointLoaded`, save events) so our own file (courses, tours, best times, ghosts) can be written at the same moment. Also a **save snapshot and restore script** in `tools/` so
the maintainer can test crimes and chases safely.

**MSC-F2 (P2) Stable coordinates.** The world origin and tile coordinates; whether coordinates differ between game versions or DLC (so a shared course survives a patch); whether streaming moves the origin; converting a position to a
district or landmark name and to map UV (for a mini preview of a course in our own UI).

**MSC-F3 (P3) Localization of mod text.** How the string tables are keyed (CRC32?), whether a mod can **add** strings without collisions, how `GetLocalizedText` resolves.

#### G. Combat tour specifics

**MSC-G1 (P0) The combat challenge machinery** (Taskmaster combat challenges, hideouts, Sable outposts, Demon warehouse fights). *Why:* a tour stop is a combat challenge. *Find:* the **data format and runtime** of one: actor, config and
zone assets, wave definition (enemy lists, counts, delays, spawn points, reinforcements), **bonus objectives** (their definitions and how they are tracked), scoring and medals, the arena leash and alarm, the HUD (wave counter,
remaining enemies, bonus list: C2), the **start trigger**, the end flow (results, rewards, restoring ambient). **Can I instantiate one at an arbitrary place, with my own enemy list?** (UI_HUNT plan (b).) If not, the minimal set of actors
and components to emulate it.

**MSC-G2 (P0) Performance and bonus-objective tracking.** Where the game counts per-fight stats: takedown types, combo length and style, perfect dodges, parries, environmental, gadget and web kills, stealth takedowns, time to finish,
damage taken, "untouched". Events (hashes, payloads) or hookable counters, so a tour can score using the game's own numbers. (Overlaps EA scoring; answer with offsets.)
Answer (2026-11-15): docs/MISC_RECON.md section 8 (partial: events).

**MSC-G3 (P1) Combat in the open city.** What the streets do while I fight: do police arrive, do pedestrians panic, do civilians block navigation, do crimes trigger, do enemies leave their leash and chase over roofs; combat music state,
**alert or heat** levels (`OmniAwareness`?), outdoor aggro radius, vehicles and traffic; how to **suppress** pedestrians and traffic around a fight (a "combat bubble": density down within radius R) and **restore** them. (Shared with
Streets crowd/traffic, EA-I1.)

**MSC-G4 (P1) Enemy catalog for street fights.** A table: faction -> enemy class (melee, ranged, heavy, shielded, jetpack, sniper, elite, drone, tech, cloaked) -> actor path -> **dependency chain** -> per-act variants (Fisk, Demons, Maggia,
Sable, Flame, Negative-corrupted, Roxxon, Oscorp) -> spawn flags and engaged or idle behavior -> **does it fight standalone at street level?** (some are mission-only and need a script to start AI). All tiers per faction so a tour can ramp up.
(Shared with EA-D*.)

**MSC-G5 (P1) Stealth encounters (optional stop type).** Patrol path data (`BotCommandTrafficPath`, `FollowPath` locators), awareness cones (`BotConfigAwareness`), alert states; how to **spawn a patrol with a path** and detect
"spotted" and "cleared silently".

**MSC-G6 (P2) Travel legs and boundaries.** Guidance between stops (waypoint, objective marker, map pin, route line), fast travel (disable during tours, or use between stops), time limits, and a **soft boundary** that stops the hero
leaving the arena (UI warning, fail, teleport back).

**MSC-G7 (P2) Environmental hazards and props.** Spawnable explosive barrels, gas canisters, electrified objects, destructible cover, throwables: names, spawn recipe, and whether the **takedown system recognizes them** when spawned by a mod.

#### H. Cross-cutting engine facts

**MSC-H1 (P1) The activity framework.** One doc listing **every kind of activity** (crimes, hideouts and bases, Taskmaster/Screwball/Spark challenges, tails, research stations, backpacks, landmarks, photo ops, pigeons, Sable outposts,
Demon and Negative content, drones and bots) with: where each is **registered** (manager class, table), start/update/end calls, state flags, HUD, **map icon** type, completion persistence. A menu of "native building blocks".
Answer (2026-11-15): docs/MISC_RECON.md section 5 (partial: recon/script_nodes_4.0630.txt).

**MSC-H2 (P1) Events to subscribe to.** Hero death and respawn, mission and crime start/end, checkpoint and save, cutscene, fast travel, loading, pause, photo mode, combat start/end, takedown, gadget use, swing and zip start/end, district
change, tile load and unload. Hashes and payloads (shared with FOF-G1: one table).
Answer (2026-11-15): docs/MISC_RECON.md section 8.

**MSC-H3 (P1) A reusable "attempt" lifecycle.** *Enter an activity (snapshot every mutable knob: time scale, gadget locks, ambient density, HUD, music) -> run -> leave and restore everything exactly, even if the hero dies, a loading
screen intervenes or the player pauses.* The list of knobs, where each is stored, and idempotent restore values.

**MSC-H4 (P1) Hero death and respawn in the open city.** What the game does on death at street level (checkpoint reload? nearest safe point? penalty?), what we can intercept, to **abort a race, tour or chase** cleanly. (EA-J1 covers the
arena case; I need the open-world one.)
Answer (2026-11-15): docs/MISC_RECON.md section 7.

**MSC-H5 (P2) Time, pause, slow-mo for an editor.** Freeze the world while the editor is open but keep camera and cursor alive: time-scale channels, pause equivalents, which systems ignore time scale. (Shared with FOF-G4.)

**MSC-H6 (P2) Safe execution points.** Which thread and phase each call above must run on; which calls are unsafe during loads and cutscenes; physics-world lock rules; extend the crash catalogue in `GAME_INTERNALS.md`.

**MSC-H7 (P2) Audio state.** Music state or stem names per chase, combat and crime; Wwise RTPCs, states and switches a mod can set (for example a combat-intensity RTPC a tour can dial); crime and chase stingers. (Shared with EA-F7: one table.)

**MSC-H8 (P3) Cinematic hand-off.** Can a mod play a short cinematic or camera move on finishing (victory pose, a camera orbit at the finish line)?

**MSC-H9 (P3) Sharing user content.** What an Overstrike `.script` can ship besides the DLL, and anything in the game or Overstrike that stops a script from loading **user-created course and tour files** without a restart (we expect
none: our own parser and a reload command).

#### I. Tooling and documentation asks (cheap, valuable)

**MSC-I1 (P1) An asset finder with reverse indexes** (`tools/`): given the toc and ALERT, list assets referencing a needle (chase, pursuit, flee, race, checkpoint, gate, ring, beacon, ...) with their class, plus reverse indexes
(nodegraph -> actors, configs, zones; zone -> nodegraphs; crime type -> assets).

**MSC-I2 (P1) A nodegraph-to-text dumper** (see C1 (5)) and a **config schema printer** for the `Crime*`, `Challenge*`, `Hideout*`, chase-controller and race-like classes (field names, types, defaults, runtime offsets; compare FOF-H3).
Answer (2026-11-15): docs/MISC_RECON.md section 5 (partial: tools/sigtools/nodes.py and fx.py ddl).

**MSC-I3 (P2) Tracing hooks** left in `common/` behind commands, in the style of `uilog`: `crimelog` (director decisions), `nodelog` (graph nodes executed, pin values), `triglog` (trigger enter/exit), `herolog` (hero state changes,
velocity), `camlog` (camera takeovers), `rewardlog`.

**MSC-I4 (P2) A snapshot-and-replay aid** for an activity in progress (positions of hero and target, graph state ids) so a bug can be reproduced without replaying five minutes.

**MSC-I5 (P3) Anything else** you notice about open-city activities, DLC content (Heist, Turf Wars, Silver Lining), cut content or unused assets (co-op heroes, test areas such as `CHK_P35_CHARACTER_TEST_AREA`, `character_test*`,
any race, chase or tour demo). One paragraph each is gold. Leaked Insomniac material stays out (see `docs/REVERSE_ENGINEERING.md`).

### Priority summary (what unblocks what)

- **Villain Chases** (first milestone: force a Maggia Pursuit crime, then a chase as a new crime): A1, A2, A4, B1, B2, B3, then C1, C2, B4, B5, B6, B7, A3.
- **Race Editor** (first milestone: place five gates with a cursor and race them with a timer): D1, D2, D3, C2, C4, E1, E2, E3, then D4, D5, D6, E4, E5, F1, F2, then D7, E6, E7, H9.
- **Combat Tour Editor** (first milestone: two street stops with fights and a score): G1, G2, G3, G4, D1, D5, C2, then G6, G5, G7, H3, H4.
- **Shared and worth doing early:** C1 (runtime and dumper), C2 (HUD call tables), D1 (raycast), D2 (free camera), A1 (crime director), I1 and I2 (finder, graph dumper), H3 and H4 (attempt lifecycle, death).

### Where to put the answers

Number them in `docs/MISC_RECON.md`, add a one-line pointer under each request (`Answer: docs/MISC_RECON.md section <n>`), and put anything the **other mods** can use in `docs/GAME_INTERNALS.md`. Findings that change
what is feasible (for example "chases cannot be started outside the story" or "the crime director cannot be extended") also go in `docs/ROADMAP.md`. If you find a **better question** (an easier route to chases, a generic activity
system I did not know about, a race or tour editor already hiding in the game), answer that and say so first.
