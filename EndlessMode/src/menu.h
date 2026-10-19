#pragma once

#include <cstdint>

// The ENDLESS ARENA tab in the game's own Settings (pause menu > Settings > after the game's last tab): the rows of
// settings.h, built by the game's own row code so they look, scroll, reset and take the controller like its own.
//
// How (SpidyVR by IlyaMezerowsky, GPL-3.0, found the way in; its src/game_menu.cpp and docs/REFERENCE.md describe it for
// this same game build): the Settings are Scaleform screens fed from the game's UISystemMenuConfig. When the pause menu
// hands the list of tabs to Flash (OpenOptions) we copy one of the game's own tabs, settings and headings, point the
// copies at our texts and setting numbers (0x200 and up), build the tab with the game's builder and append it. The game's
// per-setting functions are hooked to answer for our numbers and take their changes; its own settings and save never see them.
namespace menu {
    // Hooks the Settings menu. 0 when hooked; 93xx when the code at a hook is not this game's; 94xx when hooking failed.
    uint32_t Install();
    void LogStatus();
}
