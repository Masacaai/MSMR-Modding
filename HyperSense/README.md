# Hyper-Sense 1.0.0

Slow motion (**T** / **D-pad Right**) plus a fading trail of Spider-Man after-images, for Marvel's Spider-Man Remastered 4.0630.0.0. Supersedes Quantum Break Renewed
(do not install both: both toggle the same keys and write the same time-scale fields).

All settings are in the in-game **HYPER-SENSE** tab of the shared mod menu (Settings screen; code in `../ModMenu`); each choice is saved at once to
`Documents\Marvel's Spider-Man Remastered\HyperSense\HyperSense.ini` (`[menu]`, choice indexes). `[advanced]` there has knobs with no row: `min_speed`, `max`, `spacing`, `y_offset`,
`yaw_offset`, `back_offset`, `hide_spawn`, `pose_copy`, `only_slowmo`, `actor`, `effect` (a .material path).

Presets (as in Manhattan Skies): the PRESET row at the top applies one of the ini files in `Documents\Marvel's Spider-Man Remastered\HyperSense\Presets\` (11 are written on first run: Default, Flash, Reverse Flash, Hologram Trail, Scanline Echo, Ghost Train, Frozen Moment, Rainbow Burst, Matrix, Shadow Clones, Rising Spirits). Files hold `key=LABEL` lines, any row key, missing key = default; MESSAGES is never part of a preset. CUSTOM is your own unsaved settings (kept in `custom.ini`); To make a preset, copy an existing file in that folder, rename it and edit the values (it shows up after the menu is closed and reopened). Share a preset by copying its file.

Rows: slow-motion strength, Spider-Man speed (1x/1.5x/2x/3x, the hero-vs-world split), messages; after-images mode / look (the original, hologram, scanlines, red glow, flat grey, distortion, fear,
highlight, plus plain colours and RAINBOW) / clone decay / opacity decay scale / starting opacity / clone spacing / flicker; experimental: clone drift, clone size over life, speed blur, speed streaks.
No lean (removed on request, 2026-11-20).

Behaviour carried over from QBR 0.3.0: clones only while slow motion is on, keep spawning while standing still, copy his current pose, held still in pause/photo mode, all removed the moment
slow motion ends, spawned out of sight to hide the A-pose frame.

Build: `build.cmd` (shares `../EndlessArena/src`, `../common`, `../ModMenu`), packs `build/HyperSense.script` and copies it to Overstrike's Mods Library.
Notes: `../docs/FLASH_RECON.md`, plan in `../docs/ROADMAP.md` section 6. Author: Quail0. GPL-3.0-or-later.
