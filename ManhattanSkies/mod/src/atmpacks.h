#pragma once

#include <cstdint>
#include <string>

// Atmosphere mods (.smpcmod files) dropped in Documents\Marvel's Spider-Man Remastered\ManhattanSkies\Atmospheres\ and used at run time, several at once,
// without Overstrike installing them (docs/RECON.md Part 17). On the first world load a background thread reads each mod, works out which game atmosphere
// it replaces (day, early morning, sunset, night or overcast), gives its atmosphere and the textures it ships brand-new asset ids (rewriting the references
// inside the atmosphere), caches the decompressed files and adds them to the game's in-memory asset table (toc.cpp). Each time of day then has a list of
// "versions": the game's own plus the mods found.
//
// A slot is a time of day: 0 day, 1 early morning, 2 sunset, 3 night, 4 overcast.
namespace atmpacks {
    void Start();                          // once, from the worker thread after the world has settled; the scan runs on its own thread
    bool Ready();
    bool IsMod(uint64_t atmosphere_id);    // the id is one of the mods' atmospheres
    bool Blocked();                        // mods are in the Atmospheres folder but no Overstrike archive exists to borrow (the menu says what to do)
    bool AnyMods();                        // at least one atmosphere mod is loaded
    void Select(uint64_t atmosphere_id);   // the mod's atmosphere is about to load: swap its extra files into the game's table (after Prepare)
    void Deselect();                       // the game's own atmosphere is on: put back the game's files a mod's atmosphere had swapped in
    bool Prepare(uint64_t atmosphere_id);   // unpack that mod's textures into the cache (first use; under a second); true when ready or not a mod's id
    int SlotOfMode(int lighting_mode);     // lighting mode (0 day, 4 early morning, 2 sunset, 5/1 night, 3/6 overcast) -> slot, or -1
    int LabelCount(int slot);              // 1 (the default) + the mods for that slot
    const char* const* Labels(int slot);   // "DEFAULT", then the mod names (stable once Ready)
    std::string NameAt(int slot, int index);   // index >= 1: the mod's name
    uint64_t IdFor(int slot, const std::string& name);   // the new asset id of that mod's atmosphere, or 0
    void LogStatus();
}
