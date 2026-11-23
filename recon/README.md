# recon: generated reference dumps

Machine-written tables read out of `Spider-Man.exe` 4.0630.0.0 (names, offsets and addresses only; no game assets). They are **data for lookup**
(`grep` them); the prose that explains them is in [docs/internals/](../docs/internals/README.md), mainly [reflection-tables.md](../docs/internals/reflection-tables.md).
Do not hand-edit them. Where a tool is named it regenerates the file; the others are **snapshots** (the one-off script that wrote them was not kept). Every address is `exe+RVA`; they are valid for 4.0630.0.0 only. Reading the executable needs your own copy of the game and
`pip install capstone numpy pefile` (see [REVERSE_ENGINEERING.md](../docs/REVERSE_ENGINEERING.md)).

| File | What | Made by |
|---|---|---|
| `ddl_types_4.0630.txt` (5 MB) | Every reflected type: fields, types, offsets | `tools/sigtools/fx.py ddls recon/ddl_types_4.0630.txt` |
| `enums_4.0630.txt` | Enum types, values and names | `fx.py enums recon/enums_4.0630.txt` |
| `events_4.0630.txt` | Event types sent through the event system, with their payloads | snapshot (read it with [events.py](../tools/sigtools/events.py) / `event_names.py` for more) |
| `messages_4.0630.txt` | Entity messages: type object and handler addresses | snapshot |
| `component_types_4.0630.txt` | Component type objects (what `FindComponent` takes) | snapshot; `fx.py cls` / `fx.py x` answer the same questions live |
| `script_nodes_4.0630.txt` | The 1,032 script-graph node types | snapshot; `tools/sigtools/nodes.py` reads nodes live |
| `input_actions_4.0630.txt` | Input action name hashes and who reads them | snapshot (see [camera-time-and-input.md](../docs/internals/camera-time-and-input.md)) |
| `atmospheredef_flat.txt` | The flattened `AtmosphereDef` layout | `fx.py ddlflat AtmosphereDef` |
| `atm_parser_map.txt` | What the atmosphere parser copies where (slot offsets) | snapshot of a disassembly, see [atmosphere-and-menu-recon-log.md](../docs/internals/atmosphere-and-menu-recon-log.md) |

Git-ignored here: `recon/ui/` and `recon/LineOfSightAwareness.config.json` (extracted game data; regenerate locally). Hand-recorded paths for Endless Arena
(walk paths, portal points) are in `EndlessArena/data/recon/`.
