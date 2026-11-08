#include "gadgets.h"

#include "entity.h"
#include "gamethread.h"
#include "log.h"
#include "memory.h"
#include "natives.h"
#include "scan.h"

#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <mutex>
#include <set>
#include <thread>

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
            // The lock only guards the gadget ids (9C82-9C89). Everything else the game equips (id 0 clears, C350/C351 companions of a
            // gadget, ...) must go through: refusing those left slot 0 without a record and crashed the game (2026-11-07, exe+374237).
            const bool gadget = id >= 0x9C82 && id <= 0x9C89;
            allowed = !g_lock || t_ours || !gadget || g_allowed.count((uint32_t)id) > 0;
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

    // Hide: the manager's gadget table (entries of 0x18 bytes at +0x1B8, count at +0x628, id first) is the game's lookup; with an entry's
    // id zeroed the wheel still lists the gadget but red, ammo 0/0 and unusable (verified 2026-11-07). Show writes the id back.
    static std::set<uint32_t> g_hidden;
    static uintptr_t Entry(uint32_t id) {
        uintptr_t mgr = g_mgr.load();
        uint32_t n = 0;
        if (!mgr || !memory::Read(mgr + 0x628, n) || n > 64) return 0;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t v = 0;
            if (memory::Read(mgr + 0x1B8 + 0x18 * i, v) && v == id) return mgr + 0x1B8 + 0x18 * i;
        }
        return 0;
    }
    bool Hide(uint32_t id, bool hide) {
        if (!g_mgr) { LOG("gadgets: no manager seen yet (equip a gadget once with the hook on)"); return false; }
        std::lock_guard<std::mutex> lock(g_mutex);
        if (hide) {
            uintptr_t e = Entry(id);
            if (!e) { LOG("gadgets: hide %04X: not in the table (already hidden?)", id); return false; }
            memory::Write<uint32_t>(e, 0);
            g_hidden.insert(id);
        } else {
            uintptr_t mgr = g_mgr.load();
            uint32_t n = 0;
            if (!g_hidden.count(id) || !memory::Read(mgr + 0x628, n)) return false;
            for (uint32_t i = 0; i < n && i < 64; i++) {   // the zeroed entry still carries the gadget's hash after the id: find it by slot order
                uint32_t v = 1;
                if (memory::Read(mgr + 0x1B8 + 0x18 * i, v) && v == 0 && id >= 0x9C82 && id - 0x9C82 == i - 1) {   // table order: 9C43, 9C82..9C89, 9C92
                    memory::Write<uint32_t>(mgr + 0x1B8 + 0x18 * i, id);
                    g_hidden.erase(id);
                    break;
                }
            }
        }
        LOG("gadgets: %s %04X", hide ? "hid" : "showed", id);
        return true;
    }

    // The hero's HeroWeaponManagerLocal is the EquipGadget `this` (the gadget table was at its +0x1B8): found without waiting for an equip call.
    static void FindManager() {   // game thread
        auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
        uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
        uint32_t handle = 0;
        if (!get_actor || !memory::Read(system + 0x1C, handle) || !handle) return;
        uintptr_t actor = get_actor(&handle);
        uintptr_t m = actor ? entity::FindComponent(actor, "HeroWeaponManagerLocal") : 0;
        if (m) g_mgr = m;
    }

    // ---- a run's gadgets (Campaign rewards) ----
    static uint32_t g_saved = 0;   // what was equipped before the run
    static const uint32_t kWebShooter = 0x9C89;
    static bool g_run = false;

    void BeginRun() {
        if (!Install()) return;
        gamethread::Run([] {
            FindManager();
            if (!g_mgr) { LOG("gadgets: no weapon manager; gadgets stay as they are"); return; }
            memory::Read(g_mgr.load() + 0x6c, g_saved);
            for (uint32_t id = 0x9C82; id <= 0x9C88; id++) Hide(id, true);
            ClearAllowed();
            Allow(kWebShooter);
            Lock(true);
            Equip(kWebShooter, 0);
            g_run = true;
            LOG("gadgets: run start: all gadgets hidden, Web Shooter equipped (was %04X)", g_saved);
        }, 5000);
    }

    // Auto-equip again (user, 2026-11-07): a granted gadget shows red 0/0 until an equip goes through, and only an equip the player makes
    // cleared it in tests, so the grant equips it itself and retries while the Web Shooter is still in hand (the first equip often
    // returns 0 for a few seconds).
    void Grant(uint32_t id) {
        gamethread::Run([id] {
            if (!g_run) FindManager();
            Hide(id, false);
            Allow(id);
            bool ok = Equip(id, 0);
            LOG("gadgets: granted %04X%s", id, ok ? "" : " (equip refused for now; retrying)");
            if (ok) return;
            std::thread([id] {
                for (int i = 0; i < 10 && g_run; i++) {
                    Sleep(800);
                    bool done = false;
                    gamethread::Run([id, &done] {
                        FindManager();
                        uint32_t cur = 0;
                        if (!g_mgr || !memory::Read(g_mgr.load() + 0x6c, cur)) { done = true; return; }
                        if (cur != kWebShooter && cur != 0) { done = true; return; }   // already switched (by the game or the player): leave it
                        done = Equip(id, 0);
                    }, 3000);
                    if (done) return;
                }
            }).detach();
        }, 5000);
    }

    bool Owned(uint32_t id) { std::lock_guard<std::mutex> lock(g_mutex); return g_allowed.count(id) > 0 && id != kWebShooter; }

    void EndRun() {
        if (!g_run) return;
        g_run = false;
        gamethread::Run([] {
            // The hero may be dead or respawning: the manager pointer from the run start is stale then (calling EquipGadget on it
            // crashed the game at respawn, 2026-11-07). Re-find it; with no live manager only the lock is cleared, and no equip
            // call is made at all (the Web Shooter or the last gadget simply stays equipped).
            g_mgr = 0;
            FindManager();
            if (g_mgr) for (uint32_t id = 0x9C82; id <= 0x9C88; id++) Hide(id, false);
            { std::lock_guard<std::mutex> lock(g_mutex); g_hidden.clear(); }
            Lock(false);
            ClearAllowed();
            LOG("gadgets: run end: gadgets %s", g_mgr ? "restored" : "left hidden (no live hero; reload fixes it)");
        }, 5000);
    }

    bool Equip(uint32_t id, int slot) {
        if (!g_orig || !g_mgr) { LOG("gadgets: no manager seen yet (equip a gadget once with `gadget watch on`)"); return false; }
        t_ours = true;
        bool r = g_orig((void*)g_mgr.load(), (int)id, slot, true);
        t_ours = false;
        LOG("gadgets: our equip %04X slot %d -> %d", id, slot, (int)r);
        return r;
    }
}
