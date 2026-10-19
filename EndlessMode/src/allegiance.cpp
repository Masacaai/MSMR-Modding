#include "allegiance.h"

#include "entity.h"
#include "log.h"
#include "memory.h"
#include "scan.h"

#include <cstring>

namespace allegiance {
    static const char* const kNames[kCount] = { "kNeutral", "kBlueTeam", "kRedTeam", "kSuperFriends", "kBaseDestructables",
                                                "kHeroOnlyTarget", "kFreeForAll", "kPolice", "kInnerDemons", "kSableFriendly",
                                                "kSableEnemy", "kFisk", "kTaskmaster", "kPrisoner", "kHammerhead", "kMaggia" };
    constexpr uintptr_t kSetterRva = 0x2117e00, kVtableRva = 0x4f89dc0;

    const char* Name(int v) { return v >= 0 && v < kCount ? kNames[v] : "?"; }

    int Parse(const std::string& s) {
        if (s.empty()) return -1;
        if (isdigit((unsigned char)s[0])) { int v = atoi(s.c_str()); return v >= 0 && v < kCount ? v : -1; }
        for (int i = 0; i < kCount; i++) {
            if (_stricmp(s.c_str(), kNames[i]) == 0 || _stricmp(s.c_str(), kNames[i] + 1) == 0) return i;
        }
        return -1;
    }

    static uintptr_t Component(uintptr_t entity) {
        uintptr_t found = 0;
        entity::ForEachComponent(entity, [&](const std::string&, uintptr_t inst) {
            uintptr_t vt = 0;
            if (!found && inst && memory::Read(inst, vt) && vt == scan::FromRva(kVtableRva)) found = inst;
        });
        return found;
    }

    bool Get(uintptr_t entity, int& value) {
        uintptr_t c = Component(entity);
        return c && memory::Read(c + 0x48, value);
    }

    bool Set(uintptr_t entity, int value) {
        static const unsigned char expect[] = { 0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x89, 0x51, 0x48, 0x48, 0x8b, 0xd9, 0x48, 0x8b, 0xd1 };
        static int checked = 0;
        if (!checked) {
            unsigned char b[sizeof(expect)] = {};
            checked = memory::SafeRead(scan::FromRva(kSetterRva), b, sizeof(b)) && !memcmp(b, expect, sizeof(b)) ? 1 : -1;
        }
        uintptr_t c = Component(entity);
        if (checked != 1 || !c || value < 0 || value >= kCount) return false;
        reinterpret_cast<void (*)(uintptr_t, int)>(scan::FromRva(kSetterRva))(c, value);
        return true;
    }
}
