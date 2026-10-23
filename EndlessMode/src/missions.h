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
    void Log();                                               // logs the state of the known story missions (both spellings of each id)
}
