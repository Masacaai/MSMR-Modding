#pragma once

#include <cstdint>

// The MANHATTAN SKIES tab in the game's Settings screen (added by ModMenu/modmenu.h). Its TIME OF DAY row shows the
// live lighting mode and sets it like the game's own option does, including the modes the game never offers.
namespace menu {
    // Registers the tab. 0 when in; otherwise a ModMenu error code (93xx code mismatch, 94xx hooking, 96xx sharing).
    uint32_t Install();
    void LogStatus();
}
