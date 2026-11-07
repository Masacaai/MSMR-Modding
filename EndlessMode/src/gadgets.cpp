#include "gadgets.h"

#include "log.h"
#include "memory.h"
#include "scan.h"

#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <mutex>
#include <set>

namespace gadgets {
    using EquipFn = bool (*)(void* self, int id, int slot, bool force);
    static EquipFn g_orig = nullptr;
    static std::atomic<bool> g_watch = false, g_lock = false;
    static std::atomic<uintptr_t> g_mgr = 0;
    static thread_local bool t_ours = false;
    static std::mutex g_mutex;
    static std::set<uint32_t> g_allowed;

    static bool Detour(void* self, int id, int slot, bool force) {
        g_mgr = (uintptr_t)self;
        bool allowed;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            allowed = !g_lock || t_ours || g_allowed.count((uint32_t)id) > 0;
        }
        if (!allowed) {
            LOG("gadgets: refused equip of %04X (slot %d): locked", id, slot);
            return false;
        }
        bool r = g_orig(self, id, slot, force);
        if (g_watch) LOG("gadgets: equip id=%04X slot=%d force=%d -> %d", id, slot, (int)force, (int)r);
        return r;
    }

    bool Install() {
        static bool tried = false, ok = false;
        if (tried) return ok;
        tried = true;
        static const uint8_t expect[] = { 0x48, 0x8b, 0xc4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x68, 0x18, 0x48, 0x89, 0x70, 0x20,
                                          0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xec, 0xb0 };
        uintptr_t t = scan::FromRva(0x2162300);
        uint8_t cur[sizeof(expect)] = {};
        if (!memory::SafeRead(t, cur, sizeof(cur)) || memcmp(cur, expect, sizeof(expect)) != 0) {
            LOG("gadgets: exe+2162300 is not the expected EquipGadget; not hooking");
            return false;
        }
        MH_STATUS s = MH_Initialize();
        if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) return false;
        ok = MH_CreateHook((void*)t, (void*)&Detour, (void**)&g_orig) == MH_OK && MH_EnableHook((void*)t) == MH_OK;
        LOG("gadgets: EquipGadget hook %s", ok ? "installed" : "FAILED");
        return ok;
    }

    void Watch(bool on) { if (on && !Install()) return; g_watch = on; LOG("gadgets: watch %s", on ? "on" : "off"); }
    void Lock(bool on) { if (on && !Install()) return; g_lock = on; LOG("gadgets: lock %s", on ? "on" : "off"); }
    void Allow(uint32_t id) { std::lock_guard<std::mutex> lock(g_mutex); g_allowed.insert(id); LOG("gadgets: allowed %04X", id); }
    void ClearAllowed() { std::lock_guard<std::mutex> lock(g_mutex); g_allowed.clear(); }

    bool Equip(uint32_t id, int slot) {
        if (!g_orig || !g_mgr) { LOG("gadgets: no manager seen yet (equip a gadget once with `gadget watch on`)"); return false; }
        t_ours = true;
        bool r = g_orig((void*)g_mgr.load(), (int)id, slot, true);
        t_ours = false;
        LOG("gadgets: our equip %04X slot %d -> %d", id, slot, (int)r);
        return r;
    }
}
