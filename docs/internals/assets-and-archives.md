# Assets, models, archives and the toc

## Assets and models

### Loading and reference counts

- `LoadAssetByHash` returns an asset whose `byte [asset+1] == 1` once loaded and which holds a `u16` reference count at `[asset+4]`.
- `AssetRelease(manager, asset)` = exe+1626940 drops one reference. **We never release during a run** (see below).
- An enemy actor is not just its `.actor`: it depends on other actors (weapons, jetpacks, heads, ammo items, projectiles).
  These must be loaded too or the enemy spawns with **white cubes** where the missing models belong. The chain is
  `.actor -> rifle_firing config -> WeaponItemConfig -> ItemActor` for weapons and jetpacks. `tools/assets/deps.py` reads
  the archives and generates `src/preload_deps.inc`; `spawn::Preload` loads those extras and `spawn::Ready` waits for them
  (20 s cap, then logs "not fully loaded"). **[verified]**

### Model capacity and asset lifetime

- **Releasing assets mid-run is fatal**: surviving enemies lose their bodies (head and weapon only) and the game
  crashes at `exe+15f9bc9` (a null mesh pointer). So the mod releases only 30 s after a run ends. **[verified]**
- **The game runs out of room for enemy models** and breaks "whatever loads last": white cubes, floating heads, invisible enemies.
  Observations: 33 unit types fine; 39 fine; 50 broke Olympus (Horde); a 57-type "parade" at Ryker's was fine; 86 broke. The real limit
  and cause are **[?]** (candidates: live bodies and enemies, mid-run loading, or something specific to a faction).
- Therefore pools are chosen by a **unit-type budget** (`plan::kMaxTypes`, default 44; `[run] max_types` in the ini):
  `plan::BudgetedPool` adds factions greedily until the budget is spent. Faction sizes: street 7, fisk 12, demons 9, prisoners
  15, sable 9, hammerhead 10, olympus 11, tombstone 8, taskmaster 8 (89 types in all).
- A history of the "respawn load hang after a long Horde run" (all nine factions, 1000+ assets) fixed by the budget and the
  post-run release is in [BUGS.md](../../EndlessArena/docs/BUGS.md).

### Archive format notes (from the ALERT wiki)

Assets live in `asset_archive`, found via the `toc`; asset ids are CRC64 of the file name; containers are DAT1; `.config` files
are binary JSON-like with a `Type`. `tools/ALERT` (a clone of Tkachov's ALERT, not in this repo) converts configs to JSON and back.
`tools/toctool` searches the table of contents, and `tools/assets/extract.py` pulls assets out.
Don't commit extracted bytes.

## Archives, the toc object and texture streaming (2026-11-15; `[RE]`, not run)

- **File system**: `fs = q[exe+74a6a00]` (`FileSystem::ArchiveFileSystem`, vtable `exe+3cf9920`). `fs+0x10` = toc DAT1 buffer, **`fs+0x18` = the toc object** (section pointers set by
  `exe+16d7710`), `q[fs+0x40]` = location table (8 bytes per entry: archive index, offset), `q[fs+0x68]` = per-archive open table (0x200 archives max). No heap scan is needed.
- **Reads are by id**: `exe+16d83e0(request, assetId, kind)` -> `fs->vtable[+0x28]` (`exe+16da4c0`) -> `exe+16d7990(toc, id, languageGroup, kind)`; `size = dword[entry+4]`,
  `location = q[q[fs+0x40] + dword[entry+8]*8]`; the archive file is opened on demand (`exe+16da380`). Kind 0 = the asset, kind 1 = a texture's pixel stream.
- **Textures**: `TextureManager` static `exe+621b1a0`, records 0xc0 bytes at `q[exe+621b2b0]` (`+0x34` highMipSize, `+0x84/+0x86` height/width, `+0x8e/+0x8f` mip nibbles);
  `TextureStream` static `exe+621b4e0`, tick `exe+19b6d70`, request one texture `exe+19b5d70(stream, record, 0)`, evict one `exe+19b5e70(stream, record, idx)`, staging buffer
  `exe+19b7f30`, finish job `exe+19b5a20`. Runtime textures from memory: `exe+19b1500` (2D), `exe+19b1280` (3D).
