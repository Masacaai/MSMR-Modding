#include "sky.h"

#include "log.h"
#include "gamethread.h"
#include "natives.h"
#include <algorithm>
#include "scan.h"
#include "memory.h"
#include <random>
#include "../../../ModMenu/modmenu.h"
#include "recorder.h"

#include <Windows.h>
#include <ShlObj.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <mutex>
#include <string>
#include <vector>

namespace sky {
namespace {
    // ---- the looks: SKY STYLE groups them, SKY VARIANT picks one. `file` is the end of the atmosphere's path (see recon/atmosphere_catalog.txt) ----
    struct Look { int style; const char* label; const char* file; uint64_t id; };
    enum { kAuto, kDay, kSunset, kMorning, kNight, kNightMore, kOvercast, kStrange, kInterior, kStyleCount };
    const char* const kStyleLabels[kStyleCount] = { "AUTO", "DAY", "SUNSET", "MORNING", "NIGHT", "MORE NIGHT", "OVERCAST", "STRANGE", "INTERIOR" };
    Look g_looks[] = {
        { kDay, "CLASSIC", "atm_ibl_lgt_day_02.atmosphere" },
        { kDay, "PAINTED SKY", "atm_ibl_lgt_day_02_cine.atmosphere" },
        { kDay, "BRIGHT HDR", "atm_ibl_lgt_day_spidey_01.atmosphere" },
        { kDay, "SCORPION GREEN", "atm_ibl_lgt_vn_scorpion_open_world.atmosphere" },
        { kDay, "CITY HALL", "atm_ibl_lgt_day_cityhall_destroy.atmosphere" },
        { kDay, "SOFT CLOUDS", "atm_cine_hh_theater_01.atmosphere" },
        { kDay, "HAZY", "atm_milestease_vmfog.atmosphere" },
        { kDay, "FEAST DAY", "atm_feast_day_2018.atmosphere" },
        { kSunset, "CLASSIC", "atm_ibl_lgt_sunset_02.atmosphere" },
        { kSunset, "CLOUDY GOLD", "atm_ibl_lgt_sunset_infinitytrailer.atmosphere" },
        { kSunset, "LOW SUN", "atm_cine_loadscreen_sunset_03.atmosphere" },
        { kSunset, "ORANGE GLOW", "atm_cine_antiserum_02.atmosphere" },
        { kSunset, "2018 SUNSET", "atm_2018_sunset.atmosphere" },
        { kSunset, "GRAND CENTRAL", "atm_grand_central_sunset.atmosphere" },
        { kSunset, "PENTHOUSE", "sunset_normanpenthouse.atmosphere" },
        { kSunset, "MAIN MENU", "atm_main_menu_sunset.atmosphere" },
        { kSunset, "HIGH SUN", "atm_cine_epilogue_mayfuneral_01.atmosphere" },
        { kMorning, "OPENING", "atm_ibl_lgt_morning_01_ow.atmosphere" },
        { kMorning, "FISK", "atm_ibl_lgt_morning_01_fisk.atmosphere" },
        { kMorning, "PETER'S APARTMENT", "atm_peter_apt.atmosphere" },
        { kNight, "CLASSIC", "atm_ibl_lgt_night_02.atmosphere" },
        { kNight, "CLOUDY", "atm_ibl_lgt_night_02_old_forcines.atmosphere" },
        { kNight, "STARRY", "atm_ibl_lgt_peterscook.atmosphere" },
        { kNight, "MOONLIT", "atm_night_feast_2017.atmosphere" },
        { kNight, "BLUE HOUR", "atm_ottofightnight.atmosphere" },
        { kNight, "DEEP BLUE", "atm_esu_night_interior.atmosphere" },
        { kNight, "BANK NIGHT", "atm_night_shocker_bank_2017.atmosphere" },
        { kOvercast, "CLASSIC", "atm_ibl_lgt_overcast_02.atmosphere" },
        { kOvercast, "DARK STORM", "atm_ibl_lgt_night_rain_02.atmosphere" },
        { kOvercast, "GREEN HAZE", "atm_cine_rhinofight_outro_01.atmosphere" },
        { kOvercast, "LOW SUN", "atm_loadscreen_traversal_overcast_02.atmosphere" },
        { kStrange, "SCORPION BLUE", "atm_ibl_lgt_day_scorpion_hallucination_02.atmosphere" },
        { kStrange, "SCORPION YELLOW", "atm_ibl_lgt_day_scorpion_hallucination_03.atmosphere" },
        { kStrange, "SCORPION GREEN", "atm_ibl_lgt_day_scorpion_hallucination_01.atmosphere" },
        { kStrange, "1700S SUNSET", "test_atm_corrupt_biotech_nightmarelab.atmosphere" },
        { kStrange, "CORRUPTION", "atm_corrupt_truck_chase.atmosphere" },
        { kStrange, "GREEN NIGHT", "atm_cine_mj_investigates_standish_01.atmosphere" },
#include "looks_extra.inc"
    };
    std::vector<const char*> g_variant_labels[kStyleCount];   // per style
    std::vector<int> g_variant_look[kStyleCount];             // per style: index into g_looks

    // ---- the sun rows ----
    std::vector<std::string> g_dir_text, g_height_text;
    std::vector<const char*> g_dir_labels, g_height_labels;
    const int kHeights[] = { -10, -5, 0, 5, 10, 15, 20, 30, 40, 50, 60, 70, 80, 90 };
    struct ColorPreset { const char* label; float rgb[3]; };
    const ColorPreset kColors[] = {
        { "AUTO", { 0, 0, 0 } }, { "WHITE", { 1, 1, 1 } }, { "WARM", { 1, 0.82f, 0.6f } }, { "GOLDEN", { 1, 0.7f, 0.25f } }, { "ORANGE", { 1, 0.45f, 0.1f } },
        { "RED", { 1, 0.1f, 0.08f } }, { "PINK", { 1, 0.4f, 0.7f } }, { "PURPLE", { 0.6f, 0.25f, 1 } }, { "BLUE", { 0.3f, 0.5f, 1 } },
        { "CYAN", { 0.2f, 0.9f, 1 } }, { "GREEN", { 0.3f, 1, 0.3f } },
    };
    std::vector<const char*> g_color_labels;
    struct BrightPreset { const char* label; float mul; };
    const BrightPreset kBrights[] = { { "AUTO", 1 }, { "25%", 0.25f }, { "50%", 0.5f }, { "75%", 0.75f }, { "150%", 1.5f }, { "200%", 2 }, { "300%", 3 }, { "500%", 5 } };
    std::vector<const char*> g_bright_labels;
    struct FogPreset { const char* label; float mul; };
    const FogPreset kFogs[] = { { "AUTO", 1 }, { "NONE", 0 }, { "25%", 0.25f }, { "50%", 0.5f }, { "150%", 1.5f }, { "300%", 3 }, { "600%", 6 } };
    std::vector<const char*> g_fog_labels;
    // The image controls found by tools/fieldprobe.py (docs/RECON.md Part 12). Offsets are inside the 0x8c0-byte atmosphere slot.
    constexpr uintptr_t kOffFogColor = 0x5b4;   // the fog colour (RGB; the haze value follows at +0x5c0): the "NYC Warm Night" mod edits exactly this triple
    constexpr uintptr_t kOffFogDist = 0x588, kOffHaze = 0x5c0, kOffExposure = 0x484, kOffFilter = 0x668, kOffClouds = 0x81c;
    const FogPreset kFogDists[] = { { "AUTO", 1 }, { "25%", 0.25f }, { "50%", 0.5f }, { "150%", 1.5f }, { "300%", 3 }, { "600%", 6 } };
    struct HazePreset { const char* label; float value; };
    const HazePreset kHazes[] = { { "AUTO", 0 }, { "NONE", 0 }, { "LIGHT", 0.1f }, { "MEDIUM", 0.25f }, { "HEAVY", 0.5f }, { "THICK", 1.0f } };
    struct ExposurePreset { const char* label; float ev; };   // added to the sky's own exposure bias (stops)
    const ExposurePreset kExposures[] = { { "AUTO", 0 }, { "-3 STOPS", -3 }, { "-2 STOPS", -2 }, { "-1 STOP", -1 }, { "-0.5 STOP", -0.5f },
                                          { "+0.5 STOP", 0.5f }, { "+1 STOP", 1 }, { "+2 STOPS", 2 }, { "+3 STOPS", 3 } };
    const ColorPreset kFilters[] = {   // absolute red, green, blue multipliers on the whole image (the sky's own grade is replaced)
        { "AUTO", { 1, 1, 1 } }, { "NEUTRAL", { 1, 1, 1 } }, { "WARM", { 1.2f, 1.0f, 0.8f } }, { "COOL", { 0.85f, 0.95f, 1.2f } }, { "GOLDEN", { 1.25f, 1.05f, 0.55f } },
        { "ORANGE", { 1.4f, 0.9f, 0.4f } }, { "RED", { 1.4f, 0.6f, 0.6f } }, { "PINK", { 1.3f, 0.7f, 1.0f } }, { "PURPLE", { 1.0f, 0.55f, 1.3f } },
        { "BLUE", { 0.6f, 0.75f, 1.4f } }, { "TEAL", { 0.6f, 1.1f, 1.1f } }, { "GREEN", { 0.7f, 1.2f, 0.7f } },
    };
    const HazePreset kCloudsChoices[] = { { "AUTO", 0 }, { "ON", 1.0f }, { "OFF", 0 } };   // +0x81c: any positive value turns the cloud layer on, 0 off
    const ColorPreset kFogColors[] = {   // absolute fog colours; the sky's own is a pale blue at night, grey-white by day
        { "AUTO", { 1, 1, 1 } }, { "WHITE", { 1.0f, 1.0f, 1.0f } }, { "NEUTRAL", { 0.6f, 0.6f, 0.6f } }, { "WARM", { 0.8f, 0.6f, 0.35f } }, { "GOLDEN", { 1.0f, 0.7f, 0.25f } },
        { "ORANGE", { 1.0f, 0.45f, 0.1f } }, { "RED", { 1.0f, 0.2f, 0.15f } }, { "PINK", { 1.0f, 0.45f, 0.75f } }, { "PURPLE", { 0.6f, 0.3f, 1.0f } },
        { "BLUE", { 0.3f, 0.5f, 1.0f } }, { "TEAL", { 0.2f, 0.8f, 0.8f } }, { "GREEN", { 0.35f, 1.0f, 0.35f } }, { "DARK", { 0.05f, 0.05f, 0.06f } },
    };
    std::vector<const char*> g_fogcolor_labels;
    struct CyclePreset { const char* label; int minutes; };   // how long one full day and night takes
    const CyclePreset kCycles[] = { { "OFF", 0 }, { "2 MIN", 2 }, { "5 MIN", 5 }, { "10 MIN", 10 }, { "20 MIN", 20 }, { "30 MIN", 30 }, { "60 MIN", 60 } };
    std::vector<const char*> g_cycle_labels;
    std::atomic<bool> g_smooth_fade{ true };     // dev switch (command smoothfade): swallow the lighting transition's fade for cycle-driven changes
    std::vector<const char*> g_fogdist_labels, g_haze_labels, g_exposure_labels, g_filter_labels, g_clouds_labels;
    constexpr int kDirSteps = 24;   // 15 degrees apiece

    std::atomic<int> g_set[kSettingCount];
    std::atomic<int> g_saved_mode{ -1 };
    std::atomic<bool> g_mode_pending{ false };
    std::atomic<bool> g_dirty_ini{ false };
    std::atomic<ULONGLONG> g_dirty_at{ 0 };
    std::once_flag g_init_once;

    void Init() {
        for (auto& l : g_looks) l.id = recorder::AtmIdBySuffix(l.file);
        for (int s = 0; s < kStyleCount; ++s) { g_variant_labels[s].clear(); g_variant_look[s].clear(); }
        for (int i = 0; i < (int)(sizeof(g_looks) / sizeof(g_looks[0])); ++i) {
            if (!g_looks[i].id) { LOG("sky: look %s %s is not in the atmosphere table", kStyleLabels[g_looks[i].style], g_looks[i].label); continue; }
            g_variant_labels[g_looks[i].style].push_back(g_looks[i].label);
            g_variant_look[g_looks[i].style].push_back(i);
        }
        g_dir_text.assign(1, "AUTO");
        for (int i = 0; i < kDirSteps; ++i) g_dir_text.push_back(std::to_string(i * 15) + " DEG");
        g_height_text.assign(1, "AUTO");
        for (int h : kHeights) g_height_text.push_back(std::to_string(h) + " DEG");
        for (const auto& s : g_dir_text) g_dir_labels.push_back(s.c_str());
        for (const auto& s : g_height_text) g_height_labels.push_back(s.c_str());
        for (const auto& c : kColors) g_color_labels.push_back(c.label);
        for (const auto& b : kBrights) g_bright_labels.push_back(b.label);
        for (const auto& f : kFogs) g_fog_labels.push_back(f.label);
        for (const auto& f : kFogColors) g_fogcolor_labels.push_back(f.label);
        for (const auto& f : kFogDists) g_fogdist_labels.push_back(f.label);
        for (const auto& f : kHazes) g_haze_labels.push_back(f.label);
        for (const auto& f : kExposures) g_exposure_labels.push_back(f.label);
        for (const auto& f : kFilters) g_filter_labels.push_back(f.label);
        for (const auto& f : kCloudsChoices) g_clouds_labels.push_back(f.label);
        for (const auto& f : kCycles) g_cycle_labels.push_back(f.label);
    }
    void EnsureInit() { std::call_once(g_init_once, Init); }

    std::string IniPath() {
        char docs[MAX_PATH] = {};
        std::string dir;
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs)) && docs[0]) dir = std::string(docs) + "\\Marvel's Spider-Man Remastered\\ManhattanSkies";
        if (dir.empty()) return "";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return ec ? "" : dir + "\\ManhattanSkies.ini";
    }
    const char* const kKeys[kSettingCount] = { "style", "variant", "dir", "height", "color", "bright", "fog", "fogdist", "haze", "exposure", "filter", "clouds", "fogcolor", "cycle" };

    // ---- runtime state (worker thread, except where noted) ----
    constexpr float kNoValue = -12345.f;
    struct Sun { float rgb[3], w, rot, elev; };
    std::map<uint64_t, Sun> g_orig;               // each atmosphere's own sun values, read before the first edit
    struct Fog { float d[6]; };
    std::map<uint64_t, Fog> g_orig_fog;           // ... and its own fog densities
    struct Image { float fogdist[4], exposure, filter[3], haze, clouds, fogcolor[3]; };
    std::map<uint64_t, Image> g_orig_image;       // ... and its own distance bands, exposure, filter, haze and cloud values
    bool g_world_up = false;
    ULONGLONG g_hold_until = 0;                   // don't touch anything before this tick count (the world is settling)
    std::atomic<int> g_base_idx{ -1 };            // the slot the game last activated as a base atmosphere (filter, game thread)
    std::atomic<uint64_t> g_base_asset{ 0 };
    std::atomic<ULONGLONG> g_base_seen_at{ 0 };
    std::set<uint64_t> g_held;                    // atmospheres this module loaded and still holds a reference on (released when its look goes off)
    uint64_t g_failed_id = 0;                     // a look that would not load (pool full?): not retried until the setting changes
    int g_load_tries = 0;
    uint64_t g_look_id = 0;                       // the override atmosphere that is active (0: none)
    int g_look_idx = -1;
    ULONGLONG g_look_at = 0, g_load_req_at = 0;
    int g_edit_idx = -1;                          // the slot whose sun we last wrote
    uint64_t g_edit_asset = 0;
    int g_edit_seen[kSettingCount] = {};          // the sun settings as last written
    bool g_edit_touched = false;                  // we have written to g_edit_idx (so AUTO must restore the original)

    std::atomic<uint64_t> g_cyc_look{ 0 };        // the sky the dynamic cycle chose for the current phase (0: none; the user's own SKY STYLE wins)

    uint64_t ModeBaseAtmosphere(int mode) {
        switch (mode) {
            case 0: return recorder::AtmIdBySuffix("atm_ibl_lgt_day_02.atmosphere");
            case 2: return recorder::AtmIdBySuffix("atm_ibl_lgt_sunset_02.atmosphere");
            case 4: return recorder::AtmIdBySuffix("atm_ibl_lgt_morning_01_ow.atmosphere");
            case 1: case 5: return recorder::AtmIdBySuffix("atm_ibl_lgt_night_02.atmosphere");
            case 3: case 6: return recorder::AtmIdBySuffix("atm_ibl_lgt_overcast_02.atmosphere");
            default: return 0;
        }
    }
    uint64_t DesiredLook() {
        const int style = g_set[kStyle], variant = g_set[kVariant];
        if (style <= 0 && g_cyc_look) return g_cyc_look;   // the dynamic cycle's pick, while no SKY STYLE is chosen
        if (style <= 0 || style >= kStyleCount) return 0;
        const auto& v = g_variant_look[style];
        if (variant < 0 || variant >= (int)v.size()) return 0;
        return g_looks[v[(size_t)variant]].id;
    }
    // The slot that is the game's base atmosphere right now: the last one it activated, if that slot still holds the same asset.
    int BaseSlot() {
        const int idx = g_base_idx.load();
        uint64_t a; int w;
        if (idx >= 0 && recorder::SlotInfo(idx, a, w) && a == g_base_asset.load() && a != 0) return idx;
        return -1;
    }
    // +/- 2 only when it changes something (the pool clamps at 0, but a double +2 would stack).
    void Activate(int idx, bool on) {
        uint64_t a; int w;
        if (idx < 0 || !recorder::SlotInfo(idx, a, w)) return;
        if (on && w <= 0) recorder::Adjust(idx, 2);
        else if (!on && w > 0) recorder::Adjust(idx, -2);
    }

    // A look we loaded goes off: drop our reference so the asset system can free its pool slot (the pool holds only ~16 atmospheres).
    void ReleaseLook(uint64_t id, int idx) {
        if (!id || !g_held.count(id)) return;
        uint64_t a; int w;
        if (idx >= 0 && recorder::SlotInfo(idx, a, w) && a == id && w <= 0 && recorder::ReleaseSlot(idx)) g_held.erase(id);
    }

    void ApplyLook() {
        const uint64_t want = DesiredLook();
        if (want == g_look_id) return;
        const ULONGLONG now = GetTickCount64();
        if (want) {
            const int idx = recorder::FindSlot(want);
            if (want == g_failed_id) return;
            if (idx < 0) {   // not in the pool yet: ask for it and try again on the next ticks
                if (!g_load_req_at || now - g_load_req_at > 6000) {
                    if (g_load_tries >= 3) { g_failed_id = want; LOG("sky: look %016llX would not load after 3 tries (the atmosphere pool is probably full); giving up on it", (unsigned long long)want); return; }
                    ++g_load_tries;
                    recorder::LoadAtmosphere(want);
                    g_load_req_at = now;
                    LOG("sky: loading look %016llX (try %d)", (unsigned long long)want, g_load_tries);
                }
                return;
            }
            if (g_load_req_at) g_held.insert(want);   // it is in the pool because we asked for it: we hold a reference
            // swap the active atmosphere: the old look (or the game's base) off, the new one on
            const uint64_t old_look = g_look_id; const int old_idx = g_look_idx;
            if (g_look_id) Activate(g_look_idx, false); else Activate(BaseSlot(), false);
            Activate(idx, true);
            g_look_id = want; g_look_idx = idx; g_look_at = now; g_load_req_at = 0; g_load_tries = 0;
            ReleaseLook(old_look, old_idx);
            LOG("sky: look on: slot %d asset %016llX", idx, (unsigned long long)want);
            return;
        }
        // back to AUTO: the look off, the game's own base atmosphere for the current mode on again
        const uint64_t old_look = g_look_id; const int old_idx = g_look_idx;
        Activate(g_look_idx, false);
        int base = BaseSlot();
        if (base < 0) {   // the base slot was recycled: ask for the mode's atmosphere and wait for it
            const uint64_t id = ModeBaseAtmosphere(recorder::CurrentMode());
            if (!id) { g_look_id = 0; return; }
            base = recorder::FindSlot(id);
            if (base < 0) {
                if (!g_load_req_at || now - g_load_req_at > 6000) { recorder::LoadAtmosphere(id); g_load_req_at = now; }
                return;
            }
            g_base_idx = base; g_base_asset = id;
        }
        Activate(base, true);
        LOG("sky: look off: base slot %d active again", base);
        g_look_id = 0; g_look_idx = -1; g_load_req_at = 0; g_load_tries = 0;
        ReleaseLook(old_look, old_idx);
    }

    // The sun values wanted for an atmosphere (its own, with the settings applied).
    // ---- dynamic cycle: a pose (lighting mode + sun path) for each moment of one full cycle f in [0,1) ----
    //   [0.93,0.08) Sunrise   [0.08,0.55) Day (or an overcast day, kOvercastChance)   [0.55,0.68) Sunset   [0.68,0.93) Night
    // Each phase may use a random sky from the lists below (or the game's own for that lighting mode) instead of the default.
    std::atomic<bool> g_cyc_sun_on{ false };      // the cycle is steering the sun (not at night)
    std::atomic<float> g_cyc_rotd{ 0.f }, g_cyc_elev{ 0.f };
    std::atomic<uint32_t> g_cyc_step{ 0 };
    uint32_t g_cyc_seen = 0;
    bool g_cyc_overcast = false;                  // today's day is an overcast day
    constexpr double kOvercastChance = 0.25;
    double g_prev_phase = 0.0;
    float g_last_ts = 1.f;
    const uintptr_t kWorldTimers[3] = { 0x5d6a028, 0x5d6a168, 0x5d6a2a8 };   // exe RVAs of world-update timers (freeze while the game is paused)
    float g_last_timers[3] = { 0.f, 0.f, 0.f };
    ULONGLONG g_still_since = 0;
    bool g_was_paused = false;
    double g_phase = 0.2;
    bool g_cyc_started = false;
    ULONGLONG g_cyc_last = 0, g_cyc_switch_at = 0, g_world_up_at = 0;
    // Softening the switch: shortly before a mode change the next sky is loaded quietly (weight 0) and its sun color, fog, haze, exposure and
    // filter are blended into the current one, so at the instant of the switch (the fade is swallowed) only the sky texture and the lights jump.
    struct Target { Sun sun; Fog fog; Image im; };
    Target g_tgt{};
    bool g_tgt_valid = false;
    uint64_t g_tgt_asset = 0;
    ULONGLONG g_tgt_req_at = 0;
    std::vector<uint64_t> g_cyc_hold;             // atmospheres the cycle loaded: released once they are idle
    std::atomic<float> g_cyc_blend{ 0.f };
    float g_blend_prev = 0.f;
    // Spider-Man's speed (m/s, smoothed; -1 unknown), sampled on the game thread from the hero's transform. The lighting switch is a mini load that
    // stops him in mid-air (confirmed for every mode change), so the cycle waits for a calm moment.
    std::atomic<float> g_hero_speed{ -1.f };
    std::atomic<bool> g_hero_job_busy{ false };
    ULONGLONG g_due_wait_ms = 0;                  // how long (unpaused) a due switch has been waiting for Spider-Man to slow down
    bool g_wait_logged = false;
    void SampleHero() {
        if (g_hero_job_busy.exchange(true)) return;
        if (!gamethread::Installed() && !gamethread::Install()) { g_hero_job_busy = false; return; }
        gamethread::Post([] {
            static bool have = false;
            static float prev[3];
            static ULONGLONG prev_t = 0;
            uint32_t handle = 0;
            uintptr_t hero = 0, transform = 0;
            float pos[3];
            const uintptr_t get_actor = natives::Get(natives::GetActor);
            bool ok = get_actor && memory::Read(scan::FromRva(0x5D9DD00) + 0x1C, handle) && handle &&
                      memory::SafeCall(reinterpret_cast<uintptr_t (*)(void*)>(get_actor), &handle, hero) && hero && memory::Read(hero, transform) && transform &&
                      memory::SafeRead(transform + 0x30, pos, sizeof(pos));
            if (!ok) { have = false; g_hero_speed = -1.f; }
            else {
                const ULONGLONG t = GetTickCount64();
                if (have && t > prev_t) {
                    const float dx = pos[0] - prev[0], dy = pos[1] - prev[1], dz = pos[2] - prev[2];
                    const float v = std::sqrt(dx * dx + dy * dy + dz * dz) / ((float)(t - prev_t) / 1000.f);
                    const float cur = g_hero_speed;
                    g_hero_speed = cur < 0.f ? v : cur * 0.6f + v * 0.4f;
                }
                memcpy(prev, pos, sizeof(pos)); prev_t = t; have = true;
            }
            g_hero_job_busy = false;
        });
    }
    uint64_t g_next_look = 0;                     // the sky picked for the coming phase (0: the game's own for its lighting mode)
    int g_pick_nm = -1;                           // the lighting mode the pick was made for (-1: none yet)

    // The skies the cycle may use, by (style, label) in the look tables; the game's own sky for the phase is one more choice in each list.
    struct PickDef { int style; const char* label; };
    const PickDef kPickNight[] = { { kNight, "BLUE HOUR" }, { kNight, "PRISON INTRO NIGHT" }, { kNightMore, "SHIPYARD OUTRO" }, { kOvercast, "PRISON HELI CRASH" } };
    const PickDef kPickSunset[] = { { kSunset, "CLOUDY GOLD" }, { kSunset, "PENTHOUSE" }, { kSunset, "HIGH SUN" }, { kSunset, "FLU MONTAGE SUNSET" }, { kSunset, "START MENU SUNSET" } };
    const PickDef kPickDay[] = { { kDay, "PAINTED SKY" }, { kDay, "BRIGHT HDR" }, { kDay, "FLU SHOT CUTSCENE" }, { kDay, "HONORED CUTSCENE" } };
    const PickDef kPickOvercast[] = { { kOvercast, "GREEN HAZE" }, { kOvercast, "LOW SUN" }, { kOvercast, "SABLE INTRO" }, { kOvercast, "FEAST OVERCAST" }, { kOvercast, "FUNERAL" } };
    uint64_t LookId(int style, const char* label) {
        for (const auto& l : g_looks) if (l.style == style && std::strcmp(l.label, label) == 0) return l.id;
        return 0;
    }
    uint64_t PickFrom(const PickDef* defs, size_t n) {
        static std::mt19937 rng((unsigned)GetTickCount64());
        const size_t r = std::uniform_int_distribution<size_t>(0, n)(rng);   // n = the game's own
        return r < n ? LookId(defs[r].style, defs[r].label) : 0;
    }
    uint64_t PickLook(int nm) {
        switch (nm) {
            case 0: return PickFrom(kPickDay, sizeof(kPickDay) / sizeof(kPickDay[0]));
            case 3: return PickFrom(kPickOvercast, sizeof(kPickOvercast) / sizeof(kPickOvercast[0]));
            case 2: return PickFrom(kPickSunset, sizeof(kPickSunset) / sizeof(kPickSunset[0]));
            case 5: return PickFrom(kPickNight, sizeof(kPickNight) / sizeof(kPickNight[0]));
            default: return 0;   // Sunrise keeps its own sky
        }
    }

    bool ReadAtm(int slot, Target& t) {
        float c[6], f[6];
        if (!recorder::SlotSun(slot, c, false) || !recorder::SlotFog(slot, f, false)) return false;
        memcpy(t.sun.rgb, c, 12); t.sun.w = c[3]; t.sun.rot = c[4]; t.sun.elev = c[5];
        memcpy(t.fog.d, f, sizeof(f));
        return recorder::SlotFloats(slot, kOffFogDist, t.im.fogdist, 4, false) && recorder::SlotFloats(slot, kOffExposure, &t.im.exposure, 1, false) &&
               recorder::SlotFloats(slot, kOffFilter, t.im.filter, 3, false) && recorder::SlotFloats(slot, kOffHaze, &t.im.haze, 1, false) &&
               recorder::SlotFloats(slot, kOffClouds, &t.im.clouds, 1, false) && recorder::SlotFloats(slot, kOffFogColor, t.im.fogcolor, 3, false);
    }
    void ReleaseIdleHolds() {
        for (size_t i = 0; i < g_cyc_hold.size();) {
            const uint64_t id = g_cyc_hold[i];
            const int idx = recorder::FindSlot(id);
            uint64_t a; int w;
            if (idx < 0 || !recorder::SlotInfo(idx, a, w)) { g_cyc_hold.erase(g_cyc_hold.begin() + (long)i); continue; }
            if (w <= 0 && id != g_tgt_asset && id != g_cyc_look.load() && recorder::ReleaseSlot(idx)) { g_cyc_hold.erase(g_cyc_hold.begin() + (long)i); continue; }
            ++i;
        }
    }
    // The next boundary after phase f and the lighting mode that follows it.
    void NextBoundary(double f, bool day_oc, double& fb, int& nm) {
        if (f >= 0.93 || f < 0.08) { fb = 0.08; nm = day_oc ? 3 : 0; return; }
        if (f < 0.55) { fb = 0.55; nm = 2; return; }
        if (f < 0.68) { fb = 0.68; nm = 5; return; }
        fb = 0.93; nm = 4;
    }
    float Mix(float a, float b, float t) { return a + (b - a) * t; }
    struct Pose { int mode; bool sun; float rotd, elev; };
    float Lerp(double a, double b, double t) { return (float)(a + (b - a) * t); }
    Pose PoseAt(double f, bool day_oc) {
        Pose p{ 0, false, 0.f, 0.f };
        if (f >= 0.93 || f < 0.08) { const double g = (f >= 0.93 ? f - 0.93 : f + 0.07) / 0.15; p.mode = 4; p.sun = true; p.rotd = -85.f; p.elev = Lerp(-1, 12, g); }
        else if (f < 0.55) {
            if (day_oc) { p.mode = 3; return p; }   // an overcast day: no steered sun
            const double u = (f - 0.08) / 0.47; p.mode = 0; p.sun = true; p.rotd = Lerp(-80, 80, u); p.elev = (float)(8.0 + 62.0 * std::sin(3.14159265358979 * u));
        }
        else if (f < 0.68) { const double u = (f - 0.55) / 0.13; p.mode = 2; p.sun = true; p.rotd = Lerp(80, 100, u); p.elev = Lerp(8, -2, u); }
        else p.mode = 5;
        return p;
    }
    double PhaseOfMode(int mode) {
        switch (mode) { case 4: return 0.03; case 2: return 0.58; case 5: case 1: return 0.8; default: return 0.2; }
    }
    bool ModeMatches(int cur, int want) {
        if (want == 5) return cur == 5 || cur == 1;
        if (want == 3) return cur == 3 || cur == 6;
        return cur == want;
    }
    // Worker thread, every 40 ms while the cycle steers the sun (after the world has settled): advance the phase, steer the sun, and switch the
    // lighting mode (and the sky) at dawn, day, dusk and night. The first switch waits 30 s after a load (a setter call during a load hung it).
    void Cycle(ULONGLONG now) {
        const int csel = g_set[kCycle];
        const int minutes = kCycles[csel > 0 && csel < (int)(sizeof(kCycles) / sizeof(kCycles[0])) ? csel : 0].minutes;
        if (!minutes) {
            if (g_cyc_sun_on.exchange(false)) ++g_cyc_step;
            g_cyc_started = false; g_cyc_blend = 0.f; g_tgt_asset = 0; g_tgt_valid = false; g_pick_nm = -1;
            if (g_cyc_look.exchange(0)) g_cyc_step++;
            return;
        }
        if (now - g_world_up_at < 30000) return;
        const int mode = recorder::CurrentMode();
        if (mode < 0) return;
        SampleHero();
        if (!g_cyc_started) { g_phase = PhaseOfMode(mode); g_prev_phase = g_phase; g_cyc_overcast = mode == 3 || mode == 6; g_cyc_last = now; g_cyc_started = true; }
        // Follow the game's own clock: the world time scale (the pause menu and prompts freeze it), so a paused game pauses the cycle.
        float ts = 1.f;
        if (!memory::Read(scan::FromRva(0x621b890) + 0x18, ts) || !(ts >= 0.f) || ts > 4.f) ts = 1.f;
        if (std::fabs(ts - g_last_ts) > 0.05f) { LOG("sky: cycle time scale %.3f", ts); g_last_ts = ts; }
        // Is the world updating? Three timers in the exe's .data (found by diffing dumps taken running, paused and running again, RECON Part 16) advance
        // every frame while the game runs and stand still in the pause menu. None moved since the last sample: paused, so the clock stands still too.
        float timers[3] = { 0.f, 0.f, 0.f };
        for (int i = 0; i < 3; ++i) memory::Read(scan::FromRva(kWorldTimers[i]), timers[i]);
        bool moved = false;
        for (int i = 0; i < 3; ++i) moved = moved || timers[i] != g_last_timers[i];
        for (int i = 0; i < 3; ++i) g_last_timers[i] = timers[i];
        if (moved) g_still_since = 0; else if (!g_still_since) g_still_since = now;
        const bool paused = g_still_since && now - g_still_since > 90;
        if (paused != g_was_paused) { LOG("sky: cycle %s", paused ? "paused with the game" : "running again"); g_was_paused = paused; }
        const ULONGLONG dtms = paused ? 0 : now - g_cyc_last;
        g_cyc_last = now;
        g_prev_phase = g_phase;
        g_phase += (double)(dtms > 1000 ? 1000 : dtms) * ts / (minutes * 60000.0);
        g_phase -= std::floor(g_phase);
        if (g_prev_phase < 0.93 && g_phase >= 0.93) {   // a new day begins: roll whether its daytime is overcast
            static std::mt19937 wrng((unsigned)GetTickCount64() ^ 0x5bd1e995u);
            g_cyc_overcast = std::uniform_real_distribution<double>(0.0, 1.0)(wrng) < kOvercastChance;
            LOG("sky: cycle: today's day will be %s", g_cyc_overcast ? "OVERCAST" : "clear");
        }
        const Pose p = PoseAt(g_phase, g_cyc_overcast);
        const bool pending = !ModeMatches(mode, p.mode);   // a switch is due
        if (pending) g_due_wait_ms += dtms > 1000 ? 1000 : dtms; else { g_due_wait_ms = 0; g_wait_logged = false; }
        g_cyc_rotd = p.rotd; g_cyc_elev = p.elev; g_cyc_sun_on = p.sun;
        // ---- get ready for the next switch: pick its sky, load it quietly and blend toward it over the last seconds ----
        if (pending) { g_cyc_blend = g_tgt_valid ? 1.f : 0.f; }   // waiting for a calm moment: keep the colors at the next sky's
        else {
            double fb = 0.0; int nm = 0;
            NextBoundary(g_phase, g_cyc_overcast, fb, nm);
            double dph = fb - g_phase;
            if (dph < 0) dph += 1.0;
            const double dsec = dph * minutes * 60.0;
            const double lead = std::min(10.0, minutes * 60.0 * 0.06);
            float bt = 0.f;
            if (dsec <= lead + 5.0) {
                if (g_pick_nm != nm) { g_pick_nm = nm; g_next_look = PickLook(nm); LOG("sky: cycle: next phase (mode %d) sky %016llX%s", nm, (unsigned long long)g_next_look, g_next_look ? "" : " (the game's own)"); }
                const uint64_t id = g_next_look ? g_next_look : ModeBaseAtmosphere(nm);
                if (id && id != g_tgt_asset) { g_tgt_asset = id; g_tgt_valid = false; g_tgt_req_at = 0; }
                if (id && !g_tgt_valid) {
                    const int slot = recorder::FindSlot(id);
                    if (slot < 0) {
                        if (!g_tgt_req_at || now - g_tgt_req_at > 6000) {
                            recorder::LoadAtmosphere(id);
                            g_tgt_req_at = now;
                            if (std::find(g_cyc_hold.begin(), g_cyc_hold.end(), id) == g_cyc_hold.end()) g_cyc_hold.push_back(id);
                        }
                    } else if (ReadAtm(slot, g_tgt)) g_tgt_valid = true;
                }
                if (g_tgt_valid && dsec <= lead) {
                    const double u = std::max(0.0, std::min(1.0, 1.0 - dsec / lead));
                    bt = (float)(u * u * (3.0 - 2.0 * u));   // smoothstep
                }
            } else if (g_tgt_asset) { g_tgt_asset = 0; g_tgt_valid = false; }
            g_cyc_blend = bt;
        }
        ReleaseIdleHolds();
        if (p.sun || g_cyc_blend > 0.f || g_blend_prev > 0.f) ++g_cyc_step;
        g_blend_prev = g_cyc_blend;
        if (pending && now - g_cyc_switch_at > 8000) {
            const float hs = g_hero_speed;
            if (hs >= 3.0f && g_due_wait_ms < 45000) {   // Spider-Man is moving fast: the switch would stop him in mid-air; wait up to 45 s of game time
                if (!g_wait_logged) { LOG("sky: cycle: switch to mode %d waits for Spider-Man to slow down (%.1f m/s)", p.mode, hs); g_wait_logged = true; }
                return;
            }
            g_cyc_switch_at = now;
            if (g_pick_nm != p.mode) { g_pick_nm = p.mode; g_next_look = PickLook(p.mode); }   // no preparation happened (a late start): pick now
            g_cyc_look = g_next_look;                     // the chosen sky goes on with the switch (0: the game's own)
            g_pick_nm = -1;
            LOG("sky: cycle phase %.3f: lighting mode %d -> %d, sky %016llX", g_phase, mode, p.mode, (unsigned long long)g_next_look);
            modmenu::KeepMenuOpen(6000);
            if (g_smooth_fade) modmenu::SuppressFade(5000);
            recorder::SetMode(p.mode);
        }
    }

    Sun Wanted(const Sun& o) {
        Sun w = o;
        const int dir = g_set[kDir], height = g_set[kHeight], color = g_set[kColor], bright = g_set[kBright];
        if (dir > 0 && dir <= kDirSteps) w.rot = (float)((dir - 1) * 15);
        if (height > 0 && height <= (int)(sizeof(kHeights) / sizeof(kHeights[0]))) w.elev = (float)kHeights[height - 1];
        if (color > 0 && color < (int)(sizeof(kColors) / sizeof(kColors[0]))) for (int i = 0; i < 3; ++i) w.rgb[i] = kColors[color].rgb[i];
        if (bright > 0 && bright < (int)(sizeof(kBrights) / sizeof(kBrights[0]))) w.w = o.w * kBrights[bright].mul;
        if (g_cyc_sun_on) { w.rot = o.rot + g_cyc_rotd; w.elev = g_cyc_elev; }   // the day-night cycle steers the sun
        return w;
    }
    void ApplySun() {
        const ULONGLONG now = GetTickCount64();
        const int idx = g_look_id ? g_look_idx : BaseSlot();
        uint64_t asset; int weight;
        if (idx < 0 || !recorder::SlotInfo(idx, asset, weight) || !asset || weight <= 0) return;
        if (g_look_id ? now - g_look_at < 500 : now - g_base_seen_at.load() < 900) return;   // let a fresh slot finish loading
        bool any = false;
        for (int s = kDir; s < kSettingCount; ++s) any = any || g_set[s];
        bool changed = idx != g_edit_idx || asset != g_edit_asset || g_cyc_seen != g_cyc_step;
        for (int s = kDir; s < kSettingCount; ++s) if (g_edit_seen[s] != g_set[s]) changed = true;
        if (!changed) return;
        if (!any && !(g_edit_touched && idx == g_edit_idx && asset == g_edit_asset)) {   // nothing asked for and nothing of ours to undo here
            g_edit_idx = idx; g_edit_asset = asset;
            for (int s = kDir; s < kSettingCount; ++s) g_edit_seen[s] = g_set[s];
            g_edit_touched = false;
            return;
        }
        // the sky's own values, read before the first edit of this atmosphere
        float cur[6];
        if (!recorder::SlotSun(idx, cur, false)) return;
        auto it = g_orig.find(asset);
        if (it == g_orig.end()) { Sun o; memcpy(o.rgb, cur, 12); o.w = cur[3]; o.rot = cur[4]; o.elev = cur[5]; it = g_orig.emplace(asset, o).first; }
        auto fit = g_orig_fog.find(asset);
        float fcur[6];
        if (fit == g_orig_fog.end() && recorder::SlotFog(idx, fcur, false)) { Fog f; memcpy(f.d, fcur, sizeof(fcur)); fit = g_orig_fog.emplace(asset, f).first; }
        auto iit = g_orig_image.find(asset);
        if (iit == g_orig_image.end()) {
            Image im{};
            if (recorder::SlotFloats(idx, kOffFogDist, im.fogdist, 4, false) && recorder::SlotFloats(idx, kOffExposure, &im.exposure, 1, false) &&
                recorder::SlotFloats(idx, kOffFilter, im.filter, 3, false) && recorder::SlotFloats(idx, kOffHaze, &im.haze, 1, false) &&
                recorder::SlotFloats(idx, kOffClouds, &im.clouds, 1, false) && recorder::SlotFloats(idx, kOffFogColor, im.fogcolor, 3, false))
                iit = g_orig_image.emplace(asset, im).first;
        }
        // the sky's own values, blended toward the next sky's when a switch is near (the day-night cycle)
        Sun osun = it->second;
        Fog ofog{};
        const bool have_fog = fit != g_orig_fog.end();
        if (have_fog) ofog = fit->second;
        Image oim{};
        const bool have_im = iit != g_orig_image.end();
        if (have_im) oim = iit->second;
        const float bt = g_cyc_blend;
        if (bt > 0.f && g_tgt_valid) {
            for (int i = 0; i < 3; ++i) osun.rgb[i] = Mix(osun.rgb[i], g_tgt.sun.rgb[i], bt);
            osun.w = Mix(osun.w, g_tgt.sun.w, bt);
            for (int i = 0; i < 6; ++i) ofog.d[i] = Mix(ofog.d[i], g_tgt.fog.d[i], bt);
            for (int i = 0; i < 4; ++i) oim.fogdist[i] = Mix(oim.fogdist[i], g_tgt.im.fogdist[i], bt);
            oim.exposure = Mix(oim.exposure, g_tgt.im.exposure, bt);
            for (int i = 0; i < 3; ++i) { oim.filter[i] = Mix(oim.filter[i], g_tgt.im.filter[i], bt); oim.fogcolor[i] = Mix(oim.fogcolor[i], g_tgt.im.fogcolor[i], bt); }
            oim.haze = Mix(oim.haze, g_tgt.im.haze, bt);
        }
        // sun
        const Sun w = Wanted(osun);
        float buf[6] = { w.rgb[0], w.rgb[1], w.rgb[2], w.w, w.rot, w.elev };
        recorder::SlotSun(idx, buf, true);
        // fog density
        const int fogsel = g_set[kFog];
        float fmul = 1.f;
        if (fogsel > 0 && fogsel < (int)(sizeof(kFogs) / sizeof(kFogs[0]))) fmul = kFogs[fogsel].mul;
        if (have_fog) {
            float fb[6];
            for (int i = 0; i < 6; ++i) fb[i] = ofog.d[i] * fmul;
            recorder::SlotFog(idx, fb, true);
        }
        // image controls
        if (have_im) {
            const Image& o = oim;
            auto pick = [](int sel, int n) { return sel > 0 && sel < n ? sel : 0; };
            const int dsel = pick(g_set[kFogDist], (int)(sizeof(kFogDists) / sizeof(kFogDists[0])));
            float db[4];
            for (int i = 0; i < 4; ++i) db[i] = o.fogdist[i] * kFogDists[dsel].mul;
            recorder::SlotFloats(idx, kOffFogDist, db, 4, true);
            const int hsel = pick(g_set[kHaze], (int)(sizeof(kHazes) / sizeof(kHazes[0])));
            float haze = hsel ? kHazes[hsel].value : o.haze;
            recorder::SlotFloats(idx, kOffHaze, &haze, 1, true);
            const int esel = pick(g_set[kExposure], (int)(sizeof(kExposures) / sizeof(kExposures[0])));
            float ev = o.exposure + kExposures[esel].ev;
            recorder::SlotFloats(idx, kOffExposure, &ev, 1, true);
            const int fsel = pick(g_set[kFilter], (int)(sizeof(kFilters) / sizeof(kFilters[0])));
            float filter[3];
            for (int i = 0; i < 3; ++i) filter[i] = fsel ? kFilters[fsel].rgb[i] : o.filter[i];
            recorder::SlotFloats(idx, kOffFilter, filter, 3, true);
            const int csel = pick(g_set[kClouds], (int)(sizeof(kCloudsChoices) / sizeof(kCloudsChoices[0])));
            float clouds = csel ? kCloudsChoices[csel].value : o.clouds;
            recorder::SlotFloats(idx, kOffClouds, &clouds, 1, true);
            const int gsel = pick(g_set[kFogColor], (int)(sizeof(kFogColors) / sizeof(kFogColors[0])));
            float fogc[3];
            for (int i = 0; i < 3; ++i) fogc[i] = gsel ? kFogColors[gsel].rgb[i] : o.fogcolor[i];
            recorder::SlotFloats(idx, kOffFogColor, fogc, 3, true);
        }
        recorder::Recompute();
        g_cyc_seen = g_cyc_step;
        g_edit_idx = idx; g_edit_asset = asset; g_edit_touched = any || g_edit_touched;
        for (int s = kDir; s < kSettingCount; ++s) g_edit_seen[s] = g_set[s];
        if (!g_cyc_sun_on) LOG("sky: edits on slot %d (%016llX): sun rgb %.2f %.2f %.2f intensity %.2f rotation %.1f elevation %.1f, fog x%.2f, distance/haze/exposure/filter/clouds %d/%d/%d/%d/%d",
            idx, (unsigned long long)asset, w.rgb[0], w.rgb[1], w.rgb[2], w.w, w.rot, w.elev, fmul, g_set[kFogDist].load(), g_set[kHaze].load(), g_set[kExposure].load(), g_set[kFilter].load(), g_set[kClouds].load());
    }

    void SaveIni() {
        const std::string p = IniPath();
        if (p.empty()) return;
        for (int s = 0; s < kSettingCount; ++s) WritePrivateProfileStringA("sky", kKeys[s], std::to_string(g_set[s].load()).c_str(), p.c_str());
        WritePrivateProfileStringA("sky", "mode", std::to_string(g_saved_mode.load()).c_str(), p.c_str());
    }
}

    void Load() {
        EnsureInit();
        const std::string p = IniPath();
        for (int s = 0; s < kSettingCount; ++s) g_set[s] = p.empty() ? 0 : (int)GetPrivateProfileIntA("sky", kKeys[s], 0, p.c_str());
        g_saved_mode = p.empty() ? -1 : (int)GetPrivateProfileIntA("sky", "mode", -1, p.c_str());
        if (g_saved_mode < 0 || g_saved_mode > 6) g_saved_mode = -1;
        // clamp what the tables allow (a changed table must not leave an index past its end)
        if (g_set[kStyle] < 0 || g_set[kStyle] >= kStyleCount) g_set[kStyle] = 0;
        if (g_set[kVariant] < 0 || g_set[kVariant] >= (int)g_variant_look[g_set[kStyle]].size()) g_set[kVariant] = 0;
        if (g_set[kDir] < 0 || g_set[kDir] > kDirSteps) g_set[kDir] = 0;
        if (g_set[kHeight] < 0 || g_set[kHeight] > (int)(sizeof(kHeights) / sizeof(kHeights[0]))) g_set[kHeight] = 0;
        if (g_set[kColor] < 0 || g_set[kColor] >= (int)(sizeof(kColors) / sizeof(kColors[0]))) g_set[kColor] = 0;
        if (g_set[kBright] < 0 || g_set[kBright] >= (int)(sizeof(kBrights) / sizeof(kBrights[0]))) g_set[kBright] = 0;
        if (g_set[kFog] < 0 || g_set[kFog] >= (int)(sizeof(kFogs) / sizeof(kFogs[0]))) g_set[kFog] = 0;
        if (g_set[kFogDist] < 0 || g_set[kFogDist] >= (int)(sizeof(kFogDists) / sizeof(kFogDists[0]))) g_set[kFogDist] = 0;
        if (g_set[kHaze] < 0 || g_set[kHaze] >= (int)(sizeof(kHazes) / sizeof(kHazes[0]))) g_set[kHaze] = 0;
        if (g_set[kExposure] < 0 || g_set[kExposure] >= (int)(sizeof(kExposures) / sizeof(kExposures[0]))) g_set[kExposure] = 0;
        if (g_set[kFilter] < 0 || g_set[kFilter] >= (int)(sizeof(kFilters) / sizeof(kFilters[0]))) g_set[kFilter] = 0;
        if (g_set[kFogColor] < 0 || g_set[kFogColor] >= (int)(sizeof(kFogColors) / sizeof(kFogColors[0]))) g_set[kFogColor] = 0;
        if (g_set[kCycle] < 0 || g_set[kCycle] >= (int)(sizeof(kCycles) / sizeof(kCycles[0]))) g_set[kCycle] = 0;
        if (g_set[kClouds] < 0 || g_set[kClouds] >= (int)(sizeof(kCloudsChoices) / sizeof(kCloudsChoices[0]))) g_set[kClouds] = 0;
        LOG("sky: settings style %d variant %d dir %d height %d color %d bright %d fog %d", g_set[kStyle].load(), g_set[kVariant].load(), g_set[kDir].load(), g_set[kHeight].load(), g_set[kColor].load(), g_set[kBright].load(), g_set[kFog].load());
    }
    int Get(Setting s) { return g_set[s]; }
    int SavedMode() { return g_saved_mode; }
    void SaveMode(int mode) { g_saved_mode = mode; g_mode_pending = false; g_dirty_ini = true; g_dirty_at = GetTickCount64(); }
    void Set(Setting s, int value) {
        EnsureInit();
        g_set[s] = value;
        if (s == kStyle) g_set[kVariant] = 0;
        if (s == kStyle || s == kVariant) g_failed_id = 0;   // a new pick gets its tries again
        g_dirty_ini = true;
        g_dirty_at = GetTickCount64();
    }
    void ResetAll() { for (int s = 0; s < kSettingCount; ++s) g_set[s] = 0; g_dirty_ini = true; g_dirty_at = GetTickCount64(); }

    const char* Key(Setting s) { return kKeys[s]; }
    int LabelCount(Setting s) {
        switch (s) {
            case kStyle: return StyleCount();
            case kVariant: return VariantCount(g_set[kStyle]);
            case kDir: return DirCount();
            case kHeight: return HeightCount();
            case kColor: return ColorCount();
            case kBright: return BrightCount();
            case kFog: return FogCount();
            case kFogDist: return FogDistCount();
            case kHaze: return HazeCount();
            case kExposure: return ExposureCount();
            case kFilter: return FilterCount();
            case kClouds: return CloudsCount();
            case kCycle: return CycleCount();
            default: return FogColorCount();
        }
    }
    const char* LabelAt(Setting s, int i) {
        const char* const* l = nullptr;
        switch (s) {
            case kStyle: l = StyleLabels(); break;
            case kVariant: l = VariantLabels(g_set[kStyle]); break;
            case kDir: l = DirLabels(); break;
            case kHeight: l = HeightLabels(); break;
            case kColor: l = ColorLabels(); break;
            case kBright: l = BrightLabels(); break;
            case kFog: l = FogLabels(); break;
            case kFogDist: l = FogDistLabels(); break;
            case kHaze: l = HazeLabels(); break;
            case kExposure: l = ExposureLabels(); break;
            case kFilter: l = FilterLabels(); break;
            case kClouds: l = CloudsLabels(); break;
            case kCycle: l = CycleLabels(); break;
            default: l = FogColorLabels(); break;
        }
        return l && i >= 0 && i < LabelCount(s) ? l[i] : "";
    }

    int StyleCount() { return kStyleCount; }
    const char* const* StyleLabels() { return kStyleLabels; }
    int VariantCount(int style) { EnsureInit(); return style > 0 && style < kStyleCount ? (int)g_variant_labels[style].size() : 0; }
    const char* const* VariantLabels(int style) { EnsureInit(); return style > 0 && style < kStyleCount ? g_variant_labels[style].data() : nullptr; }
    int DirCount() { EnsureInit(); return (int)g_dir_labels.size(); }        const char* const* DirLabels() { EnsureInit(); return g_dir_labels.data(); }
    int HeightCount() { EnsureInit(); return (int)g_height_labels.size(); }  const char* const* HeightLabels() { EnsureInit(); return g_height_labels.data(); }
    int ColorCount() { EnsureInit(); return (int)g_color_labels.size(); }    const char* const* ColorLabels() { EnsureInit(); return g_color_labels.data(); }
    int BrightCount() { EnsureInit(); return (int)g_bright_labels.size(); } const char* const* BrightLabels() { EnsureInit(); return g_bright_labels.data(); }
    int FogCount() { EnsureInit(); return (int)g_fog_labels.size(); }       const char* const* FogLabels() { EnsureInit(); return g_fog_labels.data(); }
    int FogDistCount() { EnsureInit(); return (int)g_fogdist_labels.size(); }   const char* const* FogDistLabels() { EnsureInit(); return g_fogdist_labels.data(); }
    int HazeCount() { EnsureInit(); return (int)g_haze_labels.size(); }         const char* const* HazeLabels() { EnsureInit(); return g_haze_labels.data(); }
    int ExposureCount() { EnsureInit(); return (int)g_exposure_labels.size(); } const char* const* ExposureLabels() { EnsureInit(); return g_exposure_labels.data(); }
    int FilterCount() { EnsureInit(); return (int)g_filter_labels.size(); }     const char* const* FilterLabels() { EnsureInit(); return g_filter_labels.data(); }
    int TickMs() { return g_cyc_sun_on || g_cyc_blend > 0.f ? 40 : 250; }
    void SetSmoothFade(bool on) { g_smooth_fade = on; LOG("sky: smooth fade %s", on ? "on (the cycle swallows the lighting transition's fade)" : "off"); }
    int CycleCount() { EnsureInit(); return (int)g_cycle_labels.size(); }       const char* const* CycleLabels() { EnsureInit(); return g_cycle_labels.data(); }
    bool CycleSyncMode(int mode) {
        if (g_set[kCycle] <= 0) return true;
        g_cyc_overcast = mode == 3 || mode == 6;   // picking Overcast starts an overcast day
        g_cyc_look = 0; g_pick_nm = -1;
        g_phase = PhaseOfMode(mode); g_prev_phase = g_phase; g_cyc_started = true; g_cyc_last = GetTickCount64();
        return true;
    }
    int FogColorCount() { EnsureInit(); return (int)g_fogcolor_labels.size(); } const char* const* FogColorLabels() { EnsureInit(); return g_fogcolor_labels.data(); }
    int CloudsCount() { EnsureInit(); return (int)g_clouds_labels.size(); }     const char* const* CloudsLabels() { EnsureInit(); return g_clouds_labels.data(); }

    // Game thread, inside the AtmosphereAdjust detour.
    bool Filter(uintptr_t caller_rva, int slot_idx, uint64_t asset, int delta) {
        if (caller_rva == 0x1a1b59e && delta > 0) {   // the lighting object activating a base atmosphere
            g_base_idx = slot_idx;
            g_base_asset = asset;
            g_base_seen_at = GetTickCount64();
            if (g_look_id) return true;               // a sky override is on: keep it on top, skip the game's activation
        }
        return false;
    }

    void Tick() {
        EnsureInit();
        const ULONGLONG now = GetTickCount64();
        if (g_dirty_ini && now - g_dirty_at > 1500) { g_dirty_ini = false; SaveIni(); }
        const int mode = recorder::CurrentMode();
        if (mode < 0 || !recorder::Manager()) { g_world_up = false; return; }
        if (!g_world_up) {   // a (re)load: every slot we knew is gone
            g_world_up = true;
            g_look_id = 0; g_look_idx = -1; g_edit_idx = -1; g_edit_asset = 0; g_edit_touched = false; g_load_req_at = 0; g_held.clear(); g_failed_id = 0; g_load_tries = 0;
            for (int s = 0; s < kSettingCount; ++s) g_edit_seen[s] = -1;
            g_hold_until = now + 3000;
            g_world_up_at = now; g_cyc_started = false;
            return;
        }
        if (now < g_hold_until) return;
        Cycle(now);
        ApplyLook();
        ApplySun();
    }

    void LogState() {
        LOG("sky: style %d variant %d dir %d height %d color %d bright %d fog %d | look %016llX slot %d | base slot %d asset %016llX | sun edit slot %d touched %d | world %d",
            g_set[kStyle].load(), g_set[kVariant].load(), g_set[kDir].load(), g_set[kHeight].load(), g_set[kColor].load(), g_set[kBright].load(), g_set[kFog].load(), (unsigned long long)g_look_id,
            g_look_idx, g_base_idx.load(), (unsigned long long)g_base_asset.load(), g_edit_idx, (int)g_edit_touched, (int)g_world_up);
        for (int s = 1; s < kStyleCount; ++s) LOG("sky: style %s has %zu variants", kStyleLabels[s], g_variant_labels[s].size());
    }
}
