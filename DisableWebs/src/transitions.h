#pragma once

// Blocks hero state transitions that the ability locks do not cover (docs/FLASH_RECON.md section 1.7).
namespace transitions {
    bool Install();               // hooks the game functions; false when one is not where the recon says (nothing is hooked then)
    void SetBlocked(bool on);     // true: the blocked transitions are refused (the hero then stays in the state it was in)
}
