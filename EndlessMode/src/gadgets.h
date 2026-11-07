#pragma once

#include <cstdint>

// Gadgets (Impact Web, Web Bomb, ...) live in the hero's weapon manager, not in the suit-item inventory (docs/REWARD_DESIGN.md,
// "Gadget recon"). Equipping one goes through exe+2162300 EquipGadget(this, gadget id (32 bit, 0x9C82-0x9C89 seen), slot, force),
// which records the gadget at this+0x6c+slot*40 and runs the equip. We hook it to log what the game equips and, with the lock
// on, to refuse every gadget that is not allowed (the roguelike runs hand gadgets out as rewards).
namespace gadgets {
    bool Install();                 // idempotent; false if the function is not as expected (other build)
    void Watch(bool on);            // log every equip call (id, slot, force, result)
    void Lock(bool on);             // refuse gadgets that were not allowed
    void Allow(uint32_t id);
    void ClearAllowed();
    bool Equip(uint32_t id, int slot);   // our own equip through the game's function (game thread); needs one observed call first
}
