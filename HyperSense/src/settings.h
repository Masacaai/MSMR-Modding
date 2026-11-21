#pragma once

#include <string>

// Hyper-Sense settings. The player changes them in the in-game MODS tab (menu.cpp, rows below); each row is a list of choices and the choice index is what is
// saved: <Documents>\Marvel's Spider-Man Remastered\HyperSense\HyperSense.ini, section [menu]. The same file's [advanced] section holds knobs with no row
// (spacing, offsets, actor ...); this mod writes only [menu].
namespace settings {
    enum Row {
        kHeadSlow, kSlowScale, kHeroSpeed, kMessages,
        kHeadGhost, kMode, kLook, kLife, kFadeCurve, kAlpha, kInterval, kFlicker,
        kHeadFx, kDrift, kSize, kBlur, kStreaks,
        kRowCount
    };

    struct RowDef {
        const char* key;            // ini key
        const char* title;          // menu title (upper case)
        const char* help;
        const char* const* labels;  // null for a heading
        int count;
        int def;                    // default choice
    };
    const RowDef& Def(int row);
    int Choice(int row);
    void SetChoice(int row, int choice);   // saves the ini and rebuilds Get()

    // The values the rest of the mod uses (derived from the choices and [advanced]).
    struct S {
        // slow motion
        float slow_scale = 0.2f;     // world time scale while on
        float hero_speed = 1.0f;     // Spider-Man's own speed multiplier while on (HeroTimeScaleManager +0x4c)
        bool messages = false;
        // after-images
        int mode = 2;                // 0 off, 1 while fast, 2 always
        std::string effect = "own";  // look name (ghosts.cpp presets) or a .material path
        float life = 0.9f;           // seconds a clone lasts
        float fade_curve = 1.6f;     // opacity decay scale: opacity = (1 - age/life)^fade_curve (1 linear, higher = near clones stay solid, the tail drops fast)
        float alpha = 0.7f;          // starting opacity
        float interval = 0.09f;      // seconds between clones
        float flicker = 0.35f;       // 0..1 opacity jitter
        // experiments
        float drift = 0.0f;          // metres per second a clone rises while it lives
        float size_end = 1.0f;       // a clone's size at the end of its life, relative to its birth size
        float blur_mult = 1.0f;      // motion-blur strength multiplier while slow motion is on (HeroSpeedFXConfig, speedfx.cpp)
        float stage_mult = 1.0f;     // multiplier on the speeds at which the speed effects start (smaller = they start at a lower speed)
        // [advanced] (no row)
        float min_speed = 6.0f;      // m/s the hero must exceed in "while fast" mode
        int max_ghosts = 0;          // 0 = as many as the decay needs
        float y_offset = 0.0f, yaw_offset = 0.0f, back_offset = 0.0f;
        float spacing = 0.0f;        // metres since the last clone before the next; 0 = clones keep coming while he stands still
        bool hide_spawn = true;      // spawn out of sight and move into place after the pose is set (hides the A-pose frame)
        bool pose_copy = true;
        bool only_slowmo = true;     // clones only exist while slow motion is on
        std::string actor = "objects/test/JNB/Memory_Poses/jnb_spiderman_memory_pose_Stand.actor";
    };
    const S& Get();
    void Load();   // read the ini (defaults when missing)
}
