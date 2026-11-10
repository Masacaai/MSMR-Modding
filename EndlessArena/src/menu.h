#pragma once

#include <cstdint>

// The ENDLESS ARENA tab in the game's Settings screen (added by ModMenu/modmenu.h): the rows of settings.h, built by
// the game's own row code so they look, scroll, reset and take the controller like its own. Whichever participating script starts first
// hosts the menu; the method is SpidyVR's (IlyaMezerowsky, GPL-3.0, src/game_menu.cpp, docs/REFERENCE.md).
namespace menu {
    // Registers the tab. 0 when in; otherwise a ModMenu error code (93xx code mismatch, 94xx hooking, 96xx sharing).
    uint32_t Install();
    void LogStatus();
}
