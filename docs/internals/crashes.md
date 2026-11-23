# Crash and hang catalogue

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

