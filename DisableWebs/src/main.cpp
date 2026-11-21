// Disable Webs: Spider-Man cannot swing, zip, point-launch or shoot webs. Marvel's Spider-Man Remastered, Spider-Man.exe 4.0630.0.0.
// Loaded by Overstrike's scripts proxy (LoadLibrary from scripts\, then script_enable()). How and why: docs/FLASH_RECON.md section 1.
//
// The lock starts OFF; F7 (game window focused) turns it on and off. Log: scripts\DisableWebs.log. Nothing is written anywhere else.
#include "gamethread.h"
#include "hero.h"
#include "log.h"
#include "natives.h"
#include "scan.h"
#include "transitions.h"
#include "webs.h"

#include <Windows.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

static std::string ModuleDir() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&ModuleDir, &self);
    char path[MAX_PATH];
    GetModuleFileNameA(self, path, MAX_PATH);
    std::string s(path);
    return s.substr(0, s.find_last_of("\\/"));
}

// A line in the game's notification box (slot 0), as EndlessArena's hud::Show does it (the game keeps the text pointer, so the buffer stays alive).
static void Notify(const std::string& text) {
    using ShowFn = void (*)(uintptr_t hud, int slot, const char* text, float seconds, int a5, int a6, uint8_t a7, uint8_t a8);
    auto show = (ShowFn)natives::Get(natives::HudShow);
    const uintptr_t obj = natives::Get(natives::HudMessages);
    if (!show || !obj || !gamethread::Installed()) return;
    static std::shared_ptr<std::string> buffer;
    auto buf = std::make_shared<std::string>(text);
    gamethread::Post([show, obj, buf] {
        buffer = buf;
        __try { show(obj, 0, buffer->c_str(), 3.0f, 0, 0, 0, 1); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    });
}

static bool GameFocused() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

static void Main() {
    logging::Init(ModuleDir() + "\\DisableWebs.log");
    LOG("DisableWebs 1.0.0 loaded");
    const int ok = natives::ResolveAll();
    LOG("%d/%d natives at expected addresses", ok, (int)natives::Count);
    if (!natives::Get(natives::GetActor) || !gamethread::Install()) {
        LOG("GetActor or the game-thread hook is unavailable (wrong game build?); disabled");
        return;
    }
    transitions::Install();   // wall-run corner tether (a failure is logged and leaves the ability locks working)
    webs::SetEnabled(false);   // off by default: F7 locks the webs
    LOG("ready. Webs are allowed; F7 locks them (and allows them again).");

    bool was_down = false;
    ULONGLONG next = 0;
    auto pending = std::make_shared<std::atomic<bool>>(false);
    for (;;) {
        const bool down = GameFocused() && (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (down && !was_down) {
            webs::SetEnabled(!webs::Enabled());
            Notify(webs::Enabled() ? "Webs disabled (F7 to allow)" : "Webs allowed (F7 to disable)");
        }
        was_down = down;
        static bool mark_was = false;
        const bool mark = GameFocused() && (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (mark && !mark_was) LOG("---- MARK (F9) ----");
        mark_was = mark;
        // The locks run on the game thread a few times a second (idempotent; also re-applies after a respawn).
        const ULONGLONG now = GetTickCount64();
        if (now >= next && !pending->load()) {
            next = now + 250;
            pending->store(true);
            gamethread::Post([pending] { webs::Tick(); pending->store(false); });
        }
        Sleep(40);
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }

extern "C" __declspec(dllexport) void script_enable() { std::thread(Main).detach(); }
