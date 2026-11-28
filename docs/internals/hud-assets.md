# HUD and interface assets: changing how the interface looks

How the game's interface is stored and what was learnt from replacing it with asset packages (no code in the game): the Scaleform movies, their scripts, the
maps, the icons and markers, the phone call panel and the ground circle that starts activities. Build 4.0630.0.0. Driving the HUD from a script mod is on
[ui](ui.md).

## Packaging and source files

- Overstrike's `.stage` package is a zip: `info.json` (`{"game":"MSMR","name":...,"author":...,"format_version":2}`) plus replacement assets at
  `0/<asset path>`. A package replaces whole assets, so two packages that touch one movie cannot combine: the one lower in Overstrike's list wins. **[verified]**
- Take source files from the original archive, through Overstrike's backup `asset_archive/toc.BAK`. Files extracted through the live `toc` can already carry
  another installed mod's changes. **[verified]**
- The movies are uncompressed GFx (`GFX` header). JPEXS FFDec converts them to XML and back (`-swf2xml`, `-xml2swf`), which allows shapes, sprites,
  placements and timelines to be edited as text. **[verified]**
- HUD movies are under `ui/export/HUD/`, shared symbol libraries under `ui/export/art/` (`Icons`, `MapIcons`), menus under `ui/export/`, `ui/export/pause/`
  and `ui/export/popups/`.

## Fonts

- `ui/export/fonts/Font_LatinAS3.gfx` holds the interface fonts as `DefineFont3` tags (20480 units per em). Ids 1, 2 and 5 are the faces the HUD text uses.
  Replacing glyph outlines while keeping each font's id, name, code list and line metrics changes the typeface everywhere; keep the original glyph for any code
  the new family lacks. **[verified]**

## Editing a movie's ActionScript

- Replace single methods as P-code. Recompiling a whole class with FFDec rewrites switches and name lookups in methods that were not touched. **[verified]**
- Spliced P-code must name public members only. A recompile can emit names in private namespaces that do not exist in the shipped movie; the element then
  fails to appear. **[verified]**
- A class linked to a sprite is sealed. Placing a new **named** instance on such a sprite's timeline makes the object throw while it is constructed, and the
  element disappears. Unnamed placements are safe. **[verified]**
- Removing a named part can break script that reads it (`information_window.status_text.status_text.text`). To hide a part safely, keep it and scale its
  placements to zero. **[verified]**
- A call into the game can be switched off by renaming its string in the movie's constant pool (same length), so that the game receives a name it does not
  know. **[verified]** with `communicatorShowWave`.

## Rendering behaviour of the movies

- A `SCREEN` blend mode on a placement had no visible effect on the corner map. `add` (8) is honoured on world markers. **[verified]**
- BC1 textures with punch-through alpha render as see-through. Clip masks are on/off only; a soft edge has to be drawn as shrinking dots. **[verified]**
- Overlapping filled outlines inside one shape break the shape. Use separate shapes, or line styles for open paths. **[verified]**
- In ActionScript a `TextField`'s `x` / `y` are the top left of its box, which is the placement point **plus** the text bounds' origin. **[verified]**
- The movie XML alone does not give on-screen sizes for symbols placed by the game's icon spawner; measure in game (see world markers below).

## Corner map

- The terrain is one texture, `ui/textures/hud/minimap/minimap_test.texture` (2964 x 3832, BC1 sRGB, one mip, pixel data from offset 128), painted in four flat
  colours: water, ground, buildings and bridges. Repainting by class restyles the map. **[verified]**
- `mask_map` is the map's clip shape; the game and the movie's script read the map's size from it. **[RE]**
- The game moves the compass strips (`compass.strip_1`, `compass.strip_2`) itself and does not call the movie's `MiniMap.UpdateCompass`. A replacement compass
  has to run every frame (for example from an `enterFrame` listener added in `Init`) and can read the heading from the strips' positions. **[verified]**

## Pause map colours

- Building colour comes from data: `configs/UIConfig/UIObjectDisplayer.config`, `MapData.BuildingsColor` (replaces the buildings' colour) with per-district
  entries. `GroundColor` there does not reach the visible ground. **[verified]**
- Ground, water and the outer boroughs are colour constants (12-byte linear RGB) in materials: `material/global/ui_map_buildings_01/ui_map_footprints_*`, the
  materials that the map zone `levels/i20_city/debug/impostor_districts/i20_ui_impostor_map_geo.zone` assigns itself (`background_map_notx`,
  `background_map_alpha`, `gbl_ui_map_waternoise`, `ui_map_buildings_01`, `ui_map_bridges_01`, `ui_map_roads_01`), and the Manhattan fade skirt. The values
  are in each material's serialized parameter section (tag `0xf5260180`). **[verified]**
- The unlit water reaches the screen at roughly a fifth of its linear value. **[verified]**
- The backdrop around the island is `ui/textures/pause/map/ui_mapbg.texture` (256 x 256, BC1 sRGB, one mip). **[verified]**

## Map icons

There are three separate copies of the icon art. Changing one leaves the others as they were. **[verified]**

| Movie | Classes | Shown on |
|---|---|---|
| `ui/export/HUD/MiniMap.gfx` | `icon_map_*` | the corner map |
| `ui/export/pause/Map.gfx` | `icon_map_*` (own copies, same names) | the pause map itself |
| `ui/export/art/MapIcons.gfx` | `icon_map_*_simple`, `*_simple_white` | the map legend and lists, some world markers |

- All three are vector shapes. Icon class names are referenced from game data (`UIObjectDisplayer.config`), so keep the names. **[verified]** / **[RE]**
- A map icon class has named children the game switches or edits: `icon`, `base`, `objective` (frame of the tracked goal), `active`, `complete`,
  `fast_travel`, `fast_travel_disabled`. Several classes have an empty first frame and their parts in the second.
- The game greys the named `icon` child of a finished activity. A nameless copy of the symbol keeps its colour. **[verified]**
- The player arrow's art points **up** on the corner map and **down** on the pause map; the game rotates each from that direction. Art that points the other way
  shows the heading backwards. **[verified]**

## World markers

- The markers are classes in `ui/export/HUD/POIs.gfx` (`HUD.elements.POI*`). Their symbols are created at run time by `utils.IconSpawner`, which looks a class
  up by name among the loaded movies: `icon_MissionPOI`, `icon_Waypoint_POI` and `icon_CrimePOI` come from `ui/export/art/Icons.gfx`;
  `POIObjective` also loads `MapIcons` classes. **[RE]**, symbol replacement **[verified]**.
- `POIMissionObjective` tints its symbol and brackets to one flat colour by type: goal `0xFFE000`, waypoint white, quest `0x78FFFF`. Parts that must stay dark
  have to sit outside the tinted clip. **[RE]**
- The movie places spawned symbols at 1.55 times, with an `add` blend, inside a clip that rests at 0.56 times. In game the crime symbol appears at that size,
  while the waypoint symbol appears about **2.75** times in the marker's units. The reason for the difference is `unknown`. **[verified]** (measured)
- Zip-to-point marker: `POIWebZip` (shapes 3, 5 and 7). Stealth detection marker: `POIStealth`. The takedown "safe / danger" tab is
  `POIs_fla.information_window_131`, placed by both `POITarget` and `POIInteract`. **[verified]** (hidden)

## Phone call panel

- `ui/export/HUD/Communicator.gfx`. `ShowCommunicator(nameHtml, picturePath, type)`, types 0 incoming, 1 outgoing, 2 broadcast, 3 voicemail, 4 radio,
  5 eavesdrop, 6 SIM cloning, 7 playback; `HideCommunicator()`. Element at HUD root `+0x240`, ActionScript object `+0x30`. **[verified]**
- The voice wave across the panel is **not** part of the movie. When the opening animation ends the movie calls `communicatorShowWave`; the game's handler
  (`exe+76e650`, hide `exe+76e570`, dispatch near `exe+73834d`) shows a separate model-HUD wave and registers a per-frame update (`exe+76e170`) that writes
  the call audio's level to the element's `+0x68`. Without the call the wave never appears. **[verified]**; handler details **[RE]**.
- In game the panel is cut off at about 316 units from the left edge of its content sprite. Cause `unknown`. **[verified]** (observed)

## Activity start circle

- The ground circle of hideouts, challenges and replays is `visualeffect/mission/mis_ui_base_replayreticle/mis_ui_base_replayreticle.actor`, model
  `models/emeyr/mis_ui_replayreticle.model`.
- Texture `textures/effects/i20/ui/ui_reticles_replayring.texture`: 512 x 512, RGBA8, one mip, the pixels being the last 1,048,576 bytes. The channels are
  masks: **R** the middle disc, **G** the outer ring, **B** the inner ring and centre point. Repainting the masks redraws the circle. **[verified]**
- Colours are three-float constants in `material/visualeffect/mission/mis_ui_reticles/mis_ui_replayreticle.material` (blue `(0, 0.227, 1)` and dark blue
  `(0.032, 0.084, 0.205)`) and in `material/visualeffect/mission/mis_ui_replayreticleverticalglow/mis_ui_replayreticleverticalglow.material`
  (`(0, 0.521, 1)`). **[verified]** (changed together)
