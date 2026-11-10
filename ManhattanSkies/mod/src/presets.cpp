#include "presets.h"

#include "log.h"
#include "sky.h"

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>

namespace presets {
namespace {
    struct Preset {
        std::string name;
        std::map<std::string, std::string> kv;   // upper-cased values
    };
    Hooks g_hooks{};
    std::vector<Preset> g_list;
    std::vector<std::string> g_name_store;
    std::vector<const char*> g_names;
    std::mutex g_mu;

    std::string Upper(std::string s) { for (auto& c : s) c = (char)toupper((unsigned char)c); return s; }
    std::string Trim(const std::string& s) {
        size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
        return a == std::string::npos ? "" : s.substr(a, b - a + 1);
    }

    std::filesystem::path Dir() {
        char docs[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs)) || !docs[0]) return {};
        return std::filesystem::path(docs) / "Marvel's Spider-Man Remastered" / "ManhattanSkies" / "Presets";
    }

    // Looks that ship with the mod; written once, when the folder does not exist yet.
    struct Shipped { const char* file; const char* text; };
    const Shipped kShipped[] = {
        { "Warm Night.ini", "[preset]\ntime=NIGHT\ncolor=WARM\nfogcolor=WARM\nhaze=LIGHT\nfilter=WARM\n" },
        { "Golden Hour.ini", "[preset]\ntime=SUNSET\ncolor=GOLDEN\nbright=150%\nhaze=LIGHT\nfilter=GOLDEN\n" },
        { "Dreamy Haze.ini", "[preset]\ntime=DAY\nhaze=HEAVY\nfogcolor=PINK\nexposure=+0.5 STOP\nclouds=ON\n" },
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

    int FindLabel(int count, const std::function<const char*(int)>& at, const std::string& want) {
        for (int i = 0; i < count; ++i) if (Upper(at(i)) == want) return i;
        return -1;
    }
    // The player's own unsaved settings, kept while a preset is shown so picking CUSTOM brings them back (and across launches).
    int g_cus[sky::kSettingCount];
    int g_cus_time = -1;
    bool g_have_cus = false;
    std::filesystem::path CustomPath() { const auto d = Dir(); return d.empty() ? d : d.parent_path() / "custom.ini"; }
    void SaveCustomFile() {
        const auto p = CustomPath();
        if (p.empty()) return;
        std::ofstream f(p);
        if (!f) return;
        f << "[custom]\ntime=" << g_cus_time << "\n";
        for (int s = 0; s < sky::kSettingCount; ++s) f << sky::Key((sky::Setting)s) << "=" << g_cus[s] << "\n";
    }
    void LoadCustomFile() {
        const auto p = CustomPath();
        std::error_code ec;
        if (p.empty() || !std::filesystem::exists(p, ec)) return;
        const std::string ps = p.string();
        g_cus_time = (int)GetPrivateProfileIntA("custom", "time", -1, ps.c_str());
        for (int s = 0; s < sky::kSettingCount; ++s) g_cus[s] = (int)GetPrivateProfileIntA("custom", sky::Key((sky::Setting)s), 0, ps.c_str());
        g_have_cus = true;
    }
    int CurrentNoLock();
    void RememberIfCustom() {
        if (CurrentNoLock() != 0) return;
        for (int s = 0; s < sky::kSettingCount; ++s) g_cus[s] = sky::Get((sky::Setting)s);
        g_cus_time = g_hooks.get_time ? g_hooks.get_time() : -1;
        g_have_cus = true;
        SaveCustomFile();
    }
    int TimeIndex(const std::string& want) {
        for (int i = 0; i < g_hooks.time_count; ++i) if (Upper(g_hooks.time_labels[i]) == want) return i;
        return -1;
    }
}

    void Init(const Hooks& hooks) {
        g_hooks = hooks;
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

    namespace { int CurrentNoLock() {
        for (size_t i = 0; i < g_list.size(); ++i) {
            const auto& kv = g_list[i].kv;
            bool ok = true;
            for (int s = 0; s < sky::kSettingCount && ok; ++s) {
                const auto it = kv.find(Upper(sky::Key((sky::Setting)s)));
                const std::string want = it == kv.end() ? "AUTO" : it->second;
                if (s == sky::kVariant && sky::Get(sky::kStyle) == 0) continue;   // no variant under AUTO style
                ok = Upper(sky::LabelAt((sky::Setting)s, sky::Get((sky::Setting)s))) == want;
            }
            const auto t = kv.find("TIME");
            if (ok && t != kv.end() && g_hooks.get_time) ok = Upper(g_hooks.time_labels[g_hooks.get_time()]) == t->second;
            if (ok) return (int)i + 1;
        }
        return 0;
    } }
    int Current() { std::lock_guard<std::mutex> lk(g_mu); return CurrentNoLock(); }

    bool RestoreCustom() {
        std::lock_guard<std::mutex> lk(g_mu);
        if (!g_have_cus) return false;
        sky::ResetAll();
        for (int s = 0; s < sky::kSettingCount; ++s) {
            const int v = g_cus[s];
            if (v > 0 && v < sky::LabelCount((sky::Setting)s)) sky::Set((sky::Setting)s, v);
        }
        if (g_cus_time >= 0 && g_cus_time < g_hooks.time_count && g_hooks.set_time && g_cus_time != g_hooks.get_time()) g_hooks.set_time(g_cus_time);
        LOG("presets: restored CUSTOM");
        return true;
    }

    void Apply(int index) {
        std::lock_guard<std::mutex> lk(g_mu);
        if (index < 1 || index > (int)g_list.size()) return;
        RememberIfCustom();
        const auto& kv = g_list[index - 1].kv;
        sky::ResetAll();
        // style first: the variant's labels depend on it
        for (int s = 0; s < sky::kSettingCount; ++s) {
            const auto it = kv.find(Upper(sky::Key((sky::Setting)s)));
            if (it == kv.end()) continue;
            const int idx = FindLabel(sky::LabelCount((sky::Setting)s), [&](int i) { return sky::LabelAt((sky::Setting)s, i); }, it->second);
            if (idx >= 0) sky::Set((sky::Setting)s, idx);
            else LOG("presets: %s: no '%s' choice for %s", g_list[index - 1].name.c_str(), it->second.c_str(), sky::Key((sky::Setting)s));
        }
        const auto t = kv.find("TIME");
        if (t != kv.end() && g_hooks.set_time) {
            const int ti = TimeIndex(t->second);
            if (ti >= 0 && ti != g_hooks.get_time()) g_hooks.set_time(ti);
        }
        LOG("presets: applied %s", g_list[index - 1].name.c_str());
    }

    bool SaveCustom(std::string* name_out) {
        const auto dir = Dir();
        if (dir.empty()) return false;
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path path;
        int n = 1;
        for (; n < 1000; ++n) {
            path = dir / ("Custom" + std::to_string(n) + ".ini");
            if (!std::filesystem::exists(path, ec)) break;
        }
        std::ofstream f(path);
        if (!f) return false;
        f << "[preset]\n";
        if (g_hooks.get_time && g_hooks.time_labels) f << "time=" << g_hooks.time_labels[g_hooks.get_time()] << "\n";
        for (int s = 0; s < sky::kSettingCount; ++s) {
            if (s == sky::kVariant && sky::Get(sky::kStyle) == 0) continue;
            f << sky::Key((sky::Setting)s) << "=" << sky::LabelAt((sky::Setting)s, sky::Get((sky::Setting)s)) << "\n";
        }
        if (name_out) *name_out = "Custom" + std::to_string(n);
        LOG("presets: saved %s", path.string().c_str());
        return true;
    }
}
