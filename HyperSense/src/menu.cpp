#include "menu.h"

#include "log.h"
#include "presets.h"
#include "settings.h"

#include "../../ModMenu/modmenu.h"

#include <atomic>
#include <string>

// The HYPER-SENSE tab of the shared mod menu (ModMenu/modmenu.cpp hosts or joins). One row per settings::Row; a choice is saved to the ini at once.
namespace menu {
namespace {
    constexpr int kPresetId = 100;   // the one row that is not in settings.cpp
    std::atomic<bool> g_rebuild{ false };
    std::atomic<bool> g_pick_custom{ false };   // the player stepped to CUSTOM on the PRESET row: keep showing it until something else changes

    int Rows(void*, int* ids, int max) {
        int n = 0;
        if (n < max) ids[n++] = kPresetId;
        for (int i = 0; i < settings::kRowCount && n < max; i++) ids[n++] = i;
        return n;
    }
    void Row(void*, int id, modmenu::Row* out) {
        *out = {};
        if (id == kPresetId) {
            out->title = "PRESET";
            out->help = "Looks saved as files in the Presets folder (Documents, Marvel's Spider-Man Remastered, HyperSense). Share a preset by copying its file. CUSTOM means your own unsaved settings. A preset sets every row except MESSAGES.";
            out->choices = presets::Names(); out->choice_count = presets::Count() + 1; return;
        }
        if (id < 0 || id >= settings::kRowCount) { out->title = ""; out->heading = 1; return; }
        const settings::RowDef& d = settings::Def(id);
        out->title = d.title;
        out->help = d.help;
        if (!d.labels) { out->heading = 1; return; }
        out->choices = d.labels;
        out->choice_count = d.count;
    }
    int Value(void*, int id) {
        if (id == kPresetId) return g_pick_custom ? 0 : presets::Current();
        return (id >= 0 && id < settings::kRowCount) ? settings::Choice(id) : 0; }
    int Default(void*, int id) { if (id == kPresetId) return 0; return (id >= 0 && id < settings::kRowCount) ? settings::Def(id).def : 0; }
    void Set(void*, int id, int choice) {
        if (id == kPresetId) {
            if (choice >= 1) { g_pick_custom = false; presets::Apply(choice); g_rebuild = true; }
            else { g_pick_custom = true; if (presets::RestoreCustom()) { g_rebuild = true; g_pick_custom = presets::Current() != 0; } }
            return;
        }
        g_pick_custom = false;
        settings::SetChoice(id, choice);
    }
    void Reset(void*, int id) { if (id == kPresetId) return; g_pick_custom = false; if (id >= 0 && id < settings::kRowCount) settings::SetChoice(id, settings::Def(id).def); }
    void ResetAll(void*) { g_pick_custom = false; for (int i = 0; i < settings::kRowCount; i++) if (settings::Def(i).labels) settings::SetChoice(i, settings::Def(i).def); }
    int TakeRebuild(void*) { return g_rebuild.exchange(false) ? 1 : 0; }
    void Closed(void*) { presets::Rescan(); g_rebuild = true; }
    void LogLine(const char* line) { LOG("%s", line); }
}

    uint32_t Install() {
        modmenu::Mod mod{};
        mod.size = sizeof(mod);
        mod.name = "HYPER-SENSE";
        mod.rows = &Rows;
        mod.row = &Row;
        mod.value = &Value;
        mod.default_choice = &Default;
        mod.set = &Set;
        mod.reset = &Reset;
        mod.reset_all = &ResetAll;
        mod.take_rebuild = &TakeRebuild;
        mod.closed = &Closed;
        presets::Init();
        const uint32_t code = modmenu::Register(mod, &LogLine);
        if (!code) LOG("menu: HYPER-SENSE tab registered");
        return code;
    }
}
