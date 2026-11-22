#pragma once

#include <cstdint>

// The few game functions the Flash Overhaul needs on top of EndlessArena's natives (natives.cpp, compiled in).
// Same rule as there: found by signature, and only used when the hit is exactly the expected RVA (4.0630.0.0 only).
namespace fnat {
    enum Id {
        AddMaterial,    // exe+1772530 bool (ModelInst*, u32 mapping id (0 = all), material*, u8 flag): adds to the model's material override list (+0xa0)
        SetModified,    // exe+191bf60 (ModelInst*, u32 mask or -1): marks the model dirty so the renderer re-reads it (FadeModelAlphaAction calls it after writing the alpha byte)
        HashPath,       // exe+1bb2680 (out 16 bytes, const char* path): path -> asset id (u64 at +0)
        MapLookup,      // exe+1bcf4e0 (map*, u64 key) -> entry* or 0 (a template: identical copies exist, any of them behaves the same)
        SkinPart,       // exe+20c9f20 (SkinManager*, u32 part index, u32 variant 0/1) -> the part's ModelInst* or 0 (parts array [sm+0x50], stride 0x6b8, count [sm+0x4c])
        Count
    };

    // Scans every native; returns how many resolved at their expected RVA. Logs each.
    int Resolve();
    uintptr_t Get(Id id);

    // The loaded material asset for a path, or 0 (the material must already be loaded: boot-config materials are).
    // Mirrors HeroLastKnownPositionManager's silhouette (exe+93f520): hash the path, look it up in the map at exe+6058208 (entry = u32 index),
    // material = [exe+6058240] + index * [exe+6058248].
    uintptr_t FindMaterial(const char* path, bool quiet = false);

    // Asks the game to load a material it has not loaded: LoadAssetByHash(material manager at exe+6058200, id), the same call the actor spawn uses with the actor manager.
    // The load is asynchronous: poll FindMaterial until it returns non-zero. Game thread. The reference is never released (a handful of small materials).
    bool LoadMaterial(const char* path);

    // Guarded calls (access violations are caught and reported as false).
    // flag = override type: 0 base, 1 pre-composite, 2 post-composite (MaterialOverrideMap.Type); the silhouette uses 0.
    bool AddMaterial_(uintptr_t model_inst, uintptr_t material, uint8_t flag = 0);
    bool SetModified_(uintptr_t model_inst);
    uintptr_t SkinPart_(uintptr_t skin_manager, unsigned index, unsigned variant);
}
