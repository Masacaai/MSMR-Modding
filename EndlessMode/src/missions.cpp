#include "missions.h"

#include "gamethread.h"
#include "log.h"
#include "memory.h"
#include "scan.h"

#include <Windows.h>
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
        if (checked != 1 || !memory::Read(scan::FromRva(kSystemPtrRva), sys) || !sys) return false;
        alignas(16) unsigned char out[64] = {};
        __try {
            reinterpret_cast<void* (*)(uintptr_t, void*, uint32_t)>(scan::FromRva(kFindRva))(sys, out, id);
            state = (int)reinterpret_cast<uint32_t (*)(uintptr_t, void*)>(scan::FromRva(kStateRva))(sys, out);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
    }

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
