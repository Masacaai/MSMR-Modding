#pragma once

#include "paths.h"

#include <Windows.h>
#include <cstdio>
#include <string>

// Lifetime progress across runs, in <data folder>\EndlessArena_progress.ini: which Campaign chapters (factions) the player has
// seen, and the secret chapter's tally. Small and boring on purpose; the secret chapter itself is described in
// src/SECRET.md (kept out of every public document).
namespace progress {
    inline std::string File() { return DataDir() + "\\EndlessArena_progress.ini"; }

    inline bool Seen(const std::string& faction) {
        return GetPrivateProfileIntA("seen", faction.c_str(), 0, File().c_str()) != 0;
    }
    inline void MarkSeen(const std::string& faction) {
        if (!Seen(faction)) WritePrivateProfileStringA("seen", faction.c_str(), "1", File().c_str());
    }
    inline int Count(const char* key) { return (int)GetPrivateProfileIntA("secret", key, 0, File().c_str()); }
    inline void Bump(const char* key) {
        char v[16];
        snprintf(v, sizeof(v), "%d", Count(key) + 1);
        WritePrivateProfileStringA("secret", key, v, File().c_str());
    }
}
