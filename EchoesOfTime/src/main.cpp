// Echoes of Time: two time-based abilities from Spider-Man: Edge of Time for Marvel's Spider-Man Remastered (4.0630.0.0), chosen by the ABILITY row: Hyper-Sense (slow motion, T or D-pad Right,
// plus a fading trail of after-images) and Accelerated Decoy (the same button leaves one copy of him that enemies shoot at; ghosts.cpp).
// The slow-motion part is the same mechanism as nanodanc's "QuantumBreak" Script Hook mod (and QuantumBreak Renewed, the Overstrike port of it): the hero's
// "HeroTimeScaleManager" component gets a float time scale at +0x50, a float Spider-Man speed multiplier at +0x4C and a byte "active" flag at +0x5C, rewritten every
// 10 ms while on (found by reading that mod's imports, strings and code, not by copying it). Echoes of Time supersedes QBR: do not install both.
//
// Everything is set in the in-game ECHOES OF TIME tab (menu.cpp, settings.cpp); there is no .modular. The after-images are ghosts.cpp (docs/internals/web-moves-and-afterimages.md).
#include "log.h"
#include "entity.h"
#include "fnatives.h"
#include "gamethread.h"
#include "ghosts.h"
#include "hero.h"
#include "menu.h"
#include "memory.h"
#include "natives.h"
#include "psinput.h"
#include "scan.h"
#include "settings.h"
#include "speedfx.h"

#include <MinHook.h>
#include <Windows.h>
#include <Xinput.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>

namespace {
    constexpr uintptr_t kScaleOffset = 0x50;   // float: world time scale
    constexpr uintptr_t kSpeedOffset = 0x4C;   // float: Spider-Man's own speed multiplier
    constexpr uintptr_t kActiveOffset = 0x5C;  // byte: the component's "time scaling is active" flag
    constexpr int kToggleKey = 84;             // 'T'

    std::string g_dll_dir;

    std::string ModuleDir() {
        HMODULE self = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&ModuleDir, &self);
        char path[MAX_PATH];
        GetModuleFileNameA(self, path, MAX_PATH);
        std::string s(path);
        return s.substr(0, s.find_last_of("\\/"));
    }

    // ---- the game's component ----

    // The hero's HeroTimeScaleManager component, or 0 (not in a level yet, loading, ...). Looked up fresh each time: a pointer
    // kept across a load would dangle.
    uintptr_t HeroTimeScaleComponent() {
        uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
        uint32_t handle = 0;
        auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
        if (!get_actor || !memory::Read(system + 0x1C, handle) || !handle) return 0;
        uintptr_t actor = 0;
        __try { actor = get_actor(&handle); } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return actor ? entity::FindComponent(actor, "HeroTimeScaleManager") : 0;
    }

    void Apply(bool on) {
        uintptr_t c = HeroTimeScaleComponent();
        if (!c) return;
        const settings::S& s = settings::Get();
        memory::Write(c + kScaleOffset, on ? s.slow_scale : 1.0f);
        memory::Write(c + kSpeedOffset, on ? s.hero_speed : 1.0f);
        memory::Write(c + kActiveOffset, (uint8_t)(on ? 1 : 0));
    }

    // The brief slow-motion beat when a decoy appears (Accelerated Decoy's ENTRY SLOW-MOTION row): the same component, a fixed strength.
    void ApplyEntry(bool on) {
        uintptr_t c = HeroTimeScaleComponent();
        if (!c) return;
        memory::Write(c + kScaleOffset, on ? 0.12f : 1.0f);
        memory::Write(c + kSpeedOffset, 1.0f);
        memory::Write(c + kActiveOffset, (uint8_t)(on ? 1 : 0));
    }

    // ---- in-game message (optional) ----

    // The game-thread hook is installed on first need (messages, after-images), so the plain slow-motion toggle hooks nothing it does not use.
    bool EnsureGameThread() {
        static bool tried = false;
        if (!tried) { tried = true; if (!gamethread::Install()) LOG("game-thread hook unavailable"); }
        return gamethread::Installed();
    }

    void ShowMessage(const char* text) {
        if (!EnsureGameThread()) return;
        using ShowFn = void (*)(uintptr_t hud, int slot, const char* text, float seconds, int a5, int a6, uint8_t a7, uint8_t a8);
        auto show = (ShowFn)natives::Get(natives::HudShow);
        uintptr_t obj = natives::Get(natives::HudMessages);
        if (!show || !obj) return;
        static char buffer[64];
        snprintf(buffer, sizeof(buffer), "%s", text);
        gamethread::Post([show, obj] { show(obj, 0, buffer, 2.0f, 0, 0, 0, 1); });
    }

    // ---- input ----

    bool GameFocused() {
        HWND w = GetForegroundWindow();
        DWORD pid = 0;
        if (w) GetWindowThreadProcessId(w, &pid);
        return pid == GetCurrentProcessId();
    }

    using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

    XInputGetStateFn GetXInput() {
        static XInputGetStateFn fn = [] {
            HMODULE m = LoadLibraryA("xinput1_4.dll");
            if (!m) m = LoadLibraryA("xinput9_1_0.dll");
            return m ? (XInputGetStateFn)GetProcAddress(m, "XInputGetState") : nullptr;
        }();
        return fn;
    }

    // Is D-pad Right held on any controller? XInput covers Xbox pads and anything Steam Input presents as one (including PlayStation pads
    // under Steam Input); a DualSense / DualShock 4 is also read directly over HID because the game talks to it natively. A PlayStation pad
    // can therefore report through both, so the sources are OR-ed *before* the press is detected: one physical press = one toggle.
    bool PadRightDown() {
        if (psinput::Connected() && psinput::Down(psinput::kRight)) return true;
        if (auto get = GetXInput()) {
            for (DWORD i = 0; i < 4; i++) {
                XINPUT_STATE st{};
                if (get(i, &st) == ERROR_SUCCESS && (st.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)) return true;
            }
        }
        return false;
    }

    bool KeyDown() { return (GetAsyncKeyState(kToggleKey) & 0x8000) != 0; }

    // ---- "is a menu open?" ----
    //
    // The game keeps a clock (a counter at exe+5d6e7e4 that advances about 262,000 units per second in play, found by logging candidate
    // values while walking through the map, pause menu and suit menu, 2026-10-16). Menus pause the world, so the counter stands still
    // for as long as one is open; in slow motion it still moves, only slower. A counter that has not changed for 150 ms means a menu (or
    // a loading screen). Until the counter has been seen moving once we assume nothing, so a wrong value can never lock the toggle out.
    constexpr uintptr_t kClockRva = 0x5d6e7e4;

    bool InMenu() {
        static uint64_t last_value = 0;
        static DWORD last_change = 0;
        static bool seen_moving = false;
        uint64_t v = 0;
        if (!memory::Read(scan::FromRva(kClockRva), v)) return false;
        DWORD now = GetTickCount();
        if (v != last_value) {
            if (last_value != 0) seen_moving = true;
            last_value = v;
            last_change = now;
            return false;
        }
        return seen_moving && now - last_change > 150;
    }

    // ---- Harry's research station (time of day) ----
    //
    // That menu does not pause the world, so the clock test above cannot see it, and D-pad Right (move the selection) toggled slow motion.
    // It is a mission-script action, UIChangeOfDayAction (vtable exe+38a2980). Its input handler, exe+7f3320 (slot 10), reacts to one input
    // pin (0x029E19B1 = start): it marks the action running ([action+0x60] = 1), subscribes to ChangeOfDayAcceptEvent and
    // ChangeOfDayDeclineEvent and opens the menu. So: hook that handler (menu opened) and watch SendEvent (exe+16d5e80) for the Accept /
    // Decline events (menu closed). Found 2026-10-17; the UIResearchStation*Event classes in the exe are never sent through SendEvent.
    using SendEventFn = uintptr_t (*)(uintptr_t sys, uint32_t type, uint32_t target, const uint32_t* source, int32_t a5, uint8_t a6,
                                      uint64_t a7, uint32_t a8, uint8_t a9, float f1, float f2, const void* payload, uint64_t payload_size);
    SendEventFn g_send_original = nullptr;
    using ActionInputFn = uintptr_t (*)(uintptr_t action, uint32_t pin, uintptr_t c, uintptr_t d);
    ActionInputFn g_action_original = nullptr;

    constexpr uint32_t kChangeOfDayAccept = 0xE8E304DA;    // CRC32 of "ChangeOfDayAcceptEvent"
    constexpr uint32_t kChangeOfDayDecline = 0x5290A548;   // CRC32 of "ChangeOfDayDeclineEvent"
    constexpr uint32_t kChangeOfDayStartPin = 0x029E19B1;
    constexpr uintptr_t kChangeOfDayHandlerRva = 0x7f3320;
    constexpr DWORD kStationFailsafeMs = 600000;           // if the closing event is ever missed, stop treating the menu as open after 10 minutes
    std::atomic<DWORD> g_station_until{ 0 };               // GetTickCount() until which the time-of-day menu counts as open (0 = closed)

    // Events after which the slow motion and the decoy must be gone: the player died or respawns, a checkpoint (mission restart, load) or a level was loaded.
    // Left on, the slow motion keeps being written into the next hero and the load runs at the slowed rate. recon/events_4.0630.txt.
    std::atomic<uint32_t> g_reset_event{ 0 };
    bool IsResetEvent(uint32_t type) {
        switch (type) {
        case 0x123b6f37:   // PlayerDeathEvent
        case 0xc61d6131:   // HeroStartScriptedDeathEvent
        case 0x655e56da:   // HeroBeginRespawnEvent
        case 0x0b3b8b30:   // HeroRequestRespawnEvent
        case 0x414ad37a:   // CheckpointLoadedEvent
        case 0xfbe1b646:   // LevelLoadedEvent
            return true;
        default: return false;
        }
    }

    uintptr_t SendEventDetour(uintptr_t sys, uint32_t type, uint32_t target, const uint32_t* source, int32_t a5, uint8_t a6, uint64_t a7,
                              uint32_t a8, uint8_t a9, float f1, float f2, const void* payload, uint64_t payload_size) {
        if (type == kChangeOfDayAccept || type == kChangeOfDayDecline) {
            g_station_until = 0;
            LOG("station: time-of-day menu closed (%s)", type == kChangeOfDayAccept ? "accepted" : "declined");
        }
        if (IsResetEvent(type)) g_reset_event = type;
        return g_send_original(sys, type, target, source, a5, a6, a7, a8, a9, f1, f2, payload, payload_size);
    }

    uintptr_t ActionInputDetour(uintptr_t action, uint32_t pin, uintptr_t c, uintptr_t d) {
        if (pin == kChangeOfDayStartPin) {
            uint8_t running = 1;
            memory::Read(action + 0x60, running);
            if (!running) {   // the handler ignores the pin if the action is already running
                g_station_until = GetTickCount() + kStationFailsafeMs;
                LOG("station: time-of-day menu opened");
            }
        }
        return g_action_original(action, pin, c, d);
    }

    bool InstallStationHook() {
        MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) { LOG("station: MH_Initialize failed (%d)", st); return false; }
        bool any = false;
        // The menu-opened hook: located by signature, and only used if it lands exactly where we found it.
        auto hits = scan::FindAll("81 FA B1 19 9E 02 0F 85 ?? ?? ?? ?? 48 8B C4 57 48 81 EC A0 00 00 00 80 79 60 00", 4);
        if (hits.size() == 1 && scan::Rva(hits[0]) == kChangeOfDayHandlerRva) {
            void* t = (void*)hits[0];
            if (MH_CreateHook(t, (void*)&ActionInputDetour, (void**)&g_action_original) == MH_OK && MH_EnableHook(t) == MH_OK) any = true;
            else LOG("station: hooking the time-of-day action failed");
        } else {
            LOG("station: time-of-day action handler not found at exe+%llx (%zu hits); menu detection off", (unsigned long long)kChangeOfDayHandlerRva, hits.size());
        }
        // The menu-closed events.
        void* send = (void*)natives::Get(natives::SendEvent);
        if (send && MH_CreateHook(send, (void*)&SendEventDetour, (void**)&g_send_original) == MH_OK && MH_EnableHook(send) == MH_OK) any = true;
        else LOG("station: SendEvent did not hook");
        LOG("station: watching the time-of-day menu (%s)", any ? "ok" : "unavailable");
        return any;
    }

    bool StationMenuOpen() {
        DWORD until = g_station_until;
        return until != 0 && (LONG)(until - GetTickCount()) > 0;
    }

    // ---- after-images ----

    // Its own thread: spawning a ghost waits for the game thread, which must not delay the slow-motion writes below.
    void GhostLoop() {
        for (;;) {
            if (ghosts::Mode() != 0 || ghosts::HasLive() || ghosts::DecoyBusy()) ghosts::Update();
            Sleep(40);
        }
    }

    void SetGhostMode(int mode) {
        if (mode != 0 && !EnsureGameThread()) return;
        ghosts::SetMode(mode);
    }

    // ---- main loop ----

    void Main() {
        g_dll_dir = ModuleDir();
        logging::Init(g_dll_dir + "\\EchoesOfTime.log");
        LOG("Echoes of Time 1.0.0 loaded from %s", g_dll_dir.c_str());
        int ok = natives::ResolveAll();
        LOG("%d/%d natives at expected addresses", ok, (int)natives::Count);
        if (!natives::Get(natives::GetActor)) { LOG("GetActor did not resolve (wrong game build?); disabled"); return; }
        psinput::Start();
        InstallStationHook();
        fnat::Resolve();
        settings::Load();
        std::thread(GhostLoop).detach();
        std::thread([] { const uint32_t code = menu::Install(); if (code) LOG("menu: could not register the tab (error %u)", code); }).detach();
        int applied_mode = -1;   // the MODE row's choice last handed to the ghosts (the ghosts can switch themselves off after repeated spawn failures)
        int applied_ability = -1;
        unsigned seen_placed = ghosts::DecoyPlacedCount();
        DWORD entry_until = 0;   // GetTickCount() until which the entry slow-motion runs (0 = not running)

        bool enabled = false, prev_down = false;
        int hero_missing = 0;   // consecutive 100 ms polls without a hero (loading, main menu)
        DWORD last_hero_poll = 0;
        for (;;) {
            // Death, restart, load, quit to the menu: slow motion off (nothing is written any more) and no decoy.
            const char* reset_why = nullptr;
            char reset_buf[48];
            if (const uint32_t ev = g_reset_event.exchange(0)) { snprintf(reset_buf, sizeof(reset_buf), "event %08x", ev); reset_why = reset_buf; }
            { const DWORD t = GetTickCount(); if (t - last_hero_poll >= 100) { last_hero_poll = t; hero_missing = hero::Read().ok ? 0 : hero_missing + 1; if (hero_missing == 3) reset_why = "no hero"; } }
            if (reset_why) {
                if (enabled || entry_until || ghosts::DecoyBusy()) LOG("reset (%s): slow motion off, decoy removed", reset_why);
                if (enabled) { enabled = false; Apply(false); }
                if (entry_until) { entry_until = 0; ApplyEntry(false); }
                ghosts::DecoyShutdown();
            }
            const int ability = settings::Get().ability;
            if (ability != applied_ability) {   // the ABILITY row changed: put the other one away
                applied_ability = ability;
                if (ability == settings::kAcceleratedDecoy) { if (enabled) { enabled = false; Apply(false); LOG("disabled (ability switched)"); } applied_mode = -1; }
                else { ghosts::DecoyShutdown(); if (entry_until) { entry_until = 0; ApplyEntry(false); } }
                LOG("ability: %s", ability == settings::kAcceleratedDecoy ? "Accelerated Decoy" : "Hyper-Sense");
            }
            if (ability == settings::kAcceleratedDecoy) {
                const unsigned placed = ghosts::DecoyPlacedCount();
                if (placed != seen_placed) {
                    seen_placed = placed;
                    const int e = settings::Get().decoy_entry;
                    if (e > 0) { entry_until = GetTickCount() + (e >= 2 ? 800 : 350); LOG("decoy: entry slow-motion"); }
                }
                if (entry_until) {
                    if ((LONG)(GetTickCount() - entry_until) >= 0) { entry_until = 0; ApplyEntry(false); }
                    else ApplyEntry(true);   // the game resets the values, so keep writing them
                }
            }
            const int want_mode = ability == settings::kHyperSense ? settings::Get().mode : 0;   // after-image trail: Hyper-Sense only
            if (want_mode != applied_mode) { applied_mode = want_mode; SetGhostMode(applied_mode); }
            bool down = GameFocused() && (KeyDown() || PadRightDown());
            bool menu = InMenu() || StationMenuOpen();   // InMenu() must run every pass: it tracks the clock
            if (down && !prev_down && !menu && ability == settings::kAcceleratedDecoy) {
                if (EnsureGameThread()) {
                    LOG("decoy: requested");
                    ghosts::DecoyPlace();
                    if (settings::Get().decoy_messages) ShowMessage("Accelerated Decoy!");
                }
            } else if (down && !prev_down && !menu) {
                enabled = !enabled;
                LOG("%s", enabled ? "enabled" : "disabled");
                Apply(enabled);   // disabling restores normal time right away
                if (settings::Get().messages) ShowMessage(enabled ? "Hyper-Sense Enabled!" : "Hyper-Sense Disabled!");
            }
            prev_down = down;   // updated even in menus, so holding the button across a menu closing does not toggle
            ghosts::SetSlowMoActive(enabled);
            { static DWORD last_fx = 0; const DWORD t = GetTickCount(); if (t - last_fx >= 100) { last_fx = t; speedfx::Update(enabled, settings::Get().blur_mult, settings::Get().stage_mult); } }
            if (enabled) Apply(true);   // the game resets the values, so keep writing them (like the original, every 10 ms)
            Sleep(10);
        }
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }

extern "C" __declspec(dllexport) void script_enable() { std::thread(Main).detach(); }
