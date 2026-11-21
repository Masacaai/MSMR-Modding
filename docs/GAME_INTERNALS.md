# Game internals: what we've learned about Spider-Man Remastered 4.0630.0.0

Everything here comes from reading the game's executable, its asset archives and memory, plus testing in game. All
addresses are `Spider-Man.exe`-relative RVAs (`exe+RVA`; ImageBase `0x140000000`) and **valid for build 4.0630.0.0 only**.

Confidence tags: **[verified]** tested in game; **[RE]** read from disassembly, not exercised, or only partly; **[?]** unknown
or a guess. When you extend this file, keep the tags honest. How each thing was found: [REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md).
Earlier, more chronological notes: [`recon/M1_natives_4.0630.md`](../recon/M1_natives_4.0630.md), [UI_HUNT.md](UI_HUNT.md), [BUGS.md](../EndlessArena/docs/BUGS.md).

Contents: [Loading code into the game](#loading-code-into-the-game) | [The game thread](#the-game-thread) | [Entities, components, handles](#entities-components-and-handles) | [Spawning and despawning](#spawning-and-despawning-actors) | [Assets and models](#assets-and-models) | [Enemy AI and aggro](#enemy-ai-and-aggro) | [Health and damage](#health-and-damage) | [Bosses](#bosses) | [UI (Scaleform)](#ui-scaleform) | [Time, camera, input](#time-camera-and-input) | [Inventory, suit powers, mods](#inventory-suit-powers-and-mods) | [World and arena](#world-and-arena) | [Crash and hang catalogue](#crash-and-hang-catalogue)

## Loading code into the game

- **Overstrike** (Tkachov, GPLv3) installs `.script` mods. With "Enable .script support" and the `-scripts` command-line option,
  its proxy (`winmm.dll` in the game folder) hooks the game's command-line parsing and `LoadLibrary`s each script DLL from
  `<game>\scripts\`, then calls its exported `script_enable()`. **[verified]**
- `script_enable` runs very early, before the world exists. Start a thread and wait. **[verified]**
- Script Hook (the older mod loader) and its natives list were used **only as a map** of which game functions matter. Its
  `SpawnActor`, `LoadAssetByHash`, `CreateSyncObject`, `GetActor`... correspond to the natives in `natives.cpp`.

## The game thread

- **Hook point `exe+1f745f0`**, called once per frame from `exe+1f731a1`. A detour pops a queued job (`gamethread.cpp`), then
  calls the original. Signature in `natives.cpp` (`GameThreadTick`). **[verified]** An alternative per-frame hook, `exe+16244c0`, is used by other projects. **[RE]**
- The tick only runs while the game window is focused. A background game doesn't process mod jobs. **[verified]**
- Game state must be changed from this thread. Our `gamethread::Run` blocks the caller until the job ran (timeout 3 s by default).
- Frame-rate proxy: ticks per second and longest gap between ticks (`ticks`, `fps` dev commands).

## Entities, components and handles

- **Entity** (from Script Hook's `GetComponentByName`/`GetTransform`): `[ent+0x00]` Transform pointer (position `+0x30`),
  `[ent+0x68]` component array of 16-byte entries `{desc*, instance*}`, component type name `(char*)[desc+0x60]`, `int16 [ent+0x70]`
  component count. **[verified]**
- **Actor table**: `[exe+7a44380]` points to entities stored inline, `0xC0` bytes each; count `u32 [exe+7a4439c]`.
  `GetActor(handle)` (exe+15a0560, same function as the SDK's `GetActor`) = base + index*0xC0. **[verified]**
- **Handle layout**: `(word[ent+0x10] & 0x7FF) << 20 | (dword[ent+0xC] & 0xFFFFF)`. **[verified]**
- Weak references: `ResolveRef` exe+16798f0 over a table of 16-byte `{object*, generation}` entries (`[exe+7a44320]`). **[RE]**
- Hero: `HeroSystem` at `exe+5d9dd00`; hero handle data `exe+5d9de68` (= HeroSystem+0x168). **[verified]**
- Finding a component by name and dumping all of an entity's components are dev commands (`comps`, `near`). A bot has
  ~130 components (`ThugBot`, `BotCombatAlternatives`, `AnimControllerComponent`, `TargetData`, `BotStatusEffectTracker*`, ...).

## Spawning and despawning actors

Spawning is a recipe the mod replays from Script Hook's disassembly (details in `recon/M1_natives_4.0630.md`). **[verified]**

```
hash  = ConvertStringToHash(path)                       exe+1bb25d0 (u64* out, const char*)
asset = LoadAssetByHash(&ActorAssetManager, hash, 0,0,0,0,0)    exe+1626380   (ByName: exe+1626300)
wait until byte [asset+1] == 1                           "loaded"
ent   = SpawnRaw(hash, 0, &Transform)                    exe+1f65170   (first arg is the u64 HASH, not the asset pointer)
sync  = CreateSyncObject(descriptor)                     exe+1980a60  (descriptor: dword 0, word slot, ...; slot = first null of the 0x2EE0-entry table at [exe+7a65f00])
handle = register: SyncRegisterHandle(sync, &handle)     exe+1986230
attach / activate: EntityInit exe+159db20, EntityAttach exe+159e590, SyncActivate exe+19877e0 (if flags&8), SyncFlag2, SyncFinish1/0
```

- Transform is 0x40 bytes: `[0]=sin t, [8]=-cos t, [0x14]=1, [0x20]=cos t, [0x28]=sin t, [0x30..0x38]=pos, [0x3C]=1`, t = heading in radians.
- A sync object begins with the actor handle. `GetActor(sync)` returns the entity.
- **Despawn**: find the sync object from the entity, `DeleteSyncObject(sync)` (exe+1981690), then on the game thread
  `EntityDestroy(entity)` (exe+159d500). **[verified]**
- All of it runs on the game thread via `gamethread::Run`; our worker blocks while it does.
- Spawn cost is 20-38 ms per enemy; each spawn is logged as `spawn: [1] ... [8] done` (the numbered steps are the stages above).

## Assets and models

### Loading and reference counts

- `LoadAssetByHash` returns an asset whose `byte [asset+1] == 1` once loaded and which holds a `u16` reference count at `[asset+4]`.
- `AssetRelease(manager, asset)` = exe+1626940 drops one reference. **We never release during a run** (see below).
- An enemy actor is not just its `.actor`: it depends on other actors (weapons, jetpacks, heads, ammo items, projectiles).
  These must be loaded too or the enemy spawns with **white cubes** where the missing models belong. The chain is
  `.actor -> rifle_firing config -> WeaponItemConfig -> ItemActor` for weapons and jetpacks. `tools/assets/deps.py` reads
  the archives and generates `src/preload_deps.inc`; `spawn::Preload` loads those extras and `spawn::Ready` waits for them
  (20 s cap, then logs "not fully loaded"). **[verified]**

### Model capacity and asset lifetime

- **Releasing assets mid-run is fatal**: surviving enemies lose their bodies (head and weapon only) and the game
  crashes at `exe+15f9bc9` (a null mesh pointer). So the mod releases only 30 s after a run ends. **[verified]**
- **The game runs out of room for enemy models** and breaks "whatever loads last": white cubes, floating heads, invisible enemies.
  Observations: 33 unit types fine; 39 fine; 50 broke Olympus (Horde); a 57-type "parade" at Ryker's was fine; 86 broke. The real limit
  and cause are **[?]** (candidates: live bodies and enemies, mid-run loading, or something specific to a faction).
- Therefore pools are chosen by a **unit-type budget** (`plan::kMaxTypes`, default 44; `[run] max_types` in the ini):
  `plan::BudgetedPool` adds factions greedily until the budget is spent. Faction sizes: street 7, fisk 12, demons 9, prisoners
  15, sable 9, hammerhead 10, olympus 11, tombstone 8, taskmaster 8 (89 types in all).
- A history of the "respawn load hang after a long Horde run" (all nine factions, 1000+ assets) fixed by the budget and the
  post-run release is in [BUGS.md](../EndlessArena/docs/BUGS.md).

### Archive format notes (from the ALERT wiki)

Assets live in `asset_archive`, found via the `toc`; asset ids are CRC64 of the file name; containers are DAT1; `.config` files
are binary JSON-like with a `Type`. `tools/ALERT` (a clone of Tkachov's ALERT, not in this repo) converts configs to JSON and back.
`tools/toctool` searches the table of contents, and `tools/assets/extract.py` pulls assets out.
Don't commit extracted bytes.

## Enemy AI and aggro

- **Why spawned enemies fight at all**: right after `SpawnRaw` and **before activation**, the mod sets
  `BotData.SpawnEngagedWithHero` (the Bot component, `+0x168` of the Bot component per the first recon; `spawn.cpp` finds the
  `BotData`-shaped struct via `entity::FindBotData`). The game sets the same flag for hideout waves. **[verified 2026-10-12]**
- **Heavies are spawned calm on purpose.** Units whose bot class is `SlayerBot` or `SlayerRangedBot` (heavies, miniguns, Tombstone)
  get `calm = true` in the roster (`tools/roster/gen_roster.py`, `CALM_CLASSES`), so they skip the engaged flag and only join the fight when they notice
  the hero. That was a workaround for crashes that, we found later, were really caused by missing dependency assets (see Bosses).
  Whether heavies can now safely spawn engaged is untested; it likely explains why heavies often stand around.
- Awareness: `LineOfSightAwareness` config (damage events move a target from `kConfirming` to `kConfirmed`). `damage.cpp`
  sends a real damage message through the game's pipeline so an enemy reacts like to a hit. **[verified]**
- Bot groups: `Group::Alert` exe+496b40 `(group, alerter handle, flag)` sends `GroupAlertedEvent` to members. The
  generic event system is `SendEvent` exe+16d5e80 `(sys, type_hash, target, &source, 1, 0, 0, 0, 1, f, f, payload, size)` where the
  type hash is Insomniac's CRC32 of the class name. **[RE]**
- Insomniac's AI uses attack tokens and positioning, so more bodies mostly means more *ranged* pressure, not more melee.
- Enemies fight each other when hit by each other's attacks ("infighting"). We let it happen, don't count those kills.
- Known annoyances: enemies spawned onto cage/wall edges get stuck (cages, endless falling animation); `director.cpp` has
  the stationary and never-left-spawn rules to relocate them.
- Time/difficulty facts from stress tests: update rate 115/s baseline, 80/s at 80 enemies, ~60/s at 130-160; the game hung between
  160 and 170 enemies once (a per-component update loop at `exe+1d6cc90`), hence `horde_max = 80`.

## Health and damage

- `Health::SetHealth(health component*, float)` exe+1f16fc0 clamps to `[0, max]` and stores at `+0x88`. The mod raises max
  first and then sets health (used for Hammerhead's 1.8x and for god mode). **[verified]**
- Damage pipeline (as melee uses it, exe+69be20): `msg = DmgCreate(&DmgSys, &target_handle, &pos, &vec, &dir, -1)` (exe+1eb6e40),
  fill fields (`+0x138` attacker handle, `+0x144` amount), `DmgCommit(&DmgSys)` (exe+1eb7c60) dispatches. `Health::OnDamage` is
  exe+1f15c30 (vtable +0x58). **[verified]**
- Hero: `HeroHealth::OnDamage` exe+82cf70. The mod hooks it for telemetry: attacker is read from `msg+0x138`, damage from `+0x144`.
- A bot with health 0 is dead; its entity stays (a corpse) until despawned. Spawns with 0 health (some bosses) act dead, so the
  director tops them up first. **[verified]**
- The pre-death slow motion in the main game and the OVERWHELMED banner: the mod slows time on death
  (see [Time](#time-camera-and-input)).

## Bosses

Details and per-boss results: [BUGS.md](../EndlessArena/docs/BUGS.md) and the roster table in [ENDLESS_MODE.md](../EndlessArena/docs/ENDLESS_MODE.md).

- **Root cause of the early boss crashes, missing heads and missing movers (2026-10-13):** the spawner waited only for the body asset.
  Heavy actors depend on more assets, notably a separate **head actor** (`HeadActor`, for example `characters/npc/npc_hammerhead/heads/...head.actor`),
  and the movement components only come together once everything is streamed in. Fix: `spawn::Preload` loads each boss and its head
  at run start and the first spawn waits for it. **[verified]**
- Before that was understood, `BotStateGotoLocal` init (exe+1dd6660) was seen dereferencing a null `MoverStandard`. `guards.cpp`
  still patches in a null check as a backstop. **[verified]**
- Kingpin's `BossHealth` layout differs (`+0x90` max / `+0x98` current versus `+0x80/+0x88`), and he takes no damage outside his story phases: benched.
- Story gates: Hammerhead cuts off at 55% of his health in the story, so the mod gives him x1.8 health to restore the fight length. Some
  fights rely on scripted setpieces and can't be fought standalone (Kingpin: benched).
- The director has a **boss watchdog**: a boss that never loses health and never moves for 40 s is removed and skipped.
- Verified standalone bosses: Taskmaster, Silver Sable, Tombstone, Tombstone (Chained), Mister Negative, Hammerhead.

## UI (Scaleform)

The HUD is a Scaleform (Flash/ActionScript) movie. We drive it from code the way the game's own script actions do.
Full chronology, with every recording: [UI_HUNT.md](UI_HUNT.md). Summary of what works:

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

## Inventory, suit powers and mods

- Perks, suit mods, suit powers and gear are **inventory items keyed by their config asset id**. `GameInventoryManager`
  (vtable exe+38e3148) routes each item to a sub-inventory. Vfuncs: 16 (exe+1f5b720) `Add(mgr, &result, asset_id, count)`;
  19 (exe+1f5b9d0) `Remove`; 18 (exe+1f5b880) probably a count query; 45 (exe+bbf790) `CanAdd`. **[RE/verified for Add/Remove]**
- The suit loadout slots (power = slot 1, mods = slots 2..4) can be emptied and locked; `runloadout.cpp` saves, empties, restores. A
  hardware write watchpoint on the loadout block found the code that equips mods. **[verified]**
- **Item text** (names/descriptions come from the game; the mod never hard-codes them): item data record -> name hash `+0x24`,
  description hash `+0x34`, format argument `+0x38`, icon `char* +0x48`, preview video config `char* +0xA0`, preview image `char* +0xB8`,
  category `+0x1D0`. Localization lookup `exe+1749970(hash, 0)` returns a `char*`; formatting `exe+1bc1b10` (snprintf-like). Item
  manager at `exe+70957c0`. `inv text <id>` prints both.
- Suit powers and mods do nothing useful in an arena include: Quips, Sound Of Silence, Combat Analyzer, Proximity Sensor, Long Range
  Scanner, Silent Step, Bio Scanner (see the curation notes in LEDGER).

## World and arena

- **Ryker's Island** is a piece of the open world. The arena is its yard: the mod teleports the hero there (`arena::Teleport`), keeps
  them inside an invisible fence, and uses the 139 designer placements from `GP_A2_PRISON_EXT_GAMEPLAY.zone` as spawn points
  (`rykers_points.h`). The Raft interior (zone streaming via `ZoneAssetManager`) was tried; see `arena.cpp`.
- **Dock circles** in Manhattan are drawn with a spawned UI marker actor (`mis_ui_base_replayreticle`); the mod itself does the "standing on the circle" logic.
- Mission identifiers seen in community tools (the old Debug Menu mod): `GP_A2_PRISON` (the prison-break mission; the yard we use),
  `GP_A2_BLACKOUT`, `GP_A3_NYPD`, `OM_BASE_FISK_GREENWICH`, `OM_DEMONS_STORY_LI`, boss fights `GP_A1_FISK`, `VN_ELECTRO_FIGHT`,
  `VN_SCORPION_02`, `VN_TOMBSTONE_FIGHT`, `GP_A3_OTTOFIGHT`. Zone and level paths we actually use are in `arena.cpp`.
- The engine's own `ProximitySpawnSystem` (what Spider-Man 2's Chaos City uses) exists in this build with natives resolved in `natives.cpp` (`Prox*`); the mod does not use it (it needs a config and has no AI control we wanted).

## Crash and hang catalogue

| Symptom | Cause | Fix |
|---|---|---|
| Street grenadier (`enm_grenadierthug`): stack overflow in `GrenadierBotAiming` (exe+43fc00) | unknown; only this unit uses the class | removed from the street faction |
| Boss spawn crash at `exe+1dd6660` | `BotStateGotoLocal` null `MoverStandard`; root cause was dependency assets (heads) not loaded | `spawn::Preload` + `guards.cpp` null check |
| Boss acts dead | spawned with 0 health | director tops up health at spawn |
| Heavies (SlayerBot) crash when spawned engaged (null mover, exe+1fe51b5) | same dependency root cause (likely) | they spawn calm (`CALM_CLASSES`) |
| Mid-run asset release: bodiless enemies, crash `exe+15f9bc9` | released a model in use | never release mid-run |
| Respawn after a long Horde run hangs on load | too many assets held | type budget + release 30 s after the run |
| Crash at death in a Horde run, `exe+319b6a9` | GFx teardown walked a dangling managed Value | persistent heap values in `gfx.cpp` |
| Hang at 160-170 enemies | per-component update loop `exe+1d6cc90` | `horde_max = 80` |
| Hang during a Horde run, 2026-10-16 (one occurrence) | main thread waiting on a lock inside `TargetQuerySystem::ProcessJob`, worker threads idle | **[?]** record only |
| White cubes / floating heads | model capacity or missing dependency actors | dependency preloads, type budget |
| Screen stays black after the exit circle (once) | fade-in not applied | **[?]** record only |

## Things we deliberately don't know yet

See [`EndlessArena/docs/issues/`](../EndlessArena/docs/issues/) for the open questions drafted as issues, and [BUGS.md](../EndlessArena/docs/BUGS.md) for raw records.

## Boss phases (2026-10-19, disassembly; not yet run in game)
- A boss's health component (`BossHealth`, vtable `exe+3839d38`) splits its health into phases: `+0x100` phase count, `+0x110` current phase,
  `+0x114` last announced phase, `+0x80` max, `+0x88` current, `+0xac` bit 0 a minimum-health flag (`+0x84`), phase fractions of max at `([+0xb8 + 0x40])[i]`.
- `exe+32efb0 (this, phase) -> float`: the floor of a phase = max * (sum of the fractions of the later phases) (raised to `+0x84` plus a margin when the flag is set).
  `exe+32e7c0` is its damage handler (after the base handler it announces a phase end when health sits on the floor: event `0x29155b1`).
- `exe+32f060 (this, phase)`: SetPhase. Clamps to `[0, count-1]`, announces `BossPhaseChangeEvent` (`0x3b239d23`), and keeps health inside the new phase's band.
  In the story a mission script (`BossPhaseScriptAction` / `BossHealthAction`) calls it; outside a mission nobody does, which is why bosses stop taking damage at a
  floor (Hammerhead 55%, Taskmaster and Sable 1 HP) and why Kingpin took none at all (`HealthPhaseValues` and `PhaseEndBlockConfig` in his actor).
- The mod now logs each boss's health components and phases at spawn and, for Kingpin (or every boss with `[secret] phase_all=1`), calls SetPhase to the next phase
  when he is pinned at his floor. Kingpin's component may not be the `BotHealth`/`+0x88` one the others use; `entity::HealthComponent` now prefers a `BossHealth`-vtable component over a name match.

## Lighting, atmospheres and the time of day (2026-11-09; found for Manhattan Skies, details in `ManhattanSkies/docs/RECON.md`)

All `[V]` = verified in game. Addresses are `exe+RVA`.

- **Seven lighting modes** [V]: index 0 Day, 2 Sunset, 5 Night, 1 Night variant, 3 and 6 Overcast, 4 Early Morning (the game's `kBlackout`; the opening mission's dawn).
  The setter is `exe+1f7f9e0(world+0xe10d0, index)`, the world getter `exe+1f707e0`, the current mode `world+0xe2040`. The game's own Settings option only offers Day,
  Sunset and Night. Each mode swaps a base **atmosphere** (sky, sun, reflected light, exposure, fog) and a world "levellight". The lit windows and street lamps follow
  the mode, not the atmosphere: nothing we tried (overlay zones, levellight headers, atmosphere fields, a Day/Night 0/1 switch at `exe+728390c`) changes them alone.
- **The game sets its remembered mode while a world loads** [V]: the setter is called with the mode field still unset (`-1`), once per load (story missions too, which is why a
  mode forced there applies to missions). **Never call the setter later during a load**: a call 3 s after the field became readable hung the loading screen for good.
  Swapping the index inside the game's own load-time call works with no transition.
- **Atmosphere pool** [V]: manager `exe+60430c0`; `AtmosphereAdjust` `exe+162f020(mgr, slot, delta, mask, min, max)` adds weights (base = 2; the map screen adds 1000);
  recompute `exe+162ff10(mgr)`; slots are 0x8c0 bytes each. Load any of the **124** atmospheres in the toc with `exe+1626380(manager, id, ...)`, release a slot with
  `exe+1624e80(manager, slot)`. Slot fields: refcount dword `+4`, asset id `+8`, sun block `+0x2a8..0x2bc`, fog distance `+0x588..0x594`, fog density `+0x598..0x5ac`,
  fog colour `+0x5b4`, haze `+0x5c0`, exposure `+0x484`, colour filter `+0x668/66c/670`, clouds `+0x81c`. `+0x47c/+0x480` give black or white screens: leave alone.
  The game's own activation of the next base atmosphere (`caller exe+1a1b59e`, delta > 0) must be swallowed while an override is on.
- **Rain** [?]: not located. Lighting-mode names (`kOvercastRain`, `kNightRain`) suggest it belongs to the mode; third-party atmosphere mods also change rain, so pairs can't be told apart on a modded install.

## The pause menu and mod tabs in Settings (2026-11-08; `ModMenu/`, `ManhattanSkies/docs/RECON.md` Parts 5-9)

- The Settings tab method is SpidyVR's (see `THIRD_PARTY_NOTICES.md`): `OpenOptions` `exe+7dd9f0`, tab builder `exe+807740`, AddItem `exe+80a220`, callback dispatcher
  `exe+7dc400`, text lookups `exe+1749970` / `exe+1749ae0`. `ModMenu/` appends one tab per mod after the game's own tabs on the Settings screen (13 hooks, host chosen through a named shared object). A separate MODS entry on the pause menu's first page was built and verified too, then dropped on 2026-11-09 in favour of Settings tabs (`kModsPageItem` in `modmenu.cpp`).
- The Options movie takes the tab-list banner from `_locStrings.PAUSE_OPTION_SETTINGS` (it stays "SETTINGS") and the in-tab header from the tab's `menu_header`.
- The pause menu **closes itself on a lighting change** by sending Flash `CloseMenu` from `exe+7dc06b`; swallowing that call for a few seconds keeps the menu open.
- Scaleform `GfxValue` / ObjectInterface vtable slots: 2 Release, 5 SetMember, 15 PushBack.

## Fable session 2026-11-15: the game describes itself (reflection tables, events, component types)

All of this was **read from the exe on disk** (`[RE]`: nothing here was run in game). The tool is `tools/sigtools/fx.py` (commands in its header);
the dumps it writes go to `recon/*_4.0630.txt` (generated: regenerate rather than edit; they hold names and offsets only, no game bytes).

### DDL type descriptors: 9,716 reflected types with field names, types and runtime offsets
Every "DDL" type (each `*Event`, every config type such as `BotConfigMovement`, each component's `*Prius` data, script-action data) has a
**static descriptor in `.data`**:

```
+0x00 char* name            +0x08 u32 type hash (CRC-32 of the name)      +0x10 u32 base type hash    +0x14 u32 size of the runtime struct
+0x18 u32 field count       +0x48 u32* field name hashes   +0x50 char** field names   +0x58 u8* field kinds
+0x70 u32* array counts     +0x78 u32* field offsets (in the runtime struct)          +0x80 u32* field type hashes   +0x98 char** display names
kinds: 0 u8, 1 u16, 2 u32, 3 u64, 4 i8, 5 i16, 6 i32, 7 i64, 8 float, 9 double, 0xa String, 0xb enum, 0xc flags, 0xd struct, 0xf Boolean, 0x10 File,
       0x11 Tuid, 0x12 Json, 0x14 AssetId
```

- `fx.py ddl <regex>` prints matching types; `fx.py ddls recon/ddl_types_4.0630.txt` writes all of them. Example: `fx.py ddl "^DamageRequest$"`.
- The offsets are those of the **runtime object** a config or event is parsed into, so a config can be edited as data *and* read or patched live.
- A class's descriptor is returned by its vtable slot 7 (`lea rax, [descriptor]; ret`); slot 0 returns the name, slot 8 is `IsA(hash)`.

### Enums: 3,808 static enum descriptors
`{u32 type hash, u32 count, ..., +0x18 u32* value hashes, +0x20 char** value names}` in `.rdata` (a pointer table to them sits in `.data`).
`fx.py enum <regex>` (matches value or type names), `fx.py enums recon/enums_4.0630.txt`. Most enum *type* names are not in the exe; the dump lists
which DDL fields use each enum ("used by"), which names it in practice. Enum value hashes and type hashes are the same CRC-32.

### The name hash
Insomniac CRC-32: polynomial `0xEDB88320`, **initial value `0xEDB88320`, no final xor** (`fx.py h <name>`; `fx.py hn <hex>` looks a hash up among
every string in the exe). It is the id of events, DDL types and fields, enum values, script-node pins, mission names, message payload verbs.
In code the hasher is `exe+1bb88d0(const char*, 0xedb88320)`.

### Component and script-action type objects: 4,359 of them
Each component class registers a **type object** in `.data` (`recon/component_types_4.0630.txt`: `<type object RVA> <name> <factory...>`), e.g.
`HeroAbilityLock exe+6daad90`, `HeroLocal exe+6da9d90`, `HeroCameraManager exe+6dd6030`, `HeroMoverManager exe+6dd6c70`, `HeroWeaponManager exe+6dd8370`,
`BotCommandGoto exe+7bf6840`. Lookup by type, as the game does everywhere:

```
comp = word[entity+0x88] != 0 ? exe+16799a0(entity+0x80, &type) : exe+167ab40(entity+0x58, &type)     // null if the entity has none
```

`fx.py` annotates these addresses in disassembly as `type:<Name>`, and `fx.py x <type rva>` lists every function that looks the component up
(that is how the 173 users of `HeroAbilityLock` were found).

### Events: the whole catalogue, and how handlers are registered
`tools/sigtools/events.py recon/events_4.0630.txt` lists **3,061 events**: id, name, payload size and fields (from the DDL descriptor), the
functions that send it and the handlers systems register for it.

- Send: `SendEvent exe+16d5e80(&EventSys /*exe+7475690*/, u32 id, u32 target handle (0 = broadcast), source, 0, 0, 0, 0, 1, f, f, payload, size)`.
  The event object is an `EventBase` (0x138 bytes: `+0x28 Frame`, `+0x2c SenderHandle`, `+0x30 TrackedNameHash`, `+0x40 Orientation`...) followed by
  the event's own fields from `+0x138`. `EventSys`: id -> index map at `+0x291e0`, type records (0x28 bytes) at `+0x11d8`, count `+0x291d8`.
- Listen (what the game's systems do): `exe+198f160 AddEventHandler(listener, u32 id, void (*handler)(void* owner, EventBase* ev))`.
  `listener` is the system's own hidden listener object (`ResolveRef exe+16798f0(&system+0xc)`), which keeps a map "event handlers hash" at
  `+0x48` (event index -> handler) and calls `handler(q[listener+0x78], event)` from the dispatcher `exe+198f2f0`. **One handler per event per
  listener**: registering a second one for the same id on the same listener replaces the first, so a mod that borrows a system's listener must
  only add ids that system does not use (the catalogue shows who uses what) or chain to the old pointer. Not tried from a mod yet: hooking
  `SendEvent` stays the proven route.
- Components receive events through their own virtual handlers (a `cmp reg, <id>` chain); `fx.py imm <id>` finds those.

### Entity messages ("synced actor calls"): 74
`MsgRegister exe+21423d0(&type, handler, "name")`, `MsgSend exe+2142440(1, type, target handle, arg, &payload, size)` (the screen fade, hero
lock, halt traversal...). Table: `recon/messages_4.0630.txt`.

### Script-node pins
A nodegraph action's execute method (vtable slot 10) receives the triggered input pin's hash in `edx`, reads inputs with
`exe+1923410/1923490/19234e0(node+0x58 ctx, &out, pinHash)` and fires outputs with `exe+1923a90(ctx, pinHash)`. Resolving the hashes
(`fx.py hn`) gives the pin names; the node's data fields are in the DDL dump under `<Action>Prius`.

### More from the same session (pointers; all `[RE]`, read from the exe, not run)
- **World singleton** `exe+1f707e0()`: overlays `+0xe2060` (records `[+0xe2070]`, 0x38 bytes, count `+0xe2078`; load `exe+1f73580(world, &id)`, unload `exe+1f735a0`), checkpoint table
  `+0xe0900` / `[+0xe08f0]`, loading state `dword +0xe3cb0` (0x17..0x19 while loading), lighting object `+0xe10d0`.
- **Level/checkpoint loader**: `gm = exe+1ef6560()` (`GameManager`), `gm->vtable[+0xf8](gm, levelAssetId, crc32(checkpoint), delay)` = `exe+1ef4ca0` (`docs/MISC_RECON.md` section 2).
- **Mission state**: `exe+201f640` / `exe+201f800` on `q[exe+7c4bbb0]`, or `exe+1fa74f0` on `q[exe+7c3ef70]`; 0 Inactive, 1 Available, 2 Active, 3 Cleanup, 4 Complete, 5 Failed.
- **Hero**: `HeroAbilityLock` (40 abilities, 4 layers), `HeroMacroState` (+0x4c: kOnGround 0x2, kInAir 0x4...), `WeaponManagerBase::Equip exe+2162300` (five arguments), gadget ammo,
  the wheel list, the damage message: `EndlessArena/docs/RECON.md` sections 1-9. Death -> `GameManagerCampaign::OnHeroDied exe+722750` -> `HeroRespawn` component (MISC_RECON section 7).
- **Crime director**: `RandomCrimeSystem2` at `exe+707a500` (MISC_RECON section 1). **Chase system, 1,032 script nodes with pins**: MISC_RECON sections 3 and 5, `recon/script_nodes_4.0630.txt`.
- **Lighting**: modes 0 kDay, 1 kNight, 2 kSunset, 3 kOvercast, 4 kBlackout, 5 kNightRain, 6 kOvercastRain; the switch's load screen is `DynamicLoadScreenSystem` handler `exe+710970`
  (`ManhattanSkies/docs/RECON.md` Part 18); atmosphere fields by name: Part 19.
- **Crash dumps**: `tools/sigtools/dumpctx.py` (real faulting registers and stack), `dumpindex.py` (one line per dump). A heap address in an object's vtable slot = the object was freed.

## Input: the game's action layer, for every device (Fable session 2026-11-15; `[RE]`, read from the exe, not run)

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
family is stored was not located. `OSKeyboard` (vtable `exe+3cf8518`) and a Steam `GamepadTextInputDismissed` callback exist: the game can open an on-screen keyboard (MSC-D7 lead).

Not examined: the pad backends' axis storage and dead zones (FOF-A3), per-user slots in `nx::NxInputImpl` (78 methods; FOF-A4), where bindings are saved (FOF-A6).

## Archives, the toc object and texture streaming (Fable session 2026-11-15; `[RE]`, not run)

- **File system**: `fs = q[exe+74a6a00]` (`FileSystem::ArchiveFileSystem`, vtable `exe+3cf9920`). `fs+0x10` = toc DAT1 buffer, **`fs+0x18` = the toc object** (section pointers set by
  `exe+16d7710`), `q[fs+0x40]` = location table (8 bytes per entry: archive index, offset), `q[fs+0x68]` = per-archive open table (0x200 archives max). No heap scan is needed.
- **Reads are by id**: `exe+16d83e0(request, assetId, kind)` -> `fs->vtable[+0x28]` (`exe+16da4c0`) -> `exe+16d7990(toc, id, languageGroup, kind)`; `size = dword[entry+4]`,
  `location = q[q[fs+0x40] + dword[entry+8]*8]`; the archive file is opened on demand (`exe+16da380`). Kind 0 = the asset, kind 1 = a texture's pixel stream.
- **Textures**: `TextureManager` static `exe+621b1a0`, records 0xc0 bytes at `q[exe+621b2b0]` (`+0x34` highMipSize, `+0x84/+0x86` height/width, `+0x8e/+0x8f` mip nibbles);
  `TextureStream` static `exe+621b4e0`, tick `exe+19b6d70`, request one texture `exe+19b5d70(stream, record, 0)`, evict one `exe+19b5e70(stream, record, idx)`, staging buffer
  `exe+19b7f30`, finish job `exe+19b5a20`. Runtime textures from memory: `exe+19b1500` (2D), `exe+19b1280` (3D).
