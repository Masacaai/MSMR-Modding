# Combat: enemy AI, health, damage, bosses

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
  (see [Time](camera-time-and-input.md#time-camera-and-input)).

## Bosses

Details and per-boss results: [BUGS.md](../../EndlessArena/docs/BUGS.md) and the roster table in [ENDLESS_MODE.md](../../EndlessArena/docs/ENDLESS_MODE.md).

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

