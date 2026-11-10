#pragma once

#include <cstdint>

// MSMR Mod Menu: one tab per installed script mod, added after the game's own tabs on the Settings screen.
// (The earlier separate MODS entry on the pause menu's first page is still in modmenu.cpp but switched off by kModsPageItem; it is not maintained.) Spider-Man.exe 4.0630.0.0 only.
//
// Every participating script compiles this same file. The first one to call Register() in the process becomes the host: it hooks the
// game's menu functions once and builds every mod's tab. Later ones find the host through a named shared object and only hand it their
// rows and callbacks. So load order does not matter and only one copy of the hooks exists.
//
// Method: SpidyVR (IlyaMezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR) found how to add a tab and a first-page button to the
// game's Settings; Endless Arena re-implemented the tab. Addresses and the tab method: Manhattan Skies docs/RECON.md Part 6.
namespace modmenu {
    constexpr uint32_t kProtocol = 1;
    constexpr int kMaxRowId = 255;       // row ids 0..255 (they become setting numbers)
    constexpr int kMaxChoices = 32;

    // One row as the menu shows it. Strings must stay valid until the next call for the same row (the host copies them per build).
    struct Row {
        const char* title;               // upper case, like the game's rows
        const char* help;                // shown beside the row while selected; may be null
        const char* const* choices;      // null and 0: an ON/OFF switch (choice 0 off, 1 on)
        int choice_count;
        int heading;                     // nonzero: a section heading (no value)
    };

    // A mod's tab. All callbacks run on the game thread, from the game's menu code.
    struct Mod {
        uint32_t size;                   // sizeof(Mod), for later protocol versions
        const char* name;                // the tab's name, upper case ("ENDLESS ARENA")
        void* ctx;
        int (*rows)(void* ctx, int* ids, int max);            // the row ids on screen now, in order; returns how many
        void (*row)(void* ctx, int id, Row* out);
        int (*value)(void* ctx, int id);                      // the current choice
        int (*default_choice)(void* ctx, int id);
        void (*set)(void* ctx, int id, int choice);           // the player picked a choice
        void (*reset)(void* ctx, int id);                     // X on a row
        void (*reset_all)(void* ctx);                         // Y on the tab
        int (*take_rebuild)(void* ctx);                       // nonzero (once) when the rows on screen changed (a section switch, a preset)
        void (*closed)(void* ctx);                            // optional: the options screen was left (back to the pause menu); apply anything you deferred
    };

    using LogFn = void (*)(const char* line);

    // Registers a mod (copy of `mod` kept). Returns 0 when the tab is in (host or client), else an error code (see modmenu.cpp).
    // Call from a worker thread after start-up; the host may wait up to 30 s for an older Endless Arena's own hooks.
    uint32_t Register(const Mod& mod, LogFn log);

    // Keeps the pause menu open through a lighting-mode change: for `ms` milliseconds, while an options screen is open, the pause menu's own
    // per-frame "close because the world is in transition" request (the Flash call CloseMenu from exe+7dc06b) is swallowed. The screen still
    // fades out and in. Call this right before changing the mode from a mod's menu callback.
    void KeepMenuOpen(unsigned ms);

    // Dev tool: for `ms` milliseconds log every UI call the game makes (name and the code that made it) to the log function given at Register.
    void TraceUi(unsigned ms);

    // One line about this process's menu: who hosts, how many mods, hooks.
    void LogStatus();
}
