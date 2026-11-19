# Build of 2026-11-19 (built from Fable's static recon, docs/RECON.md): NOTHING in it has run in game

Not installed anywhere yet; the beta (0.2.0 beta 1) is what is live. Everything below is a test for the first session with the game open. Start `hook`, then `gadget watch on`.

## What changed
1. **Gadgets rewritten (`gadgets.cpp`).** Two bugs found by reading the code against the recon: (a) the hook had four arguments but `exe+2162300` is `Equip(this, id, slot, force, sendEvent)`, so every
   equip passed on a garbage fifth argument; (b) the hook stored `this` of *any* call as "the hero's manager", but the function is shared by every enemy's weapon manager (the `C350/C351` ids in old
   logs are enemy guns). Our hide/equip calls therefore went to enemies' managers, freed when the enemy despawned: the execute-at-0 crashes. Now the hero's manager is resolved fresh (hero handle,
   `GetActor`, `FindComponent`) and its vtable checked (`exe+38b59b8`, verified by RTTI name) on every use. Also: a slot holding a gadget about to be hidden is unequipped first (`Equip(0, slot)`), so
   no slot ever holds an id missing from the table (the `exe+374237` null).
2. **Dev commands:** `gadget slots` (manager, slot ids, whole table with the enabled byte and weapon actor handle), `gadget create <id>` (make the weapon actor: the cause of the red 0/0 state),
   `gadget refresh` (force the wheel to rebuild its list: `exe+793320(wheel, true)`), `gadget equip <id> <slot>` (only when the table record and the weapon actor exist).
3. **Two curses behind `[boons] wip_curses` (off; `dbg curse <name>` always works):** *Out of Web Fluid* (timed ability locks re-armed every 500 ms: swinging, zipping, point launch, web snap; the
   shot ability only while the Web Shooter is in hand; the four web gadgets hidden and refused; Focus x1.5) and *Street Brawler* (hero melee x2.75 by `DamageRequest.Type == kMelee`; airborne drain of
   1% of max health per 250 ms by the `HeroMacroState` kInAir flag; not lethal yet).
4. **`Health::Kill`** native (`exe+1f16590`, unique signature) and the setting **REAL BOSS KILL (BETA)** (off): both boss finishers call it instead of writing health 0.
5. **Steam Deck aid:** setting **INPUT: IGNORE WINDOW FOCUS** (off), and the log says once why input was ignored when the foreground window is not the game's.
6. **`autosave on|off|status`** dev command (the game's own switch, `exe+1f05520(exe+1f03570(), bool)`). Nothing calls it in a run yet.

## Test pass (in this order; write down what you see)
1. **Theory check, free roam, `gadget watch on`:** fight or spawn enemies. The log should show `equip id=C350 ... (not the hero's manager)` lines for enemy equips and *no* such label for the hero's own.
   `gadget slots`: the manager line says "vtable ok", slot ids are all in the table (no `NOT in the table`), the table has 10 entries (9C43, 9C82-9C89, 9C92).
2. **Hide with a gadget in hand (the latent crash):** equip Impact Web, then `gadget begin`. Expected: `slot 0 unequipped -> 1`, then `hid 9C83`, no crash; the wheel shows the gadgets red 0/0.
   `gadget end` restores them.
3. **The red state:** `gadget begin`, `gadget grant 9C83` (red expected), then `gadget create 9C83`, `gadget refresh`. Question: does the wheel now show it blue with ammo, without the player equipping it?
   Then `gadget equip 9C83 0`: works? and switching back to the Web Shooter with the wheel? (do this in free roam first, then in a debug-mode run in the yard).
4. **Out of Web Fluid:** `dbg curse Out of Web`. Expect: no swinging or zipping, no web shots with the Web Shooter, a non-web gadget (Spider Drone, Suspension Matrix, Concussive Blast, once
   granted) still fires, the four web gadgets are gone; walking, dodging and punching work. `dbg end`: locks lapse within about 1.2 s. If the Web Shooter still fires, the shot ability lock is not what stops it.
5. **Street Brawler:** `dbg curse Street Brawler`. Punch an enemy: damage about 2.75x (compare with the same enemy before). The log prints `brawl: airborne yes/no` on every change (first 60): check it
   against what you did (jump, swing, fall, wall-run, perch). Health drains while airborne (about 4% a second), stops on the ground, never below 1 HP.
6. **REAL BOSS KILL (Settings tab, then a boss wave):** the log says `boss killed through Health::Kill`; does the boss die with an animation or ragdoll and not freeze? Compare Kingpin, Hammerhead, Taskmaster.
7. **`autosave status`, then `off`, `status`, `on`:** the log prints the byte before and after. Test with a manual save in between if you want to see whether saving is blocked.
8. **Steam Deck (a tester is needed):** the new setting; the "foreground window" log line shows what the gate saw.

## Not done (no recon answer yet, or too risky to guess)
Engagement of off-camera bosses (EA-D1), the combat-move locker for web strike and throwables (`HeroCombatActionLocker` enum), The Last Hunt's chase AI, a lethal Street Brawler drain, the wheel's inventory
way of hiding gadgets (remove the item, add it back: touches the save), ammo get/set (`GadgetItemAmmoManager`), the real choice screen, music names, other arenas.

# Build installed 2026-10-15 ~12:00 (after the Horde crash): check first
0. Faction-join lines now use the notification box (not the top banner).
1. **No crash at death** (the capture Value fix): die in a Horde run, respawn, repeat once.
2. **Faction joins at budget 44:** the log's `preloads for ...: N unit types held` should show 4 factions and a "X join the riot" banner at step 2. N must stay <= 44; the question is whether a run with ~44 types shows floating heads, white cubes or a crash (it was fine at 33; 50 broke Olympus).
3. Dock panel: header, goals with completed rows (cur == max), START button, empty default Rewards box; no description; the Ryker's exit circle is a notification-box prompt again.
4. `gfx caps` lists captured text objects (the START relabel depends on it).
5. Still pending from before: boss pop-up, abandon prompt (freeze, Choice3 stays, countdown), results panel after death (dismiss with Continue), never-left-spawn lines in the log, no cages occupied.

# Build installed 2026-10-15 (after the Campaign and Horde tests): what to check first
1. `gfx caps` in the log lists text objects (the dock panel's ABOUT / START retitle depends on it).
2. Dock circles: panel only while standing on the circle, gone after travelling; Ryker's exit circle uses the same panel (LEAVE).
3. Pools by unit-type budget (`[run] max_types=40`): Horde and Campaign runs show `preloads for ...: N unit types held` with N <= ~40; no floating heads, white cubes or crash through a long Horde run (the Olympus case). If it holds comfortably try 44, then 48.
4. Boss pop-up ("<boss> has joined the fight!") and no per-wave summary banner; Horde opening line in the notification box, no rank-up banners.
5. Abandon flow: step out of the yard: after 1.5 s freeze + "Abandon this run?" panel; interact abandons; the Choice3 button stays, then a 10 s countdown with "Please return to the yard."; going back in cancels.
6. Caged enemies: look for `never left its spawn spot` lines and no one stuck in the cells around x 1306-1310, z -818..-823.
7. Horde: live score panel, "<faction> join the riot" banners, results panel after death (does Continue dismiss it; do the summary rows show).

# Next session: start here (written 2026-10-15, end of the UI session)

The build installed at 01:06 on 2026-10-15 is the one to test first. Nothing below has run in game yet unless marked verified.

## Verified live this session (old build, dev commands)
- GFx data-object building (`gfx selftest` passed), the real wave banner (message type 21), the live score panel on the left, the results panel (it appeared; title cut off, summary rows blank, duplicate text popup: all fixed in the new build, not re-checked), the hideout-style preview panel, the "base complete" banner (type 13), the medium message with our text (type 7).
- The suit menu recording (names, descriptions, icons, preview paths) and the map recording (no UI calls for icons).

## Test pass for the new build (in this order)
1. **Startup:** log shows the natives table OK and `gfx: self-check passed` on first panel use. Any `FAILED` there means panels fall back to text.
2. **Dock circles (Manhattan, both) and the Ryker's exit circle:** the redesigned panel (see ledger): best run in the header (time, kills), goals with progress, the Rewards box retitled ABOUT with the description, the button reading START (LEAVE at Ryker's). First check `gfx caps` in the log (should list text objects captured at HUD load); if it lists none the retitle can't work. Does the panel hide on stepping off, and do the texts return to REWARDS/RETRY (check a real hideout circle)? Does pressing interact still travel?
3. **Campaign run, first waves:** the faction icon on each wave banner (Street Thugs use the crime icon; Tombstone and Taskmaster borrow icons); the counter reads WAVE n of 4 and the boss wave says FINAL WAVE; the 5-second pre-wave Get Ready countdown; the ENEMIES counter during waves and its removal between them; the reward offer text now includes the game's descriptions (does it fit the notification box? it may overflow).
4. **Horde run (`run horde fast god` first):** Get Ready countdown at the start; the live score panel with rank title; rank-up banners; "X join the riot" banners (no level wording anywhere); no wave banners; the death message "OVERWHELMED - n kills"; after the respawn teleport the **results panel** (title "HORDE - S - SPECTACULAR", four rows Enemy / Style / Combo / Survival, tier banners AMAZING / SPECTACULAR / ULTIMATE with 8000 / 20000 / 40000). Questions: do the summary rows show now? Do the token-style tier rewards look like we hand out tokens (change if so)? Does Continue (Cross / A / 4, or Interact) dismiss it? Is `EndlessMode_horde_scores.csv` written and shown on the dock panel afterwards?
5. **Scoring feel (`run horde` for real, no god):** is the style meter too fast or slow, are ranks reachable, are the Survival and Combo amounts sensible? Tune from `EndlessArena/docs/SCORING.md`.
6. **Memory and models (long run, 15+ minutes, or the parade):** no white cubes, no invisible enemies, no crash. Both modes now use 5 factions per run and release assets 30 s after the run ends. `parade all` still shows cubes by design (86 types).
7. **Enemy placement:** nobody pops in in front of you (spawns are 18 m+ away, 45 m+ if in view); stationary enemies get moved closer out of view; bodies stay on the ground until the pool is nearly full and nobody is looking.
8. **Loading a save at Ryker's dock:** walking across the start line starts a run without leaving first; the exit circle leads to the right dock after a restart (`[portal] last_venue`).
9. **Dev checks:** `inv text <asset id>` for a couple of powers and mods (names should match the suit menu, including the modded Iron Arms); `gfx wave 3 6 icon_map_demons_base`; `uilog on` still works.

## Recon and open work (after the tests)
- **Map icons and waypoints:** icons come from actors with a MapElement component (docs/UI_HUNT.md, "Map recording"). Stand next to a hideout or challenge start, `near 15 mapelement`, read the component and its config, then try it on a dock circle. Waypoints: UIUpdateCustomWaypointAction (exe+7efd10).
- **Reward-pick screen:** nothing near-fullscreen found yet. The suit menu's preview video and image paths are known (`tools/assets/perk_text.json`). Keep looking (PopupShell, tutorial pop-up exe+13f1670, ObjectiveScreenShell).
- **Which UI for which message:** decide with the crime pop-up (ShowCrimeScanner), objective line (ShowMissionObjective), medium message types and the notification box, for variety (ledger, "UI variety").
- **Curated pools** (drop or special-case Equalizer, Quips, Sound Of Silence, Combat Analyzer, Proximity Sensor, Long Range Scanner, Silent Step, Bio Scanner), gadget lock, autosave suppression, pause/resume runs, unowned-items test (needs an early save).
- **Still open from earlier runs:** Mister Negative idle, the mass electrocution curiosity, jetpack and gun white cubes (recheck now that nothing is released mid-run), Horde pacing under the new silent escalation.

---

# (older) In-game test plan (2026-10-13 build)

Run these one at a time. After each one, the user reports what they saw and the dev reads the log.

1. **Startup.** Launch and load free roam. Dev: the log shows `43/43 natives`, the hook installed automatically, and `stats: hero damage hook on`, with no crash.
2. **HUD.** Dev sends `hud Hello from Endless Mode`. User: does a notification box appear, and where? Then try `hud slot <n> text` for n = 1…20 to find a better wave-banner slot.
3. **Dock.** User swings to the chosen Manhattan dock and stands on the exact spot. Dev sends `portal setdock`.
4. **Exit pad.** Dev sends `portal enter`, or the user presses F8 (it should teleport and start a run). User walks to a sensible exit spot at Ryker's. Dev sends `wave stop`, then `portal setexit`.
5. **Pads.** The user stands on the exit pad (3 s countdown, back at the dock). Then on the dock pad (3 s, Ryker's, and the run starts with a countdown).
6. **Campaign.** Play a few waves. Check the wave titles in the HUD, that Street Thugs come first, the Skirmish → Assault → Full Force progression, and the boss at wave 4 (Bounty Hunter or Big Brute).
7. **Death.** Go down on purpose. Expected: the results message, and a return to the dock about 3 s after respawning. Dev: the run JSON exists and opens in `tools/runviewer/index.html`.
8. **Bosses, one at a time** (`boss <name> 15` near the user). For each: does it fight? Health readable? Killable? Then `boss verify <name> 1|0`. Order: tombstone, bounty, big brute (coop heavy), mrnegative, sable, taskmaster, hammerhead, mecha, rhino, scorpion, shocker, fisk, docock.
9. **Health scaling.** `run wave=9`: the log shows `starts with … (scaling)`, and the enemies take visibly more hits.
10. **Stress.** `stress 10 200` at Ryker's, then `stress 10 120 engaged`. The user also reads the Steam overlay FPS at a few steps to confirm the tick rate matches the frame rate.
11. **Riot and shuffle.** `run riot shuffle` for the next Discord clip.
