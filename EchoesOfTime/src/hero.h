#pragma once

#include <cstdint>
#include <string>

// The hero, as every mod here reads him (HeroSystem handle -> GetActor -> Transform). Guarded reads; any thread.
namespace hero {
    struct Info {
        bool ok = false;
        uint32_t handle = 0;
        uintptr_t actor = 0;      // entity
        uintptr_t transform = 0;  // [entity+0] (also the ModelInst)
        float pos[3] = {};
        float fwd[3] = {};        // row 2 of the transform: forward (x, y, z)
    };
    Info Read();

    // Component of the entity whose type name ends with `suffix` (names are plain class names or "Hero::Class"), or 0.
    uintptr_t Component(uintptr_t entity, const char* suffix, std::string* full_name = nullptr);
}
