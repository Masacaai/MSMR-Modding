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

// The secret chapter's chance (see src/SECRET.md): starts at 20% once every faction's chapter has been seen, then grows with every
// cleared wave, faster the deeper the wave. Kept in per mille in the progress file across runs and deaths. Once the chapter has been
// met it no longer rolls: it comes at the end of every campaign.
namespace progress {
    inline bool AllSeen(const char* const* ids, int n) {
        for (int i = 0; i < n; i++) if (!Seen(ids[i])) return false;
        return true;
    }
    inline int ChancePm() { return Count("chance_pm"); }
    inline void SetCount(const char* key, int v) {
        char b[16];
        snprintf(b, sizeof(b), "%d", v);
        WritePrivateProfileStringA("secret", key, b, File().c_str());
    }
    inline bool Encountered() { return Count("encountered") != 0; }
    // A wave of a normal chapter was cleared: start the counter at `start_pm`, or let it compound by 2% plus 0.25% of the wave number.
    inline void OnWaveCleared(int wave, int start_pm) {
        if (Encountered()) return;
        int pm = ChancePm();
        if (pm <= 0) pm = start_pm;
        else pm = (int)(pm * (1.0f + 0.02f + 0.0025f * wave) + 0.5f);
        SetCount("chance_pm", pm > 1000 ? 1000 : pm);
    }
    inline bool TakeUnlockNotice() {
        if (!Count("notice")) return false;
        SetCount("notice", 0);
        return true;
    }
}
