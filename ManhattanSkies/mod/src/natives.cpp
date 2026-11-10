#include "natives.h"

#include "log.h"
#include "scan.h"

namespace natives {
    // All four patterns have exactly one hit in the 4.0630 exe (checked with Uncontained/tools/sigcheck.py; GameThreadTick is Uncontained's).
    Native table[Count] = {
        { GameThreadTick, "GameThreadTick",
          "48 8B CB E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 48 8B CB E8 ?? ?? ?? ?? 48 8D 8B 40 5A 00 00",
          12, 0x1f745f0 },
        // Call site in SetLightingModeAction (exe+bd8671): call WorldGetter; mov edx,ebx; lea rcx,[rax+0xe10d0]; call LightingSet; ...
        { WorldGetter, "WorldGetter",
          "E8 ?? ?? ?? ?? 8B D3 48 8D 88 D0 10 0E 00 E8 ?? ?? ?? ?? 48 8B 4F 58 BA 50 44 BF 93",
          1, 0x1f707e0 },
        { LightingSet, "LightingSet",
          "E8 ?? ?? ?? ?? 8B D3 48 8D 88 D0 10 0E 00 E8 ?? ?? ?? ?? 48 8B 4F 58 BA 50 44 BF 93",
          15, 0x1f7f9e0 },
        // Prologue: movsxd rax,[rcx+0x70] (slot count); mov esi,r8d (delta); stack args 5/6 = clamp min/max.
        { AtmosphereAdjust, "AtmosphereAdjust",
          "48 89 5C 24 08 56 48 83 EC 20 48 63 41 70 41 8B F0 44 8B 54 24 50 48 8B D9 44 8B 5C 24 58 85 C0 0F 88",
          kDirect, 0x162f020 },
        // Context of a call site; the rel32 at +43 is the call to LoadAssetByHash (same signature Uncontained uses).
        { LoadAssetByHash, "LoadAssetByHash",
          "E8 ?? ?? ?? ?? 8B 44 24 78 4C 8B CF 48 8B 54 24 58 4C 8B C3",
          43, 0x1626380 },
    };

    int ResolveAll() {
        for (int i = 0; i < Count; i++) {
            if (table[i].id != i) {
                LOG("natives: table entry %d (%s) has id %d; table order is broken, nothing resolved", i, table[i].name, (int)table[i].id);
                for (auto& n : table) n.addr = 0;
                return 0;
            }
        }
        int ok = 0;
        for (auto& n : table) {
            auto hits = scan::FindAll(n.pattern, 4);
            n.hits = (int)hits.size();
            n.addr = 0;
            if (hits.empty()) { LOG("  %-18s NOT FOUND", n.name); continue; }
            const uintptr_t a = n.rel32_off == kDirect ? hits[0] : scan::Rel32Target(hits[0], n.rel32_off);
            const uintptr_t rva = scan::Rva(a);
            const bool match = rva == n.expected_rva;
            if (match) { n.addr = a; ok++; }
            LOG("  %-18s hits=%d  exe+%llx  %s", n.name, n.hits, (unsigned long long)rva, match ? "OK" : "MISMATCH (disabled)");
        }
        return ok;
    }

    uintptr_t Get(Id id) { return table[id].addr; }
}
