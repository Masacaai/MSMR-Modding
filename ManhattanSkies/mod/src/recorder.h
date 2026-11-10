#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Passive recorder (reads memory and logs; both hooks call the original unchanged):
//   ls  - every call to the lighting-mode setter (exe+1f7f9e0): index, mode field before, caller, short backtrace
//   lm  - the lighting mode field (world+0xe2040) whenever it changes (polled)
//   at  - calls to AtmosphereAdjust (exe+162f020) that change a slot's counters: slot index, asset id, delta, mask, counters before -> after
//   as  - the first 0x40 bytes of each atmosphere slot, the first time it is seen
// Repeats that change nothing are counted, not logged (see `atm`).
namespace recorder {
    // Return true to swallow an AtmosphereAdjust call (it never reaches the game). Runs on the thread that made the call (the game thread).
    using AdjustFilter = bool (*)(uintptr_t caller_rva, int slot_idx, uint64_t asset, int delta);
    void SetAdjustFilter(AdjustFilter f);
    uintptr_t Manager();                                  // the AtmosphereManager (0 until the first AtmosphereAdjust call is seen)
    bool SlotInfo(int idx, uint64_t& asset, int& weight); // a slot's asset id and weight (counter 0); false if unreadable
    int FindSlot(uint64_t asset);                         // slot index holding this atmosphere, or -1
    uint64_t AtmIdBySuffix(const char* file);             // asset id of the atmosphere whose path ends with `file`, or 0
    bool LoadAtmosphere(uint64_t id);                     // asks the asset system to load it into the pool (async; poll FindSlot)
    bool ReleaseSlot(int idx);                            // drops the reference LoadAtmosphere took and queues the slot for destruction (weight must be 0)
    bool SlotFog(int idx, float out[6], bool write);      // read/write the six fog densities of a slot
    bool SlotFloats(int idx, uintptr_t offset, float* v, size_t n, bool write);   // n floats at a byte offset inside a slot
    bool SlotSun(int idx, float out[6], bool write);      // read/write R,G,B,intensity,rotation,elevation of a slot
    bool Install();   // both hooks, once
    void Tick();      // polls the mode field; call from the worker thread
    void Status();    // `sky`: current mode, hook state, manager pointer
    void DumpAtmospheres();   // `atm`: every slot's asset id, counters and leading bytes, plus the silent-repeat counts
    int CurrentMode();         // the lighting mode field (world+0xe2040); negative while loading or unresolved
    bool ApplyMode(int index); // setter(world+0xe10d0, index) right here: only from the game thread (the Settings menu's own callbacks qualify)
    void SetMode(int index);  // `sky <n>`: calls the lighting setter on the game thread, as the Settings menu does
    void Snap(const std::string& name);   // `snap <name>`: copy 192 KB of the world singleton around the lighting object
    void SnapDiff(const std::string& a, const std::string& b, const std::string& a2);   // `snapdiff a b [a2]`
    void FindQ(uint64_t value, int max_hits);   // `findq <hex64> [max]`: where in memory is this 8-byte value (an atmosphere asset id, say)
    void Poke32(uintptr_t addr, uint32_t v);    // `poke32 <addr|exe+rva> <hex>`
    void PokeF(uintptr_t addr, float v);        // `pokef <addr|exe+rva> <float>`
    void Recompute();                           // `recompute`: the atmosphere manager re-blends its slots
    void Adjust(int slot_index, int delta);     // `adj <slot> <delta>`: AtmosphereAdjust on a slot index, game thread
    void SlotAddr(int slot_index);              // `slotaddr <n>`
    void LoadTrace(unsigned ms);                // `loadtrace [ms]`: hook LoadAssetByHash; logs atmosphere-id loads always, every load during the window
    void AtmLoad(const std::string& what, uintptr_t manager);   // `atmload <hex id|@substring> [manager]`
    void AtmList(const std::string& filter);    // `atmlist [substring]`
    void DumpData(const std::string& path);   // `dumpdata <name>`: the exe's .data section to <scripts>\dump_<name>.bin
    void Peek(uintptr_t addr, size_t len);  // `peek <addr> <len>`: hex dump (safe reads)
}
