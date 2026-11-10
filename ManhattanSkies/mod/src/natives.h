#pragma once

#include <climits>
#include <cstdint>

// Game functions on Spider-Man.exe 4.0630.0.0, found by signature at runtime. Expected RVAs come from static analysis
// (docs/RECON.md); a mismatch means the signature landed somewhere new and the native must not be used.
// The scan/memory/log layers are copies of Uncontained's (Manhattan Skies is a separate mod).
namespace natives {
    enum Id {
        GameThreadTick,     // exe+1f745f0: per-frame function (same as ScriptHookSMPC and Endless Arena); hooked lazily to run jobs on the game thread
        WorldGetter,        // exe+1f707e0: returns the world singleton (lighting mode field at +0xe2040, lighting object at +0xe10d0)
        LightingSet,        // exe+1f7f9e0 (lighting object, mode index): wrapper that tail-calls the worker exe+1f058e0; 7 callers, the worker has no others
        AtmosphereAdjust,   // exe+162f020 (AtmosphereManager*, slot*, delta, mask, min, max): adds delta to the slot's counters picked by mask, clamps, recomputes (exe+162ff10)
        LoadAssetByHash,    // exe+1626380 (manager*, u64 asset id, 0,0,0,0,0) -> asset*: the generic "load this asset into that manager" call
        Count
    };

    constexpr int kDirect = INT_MIN;

    struct Native {
        Id id;                  // must equal the entry's index
        const char* name;
        const char* pattern;
        int rel32_off;          // kDirect: the match itself is the address
        uintptr_t expected_rva;
        uintptr_t addr;         // resolved at runtime, 0 if not found
        int hits;
    };

    extern Native table[Count];

    // Scans all natives; returns number that resolved to their expected RVA.
    int ResolveAll();

    // Address if it resolved exactly where expected, otherwise 0.
    uintptr_t Get(Id id);
}
