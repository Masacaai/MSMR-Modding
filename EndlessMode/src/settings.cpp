#include "settings.h"

#include "log.h"
#include "paths.h"
#include "roster.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <mutex>

namespace settings {
    namespace {
        std::vector<Row> g_rows;
        std::vector<int> g_index;
        std::mutex g_mutex;

        std::string Ini() { return DataDir() + "\\EndlessArena_settings.ini"; }

        void Head(const char* title) {
            Row r;
            r.title = title;
            g_rows.push_back(std::move(r));
        }
        void Switch(const char* key, const char* title, const char* help, bool def) {
            Row r;
            r.key = key; r.title = title; r.help = help; r.def = def ? 1 : 0;
            g_rows.push_back(std::move(r));
        }
        // choices as "label" with values in the parallel list; def = the index chosen by default.
        void List(const char* key, const char* title, const char* help, std::vector<std::string> choices,
                  std::vector<int> values, int def) {
            Row r;
            r.key = key; r.title = title; r.help = help;
            r.choices = std::move(choices); r.values = std::move(values); r.def = def;
            g_rows.push_back(std::move(r));
        }
        void Mix(const char* key, const char* title, const char* help) {
            List(key, title, help, { "OFF", "FEW", "NORMAL", "MANY" }, { 0, 50, 100, 200 }, 2);
        }

        void Build() {
            Head("PRESETS");
            List("preset", "PRESET", "Pick one to set many rows at once. DEFAULT puts everything back. You can still change any row afterwards. Row changes you make before choosing a preset are replaced.",
                 { "PICK ONE", "DEFAULT", "CASUAL", "BRUTAL", "CHAOS", "BOSS FOCUS", "GLASS CANNON" }, {}, 0);

            Head("RUN");
            List("countdown", "COUNTDOWN", "Seconds of Get Ready before a run starts.", { "3", "5", "10", "15", "30" }, { 3, 5, 10, 15, 30 }, 0);
            Switch("boons", "BOONS AND CURSES", "Campaign: pick a power-up after each wave, and take the curse that comes with it. OFF: no picks, you start with your normal suit and gadgets.", true);
            Switch("practice", "PRACTICE MODE", "Your health is kept full. Nothing ends the run but leaving it.", false);

            Head("DIFFICULTY");
            List("count", "ENEMIES PER WAVE", "How many enemies each wave sends, as a share of the normal number.",
                 { "50%", "75%", "100%", "125%", "150%", "200%", "300%" }, { 50, 75, 100, 125, 150, 200, 300 }, 2);
            List("field", "ENEMIES AT ONCE", "The most enemies on the field together (also the Horde cap). Past 100 the game can stutter or hang.",
                 { "10", "16", "24", "32", "40", "50", "60", "80", "100" }, { 10, 16, 24, 32, 40, 50, 60, 80, 100 }, 4);
            List("chapter", "WAVES PER FACTION", "Waves before the next faction takes over. The last one is the boss wave.",
                 { "2", "3", "4", "5", "6", "8", "10" }, { 2, 3, 4, 5, 6, 8, 10 }, 2);
            Switch("bosswaves", "BOSS WAVES", "OFF: the last wave of every faction is an elite assault instead of a boss.", true);
            List("bosscount", "BOSSES PER BOSS WAVE", "DEFAULT: one at first, more as the run goes on. Otherwise always this many (bosses from other factions fill in).",
                 { "DEFAULT", "1", "2", "3", "4" }, { 0, 1, 2, 3, 4 }, 0);
            List("crew", "BOSS CREW", "The goons that fight beside the boss.",
                 { "NONE", "SMALL", "NORMAL", "LARGE", "HUGE" }, { 0, 50, 100, 200, 300 }, 2);
            List("elites", "ELITE ENEMIES", "How often a goon is replaced by one of the faction's elite units.",
                 { "OFF", "RARE", "NORMAL", "OFTEN", "ALWAYS" }, { 0, 50, 100, 200, 400 }, 2);

            Head("REWARDS");
            List("reward_every", "REWARD EVERY", "How often clearing a wave offers a reward pick. Boss waves always offer their power pick.",
                 { "WAVE", "2 WAVES", "3 WAVES", "BOSSES ONLY" }, { 1, 2, 3, 0 }, 0);
            List("reward_choices", "CHOICES PER REWARD", "Options on every reward screen (the Riot Call curse adds one, up to 4).",
                 { "2", "3", "4" }, { 2, 3, 4 }, 1);
            Switch("boon_powers", "SUIT POWERS AS REWARDS", "Your first reward is a suit power, and bosses offer another.", true);
            Switch("boss_power", "POWER PICK AFTER BOSSES", "A boss wave offers a new suit power to swap in.", true);
            Switch("boon_mods", "SUIT MODS AS REWARDS", "Rewards can be suit mods.", true);
            Switch("stat_power", "BOON: HEAVY HITTER", "+20% damage dealt per stack.", true);
            Switch("stat_tough", "BOON: REINFORCED SUIT", "-15% damage taken per stack.", true);
            Switch("stat_vamp", "BOON: ADRENALINE RUSH", "Heal 4% of max health per kill, per stack.", true);
            Switch("stat_second", "BOON: SECOND WIND", "Heal 30% of max health after each wave, per stack.", true);
            List("stat_stacks", "BOON STACK LIMIT", "How many times one stat boon can be taken. DEFAULT: 3, and 2 for Second Wind.",
                 { "DEFAULT", "1", "2", "3", "5", "10" }, { 0, 1, 2, 3, 5, 10 }, 0);
            Switch("curated", "SKIP USELESS POWERS", "Leave out the suit powers and mods that do nothing in the arena (the author's list). You can also list your own in EndlessArena_pools.txt in the data folder.", false);

            Head("CURSES");
            List("curse_when", "CURSE OFFERS", "When a bittersweet deal is offered: after boss waves, after every wave, or never.",
                 { "AFTER BOSSES", "EVERY WAVE", "NEVER" }, { 0, 1, 2 }, 0);
            List("curse_choices", "CURSES OFFERED", "How many different deals are on offer.", { "1", "2", "3", "4" }, { 1, 2, 3, 4 }, 1);
            Switch("curse_forced", "CURSES ARE MANDATORY", "No NO DEAL option: you must take one.", false);
            List("curse_strength", "CURSE STRENGTH", "How hard the downsides of the curses bite.",
                 { "50%", "75%", "100%", "150%", "200%", "300%" }, { 50, 75, 100, 150, 200, 300 }, 2);
            Switch("curse_glass", "CURSE: GLASS CANNON", "+40% damage dealt, +30% damage taken.", true);
            Switch("curse_riot", "CURSE: RIOT CALL", "One more choice on every reward, 40% more enemies per wave.", true);
            Switch("curse_open", "CURSE: OPEN SEASON", "Heal 25% after each wave, snipers and rockets from the first wave.", true);
            Switch("curse_blood", "CURSE: BLOOD PRICE", "Heal 6% per kill, elites twice as often.", true);

            Head("YOU");
            List("dmg_out", "YOUR DAMAGE", "How hard you hit, on top of any boons.",
                 { "50%", "75%", "100%", "150%", "200%", "300%", "500%" }, { 50, 75, 100, 150, 200, 300, 500 }, 2);
            List("dmg_in", "DAMAGE YOU TAKE", "How hard enemies hit you, on top of any boons.",
                 { "25%", "50%", "75%", "100%", "150%", "200%", "300%" }, { 25, 50, 75, 100, 150, 200, 300 }, 3);
            List("wave_heal", "HEAL AFTER EACH WAVE", "Health restored when a wave is cleared.",
                 { "NONE", "10%", "25%", "50%", "FULL" }, { 0, 10, 25, 50, 100 }, 0);

            Head("ENEMY MIX");
            Mix("mix_melee", "BRAWLERS", "Fist and baton fighters.");
            Mix("mix_ranged", "GUNMEN", "Pistols and rifles.");
            Mix("mix_shield", "SHIELDS", "Riot-shield enemies.");
            Mix("mix_rocket", "ROCKETEERS", "Rocket launchers.");
            Mix("mix_sniper", "SNIPERS", "Long-range shooters.");
            Mix("mix_heavy", "HEAVIES", "Big armored enemies.");
            Mix("mix_special", "SPECIALISTS", "Jetpacks, drones and the like.");

            Head("FACTIONS");
            for (const auto& f : roster::Factions()) {
                std::string key = std::string("fac_") + f.id;
                Switch(key.c_str(), f.name, "OFF: this faction never appears in a Campaign or Horde run.", true);
                g_rows.back().title = f.name;
                for (auto& c : g_rows.back().title) c = (char)toupper((unsigned char)c);
            }
            List("order", "FACTION ORDER", "DEFAULT: the story's order, a random few. SHUFFLED: any order. CUSTOM: the slots below. Horde always mixes.",
                 { "DEFAULT", "SHUFFLED", "CUSTOM" }, { 0, 1, 2 }, 0);
            List("perrun", "FACTIONS PER RUN", "How many factions a Campaign cycles through. More means more enemy models in memory.",
                 { "2", "3", "4", "5", "6", "7", "8", "9" }, { 2, 3, 4, 5, 6, 7, 8, 9 }, 3);
            List("budget", "ENEMY TYPES IN MEMORY", "The game cannot hold unlimited enemy models. Past about 50 the new ones come out as white blocks or the game crashes.",
                 { "30 (SAFE)", "44 (NORMAL)", "50 (HIGH)", "60 (RISKY)", "80 (UNLIMITED)" }, { 30, 44, 50, 60, 80 }, 1);
            {
                std::vector<std::string> ch{ "NONE" };
                std::vector<int> vals{ 0 };
                int i = 1;
                for (const auto& f : roster::Factions()) {
                    std::string n = f.name;
                    for (auto& c : n) c = (char)toupper((unsigned char)c);
                    ch.push_back(n);
                    vals.push_back(i++);
                }
                for (int s = 1; s <= (int)roster::Factions().size(); s++) {
                    char key[16], title[32];
                    snprintf(key, sizeof(key), "slot_%d", s);
                    snprintf(title, sizeof(title), "CUSTOM SLOT %d", s);
                    List(key, title, "With FACTION ORDER: CUSTOM, the faction that takes this place in the run. NONE skips it.", ch, vals, 0);
                }
            }

            Head("BOSSES");
            Switch("unverified", "UNTESTED BOSSES", "Allow bosses nobody has fought standalone yet. They can break, hang or crash the game.", false);
            for (const auto& f : roster::Factions()) {
                if (f.bosses.empty()) continue;
                std::vector<std::string> ch{ "RANDOM", "NONE" };
                for (const auto* b : f.bosses) {
                    std::string n = b->name;
                    for (auto& c : n) c = (char)toupper((unsigned char)c);
                    ch.push_back(n);
                }
                std::string key = std::string("boss_") + f.id;
                std::string title = std::string("BOSS: ") + f.name;
                for (auto& c : title) c = (char)toupper((unsigned char)c);
                List(key.c_str(), title.c_str(), "RANDOM: any of its bosses. NONE: no boss for this faction. A name: always that boss, even if untested.", ch, {}, 0);
            }

            Head("HORDE");
            List("horde_level", "LEVEL LENGTH", "How long a Horde level lasts at most before the next faction joins.",
                 { "30 S", "45 S", "60 S", "90 S", "120 S" }, { 30, 45, 60, 90, 120 }, 2);

            if (g_rows.size() > kMaxRows) {
                LOG("settings: %zu rows exceeds the menu's %zu, truncating", g_rows.size(), kMaxRows);
                g_rows.resize(kMaxRows);
            }
            for (auto& r : g_rows)
                if (r.choices.size() > kMaxChoices) { r.choices.resize(kMaxChoices); if (r.values.size() > kMaxChoices) r.values.resize(kMaxChoices); }
        }

        void Save(size_t row) {
            const Row& r = g_rows[row];
            if (r.heading()) return;
            char v[16];
            snprintf(v, sizeof(v), "%d", g_index[row]);
            WritePrivateProfileStringA("settings", r.key.c_str(), v, Ini().c_str());
        }
    }

    static std::atomic<bool> g_rebuild{ false };
    bool TakeRebuild() { return g_rebuild.exchange(false); }

    const std::vector<Row>& Rows();
    bool SetValue(const std::string& key, int value) {
        const auto& rows = Rows();
        for (size_t i = 0; i < rows.size(); i++) {
            if (rows[i].key != key) continue;
            if (rows[i].choices.empty()) { SetIndex(i, value ? 1 : 0); return true; }
            for (size_t k = 0; k < rows[i].choices.size(); k++) {
                int v = k < rows[i].values.size() ? rows[i].values[k] : (int)k;
                if (v == value) { SetIndex(i, (int)k); return true; }
            }
            LOG("settings: no choice %d in row %s", value, key.c_str());
            return false;
        }
        return false;
    }

    bool ApplyPreset(int index) {
        struct Pair { const char* key; int value; };
        static const std::vector<std::vector<Pair>> presets = {
            /* 0 */ {},
            /* 1 DEFAULT */ {},
            /* 2 CASUAL */ { { "count", 75 }, { "field", 24 }, { "crew", 50 }, { "elites", 50 }, { "dmg_in", 50 }, { "wave_heal", 25 },
                             { "mix_rocket", 50 }, { "mix_sniper", 50 }, { "mix_heavy", 50 }, { "curse_when", 2 } },
            /* 3 BRUTAL */ { { "count", 150 }, { "field", 60 }, { "crew", 200 }, { "elites", 200 }, { "dmg_in", 150 }, { "curse_when", 1 },
                             { "curse_forced", 1 }, { "curse_strength", 150 }, { "bosscount", 2 } },
            /* 4 CHAOS */ { { "count", 200 }, { "field", 80 }, { "chapter", 2 }, { "bosscount", 3 }, { "crew", 300 }, { "elites", 200 },
                            { "mix_melee", 200 }, { "mix_ranged", 200 }, { "mix_shield", 200 }, { "mix_rocket", 200 }, { "mix_sniper", 200 },
                            { "mix_heavy", 200 }, { "mix_special", 200 }, { "order", 1 }, { "perrun", 6 }, { "budget", 50 },
                            { "dmg_out", 200 }, { "reward_every", 1 }, { "reward_choices", 4 }, { "curse_when", 1 } },
            /* 5 BOSS FOCUS */ { { "chapter", 2 }, { "bosscount", 2 }, { "crew", 50 }, { "count", 75 }, { "mix_melee", 50 }, { "mix_ranged", 50 },
                                 { "mix_shield", 50 }, { "mix_rocket", 50 }, { "mix_sniper", 50 }, { "mix_heavy", 50 }, { "mix_special", 50 } },
            /* 6 GLASS CANNON */ { { "dmg_out", 300 }, { "dmg_in", 300 }, { "wave_heal", 10 } },
        };
        if (index <= 0 || index >= (int)presets.size()) return false;
        const auto& rows = Rows();
        for (size_t i = 0; i < rows.size(); i++)
            if (!rows[i].heading() && rows[i].key != "preset") SetIndex(i, rows[i].def);
        for (const auto& p : presets[(size_t)index]) SetValue(p.key, p.value);
        g_rebuild = true;
        LOG("settings: preset %d applied", index);
        return true;
    }

    const std::vector<Row>& Rows() {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_rows.empty()) { Build(); g_index.assign(g_rows.size(), 0); for (size_t i = 0; i < g_rows.size(); i++) g_index[i] = g_rows[i].def; }
        return g_rows;
    }

    void Load() {
        Rows();
        std::lock_guard<std::mutex> lock(g_mutex);
        for (size_t i = 0; i < g_rows.size(); i++) {
            const Row& r = g_rows[i];
            if (r.heading()) continue;
            int n = r.choices.empty() ? 2 : (int)r.choices.size();
            int v = (int)GetPrivateProfileIntA("settings", r.key.c_str(), r.def, Ini().c_str());
            g_index[i] = std::clamp(v, 0, n - 1);
        }
        LOG("settings: loaded %zu rows from %s", g_rows.size(), Ini().c_str());
    }

    int Index(size_t row) {
        Rows();
        std::lock_guard<std::mutex> lock(g_mutex);
        return row < g_index.size() ? g_index[row] : 0;
    }

    void SetIndex(size_t row, int index) {
        Rows();
        bool preset = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (row >= g_rows.size() || g_rows[row].heading()) return;
            int n = g_rows[row].choices.empty() ? 2 : (int)g_rows[row].choices.size();
            index = std::clamp(index, 0, n - 1);
            if (g_index[row] == index) return;
            g_index[row] = index;
            Save(row);
            preset = g_rows[row].key == "preset";
        }
        if (preset) ApplyPreset(index);
    }

    void ResetRow(size_t row) { Rows(); SetIndex(row, g_rows[row].def); }

    void ResetAll() {
        Rows();
        for (size_t i = 0; i < g_rows.size(); i++) SetIndex(i, g_rows[i].def);
    }

    int Find(const std::string& key) {
        Rows();
        for (size_t i = 0; i < g_rows.size(); i++) if (g_rows[i].key == key) return (int)i;
        return -1;
    }

    int Value(const std::string& key, int fallback) {
        int row = Find(key);
        if (row < 0) return fallback;
        std::lock_guard<std::mutex> lock(g_mutex);
        const Row& r = g_rows[row];
        int i = g_index[row];
        return i < (int)r.values.size() ? r.values[i] : i;
    }

    bool On(const std::string& key) { return Value(key, 0) != 0; }

    bool Changed() {
        Rows();
        std::lock_guard<std::mutex> lock(g_mutex);
        for (size_t i = 0; i < g_rows.size(); i++) if (!g_rows[i].heading() && g_index[i] != g_rows[i].def) return true;
        return false;
    }

    void LogAll() {
        Rows();
        std::lock_guard<std::mutex> lock(g_mutex);
        for (size_t i = 0; i < g_rows.size(); i++) {
            const Row& r = g_rows[i];
            if (r.heading()) { LOG("settings: --- %s", r.title.c_str()); continue; }
            LOG("settings: %-14s = %s", r.key.c_str(),
                r.choices.empty() ? (g_index[i] ? "ON" : "OFF") : r.choices[g_index[i]].c_str());
        }
    }
}
