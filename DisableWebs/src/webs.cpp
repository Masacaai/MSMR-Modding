#include "webs.h"

#include "hero.h"
#include "log.h"
#include "memory.h"
#include "scan.h"
#include "transitions.h"

#include <Windows.h>
#include <atomic>
#include <cstdio>

// How the lock works (all [RE], docs/internals/web-moves-and-afterimages.md section 1):
//  - HeroAbilityLock (vtable exe+38aa530): Lock(this, ability, layer, seconds) at vtable +0x50 = exe+1e8a490, Unlock(this, ability, layer) at +0x58 = exe+1e8a640.
//    Layer 0 is a boolean layer (mission "toggle hero moves"); counters are bytes at comp+0x1c8 + layer*0x80 + ability.
//  - HeroCombatActionLocker (vtable exe+38ae8f0): +0x60 = exe+1e89a50 does `[this+0xe8] |= mask`, +0x68 = exe+1e89a60 does `[this+0xe8] &= ~mask`.
//    The mask uses the combat-action flag set (kWebStrike 0x2, kWebYankBot 0x8, ...).
namespace webs {
    enum Ability { kShoot = 15, kWebZip = 21, kWebSnap = 22, kWebSwing = 24, kZipJump = 25, kPointLaunch = 28, kZipToSurface = 35, kGadgetWheel = 38 };
    constexpr uintptr_t kLockRva = 0x1e8a490, kUnlockRva = 0x1e8a640;       // HeroAbilityLock vtable +0x50 / +0x58
    constexpr uintptr_t kCombatLockRva = 0x1e89a50, kCombatUnlockRva = 0x1e89a60;   // HeroCombatActionLocker vtable +0x60 / +0x68
    // kTraversalStrike 0x1, kWebStrike 0x2, kWebPounce 0x4, kWebYankBot 0x8, kWebYankBotAttachment 0x10, kWebYankEnviro 0x20, kWebThrowObject 0x40,
    // kImpactWeb 0x80, kElectricWeb 0x200, kWebBomb 0x1000, kWebLine 0x2000, kWebThrowWeapon 0x200000, kWebBlast 0x400000
    constexpr uint32_t kWebCombatMask = 0x6032FF;
    constexpr int kAll[] = { kShoot, kWebZip, kWebSnap, kWebSwing, kZipJump, kPointLaunch, kZipToSurface, kGadgetWheel };

    static std::atomic<bool> g_enabled{ false };   // off until F7 (the mod starts with webs allowed)
    static bool g_we_locked[128] = {};     // abilities this mod set on layer 0 (so only those are lifted)
    static bool g_we_combat = false;
    static bool g_vtables_ok = true;       // false once a vtable slot is not where the recon says (then nothing is called)
    static int g_applied = 0, g_lifted = 0;
    static uintptr_t g_last_lock = 0;
    static std::atomic<int> g_state{ 0 };   // 0 not applied yet, 1 hero found and locks active, 2 locks lifted, 3 hero/component not found

    void SetEnabled(bool on) { g_enabled = on; transitions::SetBlocked(on); LOG("webs: %s", on ? "ON (webs locked)" : "OFF (webs allowed)"); }
    bool Enabled() { return g_enabled; }

    static bool SlotIs(uintptr_t comp, int slot_off, uintptr_t rva) {
        uintptr_t vt = 0, fn = 0;
        if (!memory::Read(comp, vt) || !vt || !memory::Read(vt + slot_off, fn)) return false;
        return fn == scan::FromRva(rva);
    }

    static bool CallLock(uintptr_t comp, int ability, int layer, bool lock) {
        uintptr_t vt = 0, fn = 0;
        if (!memory::Read(comp, vt) || !vt || !memory::Read(vt + (lock ? 0x50 : 0x58), fn) || !fn) return false;
        __try {
            if (lock) ((void (*)(uintptr_t, int, int, float))fn)(comp, ability, layer, 0.0f);
            else ((void (*)(uintptr_t, int, int))fn)(comp, ability, layer);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    static bool CallCombat(uintptr_t comp, uint32_t mask, bool lock) {
        uintptr_t vt = 0, fn = 0;
        if (!memory::Read(comp, vt) || !vt || !memory::Read(vt + (lock ? 0x60 : 0x68), fn) || !fn) return false;
        __try { ((void (*)(uintptr_t, uint32_t))fn)(comp, mask); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // The gadget wheel is left alone: only the web abilities and the web shooter are locked.
    static bool Wanted(int ability) { return ability != kGadgetWheel; }

    void Tick() {
        const bool want = g_enabled;
        if (!want && !g_applied) { g_state = want ? 3 : 2; return; }   // never applied anything: nothing to lift
        const hero::Info h = hero::Read();
        if (!h.ok) { g_state = 3; return; }
        std::string lock_name, combat_name;
        const uintptr_t lock = hero::Component(h.actor, "HeroAbilityLock", &lock_name);
        const uintptr_t combat = hero::Component(h.actor, "HeroCombatActionLocker", &combat_name);
        if (lock != g_last_lock) {
            if (g_last_lock) memset(g_we_locked, 0, sizeof(g_we_locked));   // a new component (respawn): the old bytes are gone with it
            g_we_combat = false;
            g_last_lock = lock;
            LOG("webs: hero %08x HeroAbilityLock=%llx (%s) HeroCombatActionLocker=%llx (%s)", h.handle, (unsigned long long)lock, lock_name.c_str(),
                (unsigned long long)combat, combat_name.c_str());
            if (lock) {
                g_vtables_ok = SlotIs(lock, 0x50, kLockRva) && SlotIs(lock, 0x58, kUnlockRva);
                if (!g_vtables_ok) LOG("webs: HeroAbilityLock vtable slots are not at the expected addresses; the lock is NOT applied");
            }
            if (combat && !(SlotIs(combat, 0x60, kCombatLockRva) && SlotIs(combat, 0x68, kCombatUnlockRva))) {
                LOG("webs: HeroCombatActionLocker vtable slots are not at the expected addresses; the combat web lock is NOT applied");
                g_vtables_ok = false;
            }
        }
        if (!lock || !g_vtables_ok) { g_state = 3; return; }

        for (int a : kAll) {
            uint8_t cur = 0;
            if (!memory::Read(lock + 0x1c8 + a, cur)) continue;   // layer 0 counter byte
            if (want && Wanted(a)) {
                if (cur == 0 && CallLock(lock, a, 0, true)) { g_we_locked[a] = true; g_applied++; }
            } else if (g_we_locked[a]) {
                if (cur != 0) CallLock(lock, a, 0, false);
                g_we_locked[a] = false; g_lifted++;
            }
        }
        if (combat) {
            uint32_t mask = 0;
            if (memory::Read(combat + 0xe8, mask)) {
                if (want) {
                    if ((mask & kWebCombatMask) != kWebCombatMask && CallCombat(combat, kWebCombatMask, true)) { g_we_combat = true; g_applied++; }
                } else if (g_we_combat) {
                    CallCombat(combat, kWebCombatMask, false);
                    g_we_combat = false; g_lifted++;
                }
            }
        }
        const int prev = g_state;
        g_state = want ? 1 : 2;
        if (prev != g_state) LOG("webs: %s (applied %d, lifted %d)", want ? "locks active" : "locks lifted", g_applied, g_lifted);
    }

    std::string Status() {
        const char* what[] = { "not applied yet", "locks active", "locks lifted", "hero or lock component not found" };
        char buf[160];
        snprintf(buf, sizeof(buf), "webs %s: %s", g_enabled ? "ON" : "OFF", what[g_state]);
        return buf;
    }
}
