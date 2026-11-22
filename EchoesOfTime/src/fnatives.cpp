#include "fnatives.h"

#include "log.h"
#include "memory.h"
#include "natives.h"
#include "scan.h"

#include <Windows.h>

namespace fnat {
    struct Nat { const char* name; const char* pattern; uintptr_t expected_rva; uintptr_t addr; int hits; };

    // Patterns are the function prologues (bytes read from the exe on disk; each is unique except MapLookup, which is a template instantiated
    // for several maps, so any hit that is exactly the expected RVA is accepted).
    static Nat g_table[Count] = {
        { "AddMaterial", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 30 41 0F", 0x1772530, 0, 0 },
        { "SetModified", "44 8B 05 7D 3C 16 06 83 FA FF 8B 41", 0x191bf60, 0, 0 },
        { "HashPath", "48 89 5C 24 10 57 48 83 EC 20 48 8B DA 48 8B F9 48 85 D2 74 36", 0x1bb2680, 0, 0 },
        { "MapLookup", "40 53 48 83 EC 20 48 8B D9 8B C2 49 B8 00 00 00 00 FF FF FF FF 48 8B CA 49 23 C8 48 0B C8 49 85", 0x1bcf4e0, 0, 0 },
        { "SkinPart", "3B 51 4C 73 2B 4C 8B 49 50 4D 85 C9", 0x20c9f20, 0, 0 },
    };

    // Data used by FindMaterial (HeroLastKnownPositionManager silhouette, exe+93f698..93f6ac).
    constexpr uintptr_t kMaterialMapRva = 0x6058208, kMaterialBaseRva = 0x6058240, kMaterialStrideRva = 0x6058248;

    int Resolve() {
        int ok = 0;
        for (auto& n : g_table) {
            auto hits = scan::FindAll(n.pattern, 256);
            n.hits = (int)hits.size();
            n.addr = 0;
            for (uintptr_t h : hits) if (scan::Rva(h) == n.expected_rva) n.addr = h;
            if (n.addr) ok++;
            LOG("  %-14s hits=%d  exe+%llx  %s", n.name, n.hits, (unsigned long long)n.expected_rva, n.addr ? "OK" : "NOT FOUND at the expected address (disabled)");
        }
        return ok;
    }

    uintptr_t Get(Id id) { return g_table[id].addr; }

    static bool HashPathGuarded(uintptr_t fn, uint8_t* out, const char* path) {
        __try { ((void* (*)(void*, const char*))fn)(out, path); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static bool LookupGuarded(uintptr_t fn, uintptr_t map, uint64_t key, uintptr_t& entry) {
        __try { entry = ((uintptr_t(*)(uintptr_t, uint64_t))fn)(map, key); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    uintptr_t FindMaterial(const char* path, bool quiet) {
        const uintptr_t hash = Get(HashPath), lookup = Get(MapLookup);
        if (!hash || !lookup || !path || !*path) return 0;
        uint8_t out[16] = {};
        if (!HashPathGuarded(hash, out, path)) { LOG("fnat: hashing '%s' faulted", path); return 0; }
        uint64_t key = 0;
        memcpy(&key, out, 8);
        uintptr_t entry = 0;
        if (!LookupGuarded(lookup, scan::FromRva(kMaterialMapRva), key, entry)) { LOG("fnat: material map lookup faulted"); return 0; }
        if (!entry) { if (!quiet) LOG("fnat: material '%s' (id %016llx) is not loaded", path, (unsigned long long)key); return 0; }
        uint32_t index = 0;
        uint64_t base = 0, stride = 0;
        if (!memory::Read(entry, index) || !memory::Read(scan::FromRva(kMaterialBaseRva), base) || !memory::Read(scan::FromRva(kMaterialStrideRva), stride) || !base || !stride) {
            LOG("fnat: material table unreadable");
            return 0;
        }
        return (uintptr_t)(base + (uint64_t)index * stride);
    }

    constexpr uintptr_t kMaterialManagerRva = 0x6058200;   // AssetManagerBase instance; its map is at +8 (the one FindMaterial reads)

    bool LoadMaterial(const char* path) {
        const uintptr_t hash = Get(HashPath), load = natives::Get(natives::LoadAssetByHash);
        if (!hash || !load || !path || !*path) return false;
        uint8_t out[16] = {};
        if (!HashPathGuarded(hash, out, path)) return false;
        uint64_t key = 0;
        memcpy(&key, out, 8);
        __try {
            const uintptr_t asset = ((uintptr_t(*)(uintptr_t, uint64_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t))load)(scan::FromRva(kMaterialManagerRva), key, 0, 0, 0, 0, 0);
            LOG("fnat: load requested for '%s' (id %016llx) -> asset %llx", path, (unsigned long long)key, (unsigned long long)asset);
            return asset != 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) { LOG("fnat: loading '%s' faulted", path); return false; }
    }

    bool AddMaterial_(uintptr_t mi, uintptr_t material, uint8_t flag) {
        const uintptr_t fn = Get(AddMaterial);
        if (!fn || !mi || !material) return false;
        __try { ((bool (*)(uintptr_t, uint32_t, uintptr_t, uint8_t))fn)(mi, 0, material, flag); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    uintptr_t SkinPart_(uintptr_t sm, unsigned index, unsigned variant) {
        const uintptr_t fn = Get(SkinPart);
        if (!fn || !sm) return 0;
        __try { return ((uintptr_t(*)(uintptr_t, unsigned, unsigned))fn)(sm, index, variant); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    bool SetModified_(uintptr_t mi) {
        const uintptr_t fn = Get(SetModified);
        if (!fn || !mi) return false;
        __try { ((void (*)(uintptr_t, uint32_t))fn)(mi, 0xffffffffu); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}
