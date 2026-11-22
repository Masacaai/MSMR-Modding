#include "herofade.h"

#include "entity.h"
#include "fnatives.h"
#include "gamethread.h"
#include "log.h"
#include "memory.h"

#include <MinHook.h>
#include <Windows.h>
#include <atomic>
#include <cstdlib>
#include <intrin.h>
#include <memory>

namespace herofade {
namespace {
    using SetModifiedFn = void (*)(uintptr_t, uint32_t);
    SetModifiedFn g_orig = nullptr;
    bool g_hooked = false, g_hook_failed = false;

    constexpr int kMax = 96;
    std::atomic<uintptr_t> g_models[kMax];   // his model instance and skin parts
    std::atomic<int> g_count{ 0 };
    std::atomic<bool> g_on{ false };
    std::atomic<uint8_t> g_alpha{ 255 };
    std::atomic<int> g_logged{ 0 };
    std::shared_ptr<std::atomic<bool>> g_loop;   // the per-tick loop's alive flag

    bool Ours(uintptr_t mi) {
        const int n = g_count.load();
        for (int i = 0; i < n && i < kMax; i++) if (g_models[i].load() == mi) return true;
        return false;
    }

    void SetModifiedDetour(uintptr_t mi, uint32_t mask) {
        if (g_on.load(std::memory_order_relaxed) && Ours(mi)) {
            if (g_logged < 12) { g_logged++; LOG("herofade: SetModified on his model %llx from exe+%llx", (unsigned long long)mi, (unsigned long long)((uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandleA(nullptr))); }
            memory::Write<uint8_t>(mi + 0xbe, g_alpha.load());
        }
        g_orig(mi, mask);
    }

    bool EnsureHook() {
        if (g_hooked) return true;
        if (g_hook_failed) return false;
        const uintptr_t fn = fnat::Get(fnat::SetModified);
        if (!fn) { g_hook_failed = true; return false; }
        const MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) { g_hook_failed = true; LOG("herofade: MH_Initialize failed (%d)", st); return false; }
        if (MH_CreateHook((void*)fn, (void*)&SetModifiedDetour, (void**)&g_orig) != MH_OK || MH_EnableHook((void*)fn) != MH_OK) { g_hook_failed = true; LOG("herofade: hooking SetModified failed"); return false; }
        g_hooked = true;
        LOG("herofade: SetModified hooked");
        return true;
    }

    // His model instance and every skin part, collected into g_models (parts are built over time, so it is refreshed every tick).
    int Collect(uintptr_t actor, uintptr_t out[kMax]) {
        int n = 0;
        uintptr_t mi = 0;
        if (memory::Read(actor, mi) && mi) out[n++] = mi;
        const uintptr_t sm = entity::FindComponent(actor, "SkinManager");
        uint32_t cnt = 0;
        if (sm && memory::Read(sm + 0x4c, cnt) && cnt > 0 && cnt <= 256) {
            for (uint32_t i = 0; i < cnt && n < kMax; i++) for (unsigned v = 0; v < 2 && n < kMax; v++) {
                const uintptr_t part = fnat::SkinPart_(sm, i, v);
                uint8_t invalid = 1;
                if (part && memory::Read(part + 0x5f, invalid) && !invalid) out[n++] = part;
            }
        }
        return n;
    }

    void Tick(std::shared_ptr<std::atomic<bool>> alive, uintptr_t actor, float strength) {
        if (!alive->load()) return;
        uintptr_t found[kMax];
        const int n = Collect(actor, found);
        for (int i = 0; i < n; i++) g_models[i].store(found[i]);
        g_count.store(n);
        const uint8_t a = (uint8_t)(255.0f * (1.0f - strength * ((float)(rand() % 1000) / 1000.0f)));
        g_alpha.store(a);
        for (int i = 0; i < n; i++) fnat::SetModified_(found[i]);   // goes through the hook, which writes the byte
        gamethread::Post([alive, actor, strength] { Tick(alive, actor, strength); });
    }
}

    void Start(uintptr_t hero_actor, float strength) {
        if (strength <= 0.0f || !hero_actor || !EnsureHook()) return;
        if (g_loop) g_loop->store(false);
        g_loop = std::make_shared<std::atomic<bool>>(true);
        g_logged = 0;
        g_on = true;
        auto alive = g_loop;
        gamethread::Post([alive, hero_actor, strength] { Tick(alive, hero_actor, strength); });
        LOG("herofade: flicker on (strength %.2f)", strength);
    }

    void Stop() {
        if (!g_on.load()) return;
        if (g_loop) g_loop->store(false);
        // On the game thread, after the loop's last tick: full opacity through the same hook, then the hook goes passive.
        gamethread::Post([] {
            g_alpha.store(255);
            const int n = g_count.load();
            uintptr_t models[kMax];
            for (int i = 0; i < n && i < kMax; i++) models[i] = g_models[i].load();
            for (int i = 0; i < n && i < kMax; i++) fnat::SetModified_(models[i]);
            g_on = false;
            g_count = 0;
            LOG("herofade: flicker off");
        });
    }

    bool Active() { return g_on.load(); }
}
