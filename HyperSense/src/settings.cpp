#include "settings.h"

#include "log.h"

#include <Windows.h>
#include <ShlObj.h>
#include <cstdio>
#include <cstdlib>

namespace settings {
    namespace {
        const char* const kScale[] = { "SLOW (0.2X)", "SLOWER (0.1X)", "SLOWEST (0.001X)" };
        const char* const kSpeed[] = { "NORMAL", "1.5X", "2X", "3X" };
        const char* const kOffOn[] = { "OFF", "ON" };
        const char* const kModes[] = { "OFF", "WHILE FAST", "ALWAYS" };
        // The order is the order of ghosts.cpp's presets (names in kLookNames).
        const char* const kLooks[] = { "DEFAULT", "HOLOGRAM", "SCANLINES", "RED GLOW", "FLAT GREY", "DISTORTION", "FEAR", "SPIDEY-SENSE HIGHLIGHT",
                                       "CRIMSON", "YELLOW", "GREEN", "BLUE", "DARK BLUE", "WHITE", "BLACK", "BROWN", "RAINBOW" };
        const char* const kLookNames[] = { "own", "hologram", "scanlines", "red", "grey", "distort", "fear", "highlight",
                                           "crimson", "yellow", "green", "blue", "darkblue", "white", "black", "brown", "rainbow" };
        const char* const kLifeL[] = { "0.3 S", "0.5 S", "0.7 S", "0.9 S", "1.2 S", "1.6 S", "2 S", "3 S", "4 S" };
        const float kLifeV[] = { 0.3f, 0.5f, 0.7f, 0.9f, 1.2f, 1.6f, 2.0f, 3.0f, 4.0f };
        const char* const kCurve[] = { "VERY SLOW (0.5)", "SLOW (0.8)", "LINEAR (1.0)", "GENTLE (1.3)", "NORMAL (1.6)", "QUICK (2.0)", "QUICKER (2.5)", "FAST (3.0)", "VERY FAST (4.0)" };
        const float kCurveV[] = { 0.5f, 0.8f, 1.0f, 1.3f, 1.6f, 2.0f, 2.5f, 3.0f, 4.0f };
        const char* const kAlphaL[] = { "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%" };
        const float kAlphaV[] = { 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
        const char* const kIntervalL[] = { "VERY DENSE (0.05 S)", "DENSE (0.07 S)", "NORMAL (0.09 S)", "SPARSE (0.12 S)", "SPARSER (0.15 S)", "FEW (0.2 S)", "VERY FEW (0.3 S)" };
        const float kIntervalV[] = { 0.05f, 0.07f, 0.09f, 0.12f, 0.15f, 0.2f, 0.3f };
        const char* const kFlickerL[] = { "OFF", "LOW", "MEDIUM", "HIGH" };
        const float kFlickerV[] = { 0.0f, 0.15f, 0.35f, 0.6f };
        const char* const kDriftL[] = { "OFF", "SLOW", "MEDIUM", "FAST", "FALL" };
        const float kDriftV[] = { 0.0f, 0.5f, 1.5f, 3.0f, -1.5f };
        const char* const kSizeL[] = { "OFF", "SHRINK", "SHRINK A LOT", "GROW", "GROW A LOT" };
        const float kSizeV[] = { 1.0f, 0.7f, 0.4f, 1.4f, 2.0f };
        const char* const kBlurL[] = { "OFF", "2X", "4X", "8X" };
        const float kBlurV[] = { 1.0f, 2.0f, 4.0f, 8.0f };
        const char* const kStreaksL[] = { "NORMAL", "EARLIER", "MUCH EARLIER", "ALWAYS" };
        const float kStreaksV[] = { 1.0f, 0.6f, 0.3f, 0.05f };
        template <size_t N> constexpr int Count(const char* const (&)[N]) { return (int)N; }

        const RowDef kRows[kRowCount] = {
            { "", "SLOW MOTION", nullptr, nullptr, 0, 0 },
            { "slow_scale", "SLOW-MOTION STRENGTH", "How slow the world runs while slow motion is on (T, or D-pad Right). Hyper-Sense has its own slow motion: do not run it together with Quantum Break Renewed.", kScale, Count(kScale), 0 },
            { "hero_speed", "SPIDER-MAN SPEED", "How fast Spider-Man moves relative to the slowed world while slow motion is on. Above NORMAL he out-runs the world: the Flash effect.", kSpeed, Count(kSpeed), 0 },
            { "messages", "MESSAGES", "Shows Enabled / Disabled on screen when slow motion is toggled.", kOffOn, 2, 0 },
            { "", "AFTER-IMAGES", nullptr, nullptr, 0, 0 },
            { "mode", "MODE", "A fading trail of Spider-Man copies behind him, only while slow motion is on. WHILE FAST only adds copies while he moves quickly; ALWAYS keeps adding them, even standing still (they copy his idle animation).", kModes, Count(kModes), 2 },
            { "look", "LOOK", "What the copies look like. DEFAULT is Spider-Man's own suit; the others replace its materials. RAINBOW gives each copy the next color. Looks that are not loaded in the current area load on demand; the first copies wait a moment for that.", kLooks, Count(kLooks), 0 },
            { "life", "CLONE DECAY", "How long a copy lasts before it is gone. Longer means a longer trail.", kLifeL, Count(kLifeL), 3 },
            { "fade_curve", "OPACITY DECAY SCALE", "How a copy's opacity falls over its life. LINEAR fades evenly; higher values keep the newest copies solid and make the older ones drop off fast; lower values keep the whole trail solid longer.", kCurve, Count(kCurve), 4 },
            { "alpha", "STARTING OPACITY", "How solid a copy is when it is made, before the decay.", kAlphaL, Count(kAlphaL), 4 },
            { "interval", "CLONE SPACING", "How often a copy is made. Dense trails look more continuous and cost more.", kIntervalL, Count(kIntervalL), 2 },
            { "flicker", "FLICKER", "How much each copy's opacity crackles from frame to frame.", kFlickerL, Count(kFlickerL), 2 },
            { "", "EXPERIMENTAL", nullptr, nullptr, 0, 0 },
            { "drift", "CLONE DRIFT", "Copies rise (or fall) slowly while they fade. Experimental.", kDriftL, Count(kDriftL), 0 },
            { "size", "CLONE SIZE OVER LIFE", "Copies shrink or grow as they fade. Experimental; the copy scales around its feet.", kSizeL, Count(kSizeL), 0 },
            { "blur", "SPEED BLUR", "Stronger motion blur while slow motion is on (the game's own speed-blur numbers, multiplied). Experimental: it only shows while Spider-Man is moving fast.", kBlurL, Count(kBlurL), 0 },
            { "streaks", "SPEED STREAKS", "Makes the game's speed effects (screen streaks, hand contrails) start at a lower speed while slow motion is on. Experimental.", kStreaksL, Count(kStreaksL), 0 },
        };

        int g_choice[kRowCount];
        S g_s;
        bool g_init = false;

        std::string IniPath() {
            char docs[MAX_PATH] = {};
            if (!SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs)) || !docs[0]) return "";
            return std::string(docs) + "\\Marvel's Spider-Man Remastered\\HyperSense\\HyperSense.ini";
        }

        void Rebuild(const std::string& ini_path);

        void Init() {
            if (g_init) return;
            g_init = true;
            for (int i = 0; i < kRowCount; i++) g_choice[i] = kRows[i].def;
            Rebuild(std::string());
        }

        void Rebuild(const std::string& ini_path) {
            S s;
            s.slow_scale = g_choice[kSlowScale] == 2 ? 0.001f : g_choice[kSlowScale] == 1 ? 0.1f : 0.2f;
            static const float speeds[] = { 1.0f, 1.5f, 2.0f, 3.0f };
            s.hero_speed = speeds[g_choice[kHeroSpeed]];
            s.messages = g_choice[kMessages] != 0;
            s.mode = g_choice[kMode];
            s.effect = kLookNames[g_choice[kLook]];
            s.life = kLifeV[g_choice[kLife]];
            s.fade_curve = kCurveV[g_choice[kFadeCurve]];
            s.alpha = kAlphaV[g_choice[kAlpha]];
            s.interval = kIntervalV[g_choice[kInterval]];
            s.flicker = kFlickerV[g_choice[kFlicker]];
            s.drift = kDriftV[g_choice[kDrift]];
            s.size_end = kSizeV[g_choice[kSize]];
            s.blur_mult = kBlurV[g_choice[kBlur]];
            s.stage_mult = kStreaksV[g_choice[kStreaks]];
            if (!ini_path.empty() && GetFileAttributesA(ini_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                auto f = [&](const char* k, float def) { char b[64]; GetPrivateProfileStringA("advanced", k, "", b, sizeof(b), ini_path.c_str()); return b[0] ? (float)atof(b) : def; };
                auto str = [&](const char* k, const std::string& def) { char b[512]; GetPrivateProfileStringA("advanced", k, "", b, sizeof(b), ini_path.c_str()); return b[0] ? std::string(b) : def; };
                s.min_speed = f("min_speed", s.min_speed); s.max_ghosts = (int)f("max", 0); s.y_offset = f("y_offset", 0); s.yaw_offset = f("yaw_offset", 0);
                s.back_offset = f("back_offset", 0); s.spacing = f("spacing", 0); s.hide_spawn = f("hide_spawn", 1) != 0; s.pose_copy = f("pose_copy", 1) != 0;
                s.only_slowmo = f("only_slowmo", 1) != 0; s.actor = str("actor", s.actor);
                const std::string eff = str("effect", ""); if (!eff.empty()) s.effect = eff;   // a .material path overrides the LOOK row
            }
            if (s.max_ghosts <= 0) s.max_ghosts = (int)(s.life / s.interval) + 2;   // enough for the whole decay
            if (s.max_ghosts < 2) s.max_ghosts = 2;
            if (s.max_ghosts > 80) s.max_ghosts = 80;
            if (s.spacing < 0) s.spacing = 0;
            g_s = s;
        }
    }

    const RowDef& Def(int row) { Init(); return kRows[row]; }
    int Choice(int row) { Init(); return g_choice[row]; }
    const S& Get() { Init(); return g_s; }

    void SetChoice(int row, int choice) {
        Init();
        if (row < 0 || row >= kRowCount || !kRows[row].labels || choice < 0 || choice >= kRows[row].count) return;
        g_choice[row] = choice;
        const std::string path = IniPath();
        Rebuild(path);
        if (!path.empty()) {
            const size_t cut = path.find_last_of('\\');
            CreateDirectoryA(path.substr(0, cut).c_str(), nullptr);
            char v[16]; snprintf(v, sizeof(v), "%d", choice);
            WritePrivateProfileStringA("menu", kRows[row].key, v, path.c_str());
        }
        LOG("settings: %s -> %s", kRows[row].title, kRows[row].labels[choice]);
    }

    void Load() {
        Init();
        const std::string path = IniPath();
        if (!path.empty() && GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            for (int i = 0; i < kRowCount; i++) {
                if (!kRows[i].labels) continue;
                const int v = (int)GetPrivateProfileIntA("menu", kRows[i].key, kRows[i].def, path.c_str());
                g_choice[i] = (v >= 0 && v < kRows[i].count) ? v : kRows[i].def;
            }
            LOG("settings read from %s", path.c_str());
        }
        Rebuild(path);
        const S& s = g_s;
        LOG("settings: slow %.3f speed %.1f mode %d look %s life %.2f curve %.2f alpha %.2f interval %.2f flicker %.2f drift %.1f size %.1f max %d", s.slow_scale, s.hero_speed, s.mode,
            s.effect.c_str(), s.life, s.fade_curve, s.alpha, s.interval, s.flicker, s.drift, s.size_end, s.max_ghosts);
    }
}
