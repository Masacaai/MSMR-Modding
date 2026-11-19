#pragma once

#include <cstdint>

// Gadgets (Impact Web, Web Bomb, ...) live in the hero's weapon manager and the hero's inventory (docs/RECON.md sections 5-7).
// exe+2162300 is WeaponManagerBase::Equip(this, id, slot, force, sendEquippedEvent): five arguments, shared by the hero and every enemy's
// weapon manager. We hook it to log the hero's equips and, with the lock on, to refuse gadget ids (0x9C82-0x9C89) that were not allowed
// (the roguelike runs hand gadgets out as rewards). The hero's manager is resolved fresh on every use and never cached.
namespace gadgets {
    bool Install();                 // idempotent; false if the function is not as expected (other build)
    void Watch(bool on);            // log the hero's equips and any gadget id (id, slot, force, event, result)
    void Lock(bool on);             // refuse gadget ids that were not allowed
    void Allow(uint32_t id);
    void ClearAllowed();
    // A run's gadgets (no upgrades: dropped by the user 2026-11-07): BeginRun hides the seven gadgets (red, 0/0; a slot holding one is
    // unequipped first) and locks the wheel to the Web Shooter; Grant unhides and allows one (no equip call); EndRun restores everything.
    void BeginRun();
    void Grant(uint32_t id);
    bool Owned(uint32_t id);
    void EndRun();
    // Dev commands (`gadget ...`, all untested in game; every one runs on the game thread):
    bool Hide(uint32_t id, bool hide);   // hide: zero the table id (red, 0/0, unusable); show: restore it
    bool Equip(uint32_t id, int slot);   // our own equip, only when the table record and the weapon actor exist
    void Create(uint32_t id);            // create the gadget's weapon actor (what the first real equip does; the cause of the red state)
    void Refresh();                      // force the wheel to rebuild its list
    void Dump();                         // the hero's manager, equipped slots and the whole table
    // The web-fluid curse: the four web gadgets (Electric Web, Impact Web, Web Bomb, Trip Mine) are hidden and refused for the rest of the run
    // (Grant ignores them); the Web Shooter itself is handled by the ability lock in boons.cpp.
    void DenyWeb();
    bool IsWebGadget(uint32_t id);
    // What the hero holds in slot 0 (0 = the Web Shooter / no gadget), or 0xFFFFFFFF when there is no live manager. Game thread only.
    uint32_t Held();
}
