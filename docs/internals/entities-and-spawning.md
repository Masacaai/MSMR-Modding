# Entities, components, spawning

## Entities, components and handles

- **Entity** (from Script Hook's `GetComponentByName`/`GetTransform`): `[ent+0x00]` Transform pointer (position `+0x30`),
  `[ent+0x68]` component array of 16-byte entries `{desc*, instance*}`, component type name `(char*)[desc+0x60]`, `int16 [ent+0x70]`
  component count. **[verified]**
- **Actor table**: `[exe+7a44380]` points to entities stored inline, `0xC0` bytes each; count `u32 [exe+7a4439c]`.
  `GetActor(handle)` (exe+15a0560, same function as the SDK's `GetActor`) = base + index*0xC0. **[verified]**
- **Handle layout**: `(word[ent+0x10] & 0x7FF) << 20 | (dword[ent+0xC] & 0xFFFFF)`. **[verified]**
- Weak references: `ResolveRef` exe+16798f0 over a table of 16-byte `{object*, generation}` entries (`[exe+7a44320]`). **[RE]**
- Hero: `HeroSystem` at `exe+5d9dd00`; hero handle data `exe+5d9de68` (= HeroSystem+0x168). **[verified]**
- Finding a component by name and dumping all of an entity's components are dev commands (`comps`, `near`). A bot has
  ~130 components (`ThugBot`, `BotCombatAlternatives`, `AnimControllerComponent`, `TargetData`, `BotStatusEffectTracker*`, ...).

## Spawning and despawning actors

Spawning is a recipe the mod replays from Script Hook's disassembly (details in `docs/internals/natives-first-pass.md`). **[verified]**

```
hash  = ConvertStringToHash(path)                       exe+1bb25d0 (u64* out, const char*)
asset = LoadAssetByHash(&ActorAssetManager, hash, 0,0,0,0,0)    exe+1626380   (ByName: exe+1626300)
wait until byte [asset+1] == 1                           "loaded"
ent   = SpawnRaw(hash, 0, &Transform)                    exe+1f65170   (first arg is the u64 HASH, not the asset pointer)
sync  = CreateSyncObject(descriptor)                     exe+1980a60  (descriptor: dword 0, word slot, ...; slot = first null of the 0x2EE0-entry table at [exe+7a65f00])
handle = register: SyncRegisterHandle(sync, &handle)     exe+1986230
attach / activate: EntityInit exe+159db20, EntityAttach exe+159e590, SyncActivate exe+19877e0 (if flags&8), SyncFlag2, SyncFinish1/0
```

- Transform is 0x40 bytes: `[0]=sin t, [8]=-cos t, [0x14]=1, [0x20]=cos t, [0x28]=sin t, [0x30..0x38]=pos, [0x3C]=1`, t = heading in radians.
- A sync object begins with the actor handle. `GetActor(sync)` returns the entity.
- **Despawn**: find the sync object from the entity, `DeleteSyncObject(sync)` (exe+1981690), then on the game thread
  `EntityDestroy(entity)` (exe+159d500). **[verified]**
- All of it runs on the game thread via `gamethread::Run`; our worker blocks while it does.
- Spawn cost is 20-38 ms per enemy; each spawn is logged as `spawn: [1] ... [8] done` (the numbered steps are the stages above).

