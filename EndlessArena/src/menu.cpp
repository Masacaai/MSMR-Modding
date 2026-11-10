#include "menu.h"

#include "log.h"
#include "settings.h"

#include "../../ModMenu/modmenu.h"

#include <atomic>
#include <vector>

// The ENDLESS ARENA tab lives in the shared MODS menu (ModMenu/modmenu.cpp): pause menu > MODS > ENDLESS ARENA. This file only hands the
// rows of settings.h and their callbacks to it. The tab-building code that used to be here (SpidyVR's method) moved into ModMenu, so other
// scripts (Manhattan Skies) can share one menu and one set of hooks.
namespace menu {
namespace {
    std::vector<std::vector<const char*>> g_choices;   // per row, pointers into settings::Rows() (fixed after Load)
    std::atomic<uint32_t> g_status{ 1 };                // 1 not registered yet
    std::atomic<uint64_t> g_changes{ 0 };

    int Rows(void*, int* ids, int max) {
        const auto& rows = settings::Rows();
        const int section = settings::CurrentSection();
        int n = 0;
        for (size_t r = 0; r < rows.size() && n < max; ++r)
            if (rows[r].tab == -1 || rows[r].tab == section) ids[n++] = (int)r;
        return n;
    }
    void Row(void*, int id, modmenu::Row* out) {
        const auto& r = settings::Rows()[(size_t)id];
        out->title = r.title.c_str();
        out->help = r.help.c_str();
        out->choices = g_choices[(size_t)id].empty() ? nullptr : g_choices[(size_t)id].data();
        out->choice_count = (int)g_choices[(size_t)id].size();
        out->heading = r.heading() ? 1 : 0;
    }
    int Value(void*, int id) { return settings::Index((size_t)id); }
    int Default(void*, int id) { return settings::Rows()[(size_t)id].def; }
    void Set(void*, int id, int choice) { settings::SetIndex((size_t)id, choice); ++g_changes; }
    void Reset(void*, int id) { settings::ResetRow((size_t)id); ++g_changes; }
    void ResetAll(void*) { settings::ResetAll(); ++g_changes; }
    int TakeRebuild(void*) { return settings::TakeRebuild() ? 1 : 0; }
    void LogLine(const char* line) { LOG("%s", line); }
}

    uint32_t Install() {
        settings::Load();
        const auto& rows = settings::Rows();
        g_choices.assign(rows.size(), {});
        for (size_t r = 0; r < rows.size(); ++r)
            for (const auto& c : rows[r].choices) g_choices[r].push_back(c.c_str());
        modmenu::Mod mod{};
        mod.size = sizeof(mod);
        mod.name = "ENDLESS ARENA";
        mod.rows = &Rows;
        mod.row = &Row;
        mod.value = &Value;
        mod.default_choice = &Default;
        mod.set = &Set;
        mod.reset = &Reset;
        mod.reset_all = &ResetAll;
        mod.take_rebuild = &TakeRebuild;
        const uint32_t code = modmenu::Register(mod, &LogLine);
        g_status = code;
        if (!code) LOG("menu: ENDLESS ARENA tab registered with the MODS menu (%zu rows)", rows.size());
        return code;
    }

    void LogStatus() {
        LOG("menu: registered=%d status=%u changes=%llu", g_status == 0 ? 1 : 0, g_status.load(), (unsigned long long)g_changes);
        modmenu::LogStatus();
    }
}
