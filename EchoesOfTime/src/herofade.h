#pragma once

#include <cstdint>

// Flicker of Spider-Man's own model while an Accelerated Decoy is up. Writing his opacity byte (ModelInst +0xbe) from our own thread or game tick did nothing visible: the
// game rewrites it (a poke of 0x40 read back 0xFF within seconds, 2026-11-22). Every writer in the game follows the byte write with SetModified(mi, -1) (exe+191bf60), so that
// call is hooked: while active, for his model and skin parts, the byte is set to our value just before the call goes through.
namespace herofade {
    // Start flickering the model of `hero_actor` (strength 0..1: opacity per frame is random between 1 - strength and 1). Game thread.
    void Start(uintptr_t hero_actor, float strength);
    // Stop and put full opacity back. Game thread.
    void Stop();
    bool Active();
}
