# Rewards, curses and balance: design notes (2026-11-08)

Written while the maintainer was away; nothing here is built. Items marked **[ask]** need a decision. Lever status as in [ROGUELIKE.md](ROGUELIKE.md):
**ready** = the mod can do it now, **RE** = needs reverse engineering first.

## 1. Requests noted
1. **Gadgets become Campaign rewards.** As before, the player's gadgets are removed when a run starts; gadgets and gadget upgrades are unlocked through the run.
2. **New curse "Street Brawler".** Every second Spider-Man spends in the air costs health; in return raw melee damage rises by 150-200%. The drain rate is tuned in testing.
3. **Filter the pool.** Remove rewards that make no sense in the arena (stealth items and the like).

## 2. Principles, and what they mean for us
Short, from the roguelike write-ups I could find (mostly blogs and fan guides; no first-party post-mortems turned up, so treat the numbers as illustrative).

- **A good offer is a decision, not a ranking.** Hades' boons are good because the best one depends on the build so far; the strongest pick in a vacuum is rarely the best pick. So offers must be *different in kind* (a damage lever, a defence lever, a utility or gadget lever), and some should only make sense with what the player already owns. Nothing in the pool should be dead (stealth items in a fight that is always on).
- **Redundancy keeps several builds alive.** Slay the Spire has several cards that do similar jobs in different ways, so no single card is mandatory. Our pool should have at least two routes to each of: more damage, less damage taken, healing, tempo (Focus), crowd control.
- **Two axes of power: type and level.** A common boon at level 5 can beat a rare at level 1. Cheap to add: stat boons already stack (x3); gadget upgrades are the same idea.
- **Combinations break games, not single items.** Each item can be fine alone and the pair can be a win button. Cap *multiplicative* stacking (e.g. total damage dealt <= x3.5, total damage reduction >= x0.35 taken) and playtest the two or three strongest pairs, not every pair.
- **Risk should buy something.** Hades' Pact of Punishment: harder modifiers, better rewards, escalated gradually; extreme difficulty before the player is ready is just frustration. Our curses are already bittersweet; the new rule is that the *upside is worth about what the downside costs*, with the downside slightly cheaper (a curse should feel like a good deal that bites).
- **Scarcity raises the stakes of a pick.** Fewer, harder choices beat constant free refills. We offer one pick per wave; keep a reroll or skip out of the pool (the Riot Call curse already adds a choice).
- **Balance with data, not feelings alone.** Estimated win rate stabilises with run count; control for skill (a high win rate may be who plays, not what is strong); look at *where* runs end, not just how long. We already write a JSON per run (wave reached, damage taken, hits, kills, time per wave) - that is the telemetry.

## 3. Pool audit (47 items: 24 suit powers, 23 mods)
Recommendation: turn curation **on by default** (`curated` in the settings, today off), with this list in place of the one in `boons.cpp`.

| Verdict | Items | Why |
|---|---|---|
| **Cut** | Sound Of Silence, Blur Projector, Silent Step, Bio Scanner | stealth: enemies are never unalerted in the arena |
| **Cut** | Long Range Scanner, Proximity Sensor | collectibles/scanning |
| **Cut** | Combat Analyzer | XP; the arena has none |
| **Cut** | Quips | flavour only [ask: keep as a joke "power" that does nothing, in a rare slot?] |
| **Gate: offer only after the player owns a gadget** | Resupply, Battle Tech, Scavenger, Metabolic Converter | they refill or empower gadgets; with gadgets as rewards they become the *synergy* picks |
| **Keep, low weight** | Reactive Lenses (flash bangs), Ferromagnetic Harness (shock batons) | only some factions |
| **Keep as a gamble** | Equalizer ("everyone goes down in one hit, including you") | a deliberate high-stakes pick, one per run at most [ask] |
| **Keep** | the other 31 | |

After the cuts: 20 powers, 18 mods (4 gated). Add the new stat boons and gadget rewards below and the pool is roughly 55 offers, ample for a 12-wave run without repeats.

## 4. Gadgets as rewards
**What the game has** (names from its own menus): Web Shooter (the always-on web balls; keep), and six gadgets: **Impact Web, Web Bomb, Trip Mine, Electric Web, Spider Drone, Concussive Blast**, each with upgrades (the "blueprints" in the gadget menu). Gadgets spend ammo.

**Run flow**
- Run start: the six gadgets are unequipped (the Web Shooter stays), same as suit power and mods today.
- Rewards: a gadget appears as an option from wave 2 on, at most one gadget option per screen. First gadget guaranteed by wave 3 so the gated items (Resupply and friends) have something to bite on.
- Upgrades: once a gadget is owned, "<gadget> II / III" joins the pool (one tier per pick).
- Ammo is the leash: a gadget costs ammo, refills come from kills (a stat boon) or the gated items. This is what keeps six gadgets from being a free win.

**Rough power budget** (an offer, not a measurement): Impact Web utility; Web Bomb crowd control; Trip Mine area denial for a stationary fight; Electric Web crowd stun plus damage; Spider Drone a free damage source; Concussive Blast a burst knockback. Give the drone and the electric web the lowest weight (they remove the most player work), the mine the highest (it needs setup).

**Technical** (all RE): find the gadget equip path (the quick-select wheel, `UIQuickSelectWheelEquip`), a way to lock the wheel for the run, the ammo store, and the upgrade items. The ledger already tracks the first two; `runloadout` is where the clear/restore would go. [ask: should the wheel be locked completely, or may the player swap between *owned* gadgets mid-run? I suggest the latter.]

## 5. Curse: Street Brawler (spec)
- **Upside:** melee damage x2.5 to x3.0 (+150% to +200%). The melee filter needs the damage type; `stats` already sees every hit with its damage hash, so this is **ready** once the melee check is found. Gadgets, webs and powers are *not* multiplied (a melee identity).
- **Downside:** health drain while airborne. Suggested first numbers, all in the ini so you tune without a build:
  - 1 s of coyote time (a hop costs nothing, a swing does);
  - then 1.0% of max health per second, rising to 2.0% after 4 s continuously airborne;
  - drain stops at 1 HP unless `lethal=1` (I'd start non-lethal while tuning, then decide; a lethal version makes the player *choose* between swinging away and fighting);
  - airborne means not grounded, not wall-running, not on a perch (needs the hero state: SpidyVR's `native_movement` and its air-state notes are the lead; **RE**).
- **Fit:** it pairs with melee powers (Iron Arms, Electric Punch, Concussion Strike, Unrelenting Fury, Quad Damage) and fights the swing playstyle the whole game teaches; that tension is the fun.
- **Cost check:** a player airborne half the time loses ~0.5 to 1 %/s of max health, ~100 HP a minute on a 210 HP bar - a real price for roughly x2.75 melee. If testing shows the swing is too lucrative, raise the drain, not lower the damage.

## 6. More boons (stat boons are **ready** levers)
Thematic names from the comics; effect in brackets.

| Name | Effect | Source of the idea |
|---|---|---|
| **Peter Tingle** | the first hit of each wave is negated [ready: incoming x0 once per wave] | the spider-sense |
| **Under the Machinery** | below 25% health: damage taken -40% and a slow heal [ready] | the Master Planner (Amazing Spider-Man #33), Peter lifting tons of wreckage |
| **Wheatcakes** | a bigger heal after boss waves only [ready] | Aunt May's cooking (rename of Second Wind is an option) |
| **Web Fluid Cartridges** | +gadget ammo / refills on kill [RE: ammo] | Peter's running out of web fluid |
| **Spider-Tracer** | the first enemy you hit each wave is "tagged" and takes +100% from everything [needs a target mark; may be **RE**] | the tracers |
| **Front Page** | +25% score, no combat effect [ready; score only] | Peter selling photos to the Bugle |
| **Black Suit** | damage dealt rises with consecutive kills, resets on a hit taken [ready] | the symbiote suit |
| **Stand-In** (was Scarlet Spider) | a decoy hit absorbs the first hit of each wave [Holo Decoy overlap; may fold into it] | Ben Reilly |
| **Great Responsibility** | +damage proportional to enemies alive [ready] | "with great power comes great responsibility" |
| **Pumpkin Toss** | thrown objects hit harder [**RE**] | the Goblin line |
| **Trophy Hunter** (was Spider-Verse) | each boss defeated adds +5% damage dealt, permanently for the run [ready] | the Spider-Verse |

Melee-flavoured boons to support Street Brawler (need the melee filter): **Iron Fists** (+melee damage), **Counter Puncher** (perfect dodge adds melee damage for 5 s; the Discharge Power Cells mod is already a version of this).

## 7. More curses (all bittersweet: an upside and a downside)
| Name | Upside | Downside | Idea from |
|---|---|---|---|
| **Street Brawler** | melee damage x2.5-3.0 | health drain while airborne | the maintainer |
| **Parker Luck** | +25% damage dealt | rewards offer one fewer choice [ready] | Peter's bad luck |
| **Rent Is Due** | one extra reward choice | each wave cleared costs 8% max health [ready] | the rent that is always late |
| **Smear Campaign** | score x1.5 | elites twice as common [ready; score row] | J. Jonah Jameson |
| **Maximum Carnage** | kills heal 6% | +30% enemies and an extra boss every third chapter [ready] | the Carnage crossover |
| **Spider-Man No More** | +50% damage taken reduction | the suit power is disabled until the next boss [RE: power lock] | ASM #50, Peter hangs up the suit |
| **The Last Hunt** | +1 reward tier on the next boss | one persistent elite (from the current faction, forced aggro, respawns if killed) stalks you [ready via the spawner] | Kraven the Hunter |
| **Clone Saga** | rewards are doubled | the next boss wave brings its duplicate [ready, doubles the boss] | the Clone Saga |
| **One-Punch Spider** | everything dies in one hit | you die in one hit (x20 both ways) | One-Punch Man |
| **Great Power** | +60% damage dealt | enemies deal +60% too [ready] | the old classic |

## 8. Balancing method
1. **Express every effect as a multiplier** on one of two numbers: *kill speed* (offence) and *effective health* (defence). Heavy Hitter +20% damage = x1.2 offence; Reinforced Suit -15% damage taken = x1.18 defence; a heal is x(1 + heal/max). Compare picks in those terms.
2. **Budgets by tier:** common ~x1.15-1.25, rare ~x1.30-1.45, boss rewards ~x1.6-2.0 (a suit power counts as a rare plus its active use). A curse's downside costs ~0.7-0.9 of its upside's value.
3. **Pair budget:** keep total offence <= x3.5 and total defence <= x3 before any single enemy-side scaling; add enemy count, not health (the no-health-sponge rule stands).
4. **Telemetry targets** (per run JSON): median run reaches wave 10-14 on defaults; the 90th percentile builds-with-curses reach 18-22; a wave is "too easy" if damage taken per wave is under ~10% of max health, "too hard" over ~60%. Tune the pool first, enemy numbers second.
5. **Test loop:** the offline planner already prints the enemy side per wave; add a *player-power* column (the product of the multipliers in a sample build) beside it, so a design can be sanity-checked on paper before a play session. Then three sessions per change set, rather than a long one, and compare *where* runs end.
6. **Guard against one winning build:** after each balance pass, check the three strongest pairs by hand; if two items share a tag and both multiply damage, the cap above applies.

## 8b. Focus (maintainer, 2026-11-07)
Healing is worth less here because the game already heals through Focus (earn it by fighting, spend it to heal). So heal rewards (Second Wind, Wheatcakes, Adrenaline Rush) are weaker than they look; consider making the heal numbers smaller and adding Focus-based picks instead.
- Already in the game's own pool (suit mods, nothing to build): Resolve (+1/2/3 focus segments), Sustained Assault (focus drain x0.7), Fill Focus Meter (suit power).
- Boon ideas (need a focus-meter read/write, RE not done): **Slow Burn** (Focus fills 30% faster), **Quick Study** (a perfect dodge gives extra Focus), **Full Tank** (each wave starts with a full meter), **Second Gear** (an extra segment but it drains faster).
- Curse ideas (upside/downside): **Adrenaline Junkie** (Focus fills 2x but you cannot heal from it, it only powers finishers), **Dry Spell** (+25% damage but Focus builds 40% slower), **Overdrive** (Focus never drains, but the meter is one segment shorter).
- First step is recon: find the Focus meter (the Assault meter) object in memory and its fill/drain rates; the mod's perk items already name the class (AssaultMeterSegments, AssaultModeDrain), a lead.

Focus recon, offline pass (strings in Spider-Man.exe, config JSON): the meter is the "SpiderAssault" system. Names to look for in memory: `spiderAssaultManager`, `SpiderAssaultConfig`, `UpgradedAssaultMeterMultiplier`, `MinAssaultModeTime`, `HeroSpiderAssaultTimeScale`; events `OnAssaultMeterFull`, `HeroSpiderAssaultSegmentFullEvent` / `SegmentNotFullEvent` / `StartedEvent` / `StoppedEvent`; perk types `SpiderAssaultRatePerkPrius` (fill rate), `AssaultModeDrainPerkPrius`, `AssaultMeterSegmentsPerkPrius`, `StimPackAddsAssaultPerkPrius`. The fill-rate perk is the easiest lever if it can be applied at runtime. Next in-game step: `comps hero` and look for a SpiderAssault/Assault component.

**Focus recon result (2026-11-07, verified in game):** the hero component `HeroSpiderAssaultManager` holds the meter. Segments are float pairs {fill, cap} at +0x110, +0x118, +0x120 (caps +0x114, +0x11c, +0x124, all 100); segment count at +0x128 (3). (First notes said +0x150: that was the dump row address, not the offset.). Writing 0 to the third fill emptied that segment on screen. Not yet known: where the active-mode flag is (+0x148 was 1 after filling), whether more segments (Resolve) extend the array. Built (untested): **Slow Burn** (fill +30%/stack, x2), **Full Tank** (full meter at each wave start), curse **Dry Spell** (+25% damage, Focus builds 40% slower). Rate effects work by scaling the per-tick change of total fill, no hook.

## 9. Decisions (2026-11-08)
1. Equalizer stays as a deliberate gamble.
2. Gadget wheel: the player may swap between gadgets they have unlocked.
3. Street Brawler can kill you (lethal).
4. Quips stays (it is funny).
5. One-Punch Spider: bosses also die in one hit (their phases are skipped by advancing them via AdvanceBossPhase).
6. Liked: Black Suit, Wheatcakes, Clone Saga. Renamed as marked in sections 6-7.
7. The Last Hunt, revised by the maintainer: the hunter is undying (cannot be killed) and keeps coming for you even between waves; the player avoids him and fights the others. Not built yet (needs an invulnerable spawn that persists across waves).
8. Built 2026-11-07 (commit 3204b0b, untested in game): Peter Tingle, Under the Machinery, Black Suit, Wheatcakes, Parker Luck, Rent Is Due, Clone Saga, One-Punch Spider.
9. New curse **One-Punch Spider**: damage dealt and damage taken both x20 (2000%, tunable). Everything dies in one hit, you too. Open points: bosses and phase gates (do they die in one hit or keep their health floors?), and whether it overlaps too much with Equalizer (the curse is run-long, Equalizer is a single pick).
Build order unchanged: Peter Tingle, Under the Machinery, Parker Luck, Rent Is Due, Maximum Carnage, then gadget rewards, then Street Brawler.

Sources for the general design points: [Why Roguelikes Keep Growing](https://www.strayspark.studio/blog/building-roguelike-ue5-procedural-progression), [Roguelike itemization: balancing randomness and player agency](https://wayline.io/blog/roguelike-itemization-balancing-randomness-player-agency), [Boons and gods (Hades)](https://saschb2b.github.io/game-mechanics/games/hades/boons-and-gods), [Hades Ascends the Action Roguelike Throne](https://game-wisdom.com/?p=25324), [Balancing a roguelike with a bot that plays a thousand games](https://solodevstack.com/blog/balancing-a-roguelike-with-a-bot-that-plays-1000-games).

## Test results (2026-11-08, debug mode)
Verified in game: Slow Burn (x1.3 on every Focus gain), Dry Spell (x0.6), Full Tank (fills on command; wave-start trigger untested), One-Punch Spider (enemies and you die in one hit; Mister Negative and Kingpin go down, Kingpin ~17 s after the first hit via phase skipping and the finisher, body left in a frozen pose), Black Suit (+4% per kill in a row, x1.40 at 10 kills, back to x1.00 after a hit), Peter Tingle (first hit each wave does no damage). Perches (rooftop snipers/rockets): tried, not liked (one enemy fell through), switched off in code (`kPerchesOn` in director.cpp). Debug-mode quirk: `wave stop` can leave the Horde results panel stuck; `gfx hide 1|2|3` clears it. Still to test: Under the Machinery, Parker Luck, Rent Is Due, Clone Saga, Wheatcakes.
Clone Saga verified (2026-11-08): the second reward screen follows each pick, and boss waves spawn two copies (two fully modelled, fighting Kingpins). Silver Sable was a white cube that stood still in that session even as a single boss ("never fought", removed by the director), and the dev `boss` spawn gave cubes too; Kingpin and Mister Negative were fine. Cause unknown; recheck Sable after a fresh game launch (observe, do not guess).

## Gadget recon (2026-11-08)
- Swapping a gadget in the menu/wheel is NOT seen by the inventory recorder (`inv watch`): gadgets are not equipped through the suit-item manager.
- Hero components of interest: `HeroWeaponManagerLocal`, `ItemLoadoutComponent`, `GadgetItemAmmoManager`; exe classes `GadgetItem`, `GadgetConfig`, `HeroGadgetConfig`, `MasterWeaponListConfig`, `MasterItemLoadoutListConfig`; UI calls `UIQuickSelectWheelEquip`, events `kEquipGadgetUp/Left/Right`.
- A write watch on `HeroWeaponManagerLocal`+0x58 (swap) found exe+2162c01/2162c0f: an insertion into a sorted list of {float, id} pairs at +0x48/+0x58 with ids 0x9C82-0x9C89. It looks like a most-recently-used history of gadgets, not the equipped set. Callers: 2162783 <- 216284f <- 21623d4 <- 1f55e8c <- bbdcda.
- Not found yet: where the set of available gadgets / the equipped slots live, and the function that equips one. Guessing config paths for the gadget lists failed (extract.py reports MISSING).

### Gadget ids (verified in game 2026-11-07, EquipGadget exe+2162300, all slot 0)
9C82 Electric Web, 9C83 Impact Web, 9C84 Web Bomb, 9C85 Trip Mine, 9C86 Concussive Blast, 9C87 Spider Drone, 9C88 Suspension Matrix, 9C89 Web Shooter.


### Curse idea: "Out of Web Fluid" (name TBD; user, 2026-11-07)
Peter loses every web-based gadget for the rest of the run, the Web Shooter included: Web Shooter, Trip Mine, Web Bomb, Impact Web, Electric Web. Suspension Matrix, Concussive Blast and Spider Drone stay. Upside: Focus builds faster ("cornered"). Hide them from the wheel too if possible.
Verified: `gadget equip 0 0` empties the slot: HUD shows the old name but the ammo bar is gone, R1 does nothing, switching does nothing. That is the right state for the curse's web shooter (but the wheel/state must be restored after the run).
Verified: EquipGadget lock + allow list works (refused equips are logged; an allowed id switches).
Gadget table (HeroWeaponManagerLocal, found by disassembling lookup exe+2163500): entries 0x18 bytes at manager+0x1B8, count at +0x628, entry = {u32 id, FF01..., bytes 03 02/01 01 00, hash...}. No obvious unlock flag in the entry yet; the wheel's source list is still unknown.

Verified: zeroing a gadget's table id (manager+0x1B8+i*0x18) greys it red in the wheel with ammo 0/0 (still listed, unusable). Good enough for the web curse. User wants that curse to also disable throwables and web-swinging (all the way); levers for those not found yet.


HeroAbilityLock (hero component): 128 abilities x 4 layers; lock counter byte at comp+0x1C8 + layer*0x80 + abilityIndex; locked if any layer >0 (vtable fns at exe+1E8A640 unlock, 1E8A6D0 IsLocked, 1E8A730 debug draw). Ability indices unknown; bisecting by writing counters and testing swing.


Ability-lock bisect (2026-11-07, layer 0, HeroAbilityLock byte index): 0-3 = attacks/shooting (punch, web shoot), 4 = ground movement (walk; shooting also failed with it locked), 5 = likely swinging (4+5 locked blocked swing; 4 alone did not). Not confirmed: 5 alone (user hit a bug and reloaded). PINNED: the user recalls Harry's Research Station mission removing Spidey's webs; observe what the game does there before building the "no webs" curse's swing/throwable part.

### Gadget rewards built (2026-11-07, untested)
Decision: gadget upgrades dropped (mostly ammo/radius bumps); gadgets alone are the reward. `gadgets::BeginRun` (called from boons Begin) hides the 7 gadgets via the table-id trick, locks the wheel to the Web Shooter and equips it; `Grant(id)` unhides, allows and equips; `EndRun` restores all and re-equips the saved gadget. Offers: from wave 2 one gadget option may join a screen; the first is guaranteed by wave 3. Commands: `gadget begin|end|grant <hex>|hide|show <hex|all>`. Still to do: gate Resupply/Battle Tech/Scavenger/Metabolic Converter on owning a gadget; the web-fluid curse.

### Gadget test results (2026-11-07)
- Verified: `gadget begin` hides all 7 (red, 0/0), wheel locked to Web Shooter; `gadget grant` unhides + equips; `gadget end` restores everything. Hiding/showing does NOT change a gadget's real ammo (Web Bomb 3/4 stayed 3/4; all gadgets were back at their old ammo after `end`).
- A granted gadget shows the player's real saved ammo (Impact Web was 0/4 in the save, so it was red and unusable). A refill is needed on grant.
- Ammo storage NOT found: no gadget ids in GadgetItemAmmoManager (first 0x800 bytes), no change in HeroWeaponManagerLocal (first 0x800) between two shots of Trip Mine, and `inv watch` logged nothing when firing. Next ideas: the GadgetItem objects behind pointers in the ammo manager, a write watchpoint after finding the address by value search, or the game's own refill (GadgetResupplyPerk).

Crash 2026-11-07 18:02:44 (accepting Suspension Matrix, wave 2): null deref at exe+374237 after 2163360(mgr, slot) returned null (no table record for the slot-0 id). Just before, the game's own equips of id 0 (all slots) and C350 were refused by our lock. Fix: the lock only guards gadget ids 9C82-9C89; everything else passes. Re-test.

### Test 2 (2026-11-07): grant lag + respawn crash
- Taking Suspension Matrix at wave 3: nothing for a few seconds (still on Web Shooter, gadget red 0/0), then the game auto-equipped it with full ammo. `Equip` returned 0 right after the unhide. Fix: Grant retries the equip every 0.8 s (max 8) while the Web Shooter is still the equipped one.
- Crash at respawn after death (0.7 s after `EndRun`): execute at address 0 / stale weapon manager from the dead hero. Fix: `EndRun` re-finds the live manager, unhides only if there is one, and makes no EquipGadget call at all (what was equipped before the run is no longer restored).
- User wish: make "unlocked" vs "waiting on a refill" visible on the wheel. Plan: the user records the wheel in early game (before the gadgets are unlocked) so we can see how the game hides locked gadgets, then copy that state instead of the red 0/0 trick.
- Test 3: equip-and-put-back refresh from code did NOT clear the red state; only the player's own equip does. Replaced by an on-screen hint. Gadgets in the arena are the bare, un-upgraded versions (cause unknown; the user is fine with it).

Test 4 (2026-11-07): crash inside our EquipGadget(9C83) call at grant (execute at address 0). The game equips id 0 into slot 0 constantly; forcing an equip is fragile. Decision: the mod makes NO EquipGadget calls (not at run start, not on grant); grant = unhide + allow + on-screen hint; red until the player equips it. Also: the player could not switch back to the Web Shooter in an earlier run (cause unknown; equip logging is on during runs).

Test 4 follow-up: auto-equip back, guarded: it only equips while slot 0 holds a gadget id 9C82-9C89 (the crash happened with the game's own id-0 equips around); otherwise it skips and the gadget stays red until picked.

Test 5: red-until-equip only happens in real runs, where the game keeps slot 0 at id 0 and the guard skipped the equip; in debug sandbox / free roam the equip works and the gadget is blue with right ammo. Guard loosened to also allow slot 0 == 0 (the state of the one Impact Web crash). Collect more runs.

Test 6 (2026-11-07): after a guarded auto-equip of Trip Mine in a real run the player could not switch back to the Web Shooter (wheel equips id 0 -> returns 1, slot 0 stays 9C85); then our own forced equip of 9C89 crashed the game (execute at 0, via exe+1ce9e67). Conclusion: no EquipGadget calls from the mod during a run, ever. A granted gadget is red until the player equips it; a hint says so. Open: why the red state persists (only in real runs; free roam and the debug sandbox refresh it instantly).
