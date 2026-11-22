#pragma once

#include <cstdint>

// Silences the game's own Holo Decoy actor (it speaks lines through the dialog system). Spider-Man.exe exports the Wwise API (see EndlessArena's audio.h), so
// AK::SoundEngine::PostEvent is hooked and any event posted on a game object we were told about is dropped (play id 0). Hooked on first use.
namespace mute {
    // Game objects to silence, as candidate ids: the actor handle, its head actor's handles and the entity pointer (which one the game uses is logged).
    void SetObjects(uint64_t a, uint64_t b, uint64_t c, uint64_t d);
    void Clear();
}
