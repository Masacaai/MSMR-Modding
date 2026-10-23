#include "missions.h"

#include "gamethread.h"
#include "log.h"
#include "memory.h"
#include "scan.h"

#include <Windows.h>
#include <atomic>
#include <cstring>

namespace missions {
    constexpr uintptr_t kSystemPtrRva = 0x7c4bbb0, kFindRva = 0x201f640, kStateRva = 0x201f800;

    uint32_t Hash(const std::string& name, bool normalize) {
        // CRC-32 as ALERT computes it: reflected polynomial 0xEDB88320, seeded with the polynomial itself, no final xor; normalized = lower case.
        static uint32_t table[256];
        static bool init = false;
        if (!init) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t c = i;
                for (int k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
                table[i] = c;
            }
            init = true;
        }
        uint32_t v = 0xEDB88320u;
        for (unsigned char ch : name) {
            if (normalize && ch >= 'A' && ch <= 'Z') ch = (unsigned char)(ch + 32);
            v = (v >> 8) ^ table[(v ^ ch) & 0xff];
        }
        return v;
    }

    bool State(uint32_t id, int& state) {
        static const unsigned char find_bytes[] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48 };
        static const unsigned char state_bytes[] = { 0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x6c, 0x24, 0x18, 0x57, 0x48 };
        static int checked = 0;
        if (!checked) {
            unsigned char a[12] = {}, b[12] = {};
            checked = memory::SafeRead(scan::FromRva(kFindRva), a, 12) && memory::SafeRead(scan::FromRva(kStateRva), b, 12) &&
                      !memcmp(a, find_bytes, 12) && !memcmp(b, state_bytes, 12) ? 1 : -1;
        }
        uintptr_t sys = 0;
        const bool have = memory::Read(scan::FromRva(kSystemPtrRva), sys);
        static int logged = 0;
        if (logged++ < 3) LOG("missions: code check %d, system pointer %s %llx", checked, have ? "read" : "unreadable", (unsigned long long)sys);
        if (checked != 1 || !have || !sys) return false;
        alignas(16) unsigned char out[64] = {};
        __try {
            reinterpret_cast<void* (*)(uintptr_t, void*, uint32_t)>(scan::FromRva(kFindRva))(sys, out, id);
            state = (int)reinterpret_cast<uint32_t (*)(uintptr_t, void*)>(scan::FromRva(kStateRva))(sys, out);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            static int faults = 0;
            if (faults++ < 3) LOG("missions: the game's code faulted");
            return false;
        }
        return true;
    }

    static std::atomic<int> g_active{ 0 };
    static std::atomic<DWORD> g_next_poll{ 0 };

    int Active(uint32_t* ids, int cap) {
        static const unsigned char index_bytes[] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48 };
        static int checked = 0;
        if (!checked) {
            unsigned char a[12] = {};
            checked = memory::SafeRead(scan::FromRva(0x201efa0), a, 12) && !memcmp(a, index_bytes, 12) ? 1 : -1;
        }
        unsigned char flag = 0;
        uintptr_t sys = 0;
        if (checked != 1 || !memory::Read(scan::FromRva(0x7c8c1a8), flag) || !flag || !memory::Read(scan::FromRva(kSystemPtrRva), sys) || !sys) return 0;
        int32_t count = 0;
        if (!memory::Read(sys + 0x108, count) || count <= 0 || count > 4096) return 0;
        int active = 0;
        alignas(16) unsigned char out[64];
        __try {
            for (int i = 0; i < count; i++) {
                memset(out, 0, sizeof(out));
                reinterpret_cast<void* (*)(uintptr_t, void*, int)>(scan::FromRva(0x201efa0))(sys, out, i);
                if (reinterpret_cast<uint32_t (*)(uintptr_t, void*)>(scan::FromRva(kStateRva))(sys, out) == 2) {
                    if (ids && active < cap) memcpy(&ids[active], out + 0x10, 4);
                    active++;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
        return active;
    }

    void Poll() {
        const DWORD now = GetTickCount();
        if ((int)(now - g_next_poll) < 0) return;
        g_next_poll = now + 1000;
        gamethread::Post([] {
            uint32_t ids[8] = {};
            const int n = Active(ids, 8);
            if (n != g_active.exchange(n)) {
                char line[160] = {};
                int len = 0;
                for (int i = 0; i < n && i < 8; i++) len += snprintf(line + len, sizeof(line) - (size_t)len, " %08x", ids[i]);
                LOG("missions: %d active%s", n, line);
            }
        });
    }

    bool AnyActiveCached() { return g_active.load() > 0; }

    void Log() {
        gamethread::Post([] {
            static const char* const names[] = { "GP_A1_FISK", "GP_A2_PRISON", "GP_A2_BLACKOUT", "GP_A3_NYPD", "GP_A3_OTTOFIGHT", "VN_ELECTRO_FIGHT",
                                                 "VN_SCORPION_02", "VN_TOMBSTONE_FIGHT", "V10_SHOCKER_CHASE", "V10_SHOCKER_BANK", "OM_BASE_FISK_GREENWICH",
                                                 "OM_DEMONS_STORY_LI", "GP_A3_PRIS_BASE_INTRO", "CTNS__HH_FINALE", "CTNS__SS_FINALE" };
            for (const char* n : names) {
                int a = -1, b = -1;
                const bool ok_a = State(Hash(n, false), a), ok_b = State(Hash(n, true), b);
                LOG("missions: %-24s as written %08x -> %s%d   lower case %08x -> %s%d", n, Hash(n, false), ok_a ? "" : "(failed) ", a, Hash(n, true), ok_b ? "" : "(failed) ", b);
            }
        });
    }
}
