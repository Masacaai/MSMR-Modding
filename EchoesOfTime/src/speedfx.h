#pragma once

// Speed-effect experiments (docs/internals/web-moves-and-afterimages.md section 2.1, S4): the game's HeroSpeedFXConfig (motion blur, speed streaks) lives inside the hero's HeroTraversalConfig at +0x17b8.
namespace speedfx {
    // Call every ~100 ms from the mod's main thread. While `on`, scales the blur numbers by blur_mult and the stage speeds (where the effects start) by stage_mult;
    // when off (or both are 1) the original values are restored. Does nothing until the config is found and its values look sane.
    void Update(bool on, float blur_mult, float stage_mult);
}
