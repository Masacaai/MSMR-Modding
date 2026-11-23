#pragma once

#include <string>
#include <vector>

// Presets: one small ini file per look in Documents\Marvel's Spider-Man Remastered\ManhattanSkies\Presets\ (shared by copying the file).
//   [preset]
//   time=NIGHT        (a TIME OF DAY label; absent: the time of day is left alone)
//   fogcolor=WARM     (any sky setting key from sky.cpp, value = the row's label; absent: AUTO)
// Values are labels, not indices, so a preset survives new choices being added. Unknown keys and labels are ignored.
namespace presets {
    struct Hooks {
        const char* const* time_labels; int time_count;
        int (*get_time)();            // current choice index
        void (*set_time)(int);        // game thread
    };
    void Init(const Hooks& hooks);
    void Rescan();                    // re-read the folder (call when the menu closes or after a save, never while rows are on screen)
    int Count();
    const char* const* Names();       // Count()+1 entries: [0] = "CUSTOM", then the presets by name
    int Current();                    // index into Names(): the preset whose values match the live settings, else 0
    void Apply(int index);            // index >= 1; first remembers the live settings as CUSTOM when they match no preset
    bool RestoreCustom();             // back to the remembered CUSTOM settings (kept on disk); false when there are none
    bool SaveCustom(std::string* name_out);   // writes Custom1, Custom2, ... from the live settings
}
