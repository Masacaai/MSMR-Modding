#include "sky.h"

#include "log.h"
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
        { kDay, "OPEN WORLD HDR", "atm_ibl_lgt_vn_scorpion_open_world.atmosphere" },
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
    const char* const kKeys[kSettingCount] = { "style", "variant", "dir", "height", "color", "bright", "fog", "fogdist", "haze", "exposure", "filter", "clouds", "fogcolor" };

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
    Sun Wanted(const Sun& o) {
        Sun w = o;
        const int dir = g_set[kDir], height = g_set[kHeight], color = g_set[kColor], bright = g_set[kBright];
        if (dir > 0 && dir <= kDirSteps) w.rot = (float)((dir - 1) * 15);
        if (height > 0 && height <= (int)(sizeof(kHeights) / sizeof(kHeights[0]))) w.elev = (float)kHeights[height - 1];
        if (color > 0 && color < (int)(sizeof(kColors) / sizeof(kColors[0]))) for (int i = 0; i < 3; ++i) w.rgb[i] = kColors[color].rgb[i];
        if (bright > 0 && bright < (int)(sizeof(kBrights) / sizeof(kBrights[0]))) w.w = o.w * kBrights[bright].mul;
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
        bool changed = idx != g_edit_idx || asset != g_edit_asset;
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
        // sun
        const Sun w = Wanted(it->second);
        float buf[6] = { w.rgb[0], w.rgb[1], w.rgb[2], w.w, w.rot, w.elev };
        recorder::SlotSun(idx, buf, true);
        // fog density
        const int fogsel = g_set[kFog];
        float fmul = 1.f;
        if (fogsel > 0 && fogsel < (int)(sizeof(kFogs) / sizeof(kFogs[0]))) fmul = kFogs[fogsel].mul;
        if (fit != g_orig_fog.end()) {
            float fb[6];
            for (int i = 0; i < 6; ++i) fb[i] = fit->second.d[i] * fmul;
            recorder::SlotFog(idx, fb, true);
        }
        // image controls
        if (iit != g_orig_image.end()) {
            const Image& o = iit->second;
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
        g_edit_idx = idx; g_edit_asset = asset; g_edit_touched = any || g_edit_touched;
        for (int s = kDir; s < kSettingCount; ++s) g_edit_seen[s] = g_set[s];
        LOG("sky: edits on slot %d (%016llX): sun rgb %.2f %.2f %.2f intensity %.2f rotation %.1f elevation %.1f, fog x%.2f, distance/haze/exposure/filter/clouds %d/%d/%d/%d/%d",
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
            return;
        }
        if (now < g_hold_until) return;
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
