# Reflection tables: types, enums, events, component types

## the game describes itself (reflection tables, events, component types)

All of this was **read from the exe on disk** (`[RE]`: nothing here was run in game). The tool is `tools/sigtools/fx.py` (commands in its header);
the dumps it writes go to `recon/*_4.0630.txt` (generated: regenerate rather than edit; they hold names and offsets only, no game bytes).

### DDL type descriptors: 9,716 reflected types with field names, types and runtime offsets
Every "DDL" type (each `*Event`, every config type such as `BotConfigMovement`, each component's `*Prius` data, script-action data) has a
**static descriptor in `.data`**:

```
+0x00 char* name            +0x08 u32 type hash (CRC-32 of the name)      +0x10 u32 base type hash    +0x14 u32 size of the runtime struct
+0x18 u32 field count       +0x48 u32* field name hashes   +0x50 char** field names   +0x58 u8* field kinds
+0x70 u32* array counts     +0x78 u32* field offsets (in the runtime struct)          +0x80 u32* field type hashes   +0x98 char** display names
kinds: 0 u8, 1 u16, 2 u32, 3 u64, 4 i8, 5 i16, 6 i32, 7 i64, 8 float, 9 double, 0xa String, 0xb enum, 0xc flags, 0xd struct, 0xf Boolean, 0x10 File,
       0x11 Tuid, 0x12 Json, 0x14 AssetId
```

- `fx.py ddl <regex>` prints matching types; `fx.py ddls recon/ddl_types_4.0630.txt` writes all of them. Example: `fx.py ddl "^DamageRequest$"`.
- The offsets are those of the **runtime object** a config or event is parsed into, so a config can be edited as data *and* read or patched live.
- A class's descriptor is returned by its vtable slot 7 (`lea rax, [descriptor]; ret`); slot 0 returns the name, slot 8 is `IsA(hash)`.

### Enums: 3,808 static enum descriptors
`{u32 type hash, u32 count, ..., +0x18 u32* value hashes, +0x20 char** value names}` in `.rdata` (a pointer table to them sits in `.data`).
`fx.py enum <regex>` (matches value or type names), `fx.py enums recon/enums_4.0630.txt`. Most enum *type* names are not in the exe; the dump lists
which DDL fields use each enum ("used by"), which names it in practice. Enum value hashes and type hashes are the same CRC-32.

### The name hash
Insomniac CRC-32: polynomial `0xEDB88320`, **initial value `0xEDB88320`, no final xor** (`fx.py h <name>`; `fx.py hn <hex>` looks a hash up among
every string in the exe). It is the id of events, DDL types and fields, enum values, script-node pins, mission names, message payload verbs.
In code the hasher is `exe+1bb88d0(const char*, 0xedb88320)`.

### Component and script-action type objects: 4,359 of them
Each component class registers a **type object** in `.data` (`recon/component_types_4.0630.txt`: `<type object RVA> <name> <factory...>`), e.g.
`HeroAbilityLock exe+6daad90`, `HeroLocal exe+6da9d90`, `HeroCameraManager exe+6dd6030`, `HeroMoverManager exe+6dd6c70`, `HeroWeaponManager exe+6dd8370`,
`BotCommandGoto exe+7bf6840`. Lookup by type, as the game does everywhere:

```
comp = word[entity+0x88] != 0 ? exe+16799a0(entity+0x80, &type) : exe+167ab40(entity+0x58, &type)     // null if the entity has none
```

`fx.py` annotates these addresses in disassembly as `type:<Name>`, and `fx.py x <type rva>` lists every function that looks the component up
(that is how the 173 users of `HeroAbilityLock` were found).

### Events: the whole catalogue, and how handlers are registered
`tools/sigtools/events.py recon/events_4.0630.txt` lists **3,061 events**: id, name, payload size and fields (from the DDL descriptor), the
functions that send it and the handlers systems register for it.

- Send: `SendEvent exe+16d5e80(&EventSys /*exe+7475690*/, u32 id, u32 target handle (0 = broadcast), source, 0, 0, 0, 0, 1, f, f, payload, size)`.
  The event object is an `EventBase` (0x138 bytes: `+0x28 Frame`, `+0x2c SenderHandle`, `+0x30 TrackedNameHash`, `+0x40 Orientation`...) followed by
  the event's own fields from `+0x138`. `EventSys`: id -> index map at `+0x291e0`, type records (0x28 bytes) at `+0x11d8`, count `+0x291d8`.
- Listen (what the game's systems do): `exe+198f160 AddEventHandler(listener, u32 id, void (*handler)(void* owner, EventBase* ev))`.
  `listener` is the system's own hidden listener object (`ResolveRef exe+16798f0(&system+0xc)`), which keeps a map "event handlers hash" at
  `+0x48` (event index -> handler) and calls `handler(q[listener+0x78], event)` from the dispatcher `exe+198f2f0`. **One handler per event per
  listener**: registering a second one for the same id on the same listener replaces the first, so a mod that borrows a system's listener must
  only add ids that system does not use (the catalogue shows who uses what) or chain to the old pointer. Not tried from a mod yet: hooking
  `SendEvent` stays the proven route.
- Components receive events through their own virtual handlers (a `cmp reg, <id>` chain); `fx.py imm <id>` finds those.

### Entity messages ("synced actor calls"): 74
`MsgRegister exe+21423d0(&type, handler, "name")`, `MsgSend exe+2142440(1, type, target handle, arg, &payload, size)` (the screen fade, hero
lock, halt traversal...). Table: `recon/messages_4.0630.txt`.

### Script-node pins
A nodegraph action's execute method (vtable slot 10) receives the triggered input pin's hash in `edx`, reads inputs with
`exe+1923410/1923490/19234e0(node+0x58 ctx, &out, pinHash)` and fires outputs with `exe+1923a90(ctx, pinHash)`. Resolving the hashes
(`fx.py hn`) gives the pin names; the node's data fields are in the DDL dump under `<Action>Prius`.

### More from the same session (pointers; all `[RE]`, read from the exe, not run)
- **World singleton** `exe+1f707e0()`: overlays `+0xe2060` (records `[+0xe2070]`, 0x38 bytes, count `+0xe2078`; load `exe+1f73580(world, &id)`, unload `exe+1f735a0`), checkpoint table
  `+0xe0900` / `[+0xe08f0]`, loading state `dword +0xe3cb0` (0x17..0x19 while loading), lighting object `+0xe10d0`.
- **Level/checkpoint loader**: `gm = exe+1ef6560()` (`GameManager`), `gm->vtable[+0xf8](gm, levelAssetId, crc32(checkpoint), delay)` = `exe+1ef4ca0` (`docs/internals/crime-director-chases-and-script-nodes.md` section 2).
- **Mission state**: `exe+201f640` / `exe+201f800` on `q[exe+7c4bbb0]`, or `exe+1fa74f0` on `q[exe+7c3ef70]`; 0 Inactive, 1 Available, 2 Active, 3 Cleanup, 4 Complete, 5 Failed.
- **Hero**: `HeroAbilityLock` (40 abilities, 4 layers), `HeroMacroState` (+0x4c: kOnGround 0x2, kInAir 0x4...), `WeaponManagerBase::Equip exe+2162300` (five arguments), gadget ammo,
  the wheel list, the damage message: `docs/internals/hero-abilities-gadgets-and-combat-events.md` sections 1-9. Death -> `GameManagerCampaign::OnHeroDied exe+722750` -> `HeroRespawn` component (MISC_RECON section 7).
- **Crime director**: `RandomCrimeSystem2` at `exe+707a500` ([crime-director-chases-and-script-nodes.md](crime-director-chases-and-script-nodes.md), section 1). **Chase system, 1,032 script nodes with pins**: MISC_RECON sections 3 and 5, `recon/script_nodes_4.0630.txt`.
- **Lighting**: modes 0 kDay, 1 kNight, 2 kSunset, 3 kOvercast, 4 kBlackout, 5 kNightRain, 6 kOvercastRain; the switch's load screen is `DynamicLoadScreenSystem` handler `exe+710970`
  (`docs/internals/atmosphere-and-menu-recon-log.md` Part 18); atmosphere fields by name: Part 19.
- **Crash dumps**: `tools/sigtools/dumpctx.py` (real faulting registers and stack), `dumpindex.py` (one line per dump). A heap address in an object's vtable slot = the object was freed.

