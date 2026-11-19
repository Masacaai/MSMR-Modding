#include "sky.h"

#include "log.h"
#include "gamethread.h"
#include "natives.h"
#include <algorithm>
#include "scan.h"
#include "atmpacks.h"
#include "seamless.h"
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
    const BrightPreset kBrights[] = { { "AUTO", 1 }, { "25%", 0.25f }, { "50%", 0.5f }, { "75%", 0.75f }, { "150%", 1.5f }, { "200%", 2 }, { "300%", 3 }, { "500%", 5 }, { "OFF", 0 } };   // OFF last so the saved indices of earlier versions and the presets stay valid
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
    struct AmountPreset { const char* label; float t; };   // how much of a chosen color replaces the sky's own (AUTO: all of it)
    const AmountPreset kAmounts[] = { { "AUTO", 1.0f }, { "75%", 0.75f }, { "50%", 0.5f }, { "25%", 0.25f }, { "10%", 0.10f } };
    std::vector<const char*> g_amount_labels;
    // AMBIENT LIGHT: the atmosphere's AmbientFillColor (slot+0x300..0x308) times AmbientFillIntensity (+0x30c). Vanilla: the intensity is 0 in the day, night and sunset
    // atmospheres and 0.003 in the rain night (tools/atmslot.py), the color tiny (0.0003) or white. The steps are a guess around that scale (docs/RECON.md Part 25).
    struct AmbientPreset { const char* label; float v; };
    const AmbientPreset kAmbients[] = { { "AUTO", 0 }, { "1 (FAINT)", 0.001f }, { "2", 0.002f }, { "3", 0.004f }, { "4", 0.008f }, { "5", 0.016f }, { "6", 0.03f }, { "7 (STRONG)", 0.06f } };
    std::vector<const char*> g_ambient_labels;
    constexpr uintptr_t kOffAmbient = 0x300;   // 4 floats: color RGB, intensity (the safe block: never write +0x2e8 or +0x310..0x33f)
    bool g_amb_dirty = false;                  // we wrote the fill light and the sky's own values are not back yet
    std::atomic<bool> g_seamless{ false };
    float AmountOf(int sel) { return sel > 0 && sel < (int)(sizeof(kAmounts) / sizeof(kAmounts[0])) ? kAmounts[sel].t : 1.0f; }
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
        for (const auto& f : kAmounts) g_amount_labels.push_back(f.label);
        for (const auto& f : kAmbients) g_ambient_labels.push_back(f.label);
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
    const char* const kVerKeys[5] = { "day", "earlymorning", "sunset", "night", "overcast" };
    const char* const kKeys[kSettingCount] = { "style", "variant", "dir", "height", "color", "bright", "fog", "fogdist", "haze", "exposure", "filter", "clouds", "fogcolor", "cycle", "cycleatm", "coloramt", "fogcoloramt", "ambient", "ambientcolor" };

    // ---- runtime state (worker thread, except where noted) ----
    constexpr float kNoValue = -12345.f;
    struct Sun { float rgb[3], w, rot, elev; };
    std::map<uint64_t, Sun> g_orig;               // each atmosphere's own sun values, read before the first edit
    struct Fog { float d[6]; };
    std::map<uint64_t, Fog> g_orig_fog;           // ... and its own fog densities
    struct Image { float fogdist[4], exposure, filter[3], haze, clouds, fogcolor[3], ambient[4]; };
    std::map<uint64_t, Image> g_orig_image;       // ... and its own distance bands, exposure, filter, haze and cloud values
    bool g_world_up = false;
    ULONGLONG g_hold_until = 0;                   // don't touch anything before this tick count (the world is settling)
    std::atomic<int> g_base_idx{ -1 };            // the slot the game last activated as a base atmosphere (filter, game thread)
    std::atomic<uint64_t> g_base_asset{ 0 };
    std::atomic<ULONGLONG> g_base_seen_at{ 0 };
    std::set<uint64_t> g_held;                    // atmospheres this module loaded and still holds a reference on (released when its look goes off)
    std::vector<uint64_t> g_cyc_hold;             // atmospheres the cycle loaded quietly ahead of a switch: released once they are idle
    uint64_t g_failed_id = 0;                     // a look that would not load (pool full?): not retried until the setting changes
    int g_load_tries = 0;
    uint64_t g_look_id = 0;                       // the override atmosphere that is active (0: none)
    int g_look_idx = -1;
    ULONGLONG g_look_at = 0, g_load_req_at = 0;
    int g_edit_idx = -1;                          // the slot whose sun we last wrote
    uint64_t g_edit_asset = 0;
    ULONGLONG g_edit_parsed = 0;                  // the parse (recorder::SlotParsedTick) our last edit of that slot was made after
    int g_edit_seen[kSettingCount] = {};          // the sun settings as last written
    bool g_edit_touched = false;                  // we have written to g_edit_idx (so AUTO must restore the original)

    std::mutex g_ver_mu;
    std::string g_ver[5];                         // chosen atmosphere version per time-of-day slot ("" = the game's own)
    // The new asset id of the atmosphere mod chosen for this lighting mode, or 0.
    uint64_t VersionLook(int mode) {
        const int slot = atmpacks::SlotOfMode(mode);
        if (slot < 0) return 0;
        std::string name;
        { std::lock_guard<std::mutex> lk(g_ver_mu); name = g_ver[slot]; }
        return name.empty() ? 0 : atmpacks::IdFor(slot, name);
    }
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
        if (style <= 0) { const uint64_t v = VersionLook(recorder::CurrentMode()); if (v) return v; }   // the atmosphere mod chosen for this time of day
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

    bool WorldReady(ULONGLONG now);   // defined with the cycle: 8 s after a load, Spider-Man readable and the world timers moving for 4 s
    // A pick made in the menu waits until it has rested for 350 ms before the sky is loaded: scrolling through the variants used to load and release
    // several skies a second (a Settings-screen crash followed such a session). The dynamic cycle's own picks are not delayed (they must land with the switch).
    uint64_t g_want_pending = 0;
    ULONGLONG g_want_since = 0;
    void ApplyLook() {
        const uint64_t want = DesiredLook();
        const ULONGLONG now = GetTickCount64();
        if (want == g_look_id) { g_want_pending = want; return; }
        // An atmosphere mod's load requests (the atmosphere, then its sky and baked-light textures) made while the game is still loading or streaming (a relaunched save, the
        // first seconds of a world) are sometimes lost or half-served: a red screen after a relaunch, a missing or grey sky. They wait until the world is ready.
        if (want && atmpacks::IsMod(want) && !WorldReady(now)) return;
        if (g_set[kStyle] > 0) {
            if (want != g_want_pending) { g_want_pending = want; g_want_since = now; }
            if (now - g_want_since < 350) return;
        }
        if (want) {
            int idx = recorder::FindSlot(want);
            if (want == g_failed_id) return;
            // An atmosphere mod the cycle loaded quietly (weight 0) ahead of the switch never gets its textures requested when it is switched on later (the game
            // asks for them when the atmosphere finishes loading with a weight, 2026-11-13: black sky, lit city). Drop that load and load it again the way a menu
            // pick does, so it is switched on as it arrives.
            if (idx >= 0 && atmpacks::IsMod(want)) {
                const auto hit = std::find(g_cyc_hold.begin(), g_cyc_hold.end(), want);
                uint64_t a; int w;
                if (hit != g_cyc_hold.end() && recorder::SlotInfo(idx, a, w) && w <= 0 && recorder::ReleaseSlot(idx)) {
                    g_cyc_hold.erase(hit);
                    g_load_req_at = 0;
                    LOG("sky: look %016llX was loaded quietly: loading it again so its textures are requested", (unsigned long long)want);
                    return;
                }
            }
            if (idx < 0) {   // not in the pool yet: ask for it and try again on the next ticks
                if (!g_load_req_at || now - g_load_req_at > 6000) {
                    if (g_load_tries >= 3) { g_failed_id = want; LOG("sky: look %016llX would not load after 3 tries (the atmosphere pool is probably full); giving up on it", (unsigned long long)want); return; }
                    ++g_load_tries;
                    atmpacks::Prepare(want);   // an atmosphere mod's textures are unpacked on first use
                    atmpacks::Select(want);    // and the files it ships besides its atmosphere replace the game's while it is on
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
        atmpacks::Deselect();
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
    //   [0.93,0.08) Early Morning   [0.08,0.55) Day (or an overcast day, kOvercastChance)   [0.55,0.68) Sunset   [0.68,0.93) Night
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
    bool g_was_paused = false, g_was_busy = false;
    bool g_hold = false;                      // a phase boundary came and the lighting has not switched yet: the day's clock stands at the boundary, so the next phase runs its full length
    ULONGLONG g_hold_since = 0;
    ULONGLONG g_last_switch_at = 0, g_frozen_ms = 0;   // for the log: when the lighting last changed, and how much cycle time stood still since (paused or busy)
    double g_phase = 0.2;
    bool g_cyc_started = false;
    ULONGLONG g_cyc_last = 0, g_cyc_switch_at = 0, g_world_up_at = 0, g_ready_since = 0, g_phase_saved_at = 0;
    double g_saved_phase = -1.0;                  // where the cycle stood when the game was last closed (-1: unknown), and that day's weather
    bool g_saved_overcast = false;
    // The cycle's place in its day, kept in the ini every few seconds so the next launch continues from it.
    void SavePhase(double phase, bool overcast) {
        const std::string p = IniPath();
        if (p.empty()) return;
        char b[32];
        snprintf(b, sizeof(b), "%.4f", phase);
        WritePrivateProfileStringA("cycle", "phase", b, p.c_str());
        WritePrivateProfileStringA("cycle", "overcast", overcast ? "1" : "0", p.c_str());
    }
    // Softening the switch: shortly before a mode change the next sky is loaded quietly (weight 0) and its sun color, fog, haze, exposure and
    // filter are blended into the current one, so at the instant of the switch (the fade is swallowed) only the sky texture and the lights jump.
    struct Target { Sun sun; Fog fog; Image im; };
    Target g_tgt{};
    bool g_tgt_valid = false;
    uint64_t g_tgt_asset = 0;
    ULONGLONG g_tgt_req_at = 0;
    std::atomic<float> g_cyc_blend{ 0.f };
    std::atomic<int> g_blend_nm{ -1 };            // the lighting mode the colors are being blended toward (-1: none)
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
    // Random variations the dynamic cycle adds for each phase: SUN BRIGHTNESS, FOG DENSITY, FOG DISTANCE and HAZE (indices into their tables; 0 = the sky's own).
    // A value the player set in that row wins. The next phase's roll is made with its sky, so fog and haze blend toward it with the colors.
    int g_var[4] = {}, g_next_var[4] = {};
    bool g_cur_mod = false;   // the sky on now is an atmosphere mod's: it keeps its author's values (no cycle variations, no steered sun); only the rows the player set apply
    int VarIdx(Setting s) { switch (s) { case kBright: return 0; case kFog: return 1; case kFogDist: return 2; case kHaze: return 3; default: return -1; } }
    int EffSel(Setting s, bool next = false) {
        if (g_set[s]) return g_set[s];
        if (g_cur_mod) return 0;
        const int v = VarIdx(s);
        if (v < 0 || g_set[kCycle] <= 0) return 0;
        return next ? g_next_var[v] : g_var[v];
    }
    constexpr int kVarBright[] = { 0, 2, 3, 4, 5 };            // AUTO, 50%, 75%, 150%, 200%
    constexpr int kVarFog[] = { 0, 1, 2, 3, 4, 5 };            // AUTO, NONE, 25%, 50%, 150%, 300%
    constexpr int kVarFogDist[] = { 0, 2, 3, 4 };              // AUTO, 50%, 150%, 300%
    // HAZE is an absolute veil, not a multiplier: HEAVY (0.5) takes the far city to a white-out (RECON Part 12), which at night looked like a blinding white light
    // (2026-11-14). The random pools stop at MEDIUM by day and at LIGHT at night and in overcast.
    constexpr int kVarHazeDay[] = { 0, 1, 2, 3 };              // AUTO, NONE, LIGHT, MEDIUM
    constexpr int kVarHazeDark[] = { 0, 1, 2 };                // AUTO, NONE, LIGHT
    void RollVar(int out[4], int mode) {
        static std::mt19937 rng((unsigned)GetTickCount64() ^ 0x9e3779b9u);
        auto pick = [&](const int* a, size_t n) { return a[std::uniform_int_distribution<size_t>(0, n - 1)(rng)]; };
        out[0] = pick(kVarBright, sizeof(kVarBright) / sizeof(int));
        out[1] = pick(kVarFog, sizeof(kVarFog) / sizeof(int));
        out[2] = pick(kVarFogDist, sizeof(kVarFogDist) / sizeof(int));
        const bool dark = mode == 5 || mode == 1 || mode == 3 || mode == 6;   // night and overcast
        out[3] = dark ? pick(kVarHazeDark, sizeof(kVarHazeDark) / sizeof(int)) : pick(kVarHazeDay, sizeof(kVarHazeDay) / sizeof(int));
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
    // A random pick among the game's own sky for the phase (id 0), the listed skies and, when INCLUDE ATMOSPHERES is on, that time of day's atmosphere mods
    // (each used as the mod ships it, with the game's own values for everything else).
    uint64_t PickFrom(const PickDef* defs, size_t n, int nm) {
        static std::mt19937 rng((unsigned)GetTickCount64());
        std::vector<uint64_t> pool;
        pool.push_back(0);
        for (size_t i = 0; i < n; ++i) pool.push_back(LookId(defs[i].style, defs[i].label));
        if (g_set[kCycleAtm]) {
            const int slot = atmpacks::SlotOfMode(nm);
            for (int i = 1; i < atmpacks::LabelCount(slot); ++i) { const uint64_t id = atmpacks::IdFor(slot, atmpacks::NameAt(slot, i)); if (id) pool.push_back(id); }
        }
        return pool[std::uniform_int_distribution<size_t>(0, pool.size() - 1)(rng)];
    }
    uint64_t PickLook(int nm) {
        { const uint64_t v = VersionLook(nm); if (v) return v; }   // an atmosphere mod the player chose for that time of day beats the random pick
        static const PickDef kNone[1] = { { 0, "" } };
        switch (nm) {
            case 0: return PickFrom(kPickDay, sizeof(kPickDay) / sizeof(kPickDay[0]), nm);
            case 3: return PickFrom(kPickOvercast, sizeof(kPickOvercast) / sizeof(kPickOvercast[0]), nm);
            case 2: return PickFrom(kPickSunset, sizeof(kPickSunset) / sizeof(kPickSunset[0]), nm);
            case 5: return PickFrom(kPickNight, sizeof(kPickNight) / sizeof(kPickNight[0]), nm);
            case 4: return g_set[kCycleAtm] ? PickFrom(kNone, 0, nm) : 0;   // Early Morning keeps its own sky unless atmosphere mods are included
            default: return 0;
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
        if (f >= 0.93 || f < 0.08) { const double g = (f >= 0.93 ? f - 0.93 : f + 0.07) / 0.15; p.mode = 4; p.sun = true; p.rotd = -85.f; p.elev = Lerp(-3, 12, g); }
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
    // "Gameplay has started": 8 s after the world came up, Spider-Man's transform is readable and the world timers advance, both for 4 s in a row (this replaced the old
    // fixed 30 s wait; the one hang seen came from a lighting setter call 3 s after a load, while the game was still loading). UpdateReadiness runs every tick.
    void UpdateReadiness(ULONGLONG now) {
        if (now - g_world_up_at < 8000) { g_ready_since = 0; return; }
        SampleHero();
        static float pre[3] = { 0.f, 0.f, 0.f };
        bool pm = false;
        for (int i = 0; i < 3; ++i) { float v = 0.f; memory::Read(scan::FromRva(kWorldTimers[i]), v); pm = pm || v != pre[i]; pre[i] = v; }
        if (g_hero_speed.load() >= 0.f && pm) { if (!g_ready_since) g_ready_since = now; } else g_ready_since = 0;
    }
    bool WorldReady(ULONGLONG now) { return g_ready_since && now - g_ready_since >= 4000; }
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
        if (!WorldReady(now)) return;
        const int mode = recorder::CurrentMode();
        if (mode < 0) return;
        if (!g_cyc_started) {
            g_phase = PhaseOfMode(mode); g_cyc_overcast = mode == 3 || mode == 6;
            if (g_saved_phase >= 0.0 && ModeMatches(mode, PoseAt(g_saved_phase, g_saved_overcast).mode)) { g_phase = g_saved_phase; g_cyc_overcast = g_saved_overcast; LOG("sky: cycle resumes at phase %.3f", g_phase); }   // where it stopped last session
            g_prev_phase = g_phase; g_cyc_last = now; g_cyc_started = true; g_hold = false;
            RollVar(g_var, mode); ++g_cyc_step;   // the first phase gets random variations too
        }
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
        // In a crime, a hideout, a mission objective or a fight the day stands still, and no lighting switch happens: a switch is a mini-load that ends the activity.
        const bool busy = modmenu::PlayerBusy();
        if (busy != g_was_busy) { LOG("sky: cycle %s", busy ? "frozen: the player is in a crime, hideout, mission or fight" : "running again: the activity is over"); g_was_busy = busy; }
        const ULONGLONG dtms = paused || busy ? 0 : now - g_cyc_last;
        if (paused || busy) g_frozen_ms += now - g_cyc_last;
        g_cyc_last = now;
        g_prev_phase = g_phase;
        if (!g_hold) {
            g_phase += (double)(dtms > 1000 ? 1000 : dtms) * ts / (minutes * 60000.0);
            g_phase -= std::floor(g_phase);
            // A boundary was crossed: the clock stops right on it until the lighting has switched (a switch can be late: Spider-Man swinging, an activity), so
            // every phase runs its full length from the moment it actually starts, never less.
            const int before = PoseAt(g_prev_phase, g_cyc_overcast).mode, after = PoseAt(g_phase, g_cyc_overcast).mode;
            if (before != after) {
                g_phase = after == 4 ? 0.93 : (after == 2 ? 0.55 : (after == 5 ? 0.68 : 0.08));
                g_hold = true; g_hold_since = now;
            }
        }
        if (g_prev_phase < 0.93 && g_phase >= 0.93) {   // a new day begins: roll whether its daytime is overcast
            static std::mt19937 wrng((unsigned)GetTickCount64() ^ 0x5bd1e995u);
            g_cyc_overcast = std::uniform_real_distribution<double>(0.0, 1.0)(wrng) < kOvercastChance;
            LOG("sky: cycle: today's day will be %s", g_cyc_overcast ? "OVERCAST" : "clear");
        }
        if (now - g_phase_saved_at > 5000) { g_phase_saved_at = now; SavePhase(g_phase, g_cyc_overcast); }
        const Pose p = PoseAt(g_phase, g_cyc_overcast);
        const Pose ps = g_hold ? PoseAt(std::fmod(g_phase + 1.0 - 1e-6, 1.0), g_cyc_overcast) : p;   // while the clock waits at a boundary, the sun stays where the old phase left it
        const bool pending = !ModeMatches(mode, p.mode);   // a switch is due
        if (g_hold && !pending) g_hold = false;            // the lighting already matches (picked by hand): nothing to wait for
        if (pending) g_due_wait_ms += dtms > 1000 ? 1000 : dtms; else { g_due_wait_ms = 0; g_wait_logged = false; }
        g_cyc_rotd = ps.rotd; g_cyc_elev = ps.elev; g_cyc_sun_on = ps.sun;
        // ---- get ready for the next switch: pick its sky a few seconds ahead (an atmosphere mod's textures are unpacked meanwhile). There is no blending toward
        // it and nothing is loaded early: the switch is one clean change (maintainer's decision, 2026-11-14: the blends did little, and an early weight-0 load of
        // an atmosphere mod sometimes never got its skybox textures requested). ----
        g_cyc_blend = 0.f; g_blend_nm = -1; g_tgt_valid = false; g_tgt_asset = 0;
        if (!pending) {
            double fb = 0.0; int nm = 0;
            NextBoundary(g_phase, g_cyc_overcast, fb, nm);
            double dph = fb - g_phase;
            if (dph < 0) dph += 1.0;
            const double dsec = dph * minutes * 60.0;
            const double lead = std::min(10.0, minutes * 60.0 * 0.06);
            if (dsec <= lead + 5.0 && g_pick_nm != nm) {
                g_pick_nm = nm; g_next_look = PickLook(nm); RollVar(g_next_var, nm);
                LOG("sky: cycle: next phase (mode %d) sky %016llX%s", nm, (unsigned long long)g_next_look, g_next_look ? "" : " (the game's own)");
                if (g_next_look && atmpacks::IsMod(g_next_look)) atmpacks::Prepare(g_next_look);
            } else if (dsec > lead + 5.0) g_pick_nm = -1;
        }
        ReleaseIdleHolds();
        if (ps.sun || g_cyc_blend > 0.f || g_blend_prev > 0.f) ++g_cyc_step;
        g_blend_prev = g_cyc_blend;
        if (pending && now - g_cyc_switch_at > 8000) {
            if (busy) return;   // never switch during an activity, however long it lasts
            const float hs = g_hero_speed;
            if (hs >= 3.0f && g_due_wait_ms < 45000) {   // Spider-Man is moving fast: the switch would stop him in mid-air; wait up to 45 s of game time
                if (!g_wait_logged) { LOG("sky: cycle: switch to mode %d waits for Spider-Man to slow down (%.1f m/s)", p.mode, hs); g_wait_logged = true; }
                return;
            }
            g_cyc_switch_at = now;
            if (g_pick_nm != p.mode) { g_pick_nm = p.mode; g_next_look = PickLook(p.mode); RollVar(g_next_var, p.mode); }   // no preparation happened (a late start): pick now
            g_cyc_look = g_next_look;                     // the chosen sky goes on with the switch (0: the game's own)
            for (int i = 0; i < 4; ++i) g_var[i] = g_next_var[i];
            ++g_cyc_step;
            g_pick_nm = -1;
            {   // the clock stood still at the boundary since g_hold_since: that wait does not shorten the phase that starts now
                LOG("sky: cycle phase %.3f: lighting mode %d -> %d, sky %016llX; the switch waited %.0f s past the boundary, the previous look lasted %.0f s (%.0f s of cycle time stood still)", g_phase,
                    mode, p.mode, (unsigned long long)g_next_look, g_hold ? (double)(now - g_hold_since) / 1000.0 : 0.0,
                    g_last_switch_at ? (double)(now - g_last_switch_at) / 1000.0 : 0.0, (double)g_frozen_ms / 1000.0);
                g_last_switch_at = now; g_frozen_ms = 0; g_hold = false;
            }
            modmenu::KeepMenuOpen(6000);
            if (g_smooth_fade) modmenu::SuppressFade(5000);
            recorder::SetMode(p.mode);
            SaveMode(p.mode);   // the game's load-time call follows the cycle too, so a restart loads the matching time of day
        }
    }

    Sun Wanted(const Sun& o) {
        Sun w = o;
        const int dir = g_set[kDir], height = g_set[kHeight], color = g_set[kColor], bright = EffSel(kBright);
        if (dir > 0 && dir <= kDirSteps) w.rot = (float)((dir - 1) * 15);
        if (height > 0 && height <= (int)(sizeof(kHeights) / sizeof(kHeights[0]))) w.elev = (float)kHeights[height - 1];
        if (color > 0 && color < (int)(sizeof(kColors) / sizeof(kColors[0]))) for (int i = 0; i < 3; ++i) w.rgb[i] = Mix(o.rgb[i], kColors[color].rgb[i], AmountOf(g_set[kColorAmt]));
        if (bright > 0 && bright < (int)(sizeof(kBrights) / sizeof(kBrights[0]))) w.w = o.w * kBrights[bright].mul;
        if (g_cyc_sun_on && !g_cur_mod) {   // the day-night cycle steers the sun (not an atmosphere mod's)
            w.rot = o.rot + g_cyc_rotd; w.elev = g_cyc_elev;
            if (recorder::CurrentMode() == 4 && !color && !g_set[kBright]) {   // dawn: an orange sun rising from below the horizon, faint at first, easing to the sky's own color and strength
                static const float kOrange[3] = { 1.0f, 0.6f, 0.3f };
                float u = (g_cyc_elev + 3.f) / 15.f;
                u = u < 0.f ? 0.f : (u > 1.f ? 1.f : u);
                for (int i = 0; i < 3; ++i) w.rgb[i] = Mix(kOrange[i], o.rgb[i], u);
                w.w = o.w * (0.1f + 0.9f * u);
            }
        }
        return w;
    }
    void ApplySun() {
        const ULONGLONG now = GetTickCount64();
        const int idx = g_look_id ? g_look_idx : BaseSlot();
        uint64_t asset; int weight;
        if (idx < 0 || !recorder::SlotInfo(idx, asset, weight) || !asset || weight <= 0) return;
        if (g_look_id ? now - g_look_at < 500 : now - g_base_seen_at.load() < 900) return;   // let a fresh slot finish loading
        // ... and really wait for it: until the game's parser has copied the asset's own values into the slot, the slot holds the "Default Atmosphere", and reading it now would
        // keep that as the sky's "own" values for the session while the parser later overwrites our edits. (Falls back to the timing above when the parser is not hooked.)
        ULONGLONG parsed_at = 0;
        if (recorder::ParseHooked()) {
            parsed_at = recorder::SlotParsedTick(idx, asset);
            if (!parsed_at || now - parsed_at < 150) return;
        }
        g_cur_mod = atmpacks::IsMod(asset);
        // what the player set by hand always applies; what the cycle adds (the steered sun, the random variations) only to the game's own skies
        bool any = false;
        for (int s = kDir; s < kSettingCount; ++s) if (s != kCycle && s != kCycleAtm) any = any || g_set[s];
        any = any || (g_set[kCycle] > 0 && !g_cur_mod);
        bool changed = idx != g_edit_idx || asset != g_edit_asset || g_cyc_seen != g_cyc_step || parsed_at != g_edit_parsed;   // a new parse of the slot wipes our edits
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
        if (it == g_orig.end()) {
            Sun o; memcpy(o.rgb, cur, 12); o.w = cur[3]; o.rot = cur[4]; o.elev = cur[5]; it = g_orig.emplace(asset, o).first;
            float ex = 0.f, hz = 0.f, fc[3] = {};
            recorder::SlotFloats(idx, kOffExposure, &ex, 1, false); recorder::SlotFloats(idx, kOffHaze, &hz, 1, false); recorder::SlotFloats(idx, kOffFogColor, fc, 3, false);
            LOG("sky: own values of %016llX kept (slot %d, parsed %lld ms ago): sun %.2f %.2f %.2f x%.2f rot %.0f elev %.0f, exposure %.2f, haze %.3f, fog color %.2f %.2f %.2f", (unsigned long long)asset, idx,
                parsed_at ? (long long)(now - parsed_at) : -1LL, cur[0], cur[1], cur[2], cur[3], cur[4], cur[5], ex, hz, fc[0], fc[1], fc[2]);
        }
        auto fit = g_orig_fog.find(asset);
        float fcur[6];
        if (fit == g_orig_fog.end() && recorder::SlotFog(idx, fcur, false)) { Fog f; memcpy(f.d, fcur, sizeof(fcur)); fit = g_orig_fog.emplace(asset, f).first; }
        auto iit = g_orig_image.find(asset);
        if (iit == g_orig_image.end()) {
            Image im{};
            if (recorder::SlotFloats(idx, kOffFogDist, im.fogdist, 4, false) && recorder::SlotFloats(idx, kOffExposure, &im.exposure, 1, false) &&
                recorder::SlotFloats(idx, kOffFilter, im.filter, 3, false) && recorder::SlotFloats(idx, kOffHaze, &im.haze, 1, false) &&
                recorder::SlotFloats(idx, kOffClouds, &im.clouds, 1, false) && recorder::SlotFloats(idx, kOffFogColor, im.fogcolor, 3, false) &&
                recorder::SlotFloats(idx, kOffAmbient, im.ambient, 4, false))
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
            // The sun is never blended toward the next phase (blending its color and strength put the wrong light in the wrong place: a red light overhead
            // before dawn). Each phase brings its own sun (Wanted); only fog, haze, exposure, filter and fog color are blended.
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
        const bool nb = bt > 0.f && g_tgt_valid;   // toward the next phase: its random fog and haze blend in with the colors
        const int fogsel = EffSel(kFog);
        float fmul = 1.f;
        if (fogsel > 0 && fogsel < (int)(sizeof(kFogs) / sizeof(kFogs[0]))) fmul = kFogs[fogsel].mul;
        if (nb && !g_set[kFog]) { const int ns = EffSel(kFog, true); fmul = Mix(fmul, ns > 0 && ns < (int)(sizeof(kFogs) / sizeof(kFogs[0])) ? kFogs[ns].mul : 1.f, bt); }
        if (have_fog) {
            float fb[6];
            for (int i = 0; i < 6; ++i) fb[i] = ofog.d[i] * fmul;
            recorder::SlotFog(idx, fb, true);
        }
        // image controls
        if (have_im) {
            const Image& o = oim;
            auto pick = [](int sel, int n) { return sel > 0 && sel < n ? sel : 0; };
            const int dsel = pick(EffSel(kFogDist), (int)(sizeof(kFogDists) / sizeof(kFogDists[0])));
            float dmul = kFogDists[dsel].mul;
            if (nb && !g_set[kFogDist]) dmul = Mix(dmul, kFogDists[pick(EffSel(kFogDist, true), (int)(sizeof(kFogDists) / sizeof(kFogDists[0])))].mul, bt);
            float db[4];
            for (int i = 0; i < 4; ++i) db[i] = o.fogdist[i] * dmul;
            recorder::SlotFloats(idx, kOffFogDist, db, 4, true);
            const int hsel = pick(EffSel(kHaze), (int)(sizeof(kHazes) / sizeof(kHazes[0])));
            float haze = hsel ? kHazes[hsel].value : o.haze;
            if (nb && !g_set[kHaze]) { const int hn = pick(EffSel(kHaze, true), (int)(sizeof(kHazes) / sizeof(kHazes[0]))); haze = Mix(haze, hn ? kHazes[hn].value : o.haze, bt); }
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
            for (int i = 0; i < 3; ++i) fogc[i] = gsel ? Mix(o.fogcolor[i], kFogColors[gsel].rgb[i], AmountOf(g_set[kFogColorAmt])) : o.fogcolor[i];
            recorder::SlotFloats(idx, kOffFogColor, fogc, 3, true);
            const int amb = g_set[kAmbient];
            if (amb > 0 && amb < (int)(sizeof(kAmbients) / sizeof(kAmbients[0]))) {   // the fill light: the chosen color (white unless AMBIENT COLOR is set) at the chosen strength
                const int ac = g_set[kAmbientColor];
                const float* c = ac > 0 && ac < (int)(sizeof(kColors) / sizeof(kColors[0])) ? kColors[ac].rgb : nullptr;
                float f4[4] = { c ? c[0] : 1.f, c ? c[1] : 1.f, c ? c[2] : 1.f, kAmbients[amb].v };
                recorder::SlotFloats(idx, kOffAmbient, f4, 4, true);
                g_amb_dirty = true;
            } else if (g_amb_dirty) {   // back to the sky's own values
                float f4[4];
                std::memcpy(f4, o.ambient, sizeof(f4));
                recorder::SlotFloats(idx, kOffAmbient, f4, 4, true);
                g_amb_dirty = false;
            }
        }
        recorder::Recompute();
        g_cyc_seen = g_cyc_step;
        g_edit_idx = idx; g_edit_asset = asset; g_edit_parsed = parsed_at; g_edit_touched = any || g_edit_touched;
        for (int s = kDir; s < kSettingCount; ++s) g_edit_seen[s] = g_set[s];
        if (!g_cyc_sun_on) LOG("sky: edits on slot %d (%016llX): sun rgb %.2f %.2f %.2f intensity %.2f rotation %.1f elevation %.1f, fog x%.2f, distance/haze/exposure/filter/clouds %d/%d/%d/%d/%d",
            idx, (unsigned long long)asset, w.rgb[0], w.rgb[1], w.rgb[2], w.w, w.rot, w.elev, fmul, g_set[kFogDist].load(), g_set[kHaze].load(), g_set[kExposure].load(), g_set[kFilter].load(), g_set[kClouds].load());
    }

    void SaveIni() {
        const std::string p = IniPath();
        if (p.empty()) return;
        for (int s = 0; s < kSettingCount; ++s) WritePrivateProfileStringA("sky", kKeys[s], std::to_string(g_set[s].load()).c_str(), p.c_str());
        WritePrivateProfileStringA("sky", "mode", std::to_string(g_saved_mode.load()).c_str(), p.c_str());
        WritePrivateProfileStringA("sky", "seamless", g_seamless.load() ? "1" : "0", p.c_str());
        { std::lock_guard<std::mutex> lk(g_ver_mu); for (int i = 0; i < 5; ++i) WritePrivateProfileStringA("version", kVerKeys[i], g_ver[i].c_str(), p.c_str()); }
    }
}

    void Load() {
        EnsureInit();
        const std::string p = IniPath();
        for (int s = 0; s < kSettingCount; ++s) g_set[s] = p.empty() ? 0 : (int)GetPrivateProfileIntA("sky", kKeys[s], 0, p.c_str());
        g_saved_mode = p.empty() ? -1 : (int)GetPrivateProfileIntA("sky", "mode", -1, p.c_str());
        if (!p.empty()) for (int i = 0; i < 5; ++i) { char buf[256] = {}; GetPrivateProfileStringA("version", kVerKeys[i], "", buf, sizeof(buf), p.c_str()); std::lock_guard<std::mutex> lk(g_ver_mu); g_ver[i] = buf; }
        if (g_saved_mode < 0 || g_saved_mode > 6) g_saved_mode = -1;
        if (!p.empty() && GetPrivateProfileIntA("sky", "seamless", 0, p.c_str()) != 0) { seamless::SetEnabled(true); g_seamless = seamless::Enabled(); }
        if (!p.empty()) {
            char pb[32] = {};
            GetPrivateProfileStringA("cycle", "phase", "", pb, sizeof(pb), p.c_str());
            const double ph = pb[0] ? atof(pb) : -1.0;
            g_saved_phase = ph >= 0.0 && ph < 1.0 ? ph : -1.0;
            g_saved_overcast = GetPrivateProfileIntA("cycle", "overcast", 0, p.c_str()) != 0;
        }
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
        if (g_set[kCycleAtm] < 0 || g_set[kCycleAtm] > 1) g_set[kCycleAtm] = 0;
        if (g_set[kAmbient] < 0 || g_set[kAmbient] >= (int)(sizeof(kAmbients) / sizeof(kAmbients[0]))) g_set[kAmbient] = 0;
        if (g_set[kAmbientColor] < 0 || g_set[kAmbientColor] >= (int)(sizeof(kColors) / sizeof(kColors[0]))) g_set[kAmbientColor] = 0;
        if (g_set[kColorAmt] < 0 || g_set[kColorAmt] >= (int)(sizeof(kAmounts) / sizeof(kAmounts[0]))) g_set[kColorAmt] = 0;
        if (g_set[kFogColorAmt] < 0 || g_set[kFogColorAmt] >= (int)(sizeof(kAmounts) / sizeof(kAmounts[0]))) g_set[kFogColorAmt] = 0;
        if (g_set[kClouds] < 0 || g_set[kClouds] >= (int)(sizeof(kCloudsChoices) / sizeof(kCloudsChoices[0]))) g_set[kClouds] = 0;
        LOG("sky: settings style %d variant %d dir %d height %d color %d bright %d fog %d", g_set[kStyle].load(), g_set[kVariant].load(), g_set[kDir].load(), g_set[kHeight].load(), g_set[kColor].load(), g_set[kBright].load(), g_set[kFog].load());
    }
    int Get(Setting s) { return g_set[s]; }
    int SavedMode() { return g_saved_mode; }
    std::string Version(int slot) {
        if (slot < 0 || slot > 4) return "";
        std::lock_guard<std::mutex> lk(g_ver_mu);
        return g_ver[slot];
    }
    void SetVersion(int slot, const std::string& name) {
        if (slot < 0 || slot > 4) return;
        { std::lock_guard<std::mutex> lk(g_ver_mu); g_ver[slot] = name; }
        g_failed_id = 0;   // a new pick gets its tries again
        g_dirty_ini = true;
        g_dirty_at = GetTickCount64();
    }
    void SaveMode(int mode) { g_saved_mode = mode; g_mode_pending = false; g_dirty_ini = true; g_dirty_at = GetTickCount64(); }
    void Set(Setting s, int value) {
        EnsureInit();
        g_set[s] = value;
        if (s == kStyle) g_set[kVariant] = 0;
        if (s == kStyle || s == kVariant) g_failed_id = 0;   // a new pick gets its tries again
        g_dirty_ini = true;
        g_dirty_at = GetTickCount64();
    }
    void ResetAll() { for (int s = 0; s < kSettingCount; ++s) g_set[s] = 0; { std::lock_guard<std::mutex> lk(g_ver_mu); for (auto& v : g_ver) v.clear(); } g_dirty_ini = true; g_dirty_at = GetTickCount64(); }

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
            case kCycleAtm: return 2;
            case kColorAmt: case kFogColorAmt: return AmountCount();
            case kAmbient: return AmbientCount();
            case kAmbientColor: return ColorCount();
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
            case kCycleAtm: { static const char* const kOffOn[] = { "OFF", "ON" }; l = kOffOn; break; }
            case kColorAmt: case kFogColorAmt: l = AmountLabels(); break;
            case kAmbient: l = AmbientLabels(); break;
            case kAmbientColor: l = ColorLabels(); break;
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
    bool SmoothFade() { return g_smooth_fade; }
    void SetSmoothFade(bool on) { g_smooth_fade = on; LOG("sky: smooth fade %s", on ? "on (the cycle swallows the lighting transition's fade)" : "off"); }
    int CycleCount() { EnsureInit(); return (int)g_cycle_labels.size(); }       const char* const* CycleLabels() { EnsureInit(); return g_cycle_labels.data(); }
    bool CycleSyncMode(int mode) {
        if (g_set[kCycle] <= 0) return true;
        g_cyc_overcast = mode == 3 || mode == 6;   // picking Overcast starts an overcast day
        g_cyc_look = 0; g_pick_nm = -1;
        g_phase = PhaseOfMode(mode); g_prev_phase = g_phase; g_cyc_started = true; g_cyc_last = GetTickCount64(); g_hold = false;
        return true;
    }
    int AmbientCount() { EnsureInit(); return (int)g_ambient_labels.size(); }   const char* const* AmbientLabels() { EnsureInit(); return g_ambient_labels.data(); }
    bool Seamless() { return g_seamless; }
    void SetSeamless(bool on) {
        seamless::SetEnabled(on);
        g_seamless = on && seamless::Enabled();   // stays off when the hook could not be installed
        g_dirty_ini = true; g_dirty_at = GetTickCount64();
    }
    int AmountCount() { EnsureInit(); return (int)g_amount_labels.size(); }     const char* const* AmountLabels() { EnsureInit(); return g_amount_labels.data(); }
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
        atmpacks::Start();   // once, at the first tick: reads the atmosphere mods in the Atmospheres folder on a background thread (well before the world loads)
        const ULONGLONG now = GetTickCount64();
        if (g_dirty_ini && now - g_dirty_at > 1500) { g_dirty_ini = false; SaveIni(); }
        const int mode = recorder::CurrentMode();
        if (mode < 0 || !recorder::Manager()) { g_world_up = false; return; }
        if (!g_world_up) {   // a (re)load: every slot we knew is gone
            g_world_up = true;
            atmpacks::Deselect();   // a (re)load puts the game's own atmosphere back: so the game's own files too
            g_look_id = 0; g_look_idx = -1; g_edit_idx = -1; g_edit_asset = 0; g_edit_touched = false; g_load_req_at = 0; g_held.clear(); g_failed_id = 0; g_load_tries = 0;
            for (int s = 0; s < kSettingCount; ++s) g_edit_seen[s] = -1;
            g_hold_until = now + 3000;
            g_world_up_at = now; g_cyc_started = false; g_ready_since = 0;
            return;
        }
        if (now < g_hold_until) return;
        UpdateReadiness(now);
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
