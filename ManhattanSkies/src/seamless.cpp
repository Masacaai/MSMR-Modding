#include "seamless.h"

#include "log.h"
#include "scan.h"

#include <MinHook.h>
#include <Windows.h>
#include <atomic>
#include <cstring>

namespace seamless {
namespace {
    constexpr uintptr_t kHandlerRva = 0x710970;   // DynamicLoadScreenSystem::OnLightingTransitionStarted(system, event): event+0x138 from-mode, +0x13c to-mode
    const unsigned char kBytes[] = { 0x40, 0x53, 0x48, 0x83, 0xec, 0x70, 0x80, 0x79, 0x18, 0x00 };   // push rbx; sub rsp,70h; cmp byte [rcx+18h],0
    using HandlerFn = void(__fastcall*)(void*, void*);
    HandlerFn g_orig = nullptr;
    std::atomic<bool> g_enabled{ false }, g_installed{ false };
    std::atomic<ULONGLONG> g_armed_until{ 0 };
    std::atomic<unsigned> g_skipped{ 0 }, g_passed{ 0 };

    bool BytesMatch(uintptr_t at) {
        __try { return std::memcmp(reinterpret_cast<const void*>(at), kBytes, sizeof(kBytes)) == 0; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
    int ModeOf(void* ev, size_t off) {
        __try { return *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(ev) + off); } __except (EXCEPTION_EXECUTE_HANDLER) { return -99; }
    }

    void __fastcall Detour(void* sys, void* ev) {
        if (g_enabled.load() && GetTickCount64() < g_armed_until.load()) {
            const unsigned n = ++g_skipped;
            if (n <= 40) LOG("seamless: skipped the load screen of a lighting transition %d -> %d (#%u)", ModeOf(ev, 0x138), ModeOf(ev, 0x13c), n);
            return;   // exactly what the handler does itself for a transition its config marks SkipLoadScreen
        }
        ++g_passed;
        g_orig(sys, ev);
    }

    bool Install() {
        if (g_installed.load()) return true;
        const uintptr_t at = scan::FromRva(kHandlerRva);
        if (!BytesMatch(at)) { LOG("seamless: the code at exe+%llx is not this game's: not hooking", (unsigned long long)kHandlerRva); return false; }
        const MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) { LOG("seamless: MinHook init failed (%d)", (int)st); return false; }
        if (MH_CreateHook(reinterpret_cast<void*>(at), reinterpret_cast<void*>(&Detour), reinterpret_cast<void**>(&g_orig)) != MH_OK || MH_EnableHook(reinterpret_cast<void*>(at)) != MH_OK) {
            LOG("seamless: could not hook exe+%llx", (unsigned long long)kHandlerRva);
            return false;
        }
        g_installed = true;
        LOG("seamless: hooked the lighting-transition load-screen handler at exe+%llx", (unsigned long long)kHandlerRva);
        return true;
    }
}

    void SetEnabled(bool on) {
        if (on && !g_installed.load() && !Install()) { g_enabled = false; return; }
        if (g_enabled.exchange(on) != on) LOG("seamless: %s", on ? "on" : "off");
    }
    bool Enabled() { return g_enabled.load(); }
    void Arm(unsigned ms) { if (g_enabled.load()) g_armed_until = GetTickCount64() + ms; }
    void LogStatus() { LOG("seamless: enabled=%d installed=%d skipped=%u passed=%u", (int)g_enabled.load(), (int)g_installed.load(), g_skipped.load(), g_passed.load()); }
}
