# Echoes of Time 1.0.0

Two time-based abilities from *Spider-Man: Edge of Time*, for Marvel's Spider-Man Remastered 4.0630.0.0, chosen by the **ABILITY** row at the top of the mod's tab:

- **Hyper-Sense** (the Amazing Spider-Man's power in the game): slow motion (**T** / **D-pad Right**) plus a fading trail of Spider-Man after-images. Supersedes Quantum Break Renewed (do not install both: both toggle the same keys and write the same time-scale fields).
- **Accelerated Decoy** (Spider-Man 2099's power in the game): the same button leaves **one** copy of Spider-Man at the player's current position, in the same pose and suit. Enemies target the decoy before Spider-Man. No slow motion in this ability, apart from a brief beat when the decoy appears.

All settings are in the in-game **ECHOES OF TIME** tab of the shared mod menu (Settings screen; code in `../ModMenu`); each choice is saved at once to
`Documents\Marvel's Spider-Man Remastered\EchoesOfTime\EchoesOfTime.ini` (`[menu]`, choice indexes). Each ability keeps its own rows, so switching away and back finds them as they were.
`[advanced]` there has knobs with no row: `min_speed`, `max`, `spacing`, `y_offset`, `yaw_offset`, `back_offset`, `hide_spawn`, `pose_copy`, `only_slowmo`, `actor`, `effect` (a .material path), and for the decoy
`decoy_hide_spawn`, `decoy_hide_end`, `decoy_show_target` (see below).

## Hyper-Sense

Presets: the PRESET row (Hyper-Sense only) applies one of the ini files in `Documents\Marvel's Spider-Man Remastered\EchoesOfTime\Presets\` (11 are written on first run: Default, Flash, Reverse Flash, Hologram Trail,
Scanline Echo, Ghost Train, Frozen Moment, Rainbow Burst, Matrix, Shadow Clones, Rising Spirits). Files hold `key=LABEL` lines, any row key, missing key = default; MESSAGES is never part of a preset. CUSTOM is your own
unsaved settings (kept in `custom.ini`). To make a preset, copy an existing file in that folder, rename it and edit the values (it shows up after the menu is closed and reopened). Share a preset by copying its file.

Rows: slow-motion strength, Spider-Man speed (1x/1.5x/2x/3x, the hero-vs-world split), messages; after-images mode / look (the original, hologram, scanlines, red glow, flat grey, distortion, fear, highlight, plain
colours and RAINBOW) / clone decay / opacity decay scale / starting opacity / clone spacing / flicker; experimental: clone drift, clone size over life, speed blur, speed streaks.

Behaviour carried over from QBR 0.3.0: clones only while slow motion is on, keep spawning while standing still, copy Spider-Man's current pose, held still in pause/photo mode, all removed the moment slow motion ends,
spawned out of sight to hide the A-pose frame.

## Accelerated Decoy

Rows: DECOY DURATION (3 to 60 s), DECOY LOOK (default: HOLOGRAM; the DEFAULT look is Spider-Man's own suit), DECOY OPACITY, DECOY FLICKER (default HIGH; it also flickers out when it ends), PLAYER FLICKER (default HIGH; Spider-Man's own model flickers while the decoy is up and goes solid when it ends), ENTRY SLOW-MOTION (a ~0.35 s or ~0.8 s beat
when it appears), MESSAGES.

How it works (details and the in-game findings: `docs/internals/web-moves-and-afterimages.md` section 7):
- The game's own Holo Decoy actor (`proj_HoloDecoy_Deployed`) is what enemies attack: its TargetData carries the decoy flag, which the standard enemy targeting config ranks above the hero. It is spawned at Spider-Man's position and
  hidden: opacity 0 and its hologram overlay flag cleared on every game tick, scaled to nothing for its first 12 ticks (its first frames are an A-pose), kept from expiring by raising its lifetime and health fields, its 22
  voice lines (`SPID_GEN_HOLO_DECOY_001..022`) dropped before they play, and sent out of sight just before it is removed so its destruction effect is not seen.
- The visible copy is an after-image ghost (the Hyper-Sense machinery): exact pose, hologram look by default (the DEFAULT look is Spider-Man's own suit), flickering.
- Player flicker (`herofade.cpp`): the game rewrites Spider-Man's model opacity every frame, so writing it directly has no effect. Each of those writes is followed by a call to `SetModified` (`exe+191bf60`); that call is hooked, and while a decoy is up the opacity byte (`ModelInst+0xbe`) of Spider-Man's model and skin parts is set to a random value (between 1 minus the row's strength and 1) just before the call goes through.
- Enemies pick whoever has more aggro: a decoy beats Spider-Man at the same distance, but an enemy right next to Spider-Man can still prefer Spider-Man over a decoy far away. Snipers have no decoy tier and keep shooting Spider-Man;
  bosses with their own targeting are not covered.
- `[advanced]`: `decoy_hide_spawn` (1 = the first-ticks scale trick), `decoy_hide_end` (1 = send the game's decoy out of sight before removal), `decoy_show_target` (1 = leave the game's decoy visible, for testing).

## Death, restart, quit

Slow motion is switched off (and nothing is written to the time scale any more), the entry slow-motion is cancelled and any decoy is removed when the game sends `PlayerDeathEvent`, `HeroStartScriptedDeathEvent`, `HeroBeginRespawnEvent`, `HeroRequestRespawnEvent`, `CheckpointLoadedEvent` or `LevelLoadedEvent`, and when no hero exists for 300 ms (quit to the main menu, loading). Left on, the slow motion was written into the next hero and slowed the load.

## Build

`build.cmd` (shares `../common`: engine layer, `game/`, `modmenu/`, `build/`) packs `build/EchoesOfTime.script` and copies it to Overstrike's Mods Library.
Notes: [`../docs/internals/web-moves-and-afterimages.md`](../docs/internals/web-moves-and-afterimages.md). Author: Quail0. GPL-3.0-or-later.
