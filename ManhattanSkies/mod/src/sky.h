#pragma once

#include <cstdint>

// The sky settings of Manhattan Skies: which atmosphere is on top of the time of day (SKY STYLE / SKY VARIANT) and edits to its sun
// (direction, height, color, brightness). All settings are indices into the tables in sky.cpp; 0 is AUTO (the game's own value).
//
// How (docs/RECON.md Parts 9-10): the lighting mode picks a base atmosphere and a world "levellight"; the atmosphere pool keeps one slot
// per loaded atmosphere with a weight. A sky override loads another atmosphere into the pool (LoadAssetByHash with the AtmosphereManager),
// gives it the base's weight and takes the base's away; the game's own activation of the next base atmosphere is swallowed while the override
// is on. Sun edits are written into the active slot's copy of the values and the manager re-blends.
namespace sky {
    enum Setting { kStyle, kVariant, kDir, kHeight, kColor, kBright, kFog, kFogDist, kHaze, kExposure, kFilter, kClouds, kFogColor, kCycle, kSettingCount };

    void Load();                       // ini -> settings (once, at start-up)
    int Get(Setting s);
    void Set(Setting s, int value);    // from the menu; applied by Tick, saved to the ini
    const char* Key(Setting s);                 // ini / preset key ("style", "fogcolor", ...)
    int LabelCount(Setting s);                  // choices of a setting (the variant's depend on the current style)
    const char* LabelAt(Setting s, int i);
    int SavedMode();                   // the time of day (lighting mode) picked in the menu, kept across launches; -1: never picked (the game keeps its own)
    void SaveMode(int mode);
    int CycleCount();  const char* const* CycleLabels();
    int TickMs();                      // how long the worker loop should sleep: short while the day-night cycle steers the sun
    void SetSmoothFade(bool on);       // dev: swallow the lighting transition's fade for cycle-driven changes
    bool CycleSyncMode(int mode);      // a time of day was picked: the cycle (when on) continues from there; false: that mode is not in the cycle (overcast)
    void ResetAll();                   // every sky setting back to AUTO

    // Choice labels for the menu rows. Variant labels depend on the style.
    int StyleCount();
    const char* const* StyleLabels();
    int VariantCount(int style);
    const char* const* VariantLabels(int style);
    int DirCount();    const char* const* DirLabels();
    int HeightCount(); const char* const* HeightLabels();
    int ColorCount();  const char* const* ColorLabels();
    int BrightCount(); const char* const* BrightLabels();
    int FogCount();    const char* const* FogLabels();
    int FogDistCount();  const char* const* FogDistLabels();
    int HazeCount();     const char* const* HazeLabels();
    int ExposureCount(); const char* const* ExposureLabels();
    int FilterCount();   const char* const* FilterLabels();
    int CloudsCount();   const char* const* CloudsLabels();
    int FogColorCount(); const char* const* FogColorLabels();

    void Tick();                       // worker thread, every 250 ms: applies what the settings ask for when the world is ready
    bool Filter(uintptr_t caller_rva, int slot_idx, uint64_t asset, int delta);   // recorder::AdjustFilter
    void LogState();                   // `skystate`
}
