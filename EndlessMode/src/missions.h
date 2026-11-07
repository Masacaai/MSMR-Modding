#pragma once

#include <cstdint>
#include <string>

// What the game's save says about story missions (recon 2026-10-22, by disassembly of CheckMissionStateAction, exe+20187e0).
// The mission system is a singleton pointer at exe+7c4bbb0; a mission is a 32-bit id (an Insomniac CRC-32 of its name). Two calls give its state:
// exe+201f640 (system, out[24 bytes], id) finds the record, exe+201f800 (system, out) returns the state (0..5; 0 also when the id is unknown).
// Which number means "completed" is read from the game (Log below prints the states of known missions); see kStateComplete.
namespace missions {
    uint32_t Hash(const std::string& name, bool normalize);   // Insomniac CRC-32 of a name (ALERT's hash(name, normalize))
    bool State(uint32_t id, int& state);                      // game thread only; false if the system or the code is not as expected
    // Is a story mission (or any objective) running? Game thread. Mirrors AnyMissionActiveAction (exe+1faf2f0): the objective system is a
    // singleton (exe+7c4bbb0, empty when nothing runs), its count is at +0x108, exe+201efa0 (system, out, index) fills a record, and state 2 is
    // "active". Fills the ids of up to `cap` active ones, returns how many are active (0 also when the system does not exist).
    int Active(uint32_t* ids, int cap);

    // A cached answer for other threads, refreshed about once a second by Poll (call it from any thread; it posts to the game thread).
    void Poll();
    bool AnyActiveCached();

    // Recon: lists the components of the entity the game announced the mission system on (MissionSystemLoaded's target) and, for any that looks like
    // the objective system (a count at +0x108), the state of every mission in it. Posts to the game thread.
    void DumpSystemEntity();

    void Log();                                               // logs the state of the known story missions (both spellings of each id)
}
