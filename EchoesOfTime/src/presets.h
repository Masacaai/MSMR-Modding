#pragma once

#include <string>
#include <vector>

// Presets (the same system as Manhattan Skies): one small ini per look in Documents\Marvel's Spider-Man Remastered\EchoesOfTime\Presets\ (share one by copying the file).
//   [preset]
//   look=RED GLOW      (any row key from settings.cpp; the value is the choice's label, case-insensitive; a key that is absent means the row's default)
// Values are labels, not indices, so a preset survives new choices being added. Unknown keys and labels are ignored. The MESSAGES row is a personal preference and is
// never part of a preset.
namespace presets {
    void Init();
    void Rescan();                    // re-read the folder (when the menu closes, never while rows are on screen)
    int Count();
    const char* const* Names();       // Count()+1 entries: [0] = "CUSTOM", then the presets by name
    int Current();                    // index into Names(): the preset whose values match the live settings, else 0
    void Apply(int index);            // index >= 1; first remembers the live settings as CUSTOM when they match no preset
    bool RestoreCustom();             // back to the remembered CUSTOM settings (kept on disk); false when there are none
}
