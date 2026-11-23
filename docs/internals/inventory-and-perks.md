# Inventory, suit powers and mods

## Inventory, suit powers and mods

- Perks, suit mods, suit powers and gear are **inventory items keyed by their config asset id**. `GameInventoryManager`
  (vtable exe+38e3148) routes each item to a sub-inventory. Vfuncs: 16 (exe+1f5b720) `Add(mgr, &result, asset_id, count)`;
  19 (exe+1f5b9d0) `Remove`; 18 (exe+1f5b880) probably a count query; 45 (exe+bbf790) `CanAdd`. **[RE/verified for Add/Remove]**
- The suit loadout slots (power = slot 1, mods = slots 2..4) can be emptied and locked; `runloadout.cpp` saves, empties, restores. A
  hardware write watchpoint on the loadout block found the code that equips mods. **[verified]**
- **Item text** (names/descriptions come from the game; the mod never hard-codes them): item data record -> name hash `+0x24`,
  description hash `+0x34`, format argument `+0x38`, icon `char* +0x48`, preview video config `char* +0xA0`, preview image `char* +0xB8`,
  category `+0x1D0`. Localization lookup `exe+1749970(hash, 0)` returns a `char*`; formatting `exe+1bc1b10` (snprintf-like). Item
  manager at `exe+70957c0`. `inv text <id>` prints both.
- Suit powers and mods do nothing useful in an arena include: Quips, Sound Of Silence, Combat Analyzer, Proximity Sensor, Long Range
  Scanner, Silent Step, Bio Scanner (see the curation notes in LEDGER).

