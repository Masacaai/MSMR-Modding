#pragma once

// Music for the Arena, driven the way the game drives its own hideout music (recorded 2026-11-07, docs/NEXT_RELEASE_RECON.md): the open-world
// music is one Wwise state group per act (I20_MUS_OW_A1 here), started by an event and stepped up a ladder of states as a fight escalates, with
// a sting per wave and a cooldown at the end. The state values are known only by their ids (the lookup has no names for them), so the roles below
// come from when the game set them. All calls go straight to the engine's exported Wwise API (audio.h) and are no-ops when the ARENA MUSIC
// setting is off or the engine is not as expected.
namespace music {
    // A wave begins: tier 0 skirmish, 1 assault, 2 full force or harder; a boss wave is tier 2 with an extra sting. Starts the music on first use.
    void WaveStart(int tier, bool boss);
    // A wave was cleared (a short cooldown until the next one starts).
    void WaveCleared();
    // A boss spawned.
    void Sting();
    // The run is over: the game's ending states, then silence.
    void End();
}
