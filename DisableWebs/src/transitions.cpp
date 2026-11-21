#include "transitions.h"

#include "log.h"
#include "scan.h"

#include <Windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <utility>

// HeroTransitionManager (vtable exe+38b4350) has one "try to enter state X" function per transition, each returning bool (true: the transition was taken).
// Refusing it leaves the hero in the state it was in. [RE] from disassembly of exe+984be0; nothing here was played yet.
//   exe+984be0 (slot 126): wall-run corner tether. Builds a HeroStateWallRunCornerSling request and hands it to the state machine (vtable +0x50); returns its result.
//     Arguments: (this, vec3* a, vec3* b, vec3* c, bool d); the fifth is on the stack, so it is declared here and passed through.
namespace transitions {
    namespace {
        constexpr uintptr_t kCornerSlingRva = 0x984be0;
        const unsigned char kCornerSlingPrologue[] = { 0x48,0x8B,0xC4, 0x48,0x89,0x58,0x10, 0x48,0x89,0x70,0x18, 0x48,0x89,0x78,0x20, 0x41,0x56, 0x48,0x81,0xEC,0xC0,0x00,0x00,0x00 };

        using TryFn = bool (*)(void*, void*, void*, void*, bool);
        TryFn g_orig_corner = nullptr;
        std::atomic<bool> g_blocked{ false };
        std::atomic<int> g_refused{ 0 };
        bool g_installed = false;

        bool CornerDetour(void* self, void* a, void* b, void* c, bool d) {
            if (g_blocked.load(std::memory_order_relaxed)) {
                const int n = ++g_refused;
                if (n <= 3 || n % 50 == 0) LOG("transitions: corner tether refused (%d)", n);
                return false;
            }
            return g_orig_corner(self, a, b, c, d);
        }
    }

    // Wall-run pull-up: slot 128 (exe+985610, HeroStateWallRunJump; slot 132 TopCrest was tried first and refusing it changed nothing). The 2026-11-20 log showed it succeeding together with slot 128 (WallRunJump) at the moment JUMP was
    // pressed while wall-running. Signature unknown, so every register and ten stack words are passed straight on (nothing touches xmm before the call).
    namespace {
        constexpr uintptr_t kCrestRva = 0x985610;
        using AnyFn = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
        AnyFn g_crest_orig = nullptr;
        uint64_t CrestDetour(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t s0, uint64_t s1, uint64_t s2, uint64_t s3, uint64_t s4, uint64_t s5, uint64_t s6, uint64_t s7, uint64_t s8, uint64_t s9) {
            if (g_blocked.load(std::memory_order_relaxed)) {
                static std::atomic<int> n{ 0 };
                const int k = ++n;
                if (k <= 3 || k % 50 == 0) LOG("transitions: wall-run pull-up (WallRunJump, slot 128) refused (%d)", k);
                return 0;
            }
            return g_crest_orig(a, b, c, d, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9);
        }
    }

    // Wall-run leap (slot 125, exe+984a10, HeroStateWallRunLeap): with slot 128 refused, the log of 2026-11-20 showed the game falling back to this one, and the
    // pull-up still happened, so both are refused while the lock is on.
    namespace {
        constexpr uintptr_t kLeapRva = 0x984a10;
        AnyFn g_leap_orig = nullptr;
        uint64_t LeapDetour(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t s0, uint64_t s1, uint64_t s2, uint64_t s3, uint64_t s4, uint64_t s5, uint64_t s6, uint64_t s7, uint64_t s8, uint64_t s9) {
            if (g_blocked.load(std::memory_order_relaxed)) {
                static std::atomic<int> n{ 0 };
                const int k = ++n;
                if (k <= 3 || k % 50 == 0) LOG("transitions: wall-run leap (slot 125) refused (%d)", k);
                return 0;
            }
            return g_leap_orig(a, b, c, d, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9);
        }
    }

    bool Install() {
        if (g_installed) return true;
        const uintptr_t target = scan::FromRva(kCornerSlingRva);
        if (memcmp((const void*)target, kCornerSlingPrologue, sizeof(kCornerSlingPrologue)) != 0) {
            LOG("transitions: exe+%llx is not the expected function (wrong game build?); corner tether is NOT blocked", (unsigned long long)kCornerSlingRva);
            return false;
        }
        MH_STATUS s = MH_Initialize();
        if (s != MH_OK && s != MH_ERROR_ALREADY_INITIALIZED) { LOG("transitions: MH_Initialize failed (%d)", s); return false; }
        s = MH_CreateHook((void*)target, (void*)&CornerDetour, (void**)&g_orig_corner);
        if (s != MH_OK) { LOG("transitions: MH_CreateHook failed (%d)", s); return false; }
        s = MH_EnableHook((void*)target);
        if (s != MH_OK) { LOG("transitions: MH_EnableHook failed (%d)", s); MH_RemoveHook((void*)target); return false; }
        g_installed = true;
        LOG("transitions: corner tether hook installed");
        void* crest = (void*)scan::FromRva(kCrestRva);
        if (MH_CreateHook(crest, (void*)&CrestDetour, (void**)&g_crest_orig) != MH_OK || MH_EnableHook(crest) != MH_OK) LOG("transitions: could not hook the wall-run pull-up");
        else LOG("transitions: wall-run pull-up hook installed");
        void* leap = (void*)scan::FromRva(kLeapRva);
        if (MH_CreateHook(leap, (void*)&LeapDetour, (void**)&g_leap_orig) != MH_OK || MH_EnableHook(leap) != MH_OK) LOG("transitions: could not hook the wall-run leap");
        else LOG("transitions: wall-run leap hook installed");
        return true;
    }

    void SetBlocked(bool on) { g_blocked = on; }
}
