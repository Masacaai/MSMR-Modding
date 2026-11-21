#include "presets.h"

#include "log.h"
#include "settings.h"

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

namespace presets {
namespace {
    struct Preset {
        std::string name;
        std::map<std::string, std::string> kv;   // upper-cased keys and values
    };
    std::vector<Preset> g_list;
    std::vector<std::string> g_name_store;
    std::vector<const char*> g_names;
    std::mutex g_mu;
    int g_cus[settings::kRowCount];   // the player's own settings, kept while a preset is shown (choice indexes)
    bool g_have_cus = false;

    std::string Upper(std::string s) { for (auto& c : s) c = (char)toupper((unsigned char)c); return s; }
    std::string Trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    std::filesystem::path Root() {
        char docs[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs)) || !docs[0]) return {};
        return std::filesystem::path(docs) / "Marvel's Spider-Man Remastered" / "HyperSense";
    }
    std::filesystem::path Dir() { const auto r = Root(); return r.empty() ? r : r / "Presets"; }
    std::filesystem::path CustomPath() { const auto r = Root(); return r.empty() ? r : r / "custom.ini"; }

    // Rows a preset covers: every row with choices except MESSAGES (a personal preference).
    bool InPreset(int row) { return settings::Def(row).labels && row != settings::kMessages; }

    // Looks that ship with the mod, written once when the Presets folder does not exist yet. A key left out means the row's default.
    // (DISTORTION and FEAR are not used: the 2026-11-20 crash happened with DISTORTION and is not understood yet.)
    struct Shipped { const char* file; const char* text; };
    const Shipped kShipped[] = {
        { "Default.ini", "[preset]\n" },
        { "Flash.ini", "[preset]\nslow_scale=SLOWER (0.1X)\nhero_speed=3X\nlook=YELLOW\nlife=0.5 S\ninterval=DENSE (0.07 S)\nflicker=HIGH\nblur=4X\nstreaks=EARLIER\n" },
        { "Reverse Flash.ini", "[preset]\nslow_scale=SLOWER (0.1X)\nhero_speed=3X\nlook=RED GLOW\nlife=0.7 S\ninterval=DENSE (0.07 S)\nflicker=HIGH\nblur=4X\nstreaks=EARLIER\n" },
        { "Hologram Trail.ini", "[preset]\nlook=HOLOGRAM\nlife=1.6 S\nfade_curve=LINEAR (1.0)\nalpha=60%\n" },
        { "Scanline Echo.ini", "[preset]\nlook=SCANLINES\nlife=1.2 S\nalpha=80%\nflicker=LOW\n" },
        { "Ghost Train.ini", "[preset]\nlife=3 S\nfade_curve=SLOW (0.8)\nalpha=100%\ninterval=SPARSE (0.12 S)\nflicker=OFF\n" },
        { "Frozen Moment.ini", "[preset]\nslow_scale=SLOWEST (0.001X)\nlife=4 S\nfade_curve=VERY SLOW (0.5)\nalpha=100%\ninterval=FEW (0.2 S)\nflicker=OFF\n" },
        { "Rainbow Burst.ini", "[preset]\nlook=RAINBOW\nlife=0.7 S\ninterval=VERY DENSE (0.05 S)\nalpha=90%\nflicker=LOW\n" },
        { "Matrix.ini", "[preset]\nslow_scale=SLOWER (0.1X)\nhero_speed=1.5X\nlook=GREEN\nlife=1.2 S\nfade_curve=GENTLE (1.3)\nflicker=MEDIUM\n" },
        { "Shadow Clones.ini", "[preset]\nlook=BLACK\nlife=1.6 S\nalpha=90%\nflicker=OFF\ninterval=SPARSE (0.12 S)\n" },
        { "Rising Spirits.ini", "[preset]\nlook=WHITE\nlife=2 S\nalpha=60%\ndrift=MEDIUM\nsize=GROW\n" },
    };

    bool Parse(const std::filesystem::path& p, Preset* out) {
        std::ifstream f(p);
        if (!f) return false;
        out->name = p.stem().string();
        out->kv.clear();
        std::string line;
        while (std::getline(f, line)) {
            const size_t eq = line.find('=');
            if (eq == std::string::npos || line[0] == ';' || line[0] == '[') continue;
            const std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
            if (!k.empty()) out->kv[Upper(k)] = Upper(v);
        }
        return true;
    }

    int FindLabel(int row, const std::string& want) {
        const settings::RowDef& d = settings::Def(row);
        for (int i = 0; i < d.count; ++i) if (Upper(d.labels[i]) == want) return i;
        return -1;
    }

    // The choice a preset wants for a row: its named label, else the row's default (-1: the label is unknown, treat as default).
    int Wanted(const Preset& p, int row) {
        const auto it = p.kv.find(Upper(settings::Def(row).key));
        if (it == p.kv.end()) return settings::Def(row).def;
        const int idx = FindLabel(row, it->second);
        return idx >= 0 ? idx : settings::Def(row).def;
    }

    int CurrentNoLock() {
        for (size_t i = 0; i < g_list.size(); ++i) {
            bool ok = true;
            for (int r = 0; r < settings::kRowCount && ok; ++r) if (InPreset(r)) ok = settings::Choice(r) == Wanted(g_list[i], r);
            if (ok) return (int)i + 1;
        }
        return 0;
    }

    void SaveCustomFile() {
        const auto p = CustomPath();
        if (p.empty()) return;
        std::ofstream f(p);
        if (!f) return;
        f << "[custom]\n";
        for (int r = 0; r < settings::kRowCount; ++r) if (InPreset(r)) f << settings::Def(r).key << "=" << g_cus[r] << "\n";
    }
    void LoadCustomFile() {
        const auto p = CustomPath();
        std::error_code ec;
        if (p.empty() || !std::filesystem::exists(p, ec)) return;
        const std::string ps = p.string();
        for (int r = 0; r < settings::kRowCount; ++r) if (InPreset(r)) g_cus[r] = (int)GetPrivateProfileIntA("custom", settings::Def(r).key, settings::Def(r).def, ps.c_str());
        g_have_cus = true;
    }
    void RememberIfCustom() {
        if (CurrentNoLock() != 0) return;
        for (int r = 0; r < settings::kRowCount; ++r) if (InPreset(r)) g_cus[r] = settings::Choice(r);
        g_have_cus = true;
        SaveCustomFile();
    }
}

    void Init() {
        const auto dir = Dir();
        if (dir.empty()) return;
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec)) {
            std::filesystem::create_directories(dir, ec);
            for (const auto& s : kShipped) { std::ofstream f(dir / s.file); if (f) f << s.text; }
        }
        LoadCustomFile();
        Rescan();
    }

    void Rescan() {
        std::lock_guard<std::mutex> lk(g_mu);
        g_list.clear();
        const auto dir = Dir();
        std::error_code ec;
        if (!dir.empty()) {
            for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
                if (!e.is_regular_file() || Upper(e.path().extension().string()) != ".INI") continue;
                Preset p;
                if (Parse(e.path(), &p)) g_list.push_back(std::move(p));
            }
        }
        std::sort(g_list.begin(), g_list.end(), [](const Preset& a, const Preset& b) { return Upper(a.name) < Upper(b.name); });
        if ((int)g_list.size() > 30) g_list.resize(30);   // the menu's per-row choice limit is 32
        g_name_store.assign(1, "CUSTOM");
        for (const auto& p : g_list) g_name_store.push_back(Upper(p.name));
        g_names.clear();
        for (const auto& s : g_name_store) g_names.push_back(s.c_str());
        LOG("presets: %d found", (int)g_list.size());
    }

    int Count() { std::lock_guard<std::mutex> lk(g_mu); return (int)g_list.size(); }
    const char* const* Names() { std::lock_guard<std::mutex> lk(g_mu); return g_names.data(); }
    int Current() { std::lock_guard<std::mutex> lk(g_mu); return CurrentNoLock(); }

    bool RestoreCustom() {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_have_cus) return false;
        for (int r = 0; r < settings::kRowCount; ++r) if (InPreset(r)) settings::SetChoice(r, g_cus[r]);
        LOG("presets: restored CUSTOM");
        return true;
    }

    void Apply(int index) {
        std::lock_guard<std::mutex> lk(g_mu);
        if (index < 1 || index > (int)g_list.size()) return;
        RememberIfCustom();
        const Preset& p = g_list[index - 1];
        for (const auto& kv : p.kv) {   // note typos in the file
            bool known = false;
            for (int r = 0; r < settings::kRowCount; ++r) if (InPreset(r) && Upper(settings::Def(r).key) == kv.first) { known = true; if (FindLabel(r, kv.second) < 0) LOG("presets: %s: no '%s' choice for %s", p.name.c_str(), kv.second.c_str(), kv.first.c_str()); }
            if (!known) LOG("presets: %s: unknown key '%s'", p.name.c_str(), kv.first.c_str());
        }
        for (int r = 0; r < settings::kRowCount; ++r) if (InPreset(r)) settings::SetChoice(r, Wanted(p, r));
        LOG("presets: applied %s", p.name.c_str());
    }
}
