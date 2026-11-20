#include "menu.h"

#include "log.h"
#include "atmpacks.h"
#include "presets.h"
#include "recorder.h"
#include "sky.h"

#include "../../../ModMenu/modmenu.h"

#include <Windows.h>
#include <atomic>

// The MANHATTAN SKIES tab of the shared MODS menu (pause menu > MODS; ModMenu/modmenu.cpp hosts or joins):
//   TIME AND SKY: TIME OF DAY (the game's lighting mode), SKY STYLE and SKY VARIANT (any of the game's 124 atmospheres on top of it)
//   SUN AND FOG: SUN DIRECTION, SUN HEIGHT, SUN COLOR, SUN BRIGHTNESS, FOG DENSITY, FOG DISTANCE, FOG COLOR, HAZE (edits to the active atmosphere's sun and fog)
//   IMAGE: EXPOSURE, COLOR FILTER, CLOUDS (found with tools/fieldprobe.py)
namespace menu {
namespace {
    // Mode indexes from the S1 sweep (docs/RECON.md Part 4). Index 4 is the game's kBlackout (the opening mission's dawn); which of 1/5 and 3/6
    // is the rainy one is still open.
    const char* const kChoices[] = { "DAY", "EARLY MORNING", "SUNSET", "NIGHT", "NIGHT (VARIANT)", "OVERCAST", "OVERCAST (VARIANT)" };
    const int kModes[] = { 0, 4, 2, 5, 1, 6, 3 };
    constexpr int kChoiceCount = 7, kDefault = 0;
    enum { kHeadSky = 0, kTimeId, kStyleId, kVariantId, kHeadSun, kDirId, kHeightId, kColorId, kBrightId, kFogId, kFogDistId, kHazeId, kHeadImage, kExposureId, kFilterId, kCloudsId, kFogColorId, kPresetId, kCycleId, kVersionId, kCycleAtmId, kColorAmtId, kFogColorAmtId, kAmbientId, kAmbientColorId };

    // After applying, the row keeps showing the pick until the game's mode field catches up (a few frames).
    std::atomic<int> g_pending{ -1 };
    std::atomic<ULONGLONG> g_pending_at{ 0 };
    std::atomic<uint32_t> g_status{ 1 };
    std::atomic<uint64_t> g_changes{ 0 };
    std::atomic<bool> g_rebuild{ false };
    std::atomic<bool> g_was_custom{ true };
    std::atomic<bool> g_pick_custom{ false };   // the player stepped to CUSTOM on the PRESET row: keep showing it until something else changes

    // The save row exists only while the live settings match no preset; rebuild the rows when that flips.
    void NoteCustomness() {
        g_pick_custom = false;
        const bool custom = presets::Current() == 0;
        if (g_was_custom.exchange(custom) != custom) g_rebuild = true;
    }

    int ChoiceNow() {
        const int mode = recorder::CurrentMode();
        const int pending = g_pending;
        if (pending >= 0 && GetTickCount64() - g_pending_at < 3000 && kModes[pending] != mode) return pending;
        for (int k = 0; k < kChoiceCount; ++k) if (kModes[k] == mode) return k;
        return kDefault;
    }
    // The game fades the screen for a lighting change and closes every menu during the fade (the pause menu's per-frame update asks Flash to
    // CloseMenu while the world is in transition); KeepMenuOpen swallows that request, so the menu stays and the sky changes with the fade.
    void Choose(int choice) {
        if (choice < 0 || choice >= kChoiceCount) return;
        ++g_changes;
        sky::SaveMode(kModes[choice]);   // remembered across launches (the game would otherwise load its own last-used time)
        if (kModes[choice] == recorder::CurrentMode()) { LOG("menu: TIME OF DAY already %s", kChoices[choice]); return; }
        g_pending = choice;
        g_pending_at = GetTickCount64();
        modmenu::KeepMenuOpen(6000);
        if (sky::SmoothFade()) modmenu::SuppressFade(5000);   // no black fade for the switch, like the dynamic cycle's
        const bool ok = recorder::ApplyMode(kModes[choice]);   // menu callbacks run on the game thread, like the game's own option
        LOG("menu: TIME OF DAY -> %s (mode %d)%s", kChoices[choice], kModes[choice], ok ? "" : " FAILED (no world)");
    }

    int VersionSlot() { return atmpacks::SlotOfMode(kModes[ChoiceNow()]); }

    int Rows(void*, int* ids, int max) {
        int n = 0;
        auto add = [&](int id) { if (n < max) ids[n++] = id; };
        add(kPresetId);
        add(kHeadSky); add(kTimeId); if (atmpacks::LabelCount(VersionSlot()) > 1 || atmpacks::Blocked()) add(kVersionId); add(kCycleId); if (sky::Get(sky::kCycle) > 0 && atmpacks::AnyMods()) add(kCycleAtmId); add(kStyleId);
        if (sky::Get(sky::kStyle) > 0) add(kVariantId);
        add(kHeadSun); add(kDirId); add(kHeightId); add(kColorId); add(kColorAmtId); add(kBrightId); add(kFogId); add(kFogDistId); add(kFogColorId); add(kFogColorAmtId); add(kHazeId);
        add(kHeadImage); add(kExposureId); add(kFilterId); add(kCloudsId); add(kAmbientId); add(kAmbientColorId);
        return n;
    }
    void Row(void*, int id, modmenu::Row* out) {
        *out = {};
        switch (id) {
            case kPresetId:
                out->title = "PRESET";
                out->help = "Looks saved as files in the Presets folder (Documents, Marvel's Spider-Man Remastered, ManhattanSkies). Share a preset by copying its file. CUSTOM means your own unsaved settings.";
                out->choices = presets::Names(); out->choice_count = presets::Count() + 1; break;
            case kHeadSky: out->title = "TIME AND SKY"; out->heading = 1; break;
            case kTimeId:
                out->title = "TIME OF DAY";
                out->help = "The game's own lighting: sky, sun and the lights in the world. Includes Early Morning and the overcast looks the game's option never offers. The screen fades briefly while it changes.";
                out->choices = kChoices; out->choice_count = kChoiceCount; break;
            case kVersionId:
                if (atmpacks::Blocked()) {   // mods are in the folder, but there is no Overstrike archive to borrow
                    static const char* const kNeeds[] = { "NEEDS AN OVERSTRIKE MOD" };
                    out->title = "ATMOSPHERE";
                    out->help = "Atmosphere mods were found in your Atmospheres folder, but they can only be used when at least one mod that replaces game files (a .smpcmod, for example) is installed with Overstrike. Install any such mod, press Install mods, and start the game again.";
                    out->choices = kNeeds; out->choice_count = 1;
                    break;
                }
                out->title = "ATMOSPHERE";
                out->help = "Which version of this time of day to use: the game's own, or an atmosphere mod you put in the Atmospheres folder (Documents, Marvel's Spider-Man Remastered, ManhattanSkies). It changes the sky and its lighting together with the time of day above. One choice is kept for each time of day, and the dynamic cycle uses it too.";
                out->choices = atmpacks::Labels(VersionSlot()); out->choice_count = atmpacks::LabelCount(VersionSlot()); break;
            case kCycleId:
                out->title = "DYNAMIC CYCLE";
                out->help = "Plays a full day: Early Morning, Day, Sunset and Night, over and over. The value is how long one full day takes. The sun travels across the sky, and each phase may use a random sky and a random sun strength, fog and haze (some days are overcast). The day stands still, and no switch happens, during crimes, hideouts, missions and fights. The screen does not fade: the colors shift toward the next sky shortly before it arrives. Picking a time of day sets where the cycle continues from. Takes over SUN DIRECTION and SUN HEIGHT while on, and your own SKY STYLE wins over the random skies. Starts a few seconds after the game is playable, and picks up where it stopped last time.";
                out->choices = sky::CycleLabels(); out->choice_count = sky::CycleCount(); break;
            case kCycleAtmId:
                out->title = "CYCLE ATMOSPHERES";
                out->help = "ON: the dynamic cycle also picks among the atmosphere mods in your Atmospheres folder for each time of day, next to the game's own looks, each as the mod ships it: an atmosphere mod keeps all of its author's values (the cycle does not steer its sun or vary its fog, haze or sun strength), while rows you set yourself still apply. An ATMOSPHERE you pick yourself for a time of day still wins over the random pick.";
                { static const char* const kOffOn[] = { "OFF", "ON" }; out->choices = kOffOn; out->choice_count = 2; }
                break;
            case kStyleId:
                out->title = "SKY STYLE";
                out->help = "Puts a different sky on top of the time of day: AUTO leaves the game's own. The world lighting stays the time of day's, so you can mix, for example Day lighting under a Sunset sky.";
                out->choices = sky::StyleLabels(); out->choice_count = sky::StyleCount(); break;
            case kVariantId:
                out->title = "SKY VARIANT";
                out->help = "Which sky of the chosen style. They come from the game's missions and cutscenes, so a few look odd outside them.";
                out->choices = sky::VariantLabels(sky::Get(sky::kStyle)); out->choice_count = sky::VariantCount(sky::Get(sky::kStyle)); break;
            case kHeadSun: out->title = "SUN AND FOG"; out->heading = 1; break;
            case kDirId:
                out->title = "SUN DIRECTION";
                out->help = "Turns the sun around the sky (degrees). Changes the light and the shadows; AUTO is the sky's own.";
                out->choices = sky::DirLabels(); out->choice_count = sky::DirCount(); break;
            case kHeightId:
                out->title = "SUN HEIGHT";
                out->help = "How high the sun is (degrees above the horizon; below 0 is under it). AUTO is the sky's own.";
                out->choices = sky::HeightLabels(); out->choice_count = sky::HeightCount(); break;
            case kColorId:
                out->title = "SUN COLOR";
                out->help = "Tints the sunlight. Parts of the city keep the old color until the lighting reloads (change the time of day to refresh).";
                out->choices = sky::ColorLabels(); out->choice_count = sky::ColorCount(); break;
            case kColorAmtId:
                out->title = "SUN COLOR AMOUNT";
                out->help = "How much of the SUN COLOR you picked replaces the sky's own sunlight color: AUTO is all of it, 10% is a faint tint. Has no effect while SUN COLOR is AUTO.";
                out->choices = sky::AmountLabels(); out->choice_count = sky::AmountCount(); break;
            case kFogColorAmtId:
                out->title = "FOG COLOR AMOUNT";
                out->help = "How much of the FOG COLOR you picked replaces the fog's own color: AUTO is all of it, 10% is a faint tint. Has no effect while FOG COLOR is AUTO.";
                out->choices = sky::AmountLabels(); out->choice_count = sky::AmountCount(); break;
            case kAmbientId:
                out->title = "AMBIENT LIGHT";
                out->help = "A fill light that lifts the shadows and unlit surfaces, independent of the sun. AUTO is the sky's own (usually none). Experimental: the scale is a first guess and the strongest steps can wash the picture out. Pair it with AMBIENT COLOR.";
                out->choices = sky::AmbientLabels(); out->choice_count = sky::AmbientCount(); break;
            case kAmbientColorId:
                out->title = "AMBIENT COLOR";
                out->help = "The tint of the AMBIENT LIGHT fill (AUTO is white). Has no effect while AMBIENT LIGHT is AUTO.";
                out->choices = sky::ColorLabels(); out->choice_count = sky::ColorCount(); break;
            case kBrightId:
                out->title = "SUN BRIGHTNESS";
                out->help = "Scales the strength of the sun relative to the sky's own. OFF turns the sunlight off completely (no direct sun, no sun shadows).";
                out->choices = sky::BrightLabels(); out->choice_count = sky::BrightCount(); break;
            case kFogId:
                out->title = "FOG DENSITY";
                out->help = "How thick the fog is, relative to the sky's own. NONE clears it completely; some skies from the game's cutscenes are very foggy.";
                out->choices = sky::FogLabels(); out->choice_count = sky::FogCount(); break;
            case kFogDistId:
                out->title = "FOG DISTANCE";
                out->help = "How far away the fog starts to build up, relative to the sky's own. Lower brings the fog in close; higher pushes it far away.";
                out->choices = sky::FogDistLabels(); out->choice_count = sky::FogDistCount(); break;
            case kFogColorId:
                out->title = "FOG COLOR";
                out->help = "The color of the fog and the distant haze. Subtle unless the fog is thick; try it with FOG DENSITY or HAZE raised.";
                out->choices = sky::FogColorLabels(); out->choice_count = sky::FogColorCount(); break;
            case kHazeId:
                out->title = "HAZE";
                out->help = "A bright veil that grows with distance: the far city fades into white. NONE keeps the skyline crisp.";
                out->choices = sky::HazeLabels(); out->choice_count = sky::HazeCount(); break;
            case kHeadImage: out->title = "IMAGE"; out->heading = 1; break;
            case kExposureId:
                out->title = "EXPOSURE";
                out->help = "Makes the whole picture brighter or darker, in photographic stops (+/-1 doubles or halves the light). Relative to the sky's own.";
                out->choices = sky::ExposureLabels(); out->choice_count = sky::ExposureCount(); break;
            case kFilterId:
                out->title = "COLOR FILTER";
                out->help = "Tints the whole picture. NEUTRAL removes the sky's own color grade; AUTO keeps it.";
                out->choices = sky::FilterLabels(); out->choice_count = sky::FilterCount(); break;
            case kCloudsId:
                out->title = "CLOUDS";
                out->help = "Switches the cloud layer on or off. Many skies have none until you turn it on.";
                out->choices = sky::CloudsLabels(); out->choice_count = sky::CloudsCount(); break;
            default: out->title = ""; out->heading = 1; break;
        }
    }
    sky::Setting SettingOf(int id) {
        switch (id) {
            case kStyleId: return sky::kStyle;
            case kVariantId: return sky::kVariant;
            case kDirId: return sky::kDir;
            case kHeightId: return sky::kHeight;
            case kColorId: return sky::kColor;
            case kFogId: return sky::kFog;
            case kFogDistId: return sky::kFogDist;
            case kHazeId: return sky::kHaze;
            case kExposureId: return sky::kExposure;
            case kFilterId: return sky::kFilter;
            case kCloudsId: return sky::kClouds;
            case kCycleId: return sky::kCycle;
            case kCycleAtmId: return sky::kCycleAtm;
            case kFogColorId: return sky::kFogColor;
            case kColorAmtId: return sky::kColorAmt;
            case kFogColorAmtId: return sky::kFogColorAmt;
            case kAmbientId: return sky::kAmbient;
            case kAmbientColorId: return sky::kAmbientColor;
            default: return sky::kBright;
        }
    }
    int Value(void*, int id) {
        if (id == kTimeId) return ChoiceNow();
        if (id == kVersionId) {
            const int slot = VersionSlot();
            const std::string cur = sky::Version(slot);
            for (int i = 1; i < atmpacks::LabelCount(slot); ++i) if (atmpacks::NameAt(slot, i) == cur) return i;
            return 0;
        }
        if (id == kPresetId) return g_pick_custom ? 0 : presets::Current();
        if (id == kHeadSky || id == kHeadSun || id == kHeadImage) return 0;
        return sky::Get(SettingOf(id));
    }
    int Default(void*, int) { return kDefault; }
    void Set(void*, int id, int choice) {
        if (id == kPresetId) { if (choice >= 1) { ++g_changes; g_pick_custom = false; presets::Apply(choice); g_rebuild = true; g_was_custom = false; } else { g_pick_custom = true; if (presets::RestoreCustom()) { g_rebuild = true; g_pick_custom = presets::Current() != 0; g_was_custom = presets::Current() == 0; } } return; }
        if (id == kVersionId) {
            sky::SetVersion(VersionSlot(), choice > 0 ? atmpacks::NameAt(VersionSlot(), choice) : std::string());
            ++g_changes;
            LOG("menu: ATMOSPHERE -> %s", choice > 0 ? atmpacks::NameAt(VersionSlot(), choice).c_str() : "DEFAULT");
            return;
        }
        if (id == kTimeId) {
            Choose(choice);
            g_rebuild = true;   // the ATMOSPHERE row lists the versions of the time of day just picked
            if (!sky::CycleSyncMode(kModes[choice])) { sky::Set(sky::kCycle, 0); g_rebuild = true; }   // overcast is not part of the cycle
            NoteCustomness();
            return;
        }
        if (id == kHeadSky || id == kHeadSun || id == kHeadImage) return;
        sky::Set(SettingOf(id), choice);
        NoteCustomness();
        ++g_changes;
        if (id == kStyleId || id == kCycleId) g_rebuild = true;   // the variant row appears or changes its choices; the cycle's atmosphere toggle appears while the cycle is on
        LOG("menu: row %d -> %d", id, choice);
    }
    void Reset(void*, int id) {
        if (id == kPresetId) return;
        if (id == kVersionId) { sky::SetVersion(VersionSlot(), std::string()); return; }
        if (id == kTimeId) { Choose(kDefault); NoteCustomness(); return; }
        if (id == kHeadSky || id == kHeadSun || id == kHeadImage) return;
        sky::Set(SettingOf(id), 0);
        if (id == kStyleId || id == kCycleId) g_rebuild = true;
        NoteCustomness();
    }
    void ResetAll(void*) { sky::ResetAll(); g_rebuild = true; NoteCustomness(); }   // the sky rows only; the time of day is the game's
    int TakeRebuild(void*) { return g_rebuild.exchange(false) ? 1 : 0; }
    void Closed(void*) { presets::Rescan(); g_rebuild = true; }
    void LogLine(const char* line) { LOG("%s", line); }
}

    uint32_t Install() {
        modmenu::Mod mod{};
        mod.size = sizeof(mod);
        mod.name = "MANHATTAN SKIES";
        mod.rows = &Rows;
        mod.row = &Row;
        mod.value = &Value;
        mod.default_choice = &Default;
        mod.set = &Set;
        mod.reset = &Reset;
        mod.reset_all = &ResetAll;
        mod.take_rebuild = &TakeRebuild;
        mod.closed = &Closed;
        presets::Hooks ph{ kChoices, kChoiceCount, &ChoiceNow, &Choose };
        presets::Init(ph);
        const uint32_t code = modmenu::Register(mod, &LogLine);
        g_status = code;
        if (!code) LOG("menu: MANHATTAN SKIES tab registered with the MODS menu");
        return code;
    }

    void LogStatus() {
        LOG("menu: registered=%d status=%u changes=%llu", g_status == 0 ? 1 : 0, g_status.load(), (unsigned long long)g_changes);
        modmenu::LogStatus();
    }
}
