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
#include <memory>
#include <set>

// Gadgets as a run's rewards (docs/REWARD_DESIGN.md, docs/RECON.md sections 5-7, 9).
//
// What the 2026-11-15 recon changed (read from the exe, nothing here ran in game yet):
//  - exe+2162300 is `WeaponManagerBase::Equip(this, id, slot, force, sendEquippedEvent)`: FIVE arguments (the old hook had four, so every
//    equip, the game's own included, was passed garbage for the fifth), and it is shared by the hero's manager and by every BOT's weapon
//    manager. The old detour stored `this` of any call as "the hero's manager", so after an enemy equipped a gun (the C350/C351 ids in the
//    logs are enemy weapons) our hide/equip calls hit an enemy's manager, freed once the enemy was despawned: the execute-at-0 crashes.
//    Now: the hero's manager is resolved fresh (hero handle -> GetActor -> FindComponent) and its vtable checked before every use.
//  - The invariant behind the exe+374237 crash: every non-zero id in an equipped slot must be findable in the table. Hiding a gadget
//    (id zeroed) while it is the one in a slot breaks it, so a slot holding a gadget about to be hidden is unequipped first
//    (Equip(0, slot): the game itself does it all the time).
namespace gadgets {
    using EquipFn = bool (*)(void* self, int id, int slot, bool force, bool send_event);
    static EquipFn g_orig = nullptr;
    static std::atomic<bool> g_watch = false, g_lock = false;
    static std::atomic<uintptr_t> g_mgr = 0;   // the HERO's manager, resolved fresh by Mgr() (game thread); used for log filtering elsewhere
    static thread_local bool t_ours = false;
    static std::mutex g_mutex;
    static std::set<uint32_t> g_allowed;

    constexpr uintptr_t kHeroMgrVtableRva = 0x38B59B8;   // HeroWeaponManagerLocal
    constexpr int kSlots = 7;                            // 40-byte slot records from +0x6c: room for 7 (docs/RECON.md section 5)
    static const uint32_t kWebShooter = 0x9C89;

    static bool IsGadgetId(uint32_t id) { return id >= 0x9C82 && id <= 0x9C89; }

    static bool Detour(void* self, int id, int slot, bool force, bool send_event) {
        bool allowed;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            // The lock only guards the gadget ids (9C82-9C89). Everything else the game equips (id 0 clears, enemy weapons ...) must go
            // through: refusing those left slot 0 without a record and crashed the game (2026-11-07, exe+374237).
            allowed = !g_lock || t_ours || !IsGadgetId((uint32_t)id) || g_allowed.count((uint32_t)id) > 0;
        }
        if (!allowed) {
            LOG("gadgets: refused equip of %04X (slot %d): locked", id, slot);
            return false;
        }
        bool r = g_orig(self, id, slot, force, send_event);
        // Enemies equip through the same function: log the hero's manager and the gadget ids only.
        if (g_watch && ((uintptr_t)self == g_mgr.load() || IsGadgetId((uint32_t)id)))
            LOG("gadgets: equip id=%04X slot=%d force=%d event=%d -> %d%s", id, slot, (int)force, (int)send_event, (int)r,
                (uintptr_t)self == g_mgr.load() ? "" : " (not the hero's manager)");
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
            LOG("gadgets: exe+2162300 is not the expected Equip; not hooking");
            return false;
        }
        MH_STATUS s = MH_Initialize();
        if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) return false;
        ok = MH_CreateHook((void*)t, (void*)&Detour, (void**)&g_orig) == MH_OK && MH_EnableHook((void*)t) == MH_OK;
        LOG("gadgets: Equip hook %s", ok ? "installed" : "FAILED");
        return ok;
    }

    void Watch(bool on) { if (on && !Install()) return; g_watch = on; LOG("gadgets: watch %s", on ? "on" : "off"); }
    void Lock(bool on) { if (on && !Install()) return; g_lock = on; LOG("gadgets: lock %s", on ? "on" : "off"); }
    void Allow(uint32_t id) { std::lock_guard<std::mutex> lock(g_mutex); g_allowed.insert(id); LOG("gadgets: allowed %04X", id); }
    void ClearAllowed() { std::lock_guard<std::mutex> lock(g_mutex); g_allowed.clear(); }

    // ---- the hero's weapon manager, never cached across calls (game thread only) ----

    static uintptr_t Mgr() {
        auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
        uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
        uint32_t handle = 0;
        if (!get_actor || !memory::Read(system + 0x1C, handle) || !handle) { g_mgr = 0; return 0; }
        uintptr_t actor = get_actor(&handle);
        uintptr_t m = actor ? entity::FindComponent(actor, "HeroWeaponManagerLocal") : 0;
        uintptr_t vt = 0;
        if (!m || !memory::Read(m, vt) || vt != scan::FromRva(kHeroMgrVtableRva)) {
            static int shown = 0;
            if (m && shown++ < 5) LOG("gadgets: the weapon manager at %llx has an unexpected vtable (%llx); not touching it", (unsigned long long)m, (unsigned long long)vt);
            g_mgr = 0;
            return 0;
        }
        g_mgr = m;
        return m;
    }

    static uint32_t SlotId(uintptr_t m, int slot) { uint32_t v = 0; memory::Read(m + 0x6c + (uintptr_t)slot * 40, v); return v; }

    // The table: records of 0x18 bytes at m+0x1a8 (+0x0c weapon actor handle, +0x10 id, +0x14 enabled byte), count at m+0x628.
    static uintptr_t Record(uintptr_t m, uint32_t id) {
        uint32_t n = 0;
        if (!memory::Read(m + 0x628, n) || n > 64) return 0;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t v = 0;
            if (memory::Read(m + 0x1a8 + 0x18 * i + 0x10, v) && v == id) return m + 0x1a8 + 0x18 * i;
        }
        return 0;
    }

    // exe+21633c0(manager, id): the weapon actor's WeaponBase component, or null when the weapon actor does not exist yet.
    static uintptr_t WeaponOf(uintptr_t m, uint32_t id) {
        auto fn = (uintptr_t(*)(uintptr_t, int))scan::FromRva(0x21633c0);
        return fn ? fn(m, (int)id) : 0;
    }

    // Unequip a slot the way the game does (Equip(0, slot)); the hero's manager only.
    static bool ClearSlot(uintptr_t m, int slot) {
        if (!g_orig) return false;
        t_ours = true;
        bool r = g_orig((void*)m, 0, slot, true, false);
        t_ours = false;
        LOG("gadgets: slot %d unequipped -> %d", slot, (int)r);
        return r;
    }

    // Hide: the table entry's id is zeroed: the wheel lists the gadget red with ammo 0/0 and unusable (verified in game 2026-11-07). Show writes
    // the id back. Before hiding, any slot that holds the gadget is unequipped (the invariant above).
    static std::set<uint32_t> g_hidden;
    static bool HideImpl(uint32_t id, bool hide) {
        uintptr_t m = Mgr();
        if (!m) { LOG("gadgets: no hero weapon manager right now; %s %04X skipped", hide ? "hide" : "show", id); return false; }
        std::lock_guard<std::mutex> lock(g_mutex);
        if (hide) {
            uintptr_t rec = Record(m, id);
            if (!rec) { LOG("gadgets: hide %04X: not in the table (already hidden?)", id); return false; }
            for (int s = 0; s < kSlots; s++)
                if (SlotId(m, s) == id && !ClearSlot(m, s)) {   // never leave a slot holding an id that is no longer in the table
                    LOG("gadgets: hide %04X aborted: slot %d could not be unequipped", id, s);
                    return false;
                }
            memory::Write<uint32_t>(rec + 0x10, 0);
            g_hidden.insert(id);
        } else {
            uint32_t n = 0;
            if (!g_hidden.count(id) || !memory::Read(m + 0x628, n)) return false;
            for (uint32_t i = 0; i < n && i < 64; i++) {   // the zeroed record: table order is 9C43, 9C82..9C89, 9C92
                uint32_t v = 1;
                if (memory::Read(m + 0x1a8 + 0x18 * i + 0x10, v) && v == 0 && IsGadgetId(id) && id - 0x9C82 == i - 1) {
                    memory::Write<uint32_t>(m + 0x1a8 + 0x18 * i + 0x10, id);
                    g_hidden.erase(id);
                    break;
                }
            }
        }
        LOG("gadgets: %s %04X", hide ? "hid" : "showed", id);
        return true;
    }
    bool Hide(uint32_t id, bool hide) {
        auto r = std::make_shared<bool>(false);   // shared: the job may outlive a timed-out wait
        gamethread::Run([id, hide, r] { *r = HideImpl(id, hide); }, 5000);
        return *r;
    }

    // ---- a run's gadgets (Campaign rewards) ----
    static uint32_t g_saved = 0;   // what was in slot 0 before the run (logged only)
    static bool g_run = false;
    static std::atomic<bool> g_denyweb = false;
    bool IsWebGadget(uint32_t id) { return id >= 0x9C82 && id <= 0x9C85; }

    void BeginRun() {
        if (!Install()) return;
        gamethread::Run([] {
            uintptr_t m = Mgr();
            if (!m) { LOG("gadgets: no weapon manager; gadgets stay as they are"); return; }
            g_saved = SlotId(m, 0);
            for (uint32_t id = 0x9C82; id <= 0x9C88; id++) HideImpl(id, true);
            ClearAllowed();
            Allow(kWebShooter);
            Lock(true);
            g_watch = true;   // log the hero's equips during a run
            g_run = true;
            LOG("gadgets: run start: all gadgets hidden, the wheel is locked to the Web Shooter (slot 0 held %04X)", g_saved);
        }, 5000);
    }

    // A granted gadget is unhidden and allowed; it shows red 0/0 until the player equips it from the wheel (a hint says so).
    // Why it is red (RECON section 7): its weapon actor does not exist until the first real equip. `gadget create` + `gadget refresh` below
    // are the (untested) way to fix that; the mod makes no equip call by itself during a run (four crashes, 2026-11-07).
    void Grant(uint32_t id) {
        if (g_denyweb && IsWebGadget(id)) { LOG("gadgets: grant %04X ignored: the web-fluid curse", id); return; }
        gamethread::Run([id] {
            HideImpl(id, false);
            Allow(id);
            LOG("gadgets: granted %04X (not equipped; pick it in the wheel)", id);
        }, 5000);
    }

    bool Owned(uint32_t id) { std::lock_guard<std::mutex> lock(g_mutex); return g_allowed.count(id) > 0 && id != kWebShooter; }

    void EndRun() {
        if (!g_run) return;
        g_run = false;
        g_denyweb = false;
        gamethread::Run([] {
            // The hero may be dead or respawning: Mgr() finds the live manager or none (a new hero has a fresh table anyway).
            const bool live = Mgr() != 0;
            if (live) for (uint32_t id = 0x9C82; id <= 0x9C88; id++) HideImpl(id, false);
            { std::lock_guard<std::mutex> lock(g_mutex); g_hidden.clear(); }
            Lock(false);
            ClearAllowed();
            LOG("gadgets: run end: gadgets %s", live ? "restored" : "left hidden (no live hero; the next hero has a fresh table)");
        }, 5000);
    }

    void DenyWeb() {
        g_denyweb = true;
        gamethread::Run([] {
            for (uint32_t id = 0x9C82; id <= 0x9C85; id++) {
                HideImpl(id, true);
                std::lock_guard<std::mutex> lock(g_mutex);
                g_allowed.erase(id);
            }
            LOG("gadgets: the web gadgets are gone for this run");
        }, 5000);
    }

    uint32_t Held() {   // game thread
        uintptr_t m = Mgr();
        return m ? SlotId(m, 0) : 0xFFFFFFFFu;
    }

    // ---- dev commands (docs/HOW_TO.md): everything below is untested in game ----

    // `gadget equip <id> <slot>`: our own equip with every precondition from RECON section 5 checked first.
    bool Equip(uint32_t id, int slot) {
        auto r = std::make_shared<bool>(false);
        gamethread::Run([id, slot, r] {
            uintptr_t m = Mgr();
            if (!g_orig || !m) { LOG("gadgets: equip: no hook or no hero manager"); return; }
            if (slot < 0 || slot >= kSlots) { LOG("gadgets: equip: slot %d out of range", slot); return; }
            if (id != 0) {
                uintptr_t rec = Record(m, id);
                if (!rec) { LOG("gadgets: equip %04X refused by us: no table record (hidden?)", id); return; }
                if (!WeaponOf(m, id)) { LOG("gadgets: equip %04X refused by us: its weapon actor does not exist (run `gadget create %04X` first)", id, id); return; }
            }
            t_ours = true;
            *r = g_orig((void*)m, (int)id, slot, true, true);
            t_ours = false;
            LOG("gadgets: our equip %04X slot %d -> %d", id, slot, (int)*r);
        }, 5000);
        return *r;
    }

    // `gadget create <id>`: make the gadget's weapon actor (vtable +0x70 = exe+2161ba0 CreateWeapon), the missing piece of the red state.
    void Create(uint32_t id) {
        gamethread::Run([id] {
            uintptr_t m = Mgr();
            if (!m) { LOG("gadgets: create: no hero manager"); return; }
            if (!Record(m, id)) { LOG("gadgets: create %04X: no table record", id); return; }
            if (WeaponOf(m, id)) { LOG("gadgets: create %04X: the weapon actor already exists", id); return; }
            uintptr_t vt = 0, fn = 0;
            memory::Read(m, vt);
            memory::Read(vt + 0x70, fn);
            if (!fn) { LOG("gadgets: create: no vtable entry"); return; }
            ((void (*)(uintptr_t, int))fn)(m, (int)id);
            LOG("gadgets: create %04X -> weapon actor %s", id, WeaponOf(m, id) ? "now exists" : "STILL missing");
        }, 5000);
    }

    // `gadget refresh`: force the wheel to rebuild its list (PlayerHudQuickSelect::RefreshList, exe+793320(this, true)).
    void Refresh() {
        gamethread::Run([] {
            // HUD root for player 0 = qword [exe+7c2b5a0]; the wheel (GUI::PlayerHudQuickSelect, vtable exe+3894040) hangs off it at +0x1b8.
            uintptr_t root = 0, qs = 0, vt = 0;
            if (!memory::Read(scan::FromRva(0x7c2b5a0), root) || !root || !memory::Read(root + 0x1b8, qs) || !qs) { LOG("gadgets: refresh: no quick-select object"); return; }
            if (!memory::Read(qs, vt) || vt != scan::FromRva(0x3894040)) { LOG("gadgets: refresh: the object at root+0x1b8 is not the wheel (vtable %llx)", (unsigned long long)vt); return; }
            ((void (*)(uintptr_t, bool))scan::FromRva(0x793320))(qs, true);
            LOG("gadgets: wheel list refreshed");
        }, 5000);
    }

    // `gadget slots`: the hero's manager, the equipped slots and the whole table (id, enabled byte, weapon actor handle).
    void Dump() {
        gamethread::Run([] {
            uintptr_t m = Mgr();
            if (!m) { LOG("gadgets: dump: no hero weapon manager"); return; }
            LOG("gadgets: hero weapon manager %llx (vtable ok)", (unsigned long long)m);
            for (int s = 0; s < kSlots; s++) {
                uint32_t id = SlotId(m, s);
                LOG("gadgets:   slot %d: %04X%s", s, id, id && !Record(m, id) ? "  (NOT in the table!)" : "");
            }
            uint32_t n = 0;
            memory::Read(m + 0x628, n);
            for (uint32_t i = 0; i < n && i < 64; i++) {
                uintptr_t rec = m + 0x1a8 + 0x18 * i;
                uint32_t h = 0, id = 0;
                uint8_t en = 0;
                memory::Read(rec + 0x0c, h);
                memory::Read(rec + 0x10, id);
                memory::Read(rec + 0x14, en);
                LOG("gadgets:   table %u: id %04X enabled %u actor handle %08X weapon %s", i, id, en, h, id && WeaponOf(m, id) ? "yes" : "no");
            }
        }, 5000);
    }
}
