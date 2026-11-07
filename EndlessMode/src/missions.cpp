#include "missions.h"

#include "entity.h"
#include "events.h"
#include "gamethread.h"
#include "log.h"
#include "memory.h"
#include "scan.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

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

    bool AnyActiveCached() { return events::MissionActive() || g_active.load() > 0; }

    // Walks the missions of a candidate objective system (POD only: __try).
    static void Survey(uintptr_t c, int count, int* hist, const uint32_t* wanted) {
        __try {
            alignas(16) unsigned char out[64];
            for (int i = 0; i < count; i++) {
                memset(out, 0, sizeof(out));
                reinterpret_cast<void* (*)(uintptr_t, void*, int)>(scan::FromRva(0x201efa0))(c, out, i);
                const uint32_t st = reinterpret_cast<uint32_t (*)(uintptr_t, void*)>(scan::FromRva(kStateRva))(c, out);
                uint32_t id = 0;
                memcpy(&id, out + 0x10, 4);
                if (st < 8) hist[st]++;
                for (int k = 0; k < 4; k++) if (id == wanted[k]) LOG("missions:     FOUND id %08x at index %d, state %u", id, i, st);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("missions:     (the game's code faulted)");
        }
    }

    void ScanForSystems() {
        std::thread([] {
            const uintptr_t base = scan::Game().base;
            const uintptr_t vts[2] = { base + 0x4f6df50, base + 0x4f74658 };
            const char* names[2] = { "MissionSystem", "ObjectiveSystemShared" };
            std::vector<unsigned char> buf(8u << 20);
            int found = 0;
            const uint32_t wanted[4] = { Hash("GP_A2_PRISON", false), Hash("GP_A2_PRISON", true), Hash("GP_A1_FISK", false), Hash("GP_A1_FISK", true) };
            MEMORY_BASIC_INFORMATION mbi;
            uintptr_t at = 0x10000;
            while (VirtualQuery((void*)at, &mbi, sizeof(mbi)) == sizeof(mbi) && found < 12) {
                const uintptr_t region = (uintptr_t)mbi.BaseAddress, size = mbi.RegionSize;
                at = region + size;
                if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY))) continue;
                if (region >= base && region < base + 0x8000000) continue;   // the exe's own image
                for (uintptr_t off = 0; off < size && found < 12; off += buf.size()) {
                    const size_t n = (size_t)std::min<uintptr_t>(buf.size(), size - off);
                    SIZE_T got = 0;
                    if (!ReadProcessMemory(GetCurrentProcess(), (void*)(region + off), buf.data(), n, &got) || got < 8) continue;
                    for (size_t i = 0; i + 8 <= got; i += 8) {
                        uintptr_t v;
                        memcpy(&v, &buf[i], 8);
                        for (int k = 0; k < 2; k++)
                            if (v == vts[k]) {
                                const uintptr_t obj = region + off + i;
                                int32_t count = 0;
                                memory::Read(obj + 0x108, count);
                                LOG("missions: scan: %s instance at %llx, [+0x108]=%d", names[k], (unsigned long long)obj, count);
                                found++;
                                if (count > 0 && count < 4096) {
                                    int hist[8] = {};
                                    Survey(obj, count, hist, wanted);
                                    LOG("missions: scan:   states: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d", hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7]);
                                }
                            }
                    }
                }
            }
            LOG("missions: scan done, %d instance(s)", found);
        }).detach();
    }

    void DumpSystemEntity() {
        gamethread::Post([] {
            const uint32_t handle = events::MissionSystemHandle();
            uintptr_t ent = handle ? entity::FromHandle(handle) : 0;
            LOG("missions: system entity handle %08x -> %llx", handle, (unsigned long long)ent);
            if (!ent) return;
            entity::ForEachComponent(ent, [&](const std::string& name, uintptr_t c) {
                uintptr_t vt = 0;
                int32_t count = 0;
                memory::Read(c, vt);
                memory::Read(c + 0x108, count);
                LOG("missions:   component %-40s at %llx vtable exe+%llx  [+0x108]=%d", name.c_str(), (unsigned long long)c, (unsigned long long)(vt - scan::Game().base), count);
                if (count <= 0 || count > 4096) return;
                int hist[8] = {};
                const uint32_t wanted[4] = { Hash("GP_A2_PRISON", false), Hash("GP_A2_PRISON", true), Hash("GP_A1_FISK", false), Hash("GP_A1_FISK", true) };
                Survey(c, count, hist, wanted);
                LOG("missions:     states of %d entries: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d", count, hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7]);
            });
        });
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
