# World and arena

## World and arena

- **Ryker's Island** is a piece of the open world. The arena is its yard: the mod teleports the hero there (`arena::Teleport`), keeps
  them inside an invisible fence, and uses the 139 designer placements from `GP_A2_PRISON_EXT_GAMEPLAY.zone` as spawn points
  (`rykers_points.h`). The Raft interior (zone streaming via `ZoneAssetManager`) was tried; see `arena.cpp`.
- **Dock circles** in Manhattan are drawn with a spawned UI marker actor (`mis_ui_base_replayreticle`); the mod itself does the "standing on the circle" logic.
- Mission identifiers seen in community tools (the old Debug Menu mod): `GP_A2_PRISON` (the prison-break mission; the yard we use),
  `GP_A2_BLACKOUT`, `GP_A3_NYPD`, `OM_BASE_FISK_GREENWICH`, `OM_DEMONS_STORY_LI`, boss fights `GP_A1_FISK`, `VN_ELECTRO_FIGHT`,
  `VN_SCORPION_02`, `VN_TOMBSTONE_FIGHT`, `GP_A3_OTTOFIGHT`. Zone and level paths we actually use are in `arena.cpp`.
- The engine's own `ProximitySpawnSystem` (what Spider-Man 2's Chaos City uses) exists in this build with natives resolved in `natives.cpp` (`Prox*`); the mod does not use it (it needs a config and has no AI control we wanted).

