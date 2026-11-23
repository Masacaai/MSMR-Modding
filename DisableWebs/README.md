# Disable Webs 1.0.0

An Overstrike `.script` for Marvel's Spider-Man Remastered (`Spider-Man.exe` 4.0630.0.0 only): Spider-Man cannot swing, web-zip, point-launch or shoot webs, and the web-assisted wall-run moves are gone too. Part of the [MSMR-Modding](../README.md) repository.
How it was found: [docs/internals/web-moves-and-afterimages.md](../docs/internals/web-moves-and-afterimages.md) section 1.

## Use

The lock starts **off**. **F7** (game window focused) turns it on and off; turning it off lifts only what this mod set. A message in the game's notification box confirms each switch. Nothing is written to disk except the log, `scripts\DisableWebs.log`.

## What it blocks (while on)

- Swing, zip to point, web snap, zip-jump, point launch, zip to a wall or ceiling, and the web shooter with every gadget shot (the game's own `HeroAbilityLock`, layer 0).
- The web combat moves: web strike, yanks, throws, web bomb and so on (`HeroCombatActionLocker`). The gadget wheel stays.
- **Wall-run corner tether** (hold DODGE at the edge of a wall): the game function that tries it (`HeroTransitionManager` slot 126, `exe+984be0`) is hooked and refused.
- **Wall-run pull-up** (JUMP while wall-running): `WallRunJump` (slot 128) and `WallRunLeap` (slot 125) are refused. This also removes the ordinary jump off a wall.

Left alone on purpose: melee, dodge, wall-run, wall-crawl, and the **swing-kick** (hold PUNCH in the air).

The locks are re-applied four times a second, so they survive a respawn. Every game function is checked against its expected address (or prologue bytes) before it is called or hooked; on any other game build nothing is touched.

## Build

`DisableWebsuild.cmd` (build Endless Arena once first: it installs MinHook) writes `build\DisableWebs.script` and copies it to Overstrike's Mods Library. Shares `natives` and `entity` with Endless Arena and `common/`. Author: Quail0. GPL-3.0-or-later.
