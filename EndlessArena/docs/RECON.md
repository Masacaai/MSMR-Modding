# Endless Arena recon: answers to the Fable requests (2026-11-15)

Answers to the `EA-*` requests in [`docs/FABLE_REQUESTS.md`](../../docs/FABLE_REQUESTS.md). Game build **4.0630.0.0 only**; addresses are
`exe+RVA`. Tags: **[V]** read from the exe's code or data (static analysis; "verified" here means *read*, not *run*: nothing in this
file was exercised in game unless it says "live"), **[I]** inference, **[?]** unknown. Tools used are in `tools/sigtools` (`fx.py`
is the one that produced almost everything: `fx.py d <rva>` disassembles a function with names).

Shared facts that every mod can use (the component type table, the enum dump, the event catalogue, the message system) are in
[`docs/GAME_INTERNALS.md`](../../docs/GAME_INTERNALS.md), section "Fable session 2026-11-15".

## 1. `HeroAbilityLock`: the full ability table, the layers, the game's own API (EA-B1)

### 1.1 The 40 abilities [V]

The component's name getter (vtable `+0x88`, `exe+84b4a0`) reads a **static enum descriptor** (`exe+393a250`, pointer at
`exe+5e113f0`; type hash `0x41032d7a`, 40 values). The index into the lock bytes is the enum index (the debug draw at
`exe+1e8a730` walks 0..127 and prints this name). Indices 40..127 are unused.

| # | name | # | name | # | name | # | name |
|---|---|---|---|---|---|---|---|
| 0 | kDeath | 10 | kTraversalDodge | 20 | kWallRunActionExit | 30 | kForceReturn |
| 1 | kBodyReact | 11 | kLedge | 21 | **kWebZip** | 31 | kAimMode |
| 2 | kInteract | 12 | kClimb | 22 | kWebSnap | 32 | kVehicleAttach |
| 3 | **kMelee** | 13 | kZoom | 23 | kWallCrawl | 33 | kNonCustomParkour |
| 4 | kMove | 14 | kVault | 24 | **kWebSwing** | 34 | kRemoteHack |
| 5 | kJump | 15 | **kShoot** | 25 | kZipJump | 35 | **kZipToSurface** |
| 6 | kLongJump | 16 | kRun | 26 | kPerch | 36 | kAirTrick |
| 7 | kRoll | 17 | kSprint | 27 | kBeam | 37 | kIdleFidget |
| 8 | kRollShort | 18 | kWallRun | 28 | **kPointLaunch** | 38 | **kGadgetWheel** |
| 9 | kRollLong | 19 | kWallRunLeap | 29 | kFreeRun | 39 | kChargeJump |

This corrects the live bisect in the request: 0..3 are death, hit reactions, interact and melee (not "attacks and shooting"); 4 is
`kMove`, 5 is `kJump`; **web-swinging is 24**, web zip 21, the web shooter (and every gadget shot) is **15 `kShoot`**, the gadget
wheel is 38. There is **no ability for web strike, web throw or yank**: those are combat moves, see 1.5.

### 1.2 Layout and methods [V]

Component type object `exe+6daad90` (pass it to `FindComponent`, section 1.6). Class `Hero::HeroAbilityLock` (vtable `exe+38aa530`)
derives from `IndexLockComponent` (vtable `exe+38aa488`) and only overrides the name getter and type.

- `+0x48`: 16 **timed lock** records of 0x18 bytes: `u32 mask[4]` (128 ability bits), `float expiry` (`+0x10`, compared with the
  double at `exe+7a7fbd0` = current time), `u8 layer` (`+0x14`).
- `+0x1c8`: `u8 counter[4 layers][128 abilities]` (`comp + 0x1c8 + layer*0x80 + ability`).

| vtable | RVA | meaning |
|---|---|---|
| `+0x50` | `exe+1e8a490` | `Lock(this, int ability, int layer, float seconds)`. `seconds == 0`: counter lock (below). `seconds > 0`: timed lock in a free record (expiry = now + seconds); needs no unlock. |
| `+0x58` | `exe+1e8a640` | `Unlock(this, int ability, int layer)`: decrement (layer 2) or clear (other layers) |
| `+0x60` | `exe+1e8ab30` | `Clear(this, int ability, int layer)`: set the byte to 0; `layer == 5` clears all four layers of that ability |
| `+0x68` | `exe+1e8a6b0` | `ClearAll(this)`: zeroes `+0x48..+0x3c8` (timers and counters) |
| `+0x70` | `exe+1e8a6d0` | `bool IsLocked(this, int ability)`: any layer's byte > 0, or a timed lock active (`exe+1e8ad60`) |
| `+0x78` | `exe+1e8a730` | debug draw (prints the locked abilities) |
| `+0x88` | `exe+84b4a0` | `const char* Name(this, int ability)` |
| `+0x90` | `exe+1e8a860` | `bool IsCountedLayer(int layer)`: returns `layer == 2` |
| `+0x98` | `exe+1e8a870` | `bool IsLockedByCounter(this, int ability)` (ignores timers) |

Helpers that take a 128-bit ability set (`u32[4]`):

- `exe+1e8ab20(mask)` zero; `exe+1e8a970(mask, ability)` set bit; `exe+1e8b2f0(mask, ability)` clear bit; `exe+1e8b080(mask, ability)` test bit;
  `exe+1e8a990(mask, const int* ids, int n)` set from a list.
- `exe+1e8b240 LockSet(lockComp, mask, 4, int layer, bool invert)`: calls `Lock(ability, layer, 0)` for every set bit (or every clear bit if `invert`).
- `exe+1e8b520 UnlockSet(lockComp, mask, 4, int layer, bool invert)`.

### 1.3 The four layers [V from the 173 call sites that reference the type object]

| layer | kind | who uses it |
|---|---|---|
| **0** | boolean (Lock sets 1, Unlock sets 0) | **`ToggleHeroMovesAction`** (the mission node that switches hero moves off: section 2) and `SetAllowedHeroMoves` (`exe+9189e0`): the "mission designer" layer |
| **1** | boolean | **`HeroLockAction`** (`LockHeroStand` / `UnlockHero` nodes) |
| **2** | **reference count** | gameplay code: hero states (`HeroStateHelicopter`: kVehicleAttach + kWebZip; `HeroStateLookAtPOILocal`: kShoot + kGadgetWheel + kAimMode; `HeroStateJumpDismountLocal`: kWebSwing + kWebZip; `StatusEffectEMP`: kShoot; the photo gadget; `HeroStateWebYankStruggleLocal`: kGadgetWheel ...). Every Lock needs its Unlock. |
| **3** | boolean | hero states locking `kBodyReact`/`kDeath` (hit reacts, interact, takedown victim, wall-crawl transitions) |

So a mod should use **layer 0 or 1** (boolean, idempotent: no counting mistakes) and must not touch layer 2 (it would unbalance the
game's own counts). Layer 0 collides with a mission's own `ToggleHeroMovesAction`; in the arena no mission node runs, so either is free.
Nothing in the component is saved: it lives on the hero entity, so a respawn that recreates the hero gives a clean component
[I: no serialization function in the vtable; the `ResetCharacterDefaults` pin exists precisely to restore defaults].

### 1.4 What each lock does [V for the ones listed]

Abilities are *queried*, not pushed: code that wants to start something asks `IsLocked`. Known askers: `HeroWeaponManager`
slots 33/35 (`exe+9a3fd0`, `exe+9a4040`) ask `kShoot` (so **kShoot blocks the web shooter and every gadget shot**), the gadget-wheel HUD
(`PlayerHudQuickSelect` update `exe+793eb0`) asks `kGadgetWheel`, the suit-power HUD (`exe+797720`) asks `kMove`, `HeroStateRunLocal`
asks `kLongJump`, traversal (`exe+8e7540`) asks `kWallCrawl` and `kWebZip`. Locking `kShoot` also makes the weapon manager cancel
its current aim (`HeroWeaponManager` vtable `+0xc8`, called by the lock helpers when bit 15 is in the set).

### 1.5 Combat moves have their own locker [V names, I details]

`RestrictHeroCombatAction` (node; execute `exe+9b1780`; pins `Enable` / `Disable` / `ResetCharacterDefaults`, outputs `Enabled` /
`Disabled` / `OnReset`) drives the component **`HeroCombatActionLocker`** (type object `exe+6dcc950`, class
`Hero::HeroCombatActionLocker`, vtable `exe+38ae8f0`; vtable `+0x60`/`+0x68` lock and unlock an action id, `exe+8bce80`/`exe+8bcfc0` are
the per-move calls). That is where web strike, yank, throw and finishers are switched off by missions. Its move-id enum is the next
thing to dump for the web-fluid curse [?: not done; start at `exe+9b1780`].

### 1.6 Recipe: lock only the web abilities [I, built from the game's own calls; not run]

```
hero  = GetActor(hero handle)
lock  = FindComponent(hero, exe+6daad90)        // exe+16799a0(hero+0x80, type) if word[hero+0x88] != 0 else exe+167ab40(hero+0x58, type)
for a in {15 kShoot, 21 kWebZip, 22 kWebSnap, 24 kWebSwing, 25 kZipJump, 28 kPointLaunch, 35 kZipToSurface, 38 kGadgetWheel}:
    lock->vtable[+0x50](lock, a, /*layer*/ 0, 0.0f)       // undo: vtable[+0x58](lock, a, 0)
```

or in one call, exactly what the mission node does: `exe+9189e0 SetAllowedHeroMoves(heroEntity, const u32 allowed[4])` (unlocks the
allowed ones and locks the rest **of the 27 toggleable abilities** on layer 0, then tells the weapon manager, the EMP tracker and the mover).
The toggleable list (static, `exe+38b21b0`): kMove, kSprint, kJump, kLongJump, kVault, kMelee, kShoot, kRoll, kRollShort, kRollLong, kWebSwing,
kWebZip, kWallCrawl, kWallRun, kWallRunLeap, kZipJump, kPerch, kBeam, kPointLaunch, kFreeRun, kAimMode, kNonCustomParkour, kRemoteHack,
kZipToSurface, kIdleFidget, kGadgetWheel, kChargeJump.

To keep the gadget wheel but forbid web gadgets, leave 38 unlocked and hide the web gadgets in the table instead (the existing hiding trick);
`kShoot` is all-or-nothing (it gates the weapon manager, not a gadget). A softer curse: a timed lock (`seconds > 0`) re-armed each second
needs no cleanup after a crash or respawn.

## 2. How missions take moves away: `ToggleHeroMovesAction` and `HeroLockAction` (EA-B2, static part)

Both are nodegraph actions (script nodes). Pin names are CRC-32 hashes resolved from the exe's strings.

**`ToggleHeroMovesAction`** (vtable `exe+38b8230`, type object in `recon/component_types_4.0630.txt`, execute `exe+9b8020`) [V]:

- Node data: `+0x60` a 128-bit ability set (the node's list of moves), `+0x70` mode byte (0 = toggle by the bool input, 1 = "these are the
  allowed moves"), `+0x71` a flag that also toggles `TrackingSenseComponent` sense 8 (`exe+8e3c50`/`exe+8e3990`).
- Input pin `Target` (the hero), bool pin `Val`; input `ResetCharacterDefaults` (hash `0x518ca14f`) restores the **character's default set**
  (`HeroCharacterManager` (type `exe+6dd6130`) -> current character record (`exe+9175d0`) -> `exe+917100(record+0x28, mask)` builds the default
  mask from one bool per move; this is how Miles and MJ get their reduced move sets) through `exe+9189e0`. Outputs `Out`, `OnReset`.
- Mode 0: `Val` true -> `UnlockSet(lock, set, 4, layer 0)`, false -> `LockSet(lock, set, 4, layer 0)`; if the set has kShoot the weapon
  manager is told (vtable `+0xc8`), if it has kSprint the `HeroMoverManager` (`exe+9444c0(mgr, 3 or 0)`).

**`HeroLockAction`** (vtable `exe+38b6ab0`, execute `exe+9afdc0`) sends the message `HeroLockProcessHandle` (message type `exe+6dd95c8`,
handler `exe+9b00f0`) to the hero with the input pin's hash as payload [V]:

| input pin | hash | effect |
|---|---|---|
| `LockHeroInput` | `0xbe6ca2f6` | `exe+83c350(HeroLocal, 0)`: input off (`HeroLocal+0x148 = 0`) |
| `LockHeroStand` | `0x02fd8897` | lock (character default set + kMove, minus kDeath and kBodyReact) on **layer 1** |
| `LockCameraInput` | `0xc07f559b` | `exe+90db30(HeroCameraManager)` |
| `UnlockCamera` | `0x14b6ea69` | `exe+90cf30(HeroCameraManager)` |
| `UnlockHero` | `0xf6fd8e51` | `exe+83c350(HeroLocal, 1)`, `exe+83c440(HeroLocal, 1)`, unlock the same set on layer 1 |

Outputs `OutLock` / `OutUnlock`. Related nodes: `HaltTraversalAction` (message `HaltTraversalActionHaltTraversalHandle`, handler `exe+9bf920`),
`RestrictHeroCombatAction` (1.5), `HeroLockControlsEvent`.

**Which node the Harry's research-station mission uses [?]:** not looked up (it needs the mission's nodegraph data). The call it ends in is
one of the two above; `ToggleHeroMovesAction` with a set containing kWebSwing/kWebZip/kShoot is the obvious candidate [I].

## 3. The damage message is the `DamageRequest` event; full layout (EA-C2)

[V] The object `DmgCreate` returns is a `DamageRequest` (type hash `0x219aab06`, base `EventBase`, size 0x1a8). The two offsets the mod
already uses (`+0x138` attacker, `+0x144` amount) are its `Damager` and `Amount` fields, which confirms the table:

| offset | field | type | note |
|---|---|---|---|
| `+0x2c` | SenderHandle | u32 | EventBase |
| `+0x138` | **Damager** | u32 actor handle | who deals it (the kill credit) |
| `+0x13c` | **Type** | enum `DamageTypes` | 0 kNone, 1 **kMelee**, 2 kBullet, 3 kFire, 4 kExplosion, 5 kHeal, 6 **kScript**, 7 kKinetic, 8 kFalling |
| `+0x140` | **Category** | enum `DamageTypes` | same enum |
| `+0x144` | Amount | float | |
| `+0x148`, `+0x14c` | Duration, TickInterval | float | **damage over time is built in**: a request with Duration and TickInterval ticks by itself |
| `+0x150`, `+0x154` | Knockback (enum), KnockbackAmount | | |
| `+0x158` | KineticMultiplier | float | |
| `+0x15c` | ConditionDuration | float | |
| `+0x160` | RequestFlags | flags `DamageRequestFlags` | values not dumped yet [?] |
| `+0x164` | IgnoreDuration | float | |
| `+0x168` | RequestId | u32 | |
| `+0x170` | StatusData[] | `{StatusType (DamageTypes), StatusAmount, StatusDuration, StatusActionCount}` | status effects (electrified, webbed ...) |
| `+0x180` | CustomData[] | `{DataNameHash, DataBuffer}` | |
| `+0x190`, `+0x194` | ImpactImpulse, ImpactVelocity | float | |
| `+0x198` | **Weapon** | u16 | weapon index |
| `+0x19c` | **DamageHash** | u32 | hash of the damage definition (the "damage hash" the hook already logs) |
| `+0x1a0` | **WeaponNameHash** | u32 | CRC-32 of the weapon/gadget name |
| `+0x1a4` | **WeaponMacroType** | u8 | weapon family |
| `+0x1a5`, `+0x1a6` | Condition, Breakpoint | u8 | |

After the request is applied the game sends **`DamageDealtEvent`** (`0x5301d426`, to the damager's side) and, on a kill, **`DeathEvent`** (`0x2ac85d3a`) and
**`DeathDealtEvent`** (`0x7736a46f`). All three carry every `DamageRequest` field plus (size 0x2f8): `+0x1a8 Target`, `+0x1b0 HitDirection`, `+0x1e0 HitPosition`,
`+0x210 HitNormal`, `+0x240 HitRadius`, `+0x244 HitJointNameHash`, `+0x250 ResponseFlags`, **`+0x254 AmountInflicted`**, `+0x258 WeakSpotHitHash`,
**`+0x260 HealthPercent`**, `+0x26c DamagerActorFilterTypes`, `+0x270 TargetActorFilterTypes`.

How to classify (recipe, [I] until logged once in game: dump `Type`, `Category`, `Weapon`, `WeaponNameHash`, `WeaponMacroType`, `DamageHash` for one punch, one web
shot, each gadget, a suit power, a thrown object, a finisher and an enemy-on-enemy hit, then name the hashes with `python tools/sigtools/fx.py hn <hash>`):

- **hero melee** = `Damager == hero handle && Type == kMelee` (finishers and takedowns are separate hero states: exclude them by the hero's state, section 6, or by `DamageHash`);
- **gadgets and web shots** = `kBullet` / `kExplosion` / `kKinetic` with a `WeaponNameHash` (the hash names the gadget);
- **enemy on enemy** = `Damager` is not the hero; **kill credit** = `DeathDealtEvent.Damager` (sent to the killer) rather than guessing from health;
- **environment / falling** = `kFalling`, `kKinetic` with no weapon.

**Safe damage over time on the hero** [I]: send one `DamageRequest` with `Type = kScript` (6), `Amount` per tick, `Duration`, `TickInterval`: `kScript` is the type mission
scripts use, and the game applies ticks itself. To floor at 1 HP rather than kill, set the hero health component's minimum-health flag (`+0xac` bit 0 with the floor
at `+0x84`, as documented for `BossHealth` in GAME_INTERNALS: the hero's `HeroHealth` shares the base class [I, check the vtable before relying on it]).

## 4. Combat events to listen to (EA-C3, EA-C4 partly)

All ids are CRC-32 of the class name; the full list with payloads, senders and handlers is `recon/events_4.0630.txt` (generate: `python tools/sigtools/events.py out.txt`).
The ones a roguelike needs [V names and payloads; when each fires is by name]:

| event | id | payload beyond EventBase |
|---|---|---|
| `HeroPerfectDodgeEvent` | `0x3cd06219` | none |
| `HeroAccurateDodgeEvent` | `0x5f9ae3a4` | none |
| `HeroDodgeAttackerEvent` | `0xff30f0cd` | `+0x138 EngagedWithTargetHandle`, `+0x13c DodgeType`, `+0x140 DodgeAccuracy` |
| `HeroDodgeStartEvent` / `HeroDodgeEndEvent` | `0x36a3d517` / `0xd55a9e47` | |
| `HeroPerfectDodgeWindowOpenEvent` / `...CloseEvent` | `0xf2c81442` / `0x17b0c78b` | |
| `HeroActiveFinisherStartedEvent` / `...CompleteEvent` | `0x259ccc86` / `0xfbcbf4a2` | `+0x138 TargetActor` |
| `HeroComboUpdateEvent` | `0x363efce5` | `+0x138 ComboCount` |
| `HeroComboTierAdvancedEvent` / `...MaxedEvent` / `...ResetEvent` | `0x66553eda` / `0x60d7c57b` / `0xb52250df` | |
| **`HeroSpiderAssaultAddEnergyEvent`** | `0x7d6017e2` | **`+0x138 Amount` (float): the Focus gain request** |
| `HeroSpiderAssaultMeterIncreasedEvent`, `...SegmentFullEvent`, `...MeterFullEvent`, `...MeterEmptyEvent`, `...StartedEvent`, `...StoppedEvent` | see file | |
| `HeroFocusHealingStartEvent` / `...StoppedEvent` / `...UnavailableEvent` | `0x876c6ffb` / `0xfa0d252f` / `0x534fe1b3` | healing from Focus |
| `SuitPowerOnEvent` / `SuitPowerOffEvent` | `0x99b3f833` / `0x42a37129` | `+0x138 ItemCodeName` (string) |
| `SuitPowerApplyPowerEvent`, `SuitPowerNotTriggeredEvent` | `0x59479336`, `0xcbbf7cf4` | |
| `DeathEvent`, `DeathDealtEvent`, `DeathFinishedEvent` | `0x2ac85d3a`, `0x7736a46f`, `0x33b7c4b7` | section 3 |
| `BotDeathStartEvent` / `BotDeathCompleteEvent` | `0xf99b3a5d` / `0x66913b32` | |
| `BotKilledByTripMineEvent`, `BotDodgedEvent`, `BotParryImpactFrameEvent` | see file | |

**Focus gain hook (EA-C4)** [I]: Focus is added by *sending* `HeroSpiderAssaultAddEnergyEvent{Amount}` to the hero; scaling `Amount` in the existing `SendEvent` hook
(or sending an extra one for a perk) replaces the per-tick diff. The manager's handler is the place to read the per-source rules: `fx.py imm 7d6017e2`.

## 5. `EquipGadget` decompiled: it is `WeaponManagerBase::Equip`, and it takes five arguments (EA-A1, EA-A2)

[V] `exe+2162300` is vtable slot 17 (`+0x88`) of `Weapon::WeaponManagerBase` (shared by `WeaponManagerGame`, `BotWeaponManager`, `HeroWeaponManager*`: bots
equip their guns through the same code). Read from the disassembly:

```
bool Equip(this, int id /*edx*/, int slot /*r8d*/, bool force /*r9b*/, bool sendEquippedEvent /*5th argument, on the stack: [rsp+0x28] at the call*/)
    exe+2163440(this, ...)                                    // current weapon of the slot (result unused here)
    rec = Find(this, id)                                      // exe+2163500
    if (rec && rec->enabled /*byte +0x14*/ == 0 && !force) return false
    if (IsValidId(id) /*vt+0x98 = exe+5b8b60: id != 0*/ && !CanEquip(this, id) /*vt+0xa8 = exe+2162810*/) return false
    old = this->slot[slot].id                                 // dword [this + 0x6c + slot*40]
    if (old == id) return false
    elapsed = now - this->equipTime[slot]                     // float [this + 0x188 + slot*4]; now = double [exe+7a7fbd0]
    Unequip(this, slot, true)                                 // vt+0x90 = exe+2162750
    this->slot[slot].id = id;  this->equipTime[slot] = now
    if (IsValidId(id)) {
        w = WeaponOf(this, id)                                // exe+21633c0: GetActor(rec->actorHandle) -> its WeaponBase component (type exe+7c75c70)
        if (!w) { CreateWeapon(this, id) /*vt+0x70 = exe+2161ba0, 971 bytes: spawns the weapon actor*/; w = WeaponOf(this, id); if (!w) return false }   // NOTE: the slot already holds the new id here
        if (!(w->entity->flags & 1)) exe+159c9c0(w->entity)   // activate the weapon entity
        w->vt[+0x60](w)                                       // weapon OnEquipped
        if (sendEquippedEvent && !(this->flags62c & 0x10000000) && worldReady)
            SendEvent WeaponEquippedEvent (0x91fe56ac) {Actor = owner, ActorPosition, OldWeaponDuration = elapsed, OldWeaponType = old, NewWeaponType = id, EquipIndex = slot}
        SendEvent UserEquippedWeaponEvent (0xec1b83de) to the manager's own entity
            {WeaponName = w->vt[+0x1f0](), WeaponId = id, EquipIndex = slot, PrevWeaponName, PrevWeaponId = old}
    }
    this->vt[+0x150](this, slot)                              // post-equip refresh
    return true
```

Facts that explain the symptoms:

- **Five arguments.** The mod's hook and its own calls use four (`this, id, slot, force`). The fifth is read from the stack (`cmp byte [rsp+0xf0], 0` inside), so our
  calls pass **garbage** for "send `WeaponEquippedEvent`". Declare it `bool Equip(void*, int, int, bool, bool)` and pass the game's value through in the detour.
- **Id 0 is "no weapon", not the Web Shooter.** `IsValidId(id)` is literally `id != 0` (`exe+5b8b60`). `Equip(0, slot)` unequips: it sets the slot to 0, returns true and
  sends no event. That is the burst at wave start (something clears every slot) and what the wheel sends for the Web Shooter: the Web Shooter is the hero's built-in
  weapon when no gadget is equipped [I], not a table entry that gets equipped. `Unequip` (`exe+2162750(this, slot, bool holster)`) calls the old weapon's
  `vt[+0x68]`, `this->vt[+0x158](slot)`, then stores `this->vt[+0xa0]()` (the default id: 0 in the base class) and time -1.0.
- **`CanEquip` is an inventory check**: `exe+2162810` finds the owner's **`ItemInventory`** component (type object `exe+7c34790`) and asks `inventory->vt[+0x1a0](id)`; null
  means refuse. So a gadget id is an *item id in the hero's ItemInventory*; "locked" in the early story = the item is not there (EA-A6: grant or remove the item, do not
  touch the table).
- **The table**: `Find` walks `count = dword [this+0x628]` records of 0x18 bytes; the record it returns starts at `this + 0x1a8 + i*0x18`:
  `+0x0c` **weapon actor handle** (the spawned gadget entity), `+0x10` id (this is the `this+0x1b8 + i*0x18` the mod writes), `+0x14` **enabled byte** (0 = equip refused unless forced).
- **`exe+2163360(this, slot)` does not return a config**: it returns the *weapon entity* of the id in that slot (`GetActor(rec->actorHandle)`), or **null when the slot's id is
  0 or is not found in the table**. The per-frame code behind `exe+1ce9e67`/`exe+374237` dereferences that result, so **the invariant is: every non-zero id sitting in a slot
  must be findable in the table.** The hiding trick (writing 0 over a table entry's id) breaks exactly that when the hidden gadget is the one equipped: slot = 9C8x, table
  has no 9C8x, lookup null, crash at `exe+374237`. Refusing the game's own `Equip(0)` keeps a stale id in the slot with the same effect.
  **Hide with the enabled byte (`record+0x14 = 0`, i.e. `[this+0x1bc + i*0x18]`) instead of zeroing the id, and unequip the gadget (`Equip(0, slot, ...)`) before hiding it** [I, not run].
- The slot is assigned **before** the weapon actor is known to exist (see NOTE): if `CreateWeapon` fails (asset not streamed, pool full) the function returns false and leaves
  a slot id whose record has no live actor: the next per-frame lookup gets a null `WeaponBase`. This fits the three execute-at-0 crashes "at the moment of our call
  during a run" [I]. A safe caller checks first that `Find(id)` exists and `GetActor(rec->actorHandle)` is non-null (or loads the weapon), and only then equips.
- Slots: 40-byte records from `this+0x6c`, times from `this+0x188`; the gap leaves room for **7** records (0..6), although the game was seen calling slot 7 [?: read
  the caller that loops over the slots to settle the count].

**Why a gadget we unhide stays red until the player equips it [I]:** the player's equip goes through the same function but with the wheel's state behind it; the piece our
call lacks is the event/refresh pair at the end (`UserEquippedWeaponEvent` is only sent when the weapon actor exists) and the wheel's own list refresh (section 7).

## 6. Gadget ammo is an inventory item count, kept by `GadgetItemAmmoManager` (EA-A3)

[V] `GadgetItemAmmoManager` (vtable `exe+39182a8`, type object `exe+738a710`, data `GadgetItemAmmoManagerPrius{AmmoItemConfig: asset id}`): one object per ammo kind.

| offset | meaning |
|---|---|
| `+0x08` | owner entity |
| `+0x48` | u64 **asset id of the ammo item config** (`AmmoConfig{MaxReserveAmmo}` family) |
| `+0x50` | weak reference to the inventory (resolve with `exe+16798f0`; it must be a `GameInventoryManager`, type `exe+6e096f0`) |
| `+0x5c` | float: **ammo this manager has put into the inventory** (the current count) |
| `+0x60` | float: **base max ammo** |

- `exe+e478a0(this) -> float` = **effective max**: base, then the owner's `ConfigModManager` (type `exe+7c1b0c0`) modifiers with key hash `0xbf7c8c66`: kind 2 sets, kind 0
  multiplies, kind 1 adds (the `{kMultiply, kAdd, kSet}` enum of `MaxGadgetAmmoPerk{MaxAmmoMod}`: that is how upgrades and the perk raise the max).
- `exe+e47690(this)` = **refill to max**: `inventory->Remove(id, (int)current)` (inventory vtable `+0x98`), `current = 0`, `inventory->Add(id, (int)min(max, max - current))`
  (vtable `+0x80`), `current = added`. The two inventory calls are the same slots 19 and 16 the mod already uses (`inventory::Remove` / `Add`).
- The manager's update (vtable slot 7, `exe+e470f0`) recharges with `HeroGadgetConfig.GadgetRechargeData{RechargeDelay, RechargeRateInCombat, RechargeRateOutOfCombat}`
  (found through `HeroConfigManager`, type `exe+6dd6340`) and asks `CombatTracker` (type `exe+6d10990`) whether a fight is on.
- Events: `ConsumedAmmoEvent{WeaponHandle, WeaponUserHandle, WeaponConfigHash, AmmoAmount}` (`0x6b7f257f`) when a shot is spent, `AmmoPickupEvent{Instigator, Pickup, ItemConfig}`,
  `MaxGadgetAmmoPerkAddedEvent`, UI events `UIGadgetAmmoUsed` / `UIGadgetAmmoBeginFill` / `UIGadgetAmmoReady`.
- `GadgetResupplyPerk{Multiplier, GadgetType}`: enum `GadgetType` {All, ElectricWeb, ImpactWeb, WebMine, WebBomb, WebLine, ...12 values} (`fx.py enum "^GadgetType$"`).

So: **current ammo of a gadget = the inventory count of its ammo item; max = `exe+e478a0`**. Getter/setter for a mod: find the hero's `GadgetItemAmmoManager` components
(`comps`; there should be one per ammo kind [?: count not checked live]), read `+0x48` to learn which ammo item each one owns, then `Remove`/`Add` that item id and keep
`+0x5c` equal to what you added; or call `exe+e47690` for "refill". The wheel's `0/0` versus `0/4` is then "no ammo manager / max 0" versus "count 0 of max 4" [I].
`inv watch` saw nothing on firing because a shot is not an Add/Remove through the hooked entry points; hook `ConsumedAmmoEvent` instead.

## 7. The gadget wheel's list: where it comes from, why a granted gadget is red, how to refresh (EA-A5, EA-A6; FOF-B2)

[V] `GUI::PlayerHudQuickSelect` (vtable `exe+3894040`; the object at HUD root `+0x1b8`). Its functions:

| RVA | role |
|---|---|
| `exe+793320 (this, bool force)` | **RefreshList**: rebuilds the entries and sends `SetQuickSelect`; skipped when nothing changed unless `force` |
| `exe+7914a0 (this, item, entry*, heroEntity, weaponManager)` | builds one entry |
| `exe+793780` | Open (sends `OpenQuickselect`) |
| `exe+792300` | the wheel's input handling (stick to index, `SetSpinnerRotation`), calls `exe+793da0` |
| `exe+793da0` | sends `HandleWheelInput(current index)` |
| `exe+793290` | cycle to the next/previous slot (quick tap), then `exe+793da0` |
| vtable `+0x10` = `exe+793eb0` | per-frame update (2,951 bytes): asks `HeroAbilityLock::IsLocked(kGadgetWheel)`, reads `HeroButtonCache`, `HeroMacroState`, `HeroConfigManager`; starts the slow motion with `exe+19bbaa0` on `TimeScaleSystem exe+621b890` using `HeroGadgetConfig.QuickSelectData{TimeScale, ButtonHoldTime, MaxButtonTapHoldTime, MaxTimeBetweenTaps}` |
| vtable `+0x08` = `exe+793cc0` | close: releases the time scale (`exe+19bac70`) |

**RefreshList** walks the hero's `GameInventoryManager` (type `exe+6e096f0`): `equip = exe+bbfba0(inv)`; for `i < equip->vt[+0x3a0]()` (item count), `item = equip->vt[+0x1b0](i)`;
an item is listed when `dword [item+0x88] == 3` (the gadget category) and `exe+bbc3d0(equip, item) == 0`; at most 16 entries are collected.

**One entry (0x38 bytes)**, as `exe+7914a0` fills it (the item must be a `GameWeaponItem`, type `exe+6e097f0`, and pass `exe+bc3390(item)`):

| entry | source | Flash field |
|---|---|---|
| `+0x00` u64 | the item config's asset id | `guidLeft` / `guidRight` |
| `+0x08` u32 | weapon id: `u16 [item->vt[+0x108]() + 0x3c]` (the `9C8x` id) | |
| `+0x10` char* | `exe+1f514f0(item)` | `codeName` |
| `+0x18` char* | `exe+bc15c0(config, ...)` | `name` |
| `+0x20` char* | icon (`exe+bc1dc0(config)->+0x20`, else "") | `iconName` |
| `+0x28` float | **`weapon->vt[+0x188](weapon, 0)`** | `currentAmmo` |
| `+0x2c` float | **`weapon->vt[+0x190](weapon, 0)`** | `maxAmmo` |
| `+0x30` float | -1.0 | `rechargeAmmo` |
| `+0x34` u32 | **wheel position 0..7** = `dword [itemConfig + 0x10c]` (an item whose value is above 7 is not listed) | |

`weapon = exe+21633c0(heroWeaponManager, id)`: the **weapon actor's `WeaponBase` component from the weapon table** (section 5). **When that is null the entry gets
`0 / 0`**: that is the red, unusable look. So:

- **A gadget we add or unhide shows red `0/0` because its weapon actor does not exist yet** (the table record has no live actor until the first real equip creates it with
  `CreateWeapon`, vtable `+0x70`). The player's equip creates the actor; ours "did not clear it" because the wheel only re-reads on its own refresh.
  **Recipe [I, not run]:** `weaponMgr->vt[+0x70](weaponMgr, id)` (create), then `exe+793320(quickSelect, 1)` (force the list). With the enabled-byte way of hiding (section 5)
  the record and its actor stay alive, and a hidden gadget should be filtered by removing its item from the list instead: see next point.
- **What is listed** is decided by the *inventory* (category 3 items that `exe+bbc3d0` does not reject), not by the weapon table. Early-story "locked" gadgets are simply
  absent from the inventory, so they are absent from the wheel: the clean "not owned" state for a run is to **remove the gadget's item from the equip inventory and add it
  back on grant** (`inventory::Remove` / `Add` with the gadget item's asset id), then force a refresh. A modded ninth gadget appears as soon as its item is in the
  inventory with a wheel position 0..7, and disappears whenever a refresh runs while its item is missing or rejected.
- **Refresh triggers [V]:** `exe+793320` is called from `exe+793160`, from Open (`exe+793780`) and twice from the per-frame update (`exe+79479e`, `exe+7949a3`). Calling
  it yourself with `force = 1` on the game thread is the supported way to push a changed list (this also answers FOF-B2's question about the flag: 1 forces the resend).
- **Ammo for the HUD** comes from the weapon object itself (vtable `+0x188` current, `+0x190` max), which in turn reflects section 6.

For Friend or Foe (custom entries): the Flash movie only ever sees what `SetQuickSelect` carries, and the native list is built from inventory items, so a native ally
entry needs a real `GameWeaponItem` in the inventory (wheel position, icon, name come from its item config). Without one, keep intercepting `SetQuickSelect` (the entry
shape above is exactly the array element the movie expects) and treat `HandleWheelInput(index)` (`exe+793da0`) plus the close (`vtable +0x08`) as the selection signal.

## 8. "Airborne" and the other hero macro states (EA-C1, EA-B4; MSC-E1 partly)

[V] The hero has a component **`HeroMacroState`** (type object `exe+6dabca0`, class `Hero::HeroMacroState`, vtable `exe+38aaf00`): a per-frame summary of what the hero is
doing, computed in its update (vtable `+0x50` = `exe+8527e0`) from the mover, the wall probes, the WAM (melee) user, the stealth manager and the transition manager.

- `dword [comp + 0x4c]` = current flags; `dword [comp + 0x50]` = a second set (previous or pending [?]).
- `vt[+0x60](comp, mask)` = any of `mask` set in `+0x4c`; `vt[+0x68]` returns `+0x4c`; `vt[+0x70](comp, mask)` tests `+0x50`; `vt[+0x78](comp, mask)` = all of `mask` in `+0x50`.
- Flag values (flag set at `exe+5e12b78`, the one next to the bots' set; that it is this component's set is [I, by name]):

| flag | value | flag | value |
|---|---|---|---|
| kNone | 0x1 | kSprinting | 0x40 |
| **kOnGround** | **0x2** | kSpiderAssault | 0x80 |
| **kInAir** | **0x4** | kEasyAirYank | 0x100 |
| kNearGround | 0x8 | kBeam | 0x200 |
| kTimedDodge | 0x10 | kPerch | 0x400 |
| kRushToMelee | 0x20 | | |

So the cheap airborne test is `(dword [HeroMacroState + 0x4c] & 0x4) != 0`, with `kNearGround` (0x8) to forgive hops and combat launches, and `kPerch`/`kBeam` to exclude
perches and beams. Swinging, wall-running and web-zipping are not in this set; for those use the state locks the hero states take while they run (section 1.3:
`kWebSwing`, `kWebZip`, `kWallRun` on layer 2 are locked by the *other* states, so they are not a "currently swinging" flag) or the hero state machine itself.
A second enum for input/ability conditions exists: `{kAltCombatButtonHeld, kOnGround, kSwinging, kWallGrasp, kWallRunVert, kPerched, kNotOnGround, kInHelicopter}`
(enum type `0x3b871432`). The bots' equivalent flag set (`exe+5e12bb0`) is the one the takedown and finisher rules read: kWebbed 0x1, kStunned 0x2, kProneOnGround 0x4,
kGroundFlop 0x8, kKnockedUp 0x10, kKnockedDown 0x20, kWebYank 0x40, kHitReact 0x80, kFlung 0x100, kJumpedOff 0x200, kDodgedOver 0x400, kElectrified 0x800, kInAir 0x1000,
kNearGround 0x2000, kOnGround 0x4000, kBackless 0x8000, kActiveFinisherImmune 0x10000, kActiveFinisherVulnerable 0x20000, kActiveFinisherCantKill 0x40000, kAwareOfHero 0x80000,
kDiving 0x100000, kTraversalStrikeVulnerable 0x200000, kStruggling 0x400000, kSuspensionMatrix 0x800000 (useful for EA-D3/D4 diagnostics: a "stuck" unit's flags say why).

*Live check:* log `[HeroMacroState+0x4c]` at 10 Hz while walking, jumping, swinging, zipping, wall-running, perching and being launched.

## 9. The real faulting context of every dump, and what the four gadget crashes were (EA-J2, EA-A1, EA-E4)

**Tooling [V, run on the 39 dumps in the Documents folder]:** the faulting state was in the dump all along, in the exception stream's own `ThreadContext`;
`dumpstack.py` read the thread list's context (the handler's). Two new tools in `tools/sigtools`:

- `python dumpctx.py "<dump>.mdmp" [frames]`: exception code and access, the **faulting registers** (each named when it points into the exe: a type object, a
  vtable ...), a stack walk from the faulting `Rsp`, plus any `CONTEXT`/`EXCEPTION_RECORD` lying in thread stacks.
- `python dumpindex.py`: one line per dump (date, faulting `exe+RVA`, access, first frames). Stack entries after the first two or three are candidates (stale
  return addresses appear).

**The three "execute at address 0" crashes (2026-11-07 18:15, 19:25, 20:12) [V registers, I conclusion]:** in all three the top return address is `exe+2163462`,
the instruction after `call [rax+0x98]` in `exe+2163440`, reached from `exe+2162337` (the first call inside `Equip`). `Rcx`/`Rbx` = the manager `this`, `Rsi` = `0x9c89`
(the id being equipped), the slot's current id read from `this+0x6c` = `0xc350` (so the bytes still look like a weapon manager), **and `Rax` = the "vtable" is a heap
address `0x710` bytes above `this`**. A real vtable is in the exe image; a heap pointer of the same size class in the first 8 bytes is the allocator's free-list link:
**the weapon manager had been freed** (the hero entity was recreated: respawn, checkpoint, the run's teleport) and `Equip` was called on the stale pointer, by our code in
two of the dumps (`EndlessArena.dll` frames right under `Equip`). Slot `+0x98` of the "vtable" is then whatever lies in the next free block: 0.
**Fix [I]: never cache the manager.** Resolve hero handle -> `GetActor` -> `FindComponent(hero, type exe+6dd84e0 HeroWeaponManagerLocal)` on the game thread
immediately before every call, and re-check the vtable equals `exe+38b59b8` (cheap) before calling. The same rule covers the health and inventory pointers (EA-E4).

**The fourth (2026-11-07 18:02, `exe+374237`, read of `0x88`)** is the null from `exe+2163360` described in section 5 (a slot id that is not in the table).

Other dumps, for the record (from `dumpindex.py`): `exe+15f9bc9` read 2 (three times: the released-model crash), `exe+319b6a9` (GFx teardown), `exe+31e8dcc` / `exe+31e9582`
read `0x90` (three times: the Settings-screen crash of Manhattan Skies, MSK-D1: `Rcx = 0x90`, i.e. a null object's field), `exe+33005d` write `0xe4` with
`Rdx = type BotCommandQueueGame` (2026-11-14: Friend or Foe's missing-`BotCommandQueueGame` crash), `exe+1cbbd80` write 0 (twice), `exe+1fe51b5` read `0x12c` (twice: the
heavies' null mover), `exe+18046ea` read `0x340` (twice), `exe+214251b` write 1 inside `MsgSend` (twice: a message sent with a bad type index), `exe+43fc24` (the grenadier
stack overflow).

## 10. Killing properly, flooring at 1 HP, and damage flags by name (EA-D2, EA-C2, EA-C5, EA-D5)

**The game's kill [V]**: the script node `KillAction{SkipDeathAnim}` (inputs `Kill`, `Injure`; value `Target`; execute `exe+1f19090`) sends the message
`KillActionProcessKillHandle` (type `exe+7c2f0d0`, handler `exe+1f1e830(entity, u8* flags)`), which does:

- `Kill`: **`exe+1f16590(Health*, 6)` = `Health::Kill(health, damageType)`** (6 = kScript). It builds a damage request on the victim (`exe+1eb6d00(&DmgSys /*exe+62a4ec0*/, &handle)`) with
  `Damager = the victim itself`, `Type = the argument` and **`RequestFlags |= 0x160` = `kForceKill`**, so the death goes through the normal pipeline: death state, animation or
  ragdoll, `DeathEvent`, rewards. `Hero::HeroStateDeathLocal` calls the same function on the hero.
- `Injure`: `Health::SetHealth(health, 0.0)` (`exe+1f16fc0`; the clamp to the floor at `+0x84` is what makes it "injured", not dead).
- `SkipDeathAnim`: sets flag `0x20000000` on the actor's `BehaviorHost` (type `exe+7bf03d0`) with `exe+1d67c60`.

That is why writing 0 through `HealthSet` "reads back 0 but does not kill": the value is not the death; the request is. **Recipe for a stuck or gated boss [I, not run]:**
`SetPhase(last)` (so the phase floor is 0), then `exe+1f16590(healthComponent, 6)` on the game thread. For Kingpin use his real `BossHealth` component.
Find the component by type: `Health` is type object `exe+7c2c290` (the lookup returns the derived `BotHealth`/`BossHealth` too [I]).

**`DamageRequestFlags` (`DamageRequest +0x160`) [V names and values, `fx.py flags "^kPreventKill$"`]:** kContinuous 0x1, **kNoReact 0x2**, kNoHeroKnockback 0x4, kAllowFriendly 0x8,
kAllowSelf 0x10, kNoIgnore 0x20, kReliable 0x40, kVictimAuthority 0x80, **kKill 0x100**, kDudeExplode 0x200, kDudeExplodeInstant 0x400, kSmokeExplode 0x800, kHitWeakspot 0x1000,
kFromLocalPlayer 0x2000, kFriendlyKnockback 0x4000, kSelfKnockback 0x8000, **kNoSpawnRewards 0x10000**, kForceReact 0x20000, kDirectHit 0x40000, kHazardDamage 0x80000,
**kDamageIsPercent 0x100000**, kNoSideEffects 0x200000, kCrit 0x400000, kTurnInvulnerable 0x800000, kIndirectWeapon 0x1000000, kAllowDeathAnimInAir 0x2000000, kUnDodgeable 0x4000000,
kUnblockable 0x8000000, **kPreventKill 0x10000000**, kAllowOverkill 0x20000000; `kForceKill` = 0x160. (A second copy of the set for data has kPreventKill 0x40000000 and
kAllowOverkill 0x80000000: use the first set for the runtime field, and confirm once by logging the flags of a real finisher.)
**`ResponseFlags` (`DamageDealtEvent/DeathEvent +0x250`):** kDamage 0x1, **kKilled 0x2**, kBlocked 0x4, kCountered 0x8, kIgnoreDead 0x10, kIgnoreInvulnerable 0x20, kIgnoreOnce 0x40,
kIgnoreFriendly 0x80, kOverkill 0x100.

So, for the curses [I, not run]:

- **Street Brawler's drain and "Rent Is Due"**: a `DamageRequest` on the hero with `Type = kScript`, flags `kNoReact | kNoHeroKnockback` (no hit reaction, no stagger) and, while it
  must not kill, **`kPreventKill`**; `kDamageIsPercent` lets the amount be a percentage; `Duration` + `TickInterval` (section 3) make it tick by itself.
- **The Last Hunt (an enemy that cannot die)**: the health flag set is `{kImmortal 0x1, kInvulnerable 0x2}` (config field `HealthPrius.Flags` at `+0x30`; the runtime flags are the
  synced u16 at `Health +0xac`): *immortal* = takes damage and reacts but never dies (what `ally_silver_sable` has), *invulnerable* = takes none. Set immortal on the hunter and
  he stays hittable and staggering. To keep him out of the cleanup logic, tag his handle in the mod, not in the game.
- **Kill credit and melee-only bonus**: read `kFromLocalPlayer`, `Type`, `WeaponNameHash` on `DamageDealtEvent`; a response with `kKilled` is the kill.

`Health` layout confirmed by its sync registration (`exe+1f168f0`): synced fields `+0x80` max, `+0x84` floor, `+0x88` current, `+0xac` flags (u16). `exe+1f16ce0(Health*)` is the
sync callback that sends `DeathEvent` (response `kKilled`) when the replicated health is <= 0.

## 11. Saving: what records a checkpoint, and the game's own autosave switch (EA-H1; MSK-E1)

[V] Classes: `SaveFile` (vtable `exe+4f792f8`), **`GameSaveFile`** (`exe+38fce40`; static instance `exe+70a53e0`), `SaveSystemGame` (`exe+38fd118`), `ActorSaveSystem`, `SpawnedActorSaveSystem`,
`SaveComponent`. `GameSaveFile` handles three events (registered in `exe+2055260`):

| event | handler | effect |
|---|---|---|
| `CheckpointTriggeredEvent` (`0xa3aac021`) and `CheckpointLoadedEvent` (`0x414ad37a`) | vtable `+0x90` / `+0x98` -> **`exe+ce6b50(saveFile, checkpointHash)`** | writes the current checkpoint into the save block: `block+0x28` level index, `+0x2c` checkpoint hash, `+0x30` checkpoint id, marks it dirty |
| `LightingModeTransitionStartedEvent` | vtable `+0xa8` = `exe+2055440` | **`block+0x38` = the new lighting mode, `block+0x3c` = 1** (the save does store the time of day) |

`block = saveFile->vtable[+0x78]()`. The checkpoint record comes from the world's checkpoint table (`world+0xe0900` map, `[world+0xe08f0]` 0x48-byte records).

**The switch [V]**: the nodes `AutoCheckpointAction` and `SetSpawnpointAutoAction` (inputs **`AutoSaveOn` / `AutoSaveOff`**, execute `exe+9b2f40` / `exe+9b36e0`) call

```
X = exe+1f03570()                 // the game-state sync object: ResolveRef(&[exe+7c2b490])
exe+1f05520(X, bool enabled)      // synced field 1; current value = byte [X + 0x1168]
```

So "suppress saving for the run" as the game does it is `exe+1f05520(exe+1f03570(), 0)` at run start and `(…, 1)` at the end (read `[X+0x1168]` first and restore that) [I: that this
covers every autosave trigger is not verified; add a hook on `exe+ce6b50` that returns while a run is active so a mission zone's checkpoint cannot repoint the save at the arena].
Related nodes: `SetSaveOnReloadAction` (`exe+1f6aa50`, value `Value`), `SaveMissionStateAction`, `MissionSaveFileFlagsAction`, `GlobalSaveTable{Set,Check,Query}KeyAction` (a
key/value table in the save: a place the *game* uses for persistent flags), **`SaveWeaponItemsAction{ItemConfig, RestoreOnDestroy, RestoreOnMissionEnd}`** (inputs `Save` / `Restore`,
execute `exe+bc98d0`: the game's own "stash the hero's weapon items and give them back", which is what `runloadout` does by hand).
`HeroCharacterPrius` and the suit/gadget persistence were not traced [?].

## 12. Input for the dock and exit circles on every device (EA-F3)

Answered in `docs/GAME_INTERNALS.md`, section "Input: the game's action layer": the game reads named actions (`kInteract`, `kJump`, ...) from the hero's input controller
(`q[HeroLocal+0x50]`) with `exe+169a690` (find by name hash) and `exe+1698d10` (pressed this frame) / `exe+1697fa0` (value). Polling **`kInteract`** that way works for every
backend the game supports (XInput, DualSense, Steam Input and the Deck, keyboard and mouse with the player's own bindings) and does not depend on `GetForegroundWindow`.

## 13. Spawning out of view, deleting, engagement: leads from the node catalogue (EA-E3, EA-D1, EA-D4)

- **Delete [V]**: `ActorDeleteAction` (execute `exe+cf74a0`, value `Target`) sends the message `ActorDeleteActionDeleteActorFunction` (type `exe+70aa250`, handler `exe+cf7790`
  = a jump to **`exe+1f65260(entity)`**) and, for actors without a sync owner (`exe+1f7b760` null), calls `exe+1f65260(entity)` directly. That is the engine's "delete this actor"
  entry (the mod uses `DeleteSyncObject` + `EntityDestroy`; compare the two before switching). `DeleteOffscreenAction` deletes when off screen.
- **Spawn placement**: `FindSpawnLocationsScriptAction`, `SpawnerScriptAction`, `SpawnerFactoryScriptAction`, `RandomSpawner2Action`, `ProximitySpawnScriptAction`,
  `ModifyProximitySpawnScriptAction` are the game's spawn nodes (pins and data in `recon/script_nodes_4.0630.txt`); `OnChildSpawnedAction` and `OnDestroyedAction` are the notifications.
- **Why a unit stands still**: its state flags (section 8: kAwareOfHero 0x80000, kStunned, kWebbed...) and `BotCommandSetAnimFilters`' `AwarenessLevel` {kNone, kPassive,
  kConfirming, kEngaged, kSearching} name the awareness ladder; the job pools of the targeting configs (`AttackJobPool` {kNone, kSmallMelee, kLargeMelee, kStandardRanged,
  kLargeRanged}) are the attack-token classes. The pipeline itself (target query, tokens, the off-camera boss) was not read [?].
