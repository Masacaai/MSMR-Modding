# Game internals: the Spider-Man Remastered 4.0630.0.0 reference

This is the repository's reverse-engineering reference: how the game's engine works, as far as it has been worked out, written so
a modder (or an agent) can look a system up without knowing which mod found it. Everything comes from reading the game's executable,
its asset archives and memory, plus testing in game. All addresses are `Spider-Man.exe`-relative RVAs (`exe+RVA`; ImageBase
`0x140000000`) and **valid for build 4.0630.0.0 only**. The same pages are published, generated, on the project's GitHub Wiki
(see [../MAINTAINING_DOCS.md](../MAINTAINING_DOCS.md)); this folder is the source of truth.

**Confidence tags.** `[verified]` / `[V]`: tested in game. `[RE]`: read from disassembly, not exercised, or only partly. `[I]`: inferred.
`[?]`: unknown or a guess. Keep them honest when you extend a page. How things are found: [../REVERSE_ENGINEERING.md](../REVERSE_ENGINEERING.md).
Raw, regenerable dumps (types, enums, events, messages, component types, script nodes): [`../../recon/`](../../recon/README.md).

## Find it by system

| I want to know about... | Read |
|---|---|
| How a script DLL gets into the game, hooking the per-frame tick, running on the game thread | [loading-and-game-thread](loading-and-game-thread.md), [natives-first-pass](natives-first-pass.md) |
| Entities, components, handles; spawning and deleting actors; sync objects | [entities-and-spawning](entities-and-spawning.md) |
| Asset ids, the toc and archives, models, texture streaming, memory limits | [assets-and-archives](assets-and-archives.md) |
| Enemy AI, aggro, health, damage, boss phases | [combat-and-ai](combat-and-ai.md), [hero-abilities-gadgets-and-combat-events](hero-abilities-gadgets-and-combat-events.md) |
| The hero: ability locks, macro states, gadgets and the wheel, ammo, death and respawn, saving | [hero-abilities-gadgets-and-combat-events](hero-abilities-gadgets-and-combat-events.md), [crime-director-chases-and-script-nodes](crime-director-chases-and-script-nodes.md) |
| Disabling web moves; after-images and decoys (Echoes of Time, Disable Webs) | [web-moves-and-afterimages](web-moves-and-afterimages.md) |
| Inventory, perks, suit powers and mods | [inventory-and-perks](inventory-and-perks.md) |
| UI: Scaleform movies, the HUD, pause menu, Settings tabs, panels and what each accepts | [ui](ui.md), [ui-recordings](ui-recordings.md) |
| Time, camera, input (every device, the action layer) | [camera-time-and-input](camera-time-and-input.md) |
| Lighting modes, atmospheres, sky, sun, fog, weather (Manhattan Skies) | [lighting-and-atmosphere](lighting-and-atmosphere.md), [atmosphere-and-menu-recon-log](atmosphere-and-menu-recon-log.md) |
| Levels, zones, mission and checkpoint loading, the script-graph runtime, the crime director, chases | [world-and-levels](world-and-levels.md), [crime-director-chases-and-script-nodes](crime-director-chases-and-script-nodes.md) |
| Music, customization | [music-and-customization](music-and-customization.md) |
| The 2018 E3 start screen | [start-screen](start-screen.md) |
| The game's reflection tables (types, enums, events, messages) and the tool that reads them | [reflection-tables](reflection-tables.md) |
| Crashes and hangs seen so far, with their causes | [crashes](crashes.md) |
| What is deliberately not known yet | [unknowns](unknowns.md) |

Pages named `*-recon-log` or `ui-recordings` keep the order the work happened in (parts, recordings); the topic pages are organized by system.

## Every page and its sections

The block below is generated (`python tools/docs/gen_internals_index.py`); do not edit it by hand.

<!-- BEGIN GENERATED INDEX -->

### [Assets, models, archives and the toc](assets-and-archives.md)
- [Assets and models](assets-and-archives.md#assets-and-models)
- [Archives, the toc object and texture streaming (2026-11-15; `[RE]`, not run)](assets-and-archives.md#archives-the-toc-object-and-texture-streaming-2026-11-15-re-not-run)

### [Manhattan Skies: recon](atmosphere-and-menu-recon-log.md)
- [Lighting modes (weather lives here)](atmosphere-and-menu-recon-log.md#lighting-modes-weather-lives-here)
- [Atmospheres (looks without a lighting mode: Early Morning, extra environment sets)](atmosphere-and-menu-recon-log.md#atmospheres-looks-without-a-lighting-mode-early-morning-extra-environment-sets)
- [Settings tab](atmosphere-and-menu-recon-log.md#settings-tab)
- [S0 recorder (built 2026-11-08)](atmosphere-and-menu-recon-log.md#s0-recorder-built-2026-11-08)
- [One atmosphere asset per time-of-day slot](atmosphere-and-menu-recon-log.md#one-atmosphere-asset-per-time-of-day-slot)
- [Native looks beyond the three slots [V present in toc]](atmosphere-and-menu-recon-log.md#native-looks-beyond-the-three-slots-v-present-in-toc)
- [Consequences for the design [I]](atmosphere-and-menu-recon-log.md#consequences-for-the-design-i)
- [Lighting-mode index [V in game + disassembly]](atmosphere-and-menu-recon-log.md#lighting-mode-index-v-in-game--disassembly)
- [Atmosphere slots are an overlay layer [V log, I meaning]](atmosphere-and-menu-recon-log.md#atmosphere-slots-are-an-overlay-layer-v-log-i-meaning)
- [S1 build (2026-11-08, not yet run)](atmosphere-and-menu-recon-log.md#s1-build-2026-11-08-not-yet-run)
- [Findings [V]](atmosphere-and-menu-recon-log.md#findings-v)
- [The options-screen header [V disassembly of the movie + log]](atmosphere-and-menu-recon-log.md#the-options-screen-header-v-disassembly-of-the-movie--log)
- [Why the pause menu closes on a lighting change [V trace]](atmosphere-and-menu-recon-log.md#why-the-pause-menu-closes-on-a-lighting-change-v-trace)
- [Built, not yet run](atmosphere-and-menu-recon-log.md#built-not-yet-run)
- [Every atmosphere in the game [V]](atmosphere-and-menu-recon-log.md#every-atmosphere-in-the-game-v)
- [The lighting setup [V asset]](atmosphere-and-menu-recon-log.md#the-lighting-setup-v-asset)
- [Live slot edits [V in game]](atmosphere-and-menu-recon-log.md#live-slot-edits-v-in-game)
- [Loading any atmosphere [V in game]](atmosphere-and-menu-recon-log.md#loading-any-atmosphere-v-in-game)
- [Built, not yet run: the sky module (`ManhattanSkies/src/sky.cpp`)](atmosphere-and-menu-recon-log.md#built-not-yet-run-the-sky-module-manhattanskiessrcskycpp)
- [The sky override works [V in game]](atmosphere-and-menu-recon-log.md#the-sky-override-works-v-in-game)
- [Why the first browse failed after ~13 looks [V]](atmosphere-and-menu-recon-log.md#why-the-first-browse-failed-after-13-looks-v)
- [The fog table [V in game]](atmosphere-and-menu-recon-log.md#the-fog-table-v-in-game)
- [Built, not yet run](atmosphere-and-menu-recon-log.md#built-not-yet-run)
- [Method [V]](atmosphere-and-menu-recon-log.md#method-v)
- [What the slot's floats do [V in game, by screenshot]](atmosphere-and-menu-recon-log.md#what-the-slots-floats-do-v-in-game-by-screenshot)
- [Built, not yet run](atmosphere-and-menu-recon-log.md#built-not-yet-run)
- [Ruled out [V]](atmosphere-and-menu-recon-log.md#ruled-out-v)
- [Known](atmosphere-and-menu-recon-log.md#known)
- [Next leads (not started)](atmosphere-and-menu-recon-log.md#next-leads-not-started)
- [What the "NYC Warm Night" mod shows (2026-11-09) [V, file diff against toc.BAK]](atmosphere-and-menu-recon-log.md#what-the-nyc-warm-night-mod-shows-2026-11-09-v-file-diff-against-tocbak)
- [Part 14. Stage-0 gate for window and street lights (2026-11-09): not practical](atmosphere-and-menu-recon-log.md#part-14-stage-0-gate-for-window-and-street-lights-2026-11-09-not-practical)
- [Part 15. Rain experiment (2026-11-09): dropped](atmosphere-and-menu-recon-log.md#part-15-rain-experiment-2026-11-09-dropped)
- [Part 16. The dynamic cycle: what the game let us do (2026-11-09/10)](atmosphere-and-menu-recon-log.md#part-16-the-dynamic-cycle-what-the-game-let-us-do-2026-11-0910)
- [Part 17. Loading an atmosphere from a loose file at run time (2026-11-13): the core works](atmosphere-and-menu-recon-log.md#part-17-loading-an-atmosphere-from-a-loose-file-at-run-time-2026-11-13-the-core-works)
- [Part 17c. `.modular` atmosphere mods and the cycle's atmosphere pool (2026-11-13, built, not yet played)](atmosphere-and-menu-recon-log.md#part-17c-modular-atmosphere-mods-and-the-cycles-atmosphere-pool-2026-11-13-built-not-yet-played)
- [Part 17d. Black sky after a cycle switch to an atmosphere mod (2026-11-13, fixed, tested)](atmosphere-and-menu-recon-log.md#part-17d-black-sky-after-a-cycle-switch-to-an-atmosphere-mod-2026-11-13-fixed-tested)
- [Part 17e. Which archive the new assets borrow, and the placeholder idea (2026-11-14)](atmosphere-and-menu-recon-log.md#part-17e-which-archive-the-new-assets-borrow-and-the-placeholder-idea-2026-11-14)
- [Part 17f. Files an atmosphere mod ships that the loader does not use (2026-11-14)](atmosphere-and-menu-recon-log.md#part-17f-files-an-atmosphere-mod-ships-that-the-loader-does-not-use-2026-11-14)
- [Part 18. The lighting modes by name, and why a switch is a "mini-load"](atmosphere-and-menu-recon-log.md#part-18-the-lighting-modes-by-name-and-why-a-switch-is-a-mini-load)
- [Part 19. The atmosphere asset, fully named (`AtmosphereDef`), and how it maps into the 0x8c0 slot](atmosphere-and-menu-recon-log.md#part-19-the-atmosphere-asset-fully-named-atmospheredef-and-how-it-maps-into-the-0x8c0-slot)
- [Part 20. The baked lighting of an atmosphere, and runtime textures](atmosphere-and-menu-recon-log.md#part-20-the-baked-lighting-of-an-atmosphere-and-runtime-textures)
- [Part 21. Cross-fades exist in the engine: `AtmosphereBlender` and `ActivateAtmosphereAction`](atmosphere-and-menu-recon-log.md#part-21-cross-fades-exist-in-the-engine-atmosphereblender-and-activateatmosphereaction)
- [Part 22. Nodes and conditions that set or read the lighting](atmosphere-and-menu-recon-log.md#part-22-nodes-and-conditions-that-set-or-read-the-lighting)
- [Part 23. The toc object without a scan, by-id reads, and evicting a texture](atmosphere-and-menu-recon-log.md#part-23-the-toc-object-without-a-scan-by-id-reads-and-evicting-a-texture)
- [Part 24. The Settings-screen crash at `exe+31e9582`: what the faulting code is](atmosphere-and-menu-recon-log.md#part-24-the-settings-screen-crash-at-exe31e9582-what-the-faulting-code-is)
- [Part 25. Offline checks of the static maps, and what was built from them (2026-11-19)](atmosphere-and-menu-recon-log.md#part-25-offline-checks-of-the-static-maps-and-what-was-built-from-them-2026-11-19)
- [Part 26. Why atmosphere loads were "buggy": the slot's own values were captured too early (2026-11-19, fix built, not yet played)](atmosphere-and-menu-recon-log.md#part-26-why-atmosphere-loads-were-buggy-the-slots-own-values-were-captured-too-early-2026-11-19-fix-built-not-yet-played)
- [Part 27. Cleanup pass 1 (2026-11-20): what was removed and why](atmosphere-and-menu-recon-log.md#part-27-cleanup-pass-1-2026-11-20-what-was-removed-and-why)

### [Time, camera and input](camera-time-and-input.md)
- [Time, camera and input](camera-time-and-input.md#time-camera-and-input)
- [Input: the game's action layer, for every device (2026-11-15; `[RE]`, read from the exe, not run)](camera-time-and-input.md#input-the-games-action-layer-for-every-device-2026-11-15-re-read-from-the-exe-not-run)

### [Combat: enemy AI, health, damage, bosses](combat-and-ai.md)
- [Enemy AI and aggro](combat-and-ai.md#enemy-ai-and-aggro)
- [Health and damage](combat-and-ai.md#health-and-damage)
- [Bosses](combat-and-ai.md#bosses)
- [Boss phases (2026-10-19, disassembly; not yet run in game)](combat-and-ai.md#boss-phases-2026-10-19-disassembly-not-yet-run-in-game)

### [Crash and hang catalogue](crashes.md)
- [Crash and hang catalogue](crashes.md#crash-and-hang-catalogue)

### [Miscellaneous recon: crime director, chases, script nodes, loader, hero death (static analysis, 2026-11-15)](crime-director-chases-and-script-nodes.md)
- [1. The crime director: `RandomCrimeSystem2`](crime-director-chases-and-script-nodes.md#1-the-crime-director-randomcrimesystem2)
- [2. Loading a level or checkpoint, and warping to a checkpoint](crime-director-chases-and-script-nodes.md#2-loading-a-level-or-checkpoint-and-warping-to-a-checkpoint)
- [3. The chase system that is already in the game](crime-director-chases-and-script-nodes.md#3-the-chase-system-that-is-already-in-the-game)
- [4. Race-like content: the drone chase challenge](crime-director-chases-and-script-nodes.md#4-race-like-content-the-drone-chase-challenge)
- [5. The script-graph runtime, and a catalogue of 1,032 node types](crime-director-chases-and-script-nodes.md#5-the-script-graph-runtime-and-a-catalogue-of-1032-node-types)
- [6. State predicates and hero state for race rules](crime-director-chases-and-script-nodes.md#6-state-predicates-and-hero-state-for-race-rules)
- [7. Hero death and respawn](crime-director-chases-and-script-nodes.md#7-hero-death-and-respawn)
- [8. Events, damage and scoring signals](crime-director-chases-and-script-nodes.md#8-events-damage-and-scoring-signals)
- [9. Not answered this session](crime-director-chases-and-script-nodes.md#9-not-answered-this-session)
- [10. The camera, a world raycast and the views table](crime-director-chases-and-script-nodes.md#10-the-camera-a-world-raycast-and-the-views-table)
- [11. Markers, text and debug drawing](crime-director-chases-and-script-nodes.md#11-markers-text-and-debug-drawing)
- [12. Photo mode and free cameras: what exists](crime-director-chases-and-script-nodes.md#12-photo-mode-and-free-cameras-what-exists)

### [Entities, components, spawning](entities-and-spawning.md)
- [Entities, components and handles](entities-and-spawning.md#entities-components-and-handles)
- [Spawning and despawning actors](entities-and-spawning.md#spawning-and-despawning-actors)

### [Hero abilities, gadgets, damage events and saving (static analysis, 2026-11-15)](hero-abilities-gadgets-and-combat-events.md)
- [1. `HeroAbilityLock`: the full ability table, the layers, the game's own API](hero-abilities-gadgets-and-combat-events.md#1-heroabilitylock-the-full-ability-table-the-layers-the-games-own-api)
- [2. How missions take moves away: `ToggleHeroMovesAction` and `HeroLockAction`](hero-abilities-gadgets-and-combat-events.md#2-how-missions-take-moves-away-toggleheromovesaction-and-herolockaction)
- [3. The damage message is the `DamageRequest` event; full layout](hero-abilities-gadgets-and-combat-events.md#3-the-damage-message-is-the-damagerequest-event-full-layout)
- [4. Combat events to listen to](hero-abilities-gadgets-and-combat-events.md#4-combat-events-to-listen-to)
- [5. `EquipGadget` decompiled: it is `WeaponManagerBase::Equip`, and it takes five arguments](hero-abilities-gadgets-and-combat-events.md#5-equipgadget-decompiled-it-is-weaponmanagerbaseequip-and-it-takes-five-arguments)
- [6. Gadget ammo is an inventory item count, kept by `GadgetItemAmmoManager`](hero-abilities-gadgets-and-combat-events.md#6-gadget-ammo-is-an-inventory-item-count-kept-by-gadgetitemammomanager)
- [7. The gadget wheel's list: where it comes from, why a granted gadget is red, how to refresh](hero-abilities-gadgets-and-combat-events.md#7-the-gadget-wheels-list-where-it-comes-from-why-a-granted-gadget-is-red-how-to-refresh)
- [8. "Airborne" and the other hero macro states](hero-abilities-gadgets-and-combat-events.md#8-airborne-and-the-other-hero-macro-states)
- [9. The real faulting context of every dump, and what the four gadget crashes were](hero-abilities-gadgets-and-combat-events.md#9-the-real-faulting-context-of-every-dump-and-what-the-four-gadget-crashes-were)
- [10. Killing properly, flooring at 1 HP, and damage flags by name](hero-abilities-gadgets-and-combat-events.md#10-killing-properly-flooring-at-1-hp-and-damage-flags-by-name)
- [11. Saving: what records a checkpoint, and the game's own autosave switch](hero-abilities-gadgets-and-combat-events.md#11-saving-what-records-a-checkpoint-and-the-games-own-autosave-switch)
- [12. Input for the dock and exit circles on every device](hero-abilities-gadgets-and-combat-events.md#12-input-for-the-dock-and-exit-circles-on-every-device)
- [13. Spawning out of view, deleting, engagement: leads from the node catalogue](hero-abilities-gadgets-and-combat-events.md#13-spawning-out-of-view-deleting-engagement-leads-from-the-node-catalogue)

### [Inventory, suit powers and mods](inventory-and-perks.md)
- [Inventory, suit powers and mods](inventory-and-perks.md#inventory-suit-powers-and-mods)

### [Lighting, atmospheres and time of day](lighting-and-atmosphere.md)
- [Lighting, atmospheres and the time of day (2026-11-09; found for Manhattan Skies, details in `docs/internals/atmosphere-and-menu-recon-log.md`)](lighting-and-atmosphere.md#lighting-atmospheres-and-the-time-of-day-2026-11-09-found-for-manhattan-skies-details-in-docsinternalsatmosphere-and-menu-recon-logmd)

### [Loading code and the game thread](loading-and-game-thread.md)
- [Loading code into the game](loading-and-game-thread.md#loading-code-into-the-game)
- [The game thread](loading-and-game-thread.md#the-game-thread)

### [Next release recon: dynamic music and in-game customization (started 2026-10-18)](music-and-customization.md)
- [1. Music](music-and-customization.md#1-music)
- [2. In-game customization](music-and-customization.md#2-in-game-customization)
- [3. Prior art found: SpidyVR (read 2026-10-19)](music-and-customization.md#3-prior-art-found-spidyvr-read-2026-10-19)
- [4. Findings of 2026-10-19 (static, not yet run in game)](music-and-customization.md#4-findings-of-2026-10-19-static-not-yet-run-in-game)

### [M1 recon: game natives on Spider-Man.exe 4.0630.0.0](natives-first-pass.md)
- [Method](natives-first-pass.md#method)
- [Resolved natives](natives-first-pass.md#resolved-natives)
- [SpawnActor flow (from Script Hook's disassembly)](natives-first-pass.md#spawnactor-flow-from-script-hooks-disassembly)
- [Resolved after the first pass](natives-first-pass.md#resolved-after-the-first-pass)
- [Game thread](natives-first-pass.md#game-thread)
- [Correct spawn sequence (from jedijosh920's Mod Menu, `scripts\SMPCModMenu.dll`)](natives-first-pass.md#correct-spawn-sequence-from-jedijosh920s-mod-menu-scriptssmpcmodmenudll)
- [Enemy Generator (Mod Menu minigame) = prior art for our director](natives-first-pass.md#enemy-generator-mod-menu-minigame--prior-art-for-our-director)
- [Entity layout (from Script Hook GetComponentByName / GetTransform / GetPosition)](natives-first-pass.md#entity-layout-from-script-hook-getcomponentbyname--gettransform--getposition)
- [Arena leads (M7)](natives-first-pass.md#arena-leads-m7)
- [The Raft (prison-break arena), found offline](natives-first-pass.md#the-raft-prison-break-arena-found-offline)
- [In-game results (2026-10-12, first session)](natives-first-pass.md#in-game-results-2026-10-12-first-session)
- [Aggro leads (not done)](natives-first-pass.md#aggro-leads-not-done)
- [Ryker's Island (the arena) — 2026-10-12](natives-first-pass.md#rykers-island-the-arena--2026-10-12)
- [Bosses / factions (from boss-fight gameplay zones)](natives-first-pass.md#bosses--factions-from-boss-fight-gameplay-zones)
- [Aggro: best lead](natives-first-pass.md#aggro-best-lead)
- [Aggro state: live memory diff (2026-10-12)](natives-first-pass.md#aggro-state-live-memory-diff-2026-10-12)
- [Aggro: what the data says (2026-10-12, later)](natives-first-pass.md#aggro-what-the-data-says-2026-10-12-later)
- [ProximitySpawnSystem: the engine's own enemy spawner (the aggro and pop-in fix)](natives-first-pass.md#proximityspawnsystem-the-engines-own-enemy-spawner-the-aggro-and-pop-in-fix)
- [Aggro attempt 3: idle vision widened (failed) → damage-based aggro (in progress)](natives-first-pass.md#aggro-attempt-3-idle-vision-widened-failed--damage-based-aggro-in-progress)
- [Aggro attempt 4: damage pipeline (2026-10-12 evening) — inconclusive, parked](natives-first-pass.md#aggro-attempt-4-damage-pipeline-2026-10-12-evening--inconclusive-parked)
- [Hideout wave spawning (backtrace observer, 2026-10-12)](natives-first-pass.md#hideout-wave-spawning-backtrace-observer-2026-10-12)
- [Hideout enemies vs ours: component diff (live, 2026-10-12 21:55)](natives-first-pass.md#hideout-enemies-vs-ours-component-diff-live-2026-10-12-2155)
- [Event system and bot groups (offline RE, 2026-10-12 night) — untested in game](natives-first-pass.md#event-system-and-bot-groups-offline-re-2026-10-12-night--untested-in-game)
- [Event/group tests in game (2026-10-12, 22:24–22:37)](natives-first-pass.md#eventgroup-tests-in-game-2026-10-12-22242237)
- [Reframe: it's the Skirmish system, and the aggro switch is `BotData.SpawnEngagedWithHero` (offline, 2026-10-12 late)](natives-first-pass.md#reframe-its-the-skirmish-system-and-the-aggro-switch-is-botdataspawnengagedwithhero-offline-2026-10-12-late)
- [Actor paths](natives-first-pass.md#actor-paths)
- [CONFIRMED in game (2026-10-12 22:54): engaged spawns work](natives-first-pass.md#confirmed-in-game-2026-10-12-2254-engaged-spawns-work)
- [Offline build 2026-10-13 (untested in game)](natives-first-pass.md#offline-build-2026-10-13-untested-in-game)
- [Gameplay camera (verified in game 2026-10-13)](natives-first-pass.md#gameplay-camera-verified-in-game-2026-10-13)
- [Perks and inventory (static RE, 2026-10-13; not yet called live)](natives-first-pass.md#perks-and-inventory-static-re-2026-10-13-not-yet-called-live)
- [Equipped loadout (verified live 2026-10-13)](natives-first-pass.md#equipped-loadout-verified-live-2026-10-13)
- [Equip path (watchpoint, 2026-10-13)](natives-first-pass.md#equip-path-watchpoint-2026-10-13)

### [Reflection tables: types, enums, events, component types](reflection-tables.md)
- [the game describes itself (reflection tables, events, component types)](reflection-tables.md#the-game-describes-itself-reflection-tables-events-component-types)

### [E3 2018 start screen recon: "Spidey on his phone, press X, jump off into the city" (2026-11-20)](start-screen.md)
- [Verdict](start-screen.md#verdict)
- [1. The scene](start-screen.md#1-the-scene)
- [2. The demo title-screen option list](start-screen.md#2-the-demo-title-screen-option-list)
- [3. The mission graph that runs the demo](start-screen.md#3-the-mission-graph-that-runs-the-demo)
- [4. What is not there](start-screen.md#4-what-is-not-there)
- [5. How the demo path is gated (open)](start-screen.md#5-how-the-demo-path-is-gated-open)
- [6. Ways to bring it back](start-screen.md#6-ways-to-bring-it-back)
- [7. Next steps (one at a time, nothing run yet)](start-screen.md#7-next-steps-one-at-a-time-nothing-run-yet)
- [Facts to remember](start-screen.md#facts-to-remember)
- [8. First in-game test (built 2026-11-20, not yet run)](start-screen.md#8-first-in-game-test-built-2026-11-20-not-yet-run)

### [UI hunt: the game's UI and how to drive it (started 2026-10-13)](ui-recordings.md)
- [How the UI is built](ui-recordings.md#how-the-ui-is-built)
- [Calling ActionScript from code (static RE, not yet called by us)](ui-recordings.md#calling-actionscript-from-code-static-re-not-yet-called-by-us)
- [Candidate widgets per need](ui-recordings.md#candidate-widgets-per-need)
- [In-game test plan (one step at a time)](ui-recordings.md#in-game-test-plan-one-step-at-a-time)
- [Live results, session 1 (2026-10-13 22:03)](ui-recordings.md#live-results-session-1-2026-10-13-2203)
- [Live results, session 2 (2026-10-13 22:09-22:15)](ui-recordings.md#live-results-session-2-2026-10-13-2209-2215)
- [Live results, session 3 (2026-10-14 22:48-23:05)](ui-recordings.md#live-results-session-3-2026-10-14-2248-2305)
- [Recordings (uilog), session 3 (2026-10-14 23:32-)](ui-recordings.md#recordings-uilog-session-3-2026-10-14-2332-)
- [Hideout recording (2026-10-14 23:37-23:42, uilog with SetMember + element tags)](ui-recordings.md#hideout-recording-2026-10-14-2337-2342-uilog-with-setmember--element-tags)
- [Taskmaster combat challenge recording (2026-10-14 23:46-23:51)](ui-recordings.md#taskmaster-combat-challenge-recording-2026-10-14-2346-2351)
- [Suit menu recording (2026-10-15 00:56)](ui-recordings.md#suit-menu-recording-2026-10-15-0056)
- [Map recording (2026-10-15 ~01:10)](ui-recordings.md#map-recording-2026-10-15-0110)
- [Map icons, offline findings (2026-10-15)](ui-recordings.md#map-icons-offline-findings-2026-10-15)

### [UI: Scaleform, the pause menu and mod tabs](ui.md)
- [UI (Scaleform)](ui.md#ui-scaleform)
- [The pause menu and mod tabs in Settings (2026-11-08; `common/modmenu/`, `docs/internals/atmosphere-and-menu-recon-log.md` Parts 5-9)](ui.md#the-pause-menu-and-mod-tabs-in-settings-2026-11-08-commonmodmenu-docsinternalsatmosphere-and-menu-recon-logmd-parts-5-9)

### [Things we deliberately don't know yet](unknowns.md)
- [Things we deliberately don't know yet](unknowns.md#things-we-deliberately-dont-know-yet)

### [Flash overhaul recon: no-web lock and after-images (2026-11-19)](web-moves-and-afterimages.md)
- [1. Disabled webs](web-moves-and-afterimages.md#1-disabled-webs)
- [2. After-images (hypersense)](web-moves-and-afterimages.md#2-after-images-hypersense)
- [3. What the MSM2 mod "Edge of Time Hypersense" teaches (2026-11-19)](web-moves-and-afterimages.md#3-what-the-msm2-mod-edge-of-time-hypersense-teaches-2026-11-19)
- [4. Built from this recon (2026-11-19)](web-moves-and-afterimages.md#4-built-from-this-recon-2026-11-19)
- [5. Hyper-Sense crash record (2026-11-20 17:17, one occurrence, not fixed)](web-moves-and-afterimages.md#5-hyper-sense-crash-record-2026-11-20-1717-one-occurrence-not-fixed)
- [6. Running on water in slow motion (static recon 2026-11-21, nothing run in game; SHELVED by the maintainer the same day, no probe was ever run)](web-moves-and-afterimages.md#6-running-on-water-in-slow-motion-static-recon-2026-11-21-nothing-run-in-game-shelved-by-the-maintainer-the-same-day-no-probe-was-ever-run)
- [7. Accelerated Decoy (recon 2026-11-21, static only, nothing run in game)](web-moves-and-afterimages.md#7-accelerated-decoy-recon-2026-11-21-static-only-nothing-run-in-game)

### [World and arena](world-and-levels.md)
- [World and arena](world-and-levels.md#world-and-arena)

<!-- END GENERATED INDEX -->
