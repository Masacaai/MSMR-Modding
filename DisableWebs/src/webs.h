#pragma once

#include <string>

// No webs: locks the hero's web abilities through the game's own lock components (docs/internals/web-moves-and-afterimages.md section 1).
namespace webs {
    void SetEnabled(bool on);
    bool Enabled();

    // Applies (or lifts) the locks. Must run on the game thread; idempotent, so it is simply called a few times a second.
    // Re-finds the hero's components each time, so a respawn or character change is picked up.
    void Tick();

    std::string Status();   // one line for the log / HUD
}
