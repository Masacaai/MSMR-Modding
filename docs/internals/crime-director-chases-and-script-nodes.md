# Miscellaneous recon: crime director, chases, script nodes, loader, hero death (static analysis, 2026-11-15)

Static analysis of the executable (nothing here was run in game unless a section says so). Game build **4.0630.0.0 only**; addresses are `exe+RVA`.
**Everything here was read from the exe on disk; nothing was run in game.** `[V]` = read in the code or in the exe's reflection data (static),
`[I]` = inference, `[?]` = unknown. The tools are in `tools/sigtools` (`fx.py`, `events.py`, `nodes.py`); generated tables are in `recon/*_4.0630.txt`
(regenerate them, do not edit; they hold names and offsets only). Read [reflection-tables.md](reflection-tables.md), first:
it explains the reflection tables (9,716 types, 3,808 enums, 3,061 events, 4,359 component types) that this file leans on.

**The better question first (as asked):** the game already contains a **generic chase system** (`ChaseGame`, `ChaseController`, tethers, fail rules, a dozen chase
script nodes: section 3), a **generic challenge family with a drone chase through markers** (`ChallengeDroneChase*`: gates, par time, score tiers: section 4), and the
crime director exposes **the same enable/disable-by-type and timer calls the missions use** (section 1). A "villain chase as a crime" is therefore mostly data and
node calls that exist, not new machinery.

## 1. The crime director: `RandomCrimeSystem2`
**Object [V]**: static instance **`exe+707a500`**, class `RandomCrimeSystem2` (vtable `exe+38edcf8`, 82 slots). Fields read from the script actions that poke it:

| offset | meaning | evidence |
|---|---|---|
| `+0x120`, `+0x121`, `+0x122` | enable bytes (`exe+c235c0(sys, bool)`: `+0x121 = v`, and `+0x120 = v` when `+0x122` is set) | setter |
| **`+0x1d8`** (`exe+707a6d8`) | **float: time until the next crime**; `SkipCrimeTimerAction` (execute `exe+c40700`) writes 0.0 here and fires its output | [V] |
| **`+0x1e8`** | pointer to the per-type records, **0x20c8 bytes per `CrimeType`** (67 records); **byte `record+8` = type enabled** | `exe+c235a0` |
| `+0x7b8c..0x7b98` | four float delays set by `SetCrimeDelaysAction` (`exe+c388a0`; values <= 0 are ignored) | [V] |
| `+0x7e7c`, `+0x7e80`, `+0x7e84` | accelerator values set by `CrimeAcceleratorAction` (`exe+c38860`) | [V] |

**Calls [V]**:

- **`exe+c235a0(RandomCrimeSystem2*, int crimeType, bool enable)`**: enable or disable one type. This is what the node `RandomCrimesEnableByTypeAction`
  (`CrimeTypes[]`, pins `Enable`/`Disable`, outputs `OnEnabled`/`OnDisabled`; execute `exe+c3c500`) calls for each listed type, or for all 67 when its list is empty.
- "Start the next crime now": write `0.0f` to `exe+707a6d8` (what `SkipCrimeTimerAction` does).
- Other nodes on the same object: `CrimeSettingsAction` (`exe+c36f40`), `SetCrimeDelaysAction`, `CrimeAcceleratorAction`, `RandomCrimesCleanUpAction` (works on the crime's
  `RandomSpawnerBase2` component, type `exe+7082990`), `RandomCrimesStateChangeAction{SuccessScreenDelay}`, `RandomCrimes2ScriptEventAction`, `CrimeBlockerVolumeAction`,
  `CrimeObjectiveAction`, `CrimeUIAction`, `CrimeBonusRewardAction`, `CrimeWaitForCleanupAction`, `RandomCrimesTimeoutMonitorAction`, `RandomCrimesPlayerDistanceAction`,
  `AllowCrimeVariationAction`, `CrimeInstanceVarAction`, `SetCrimeLevelAction` / `GetCrimeLevelAction` / `OnCrimeLevelChangedAction`. `CrimeDebugSettingsAction`
  (`CrimeType`, `VariationId`, `CooldownTime`: the developers' "force this crime") is **compiled out** (its execute is a bare `ret`). All nodes with pins: `recon/script_nodes_4.0630.txt`.
- Events: `RandomCrimeSuccessEvent` `0x3961c9f1`, `RandomCrimeSoftFailEvent` `0xb15b157b`, `RandomCrimeHardFailEvent` `0x3225e9bd`, `RandomCrimeCompletionUpdatedEvent` `0x0fd122d5`.

**Force crime type X [I, not run]**: disable every type but X with `exe+c235a0`, zero the timer, wait for the success/fail event, then restore the enable bytes you saved.
The director still applies its own filters (below), so X must be allowed in the hero's district, campaign and lighting mode, and within `MaxPickDistance`.

**The data model [V]** (`RandomCrimeSystemConfig`, runtime struct 0x188 bytes; print any of these with `fx.py ddl <Type>`):

- `SystemSettings` (`RandomCrimeSysSettings`): `MaxDistanceToSpawn`, `MaxAngleToSpawn`, `MinCooldownDistance`, `MaxCooldownDistance`, `MaxCooldownDistanceMult`,
  `PendingModeDeactivateTimeBonus`, `Min/MaxDeactivateScalingDistance`, `MaxDeactivateScalingMult`, **`DefaultAbandonDistance`, `DefaultDisengageDistance`, `DefaultCleanupDistance`**,
  `NoTimeoutFacingAngle`, `HealthPercentOnSuccess`, `DefaultCooldowns` (`CrimeCooldownData`: `Min/MaxCooldownInitial`, `Min/MaxCooldownPostEngaged`, `Min/MaxCooldownPostTimeout`,
  `AutoDeactivateTime`), `CooldownOverrides[]`, dispatch cooldowns, `NewBestTimeRewardId`, `SuperTimeRewardId`, flavor dialog.
- **`CrimesData[]`**, one per type: `CrimeType`, `DefaultFaction` {kThugs, kDemons, kSable, kPrisoners, kCTNSFaction1, kCTNSFaction2, ...}, `UILocTags[]`, `DialogTags[]`,
  **`SpawnChance`**, `NumOtherCrimesRequired`, **`AllowForRandomCrimes`**, `AllowInCoop`, **`MissionLocks`** (`UnlockSequence`: the story gating), **`TimeOfDayLocks`**
  (`AllowDuringDay/Night/Sunset/Overcast/Blackout/NightRain/OvercastRain`), **`CampaignsAllowed`** (kI20Base, kCTNS1..3), `VariationsData`, `LinkedCrimeTypes[]`,
  `NominalTimeToBeat`, `SuperTimeToBeat`, **`MaxPickDistance`**, **`PrefabRegion`** (a file: **the crime's content is a region prefab**), `CrimeLevelData`.
- `CrimesDistrictData[]`: `District`, `MissionLocks`, `AllowedTypesOverride[]`. `CrimeFactionsData[]`: `Faction`, `CampaignForCompletionCheck`, `TallyLocTag`,
  `DistrictCompleteTags[]`, `CompletionCount`, `DepriorizeMission`, **`UnlockPostGame`**. `ClassSpawnData[]`, `SpawnTypeSets[]` (`RandomCrimeSpawnSet{GangLeader, SpawnOptions[]}`).
- `CrimeOverlayData{CrimeType}` is region game data tying a level overlay to a type; `CrimeLink{CrimeType}` links activities.

So **a new crime type by data only is not possible (the enum has 67 fixed values and the per-type array is sized by it), but re-purposing an existing type is**: its
`PrefabRegion`, text tags, locks and chance are all config (`.stage`/`.modular` on the `RandomCrimeSystemConfig` asset). `kChaseInProgress` (11), `kDemonChase` (17),
`kVanEscape` (9), `kCTNSChaseInProgress` (51), `kCTNSPoliceVanEscape` (49), `kCTNSFactionCarChase` (65) are the pursuit types (the "Maggia Pursuit" on screen is one of the
CTNS chase types [I]). What a finished game removes is decided per type by `MissionLocks`, `CampaignsAllowed` and per faction by `UnlockPostGame`.

**All 67 `CrimeType` values [V]**: 0 kInvalid, 1 kMission, 2 kPoliceShootout, 3 kPurseSnatch, 4 kPurseSnatchSequence, 5 kAssault, 6 kBreakIn, 7 kDrugDealWithCar, 8 kDrugDealNoCar,
9 kVanEscape, 10 kRobbery, 11 kChaseInProgress, 12 kCheckpoint, 13 kRooftopObservation, 14 kAPCPatrol, 15 kMugging, 16 kMassArrest, 17 kDemonChase, 18 kArmoredCar,
19 kDemonShootout, 20 kHeliAssault, 21 kBombBuilding, 22 kVehicleBomb, 23 kDemonBus, 24 kPoliceKidnap, 25 kPoliceAmbush, 26 kRiot, 27 kPrisonerVsAPC, 28 kFoodRaid,
29 kPrisonerBus, 30 kJetPackAmbush, 31 kDronePatrol, 32 kTHGKidnap, 33 kDemonRooftopAmbush, 34 kDemonSblRoofAssault, 35 kDemonChkpntAssault, 36 kTHGCarCrash, 37 kPRSJoyRide,
38 kCTNSBCSpiderBot, 39 kCTNSBCCrime02, 40 kCTNSHHWitnessEscort, 41 kCTNSHHCrime02, 42 kCTNSSBCrime01, 43 kCTNSSBCrime02, 44 kCTNSPoliceShootout, 45 kCTNSAssault, 46 kCTNSBreakIn,
47 kCTNSDrugDealWithCar, 48 kCTNSDrugDealNoCar, 49 kCTNSPoliceVanEscape, 50 kCTNSRobbery, 51 kCTNSChaseInProgress, 52 kCTNSMassArrest, 53 kCTNSSableShootout, 54 kCTNSMugging,
55 kCTNSArmoredCar, 56 kCTNSBombBuilding, 57 kCTNSVehicleBomb, 58 kCTNSBusHostages, 59 kCTNSPoliceAmbush, 60 kCTNSRiot, 61 kCTNSFoodRaid, 62 kCTNSKidnapping,
63 kCTNSRooftopAmbush, 64 kCTNSCarCrash, 65 kCTNSFactionCarChase, 66 kCTNSFactionRooftopShootout.

Not done [?]: the director's tick and its choose/place code (start from the system's vtable and from the readers of `+0x1d8`), the vehicle pursuit controller
(`ChaserVehicleScriptAction`, `ChaseControlScriptAction{TargetVehicle}`: section 3), and a live `crimelog`.

## 2. Loading a level or checkpoint, and warping to a checkpoint

**The loader [V]**: the node `LoadLevelAction` (`LoadLevelActionPrius{Level: file, CheckpointName: string, DelayTime: float}`, execute `exe+1ef9c20`, input pin hash `0x6a84a393`) does:

```
gm = exe+1ef6560()                       // GetGameManager: GetActor(handle at exe+7c2a4a0) -> its GameManager component (type object exe+7c2a390)
gm->vtable[+0xf8](gm, u64 levelAssetId, u32 checkpointNameHash, float delaySeconds)        // = exe+1ef4ca0 for GameManager, GameManagerCampaign and GameManagerMultiplayer
```

`exe+1ef4ca0` only writes **synced state fields** of the manager with `exe+1676e80(obj, fieldId, &value, size)`: field 1 (`+0x5c`) = 1, field 2 (`+0x58`) = 2, field 3 (`+0x6c`) = delay,
field 0 = a timestamp, **field 4 (`+0x60`) = level asset id, field 5 (`+0x68`) = checkpoint hash**; the manager's update then performs the load. Arguments: the level id is the asset
id of the `.level` path (CRC-64 of the lower-cased path, `toctool hash`), the checkpoint is the CRC-32 of its name (`fx.py h CHK_COOP_BC_04_DRUGLAB`).
Classes: `GameManager` (vtable `exe+388f500`), `GameManagerCampaign` (`exe+388f620`, the one in a normal game), `GameManagerMultiplayer` (`exe+388fae8`: the cut co-op/versus manager;
nodes `SetTeamGameAction`, `SetCompetitiveGameAction`, `IsCampaignAction`, `UIScoreboardAction`, `ControlPoint`, `EndLevelVictory/Defeat/MainMenu/LobbyAction` all call `exe+1ef6560`).
`GameReloadDebugMenu` (vtable `exe+38e66b8`) is the developers' reload menu and calls the same getter.

**Warp inside the loaded level [V]**: `CheckpointWarpAction{CheckpointName}` (execute `exe+9b75d0`; value pin `CheckpointName`, output `Out`) looks the checkpoint up in the world's
table and schedules the warp (callback `exe+9c0d60`). **The checkpoint table lives in the world singleton**: name-hash map at `world+0xe0900` (`exe+1bcf3e0(map, hash)` -> index),
records of 0x48 bytes at `[world+0xe08f0]`. Enumerating that array lists every checkpoint of the loaded level with its hash [I: record layout not decoded]. The hero side is
`Hero::HeroStateCheckpointWarp(Local)`; events `HeroWarpStartedEvent` `0x720f8cad`, `HeroWarpExecutedEvent` `0x43981fe2`, `HeroWarpFinishedEvent` `0x05e298dc`,
`CheckpointLoadedEvent` `0x414ad37a`, `LevelLoadedEvent` `0xfbe1b646`. Other nodes: `CheckpointAction{CheckpointName}`, `CheckpointQueryAction`, `CheckpointCompareAction`, `AutoCheckpointAction`.

Not known [?]: save coupling (the `SaveFile` system handles `CheckpointLoadedEvent`, so a mod-triggered load can be saved: suppress saving first), and whether a checkpoint
of a level that is not the current one needs the level id of its own `.level`.

## 3. The chase system that is already in the game
[V: classes, data types and node pins; behaviour is by name]. Components: **`ChaseController`** (`ChaseControllerPrius{ChaseTether, ChaseFail}`; `BlackCatChaseController` adds
`BlackCatChaseActiveAimingData`, `RangeDist`, `RangeTime`, `ZipTimeout`, `PedestrianData`), **`ChaseGame`**, `ChaseData`, `ChaseNode`, `ChaseLeapJobHost`, config asset type
**`ChaseConfig{ChaseController}`**. Movement verbs exist three times each (script node, behavior, bot state): **RunTo, JumpTo, GrappleTo, PointLaunchTo, SlideTo, VaultOver, BotJumpTo, Zip**.

| piece | fields / pins |
|---|---|
| **`ChaseGamePrius`** (the "catch me" rules) | `TargetMaxSpeed`, `TargetMinSpeed`, `TargetMaxSpeedRadius`, `TargetMinSpeedRadius` (**rubber band by distance to the hero**), `TargetSpeedPercentageAtEnd`, **`FailureRadius`**, **`CatchRadius`**, **`CatchableAfterPercentOfPath`**, `PromptType` {kDefault, kYank, kZipTo, kStrike} (the catch prompt), `MacroType` |
| `ChaseDataPrius` | `MaxSpeedDist`, `MinSpeedDist`, `SlowDownTimer`, `SlowDownScale`, `FailureDistance`, `FailureTimout`, `WarningMessage` |
| `ChaseTetherIdealDistancePrius.TetherParams` | `Min/MaxTetherDistanceHint`, `Min/MaxTetherSpeedHint`, `DecelerationHint`, `AccelerationHint`, `HeroMin/MaxSpeedHint` |
| `ChaseFailByDistancePrius` | `WarnDistance`, `StopWarnDistance`, `FailDistance` |
| node `ChaseGameControlScriptAction` | in `Setup`; values **`ChaseVolume`, `Target`, `Path`**; out `OnSetup` |
| node `ChaseStartScriptAction{ChaseConfig, AllowCombatCamera}` | in `Start`, `Stop`; value `Bot`; out `OnStopped` |
| node **`ChaseFollowCurveScriptAction`** `{DoSmoothTransition, IntroCurveMin/MaxDistFactor, IntroCurveAngleFactor, IntroCurveMaxTurnRate}` | in `Start`, `Pause`, `Resume`, `Complete`; values **`Actor`, `Curve`** |
| node `ChaseFollowCurveLinkedScriptAction` | in `Link`; values `ChildActor`, `ParentActor`, `ChildCurve`, `TetherActor` (a tethered follower) |
| node `ChaseTetherScriptAction`, `ChaseTetherIdealDistanceScriptAction`, `...ModifierIdealDistanceHeroHeight...`, `...TimedReduce...` | in `Enable`/`Disable` or `Start`; values `Bot` + the tether hints |
| node `ChaseAttackScriptAction` | in `Enable`/`Disable`; values `Bot`, `CooldownTime`, `MaxAttackDistance`, `DodgeWindow` (**the target attacks back while fleeing**) |
| node `ChaseClosestLocScriptAction` | in `Execute`; values `Bot`, `Locators` |
| node `ChaseNextNodeScriptAction` | value `ChaseBot` |
| node `ChaseJumpToScriptAction{Height, LandingWarnTime, MidAirStartVal, SpeedOverride, ArcFinderparameters}` and the other verbs | in `Start`; out `OnFailed` (all share execute `exe+1d96cd0`: they queue a bot command) |
| node `ChaseGadgetAimHelperScriptAction`, `ChaseAssistAction` | aim help during chases |
| vehicles: `ChaseControlScriptAction` | in `StartChase`, `StopChase`; value **`TargetVehicle`** |
| vehicles: `ChaserVehicleScriptAction` | in `Chase`, `Stop`, `SetDistance`; values `Vehicle`, `TargetVehicle`, `IdealDistanceOverride`, `MaxSpeedOverride`, `AccelerationOverride`, `DecelerationOverride` |

Reading of the model [I]: **the route is a curve** (`Curve`/`Path` pins: a spline actor in the zone) or a chain of `ChaseNode`s whose verb says how to leave it (run, jump, grapple,
slide, vault); the target's speed is set from the hero's distance (max/min speed radii); the hero wins inside `CatchRadius` once `CatchableAfterPercentOfPath` of the path is done
and loses beyond `FailureRadius`/`FailDistance`. So **an arbitrary mod-made route is plausible for the curve form** (spawn or reuse a curve actor and call the follow-curve
start with it) and needs authored jump/grapple data for the node form. That `BotCommandChaseRunTo` "glides and never finishes" for spawned allies fits: it is one verb of this
system and expects a `ChaseController`/chase state around it.

The cut co-op chase (`CHK_COOP_CHASE_DEMO`, `enm_coopchase_enemy_movement`) and which story chases use the curve or the node form need the assets (not opened this session) [?].
Other chase-like classes: `BehaviorElectroChase*` (Electro's rooftop chase), `ChallengeDroneChase*` (next section), `BotPedestrianFleeScriptAction`, `ChaseSparkData`
(Screwball's spark pickups: `PointsPerSpark`, `ShowPOI`, `POIType`, distance and height windows, `Tiers[]`).

## 4. Race-like content: the drone chase challenge
[V] `ChallengeDroneChase` (Taskmaster drone challenge) is a timed course through **markers**:

- `ChallengeDroneChasePrius`: `SkipCountdown`, **`ParTime`**, `ScoreTier1/2/3`, `FakeHighScore`, `Drone`, **`DronePath`**, `Destination`, **`MarkersGroup`** (actor references).
- `ChallengeDroneChaseConfig`: `UnderParBonus`, `FailTimeSlowScale/Ramp/Duration`, `FailDeathDuration`, `DroneAccel/Decel`, `DroneSpeedSlow/Normal`, `AdvanceDistance`,
  `SlowDistance`, **`AbandonDistance`**, `MarkerPointValue`, **`MarkerTolerance`**, `StreakBaseScore`, `StreakMultiplier`, `PlateauAtStreakCount`.
- **`ChallengeDroneChaseMarker`** (the gate): `AdditionalProp`, `TransitionDuration`, `ActivatedMaterial`, `MaterialVariable`, `ActivatedValue`, `CollectedValue`: a prop whose
  material variable is driven between "active" and "collected". This is the ready-made race gate (asset paths: search the toc for `ChallengeDroneChaseMarker`).
- Classes: `ChallengeDroneChase`, `ChallengeDroneChaseCTNS`, `...Action`, `...Target`, `...Object`, `...Prop`; the system is `ChallengeSystem` (`configs/system/system_ChallengeSystem.config`),
  HUD panel `activity 1` (docs/internals/, UI section). Other challenge classes: `fx.py cls "^Challenge"`.

## 5. The script-graph runtime, and a catalogue of 1,032 node types

[V] A node is a C++ object derived from `ScriptAction` (11+ vtable slots). **Slot 10 is `Execute(node, u32 inputPinHash)`**; `node+0x58` is the graph context; the node's own data
(the DDL type `<Class>Prius`) is copied into the node from `+0x60`. Pin API (hashes are CRC-32 of the pin name):

| call | meaning |
|---|---|
| `exe+1923410(ctx, &out, pinHash) -> bool` | read an actor/handle value pin |
| `exe+1923490(ctx, &outHandle, pinHash)`, `exe+19234e0(ctx, &outBool, pinHash)`, `exe+1923780`, `exe+19238a0`, `exe+1923cf0`, `exe+1923520(ctx, pinHash)` | read other value pins (typed variants) |
| **`exe+1923a90(ctx, pinHash)`** | fire an output pin |

Slots 1 and 2 build the node from its data, slot 9 returns the type object. `tools/sigtools/nodes.py` reads every `...Action` class and writes
**`recon/script_nodes_4.0630.txt`**: class, vtable, execute RVA, input pins it compares, value pins it reads, output pins it fires, data fields with offsets, component types it
touches. Examples: `UICrimeScannerAction` (in `Show`, `Clear`; data `CrimeType, ObjectiveType, Flags, VOffset, ClearOnUnload`), `TimeScaleAction` (`TimeScale, Context, RampTime, UseFX`),
`HeroLockAction` (in `LockHeroInput, LockHeroStand, LockCameraInput`...), `ToggleHeroMovesAction`, `OverlayAction` (`Load`/`Unload`), `LoadLevelAction`, `CheckpointWarpAction`.
The catalogue is the menu of "native building blocks": every activity type has its nodes there (`grep -i challenge`, `crime`, `objective`, `spawn`).

Calling a node's logic from a mod: usually skip the node and call what its `Execute` calls (the catalogue gives the RVA to read). Not done [?]: the interpreter that walks a
`.nodegraph` asset (`NodeGraphAssetManager`, vtable `exe+3d07d08`), building a graph in memory, and a graph-to-text dumper.

## 6. State predicates and hero state for race rules
- **Mission states** [V]: there are two query paths; in short `state = exe+201f800(q[exe+7c4bbb0], &handle)` after
  `exe+201f640(mgr, &handle, crc32(missionName))`, or `exe+1fa74f0(q[exe+7c3ef70], name, 0)`; **0 Inactive, 1 Available, 2 Active, 3 Cleanup, 4 Complete, 5 Failed**;
  number of active missions `exe+1fa5f90(q[exe+7c3ef70])`.
- **Loading**: `dword [world+0xe3cb0]` in 0x17..0x19 while the world loads (`world = exe+1f707e0()`) [V from the overlay natives' guard].
- **Hero on ground / in air / perched** [V]: component `HeroMacroState` (type `exe+6dabca0`), flags at `+0x4c`: kOnGround 0x2, kInAir 0x4, kNearGround 0x8, kTimedDodge 0x10,
  kRushToMelee 0x20, kSprinting 0x40, kSpiderAssault 0x80, kEasyAirYank 0x100, kBeam 0x200, kPerch 0x400 (`docs/internals/hero-abilities-gadgets-and-combat-events.md` section 8).
- **Disabling traversal categories for a race class** [V]: component `HeroAbilityLock` (type `exe+6daad90`): 40 named abilities (kWebSwing 24, kWebZip 21, kPointLaunch 28,
  kZipToSurface 35, kWallRun 18, kSprint 17, kGadgetWheel 38, kShoot 15, kMelee 3 ...), `Lock(ability, layer, seconds)` at vtable `+0x50`; use layer 0 or 1. The game's own
  one-call form is `exe+9189e0(heroEntity, allowedMask)`. Full table and recipe: `docs/internals/hero-abilities-gadgets-and-combat-events.md` sections 1 and 2. Input and camera lock: the `HeroLockProcessHandle`
  message (`LockHeroInput` -> `exe+83c350(HeroLocal, 0)`, `LockCameraInput` -> `exe+90db30(HeroCameraManager)`); combat moves: `HeroCombatActionLocker` (type `exe+6dcc950`).
- **Time scale**: `TimeScaleAction{TimeScale, Context, RampTime, UseFX}` (execute `exe+19ba360`) is the node form of `exe+19bbaa0`.
- The hero's detailed state is the `Hero::HeroState*` class family (about 400 classes: `fx.py cls "^Hero::HeroState"`); which one is current, and the velocity vector, were not located [?].

## 7. Hero death and respawn
[V] `Hero::HeroStateDeathLocal` (enter = vtable slot 11, `exe+9d4a90`) ends with: if `byte [GameManager + 0x9a]` is set, `GameManager->vtable[+0x88](gm, heroEntity)`
= `GameManagerCampaign::OnHeroDied` (`exe+722750`). That function **adds a `HeroRespawn` component** to the hero (type object `exe+6dac220`, class `Hero::HeroRespawn`,
`AddComponent exe+159cb80(hero, &type, 0)`) unless one exists, and sets its timer: `respawn+0x4c = respawn+0x50 =` the hero config value `kRespawnTime` (hash `0x78ce7323`),
`respawn+0x54 = 1`. The component's update (`Hero::HeroRespawnBase` slot 10, `exe+1f253b0`) counts down and calls the `HeroSystem` respawn functions (`exe+1f22c50`, `exe+1efd2f0`,
`exe+1f22510`), then `HeroRespawn` slot 11 (`exe+858c20`) restores ammo. The state also looks for a `ReloadOnDeathVolume` (type `exe+7074d90`) around the hero (a volume that turns a
death into a checkpoint reload) and touches the weapon manager and inventory.

So the interception points are [I, not run]: (a) hook `exe+722750` and do not call the original while your activity owns the death (no respawn timer starts; the hero stays in
the death state until you warp or revive him), (b) clear `byte [GameManager+0x9a]` for the duration, (c) shorten or lengthen `HeroRespawn+0x4c/+0x50`. `HeroSpawnedEvent`
(`0x183d74d2`: `+0x138 Actor`, `+0x140 ActorPosition`, `+0x174 CheckpointHash`) is the "hero exists again" signal; after it every cached hero pointer is stale
(the three gadget crashes in `docs/internals/hero-abilities-gadgets-and-combat-events.md` section 9 were exactly that).

## 8. Events, damage and scoring signals
`recon/events_4.0630.txt` lists all 3,061 events (id, payload fields with offsets, senders, registered handlers); subscription is described in docs/internals/. For scoring a tour
with the game's own numbers: `HeroComboUpdateEvent{ComboCount}`, `HeroComboTier*Event`, `HeroPerfectDodgeEvent`, `HeroAccurateDodgeEvent`, `HeroDodgeAttackerEvent{DodgeType, DodgeAccuracy}`,
`HeroActiveFinisherStartedEvent{TargetActor}`, `DeathDealtEvent` (who killed whom, with `Type`, `WeaponNameHash`, `DamageHash`), `DamageDealtEvent{AmountInflicted, HealthPercent}`,
`BotKilledByTripMineEvent`, `HeroSpiderAssaultAddEnergyEvent{Amount}`; the damage struct is in `docs/internals/hero-abilities-gadgets-and-combat-events.md` section 3. Activity results: `RandomCrimeSuccessEvent` and the
`Challenge*` events (`grep Challenge recon/events_4.0630.txt`).

## 9. Not answered this session

World raycast and ground queries (D1), free camera and photo mode (D2), marker assets and VFX spawn (D3, D4), navmesh queries (D5), text entry (D7), save-file format (F1),
the per-chase inventory with checkpoints (B1, needs the assets), rewards bookkeeping (C3), the trigger-volume classes (C4): none was opened. Starting points from the tables:
`fx.py cls "Trigger|Volume"`, `fx.py ddl "PhotoMode|Camera"`, `grep -i -E "reward|token" recon/script_nodes_4.0630.txt`, `fx.py cls "NavMesh|NavQuery"`.

## 10. The camera, a world raycast and the views table
Added later in the same session; this supersedes the D1/D2 lines of section 9. [V] The node `CameraRaycastPositionAction{Distance, PhysCollProfile}` (execute `exe+1e2fe10`;
outputs `RaycastHit`, `Out`; value outputs `Position`, `NormalFacingVec`) is "cast a ray from the camera along its forward vector" and shows the whole recipe:

```
view  = exe+18a0f60(&ViewTable /*exe+7a34dd0*/, 0)          // view record of view 0, or null if that view is inactive
pos   = float3 at view+0x30;   fwd = normalize(float3 at view+0x20)   (exe+2d0740(out, view+0x20))
end   = pos + fwd * distance
exe+17ff820(&filter /*~0x20 bytes*/, u32 physCollProfile)   // collision filter from a profile
hit   = exe+18264e0(q[exe+609a570] /*physics world*/, &filter, &pos, &end, "debug name")     // null = no hit
p     = *(float3*) exe+1818850(hit, 0)                      // hit position
n     = *(float3*) exe+1818770(hit, 0)                      // hit normal
exe+18045a0(&filter)                                        // release the filter
```

- **The camera**: the view record holds the camera matrix (rows at `+0x00`, `+0x10`, **forward at `+0x20`, position at `+0x30`**); 78 functions get it through
  `exe+18a0f60`. This replaces the memory-scan (`camscan`) and the "hero heading plus a cone" guess. FOV and the projection are further into the 0x1f70-byte record [?].
- **Views are a table**: `ViewTable` = `{view records* (+0), active bitmask (+8), ..., count (+0x14)}`, **0x1f70 bytes per view**, selected by index. The renderer is built
  around numbered views, so a second view is a structural possibility, not a hack; who creates views and with what render target was not read [?].
- **Collision profiles** (enum, 19 values): 0 kDefault, 1 kMove, 2 kCharacterMove, 3 kHeroMove, 4 kHeroMoveOnly, 5 kHeroAltMove, **6 kGround**, 7 kCharacterGround, 8 kHeroGround,
  9 kDamage, 10 kTarget, **11 kCamera**, 12 kCameraObstruct, 13 kProjectile, 14 kProjectileEnv, 15 kWallRun, 16 kWallCrawl, 17 kWater, 18 kGroundAndWater. For an editor cursor on
  static world geometry use `kGround` or `kCamera`; `kGroundAndWater` / `kWater` give the water test. The underlying flag set `PhysicsCollFlags`: kMove 0x1, kHeroMove 0x2,
  kGround 0x4, kProjectile 0x8, kTargeting 0x10, kCamera 0x20, kCamObstruct 0x40, kWallRun 0x80, kWater 0x100, kWallCrawl 0x200, kHeroAltMove 0x400, kDefault 0x43f.
- **Ground height at (x, z)** = the same call with a vertical ray and profile `kGround`. Other nodes: `RaycastAction{SourceLocator, TargetLocator, CollFlags}` (a `RaycastComponent`
  between two actors, pins `Enable`/`Disable`), `RayCastDeferredAction{CollisionFlags}` (`GetNextHitResult`).
- Thread: the node runs on the game thread during script update; call it there (`gamethread::Run`) [I]. Not checked: the hit entity and the surface material in the hit record.

## 11. Markers, text and debug drawing
- **Debug draw is compiled out [V]**: `DebugDrawLineAction`, `...SphereAction`, `...ArrowAction`, `...CapsuleAction`, `...CylinderAction`, `...ConeAction`, `...CurveAction`,
  `...VolumeAction` exist, but their execute functions only read their pins (`Color`, `Width`, `Radius`, `Duration`...) and return: no draw call is left in the retail exe.
  A mod needs real assets or its own overlay.
- **Text in the world [V names]**: `ScaleformDrawTextAction` (execute `exe+1f0c960`; inputs `Show`, `Disable`, `Clear`; values `DrawTextTarget`, `LocTag`, `Number`; data `UseTempText`,
  `LocTag`, `TextColor`, `TextAlpha`, `BackgroundColor`, `BackgroundAlpha`, `DisableTimer`, `ClearTimer`, `FontSize`) sends three messages (types `exe+7c2b820/28/30`) to a
  **draw-text target actor**: an actor carrying a Scaleform surface that shows a string or a number. `UseTempText` suggests free text without a localization tag [I]. Finding the
  target actor asset (search the toc for the component name next to this node's users) gives a 3D label for gates and stops.
- **Gates**: `ChallengeDroneChaseMarker` (section 4) is a prop with an "activated" and a "collected" material state; `mis_ui_base_replayreticle` (already used for the docks) is a
  ground circle. `ChaseSparkData.POIType` shows the POI icon enum has 63 values (`fx.py enum "^kRandomCrime$"`): kGeneric, kProgress, kRandomCrime, kHazard, kButtonTriangle, ...
- VFX spawn API, world-to-screen projection and the objective-marker widget were not examined [?].

## 12. Photo mode and free cameras: what exists
[V classes] `Camera2::PhotomodeCameraFreeMove`, `PhotomodeCameraOrbit`, `PhotomodeCameraSelfie`, `PhotomodeCameraFirstPerson`, `PhotomodeCameraMotionFly`,
`PhotomodeCameraInputYawAndPitch`, `PhotomodeLookCameraMover`, `PhotoCameraManager` (vtable `exe+38f8698`), `PhotoModeUpdaterComponent`, `PhotoModeLights::*` (the light rig),
`GUI::PlayerHudPhotoMode`, data `PhotomodeCameraDef`. Switch: `SetPhotomodeSystemEnabledAction` (execute `exe+ca1750`): pin `Disable` writes 1, `Enable` writes 0 to
**`byte [exe+708ea1e]`** (photo mode disallowed). Input sets: `PhotomodeControls` (16 actions: kPhotomodeSave, kPhotomodeReset, kPhotomodeTogglePanel, kPhotomodeToggleUI,
kPhotomodeToggleCameraMove, kPhotomodeFullExit, sticker rotation...), `PhotomodeCameraInputControl` (kPhotomodeCameraInputX/Y, kPhotomodeCameraMoveX/Y, kPhotomodeCameraRaise/Lower).
Developer cameras still have their input sets and classes: `kFPSCam*`, `kMayaCam*` (`GameControl`), `DebugCamera*Control`, `MarketingCameraComponent` (vtable `exe+4f6cbe0`) with
`MarketingCameraBindingsConfig` / `MarketingCamOptionBindingsConfig`, `DebugCameraSplineEditor`. `Camera2::CameraPositionLock` and the `Cinematic2*` camera tracks are the scripted
side. Entering the photo-mode free camera from a mod (the cheapest "editor camera": it already pauses the world and hides the HUD) means driving `PhotoCameraManager`; its
methods were not read [?]. Input for an editor: `docs/internals/`, "Input: the game's action layer".
