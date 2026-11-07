#pragma once

#include <cstdint>
#include <string>

// The game's audio engine (Wwise) from the inside. Spider-Man.exe EXPORTS the Wwise API under its C++ names (found 2026-10-19:
// ?PostEvent@SoundEngine@AK@@..., SetState, SetSwitch, SetRTPCValue, ...), so no signatures are needed: the functions are looked up
// with GetProcAddress on the exe.
//
// Two jobs:
//  - A recorder (`audio on` ... `audio off`): every Wwise call the game makes (events posted, states, switches, RTPC values,
//    stops) with its numeric ids, the game object and the code that called, to <data folder>\EndlessMode_audio.txt. Wwise ids
//    are the FNV-1 hash of the lower-cased name, so tools/assets/wwise_names.py turns a recording back into names using the
//    names in sound/banks/Events.wwiselookup. This is how the game's music cues, intensity states and fades are learned.
//  - Posting (`audio post <event name> [game object]`, `audio state <group> <state>`, ...): the same calls from the mod.
namespace audio {
    bool Install();                      // looks the exports up and hooks them (idempotent); false if the engine is not as expected
    void Record(bool on);                // start (truncating the file) or stop the recording
    bool Recording();
    void Mark(const std::string& text);  // a line in the recording ("hideout started", "boss died"): lets the log be read against what happened

    // Wwise name hash (FNV-1, 32 bit, lower case): AK::SoundEngine::GetIDFromString.
    uint32_t Id(const char* name);

    // Posting from the mod. These call the engine directly on the calling thread (Wwise is thread-safe). `obj` is a Wwise game object id
    // (what the recording shows the game's music using); returns the playing id, 0 on failure.
    uint32_t Post(const char* event_name, uint64_t obj);
    bool SetState(const char* group, const char* state);
    bool SetSwitch(const char* group, const char* sw, uint64_t obj);
    bool SetRtpc(const char* name, float value, uint64_t obj, int fade_ms = 0);
    void StopPlaying(uint32_t playing_id, int fade_ms = 0);

    // By id (what the game itself posts, see Id): the music module uses these. RegisterObject makes a Wwise game object of our own to post on.
    uint32_t PostId(uint32_t event_id, uint64_t obj);
    bool SetStateId(uint32_t group_id, uint32_t state_id);
    bool RegisterObject(uint64_t obj, const char* name);
}
