# Loading code and the game thread

## Loading code into the game

- **Overstrike** (Tkachov, GPLv3) installs `.script` mods. With "Enable .script support" and the `-scripts` command-line option,
  its proxy (`winmm.dll` in the game folder) hooks the game's command-line parsing and `LoadLibrary`s each script DLL from
  `<game>\scripts\`, then calls its exported `script_enable()`. **[verified]**
- `script_enable` runs very early, before the world exists. Start a thread and wait. **[verified]**
- Script Hook (the older mod loader) and its natives list were used **only as a map** of which game functions matter. Its
  `SpawnActor`, `LoadAssetByHash`, `CreateSyncObject`, `GetActor`... correspond to the natives in `natives.cpp`.

## The game thread

- **Hook point `exe+1f745f0`**, called once per frame from `exe+1f731a1`. A detour pops a queued job (`gamethread.cpp`), then
  calls the original. Signature in `natives.cpp` (`GameThreadTick`). **[verified]** An alternative per-frame hook, `exe+16244c0`, is used by other projects. **[RE]**
- The tick only runs while the game window is focused. A background game doesn't process mod jobs. **[verified]**
- Game state must be changed from this thread. Our `gamethread::Run` blocks the caller until the job ran (timeout 3 s by default).
- Frame-rate proxy: ticks per second and longest gap between ticks (`ticks`, `fps` dev commands).

