#pragma once

#include <string>

// After-image prototype "S1" (docs/FLASH_RECON.md section 2.2): every `interval` seconds, while the hero is fast, a static Spider-Man pose model is
// spawned where he is, given a ghost material, and faded out over `life` seconds. Not an exact pose copy: one fixed pose.
namespace ghosts {
    // 0 off, 1 while fast, 2 always. Switching to 0 removes every ghost.
    void SetMode(int mode);
    int Mode();

    // Called in a loop from the mod's own ghost thread (~25 Hz; it is the only thread that touches the ghosts). Spawns, fades and removes ghosts; blocks briefly while the game thread works.
    void Update();

    void SetSlowMoActive(bool on);   // called by the slow-motion toggle every pass
    bool HasLive();   // ghost thread only
    void RemoveAll();
    std::string Status();
}
