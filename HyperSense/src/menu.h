#pragma once

#include <cstdint>

// The HYPER-SENSE tab in the game's Settings screen (added by ModMenu/modmenu.h). Rows come from settings.cpp.
namespace menu {
    // Registers the tab. 0 when in; otherwise a ModMenu error code (93xx code mismatch, 94xx hooking, 96xx sharing).
    uint32_t Install();
}
