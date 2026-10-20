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
#include <thread>
#include <unordered_map>

namespace settings {
    namespace {
        std::vector<Row> g_rows;
        std::atomic<int> g_idx[kMaxRows];          // the current choice of each row (read from the game's menu hooks, so lock-free)
        std::vector<char> g_dirty;                 // rows changed and not written to the ini yet
        std::atomic<ULONGLONG> g_last_change{ 0 };
        std::mutex g_mutex;
        std::unordered_map<std::string, int> g_find;
        int g_cur_tab = 0;

        std::string Ini() { return DataDir() + "\\EndlessArena_settings.ini"; }

        void Head(const char*) {}   // no headings: the SHOWING row names the section
        void Sect(int n) { g_cur_tab = n; }
        void Switch(const char* key, const char* title, const char* help, bool def) {
            Row r;
            r.key = key; r.title = title; r.help = help; r.def = def ? 1 : 0; r.tab = g_cur_tab;
            g_rows.push_back(std::move(r));
        }
        // choices as "label" with values in the parallel list; def = the index chosen by default.
        void List(const char* key, const char* title, const char* help, std::vector<std::string> choices,
                  std::vector<int> values, int def) {
            Row r;
            r.key = key; r.title = title; r.help = help;
            r.choices = std::move(choices); r.values = std::move(values); r.def = def; r.tab = g_cur_tab;
            g_rows.push_back(std::move(r));
        }
        void Mix(const char* key, const char* title, const char* help) {
            List(key, title, help, { "OFF", "FEW", "NORMAL", "MANY" }, { 0, 50, 100, 200 }, 2);
        }

        void MixRows(const std::string& p, const char* heading) {
            Head(heading);
            Mix((p + "mix_melee").c_str(), "BRAWLERS", "Fist and baton fighters.");
            Mix((p + "mix_ranged").c_str(), "GUNMEN", "Pistols and rifles.");
            Mix((p + "mix_shield").c_str(), "SHIELDS", "Riot-shield enemies.");
            Mix((p + "mix_rocket").c_str(), "ROCKETEERS", "Rocket launchers.");
            Mix((p + "mix_sniper").c_str(), "SNIPERS", "Long-range shooters.");
            Mix((p + "mix_heavy").c_str(), "HEAVIES", "Big armored enemies.");
            Mix((p + "mix_special").c_str(), "SPECIALISTS", "Jetpacks, drones and the like.");
        }

        void FactionRows(const std::string& p, bool campaign) {
            Head(campaign ? "FACTIONS" : "HORDE FACTIONS");
            for (const auto& f : roster::Factions()) {
                std::string key = p + "fac_" + f.id;
                Switch(key.c_str(), f.name, campaign ? "OFF: this faction never appears in a Campaign run." : "OFF: this faction never appears in a Horde run.", true);
                g_rows.back().title = f.name;
                for (auto& c : g_rows.back().title) c = (char)toupper((unsigned char)c);
            }
            if (campaign)
                List("order", "FACTION ORDER", "DEFAULT: the story's order, a random few. SHUFFLED: any order. CUSTOM: the slots below.",
                     { "DEFAULT", "SHUFFLED", "CUSTOM" }, { 0, 1, 2 }, 0);
            else
                List("h_order", "INVASION ORDER", "RANDOM: a different order every run. CUSTOM: the slots below.",
                     { "RANDOM", "CUSTOM" }, { 0, 2 }, 0);
            if (campaign)
                List("perrun", "FACTIONS PER RUN", "How many factions a Campaign cycles through. More means more enemy models in memory.",
                     { "2", "3", "4", "5", "6", "7", "8", "9" }, { 2, 3, 4, 5, 6, 7, 8, 9 }, 3);
            List((p + "budget").c_str(), "ENEMY TYPES IN MEMORY", "The game cannot hold unlimited enemy models. Past about 50 the new ones come out as white blocks or the game crashes.",
                 { "30 (SAFE)", "44 (NORMAL)", "50 (HIGH)", "60 (RISKY)", "80 (UNLIMITED)" }, { 30, 44, 50, 60, 80 }, 1);
            std::vector<std::string> ch{ "NONE" };
            std::vector<int> vals{ 0 };
            int i = 1;
            for (const auto& f : roster::Factions()) {
                std::string n = f.name;
                for (auto& c : n) c = (char)toupper((unsigned char)c);
                ch.push_back(n);
                vals.push_back(i++);
            }
            for (int sl = 1; sl <= (int)roster::Factions().size(); sl++) {
                char title[32];
                snprintf(title, sizeof(title), "CUSTOM SLOT %d", sl);
                List((p + "slot_" + std::to_string(sl)).c_str(), title, "With the order set to CUSTOM, the faction that takes this place in the run. NONE skips it.", ch, vals, 0);
            }
        }

        void BossRows() {
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
        }

        void Build() {
            Sect(-1);
            List("section", "MODE", "Campaign and Horde keep their own settings. The shoulder buttons switch between them.",
                 { "CAMPAIGN", "HORDE" }, {}, 0);
            Sect(0);
            List("preset", "PRESET", "Pick one to set many rows at once. DEFAULT puts everything back. You can still change any row afterwards. Row changes you make before choosing a preset are replaced.",
                 { "PICK ONE", "DEFAULT", "CASUAL", "BRUTAL", "CHAOS", "BOSS FOCUS", "GLASS CANNON" }, {}, 0);

            Head("RUN");
            List("countdown", "COUNTDOWN", "Seconds of Get Ready before a run starts.", { "3", "5", "10", "15", "30" }, { 3, 5, 10, 15, 30 }, 0);
            Switch("boons", "BOONS AND CURSES", "Campaign: pick a power-up after each wave, and take the curse that comes with it. OFF: no picks, you start with your normal suit and gadgets.", true);
            Switch("practice", "PRACTICE MODE", "Your health is kept full. Nothing ends the run but leaving it.", false);

            Sect(1);
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

            Sect(2);
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

            Sect(3);
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

            Sect(1);
            List("dmg_out", "YOUR DAMAGE", "How hard you hit, on top of any boons.",
                 { "50%", "75%", "100%", "150%", "200%", "300%", "500%" }, { 50, 75, 100, 150, 200, 300, 500 }, 2);
            List("dmg_in", "DAMAGE YOU TAKE", "How hard enemies hit you, on top of any boons.",
                 { "25%", "50%", "75%", "100%", "150%", "200%", "300%" }, { 25, 50, 75, 100, 150, 200, 300 }, 3);
            List("wave_heal", "HEAL AFTER EACH WAVE", "Health restored when a wave is cleared.",
                 { "NONE", "10%", "25%", "50%", "FULL" }, { 0, 10, 25, 50, 100 }, 0);

            Sect(4);
            MixRows("", "ENEMY MIX");
            Sect(5);
            FactionRows("", true);
            Sect(6);
            BossRows();

            // ---- Horde (its own copy of what applies to it) ----
            Sect(7);
            List("h_preset", "PRESET", "Pick one to set many Horde rows at once. DEFAULT puts them all back. Changes you make before choosing a preset are replaced.",
                 { "PICK ONE", "DEFAULT", "CASUAL", "BRUTAL", "CHAOS", "BOSS FOCUS", "GLASS CANNON" }, {}, 0);
            Head("HORDE RUN");
            List("h_countdown", "COUNTDOWN", "Seconds of Get Ready before a Horde run starts.", { "3", "5", "10", "15", "30" }, { 3, 5, 10, 15, 30 }, 0);
            Switch("h_practice", "PRACTICE MODE", "Your health is kept full. Nothing ends the run but leaving it.", false);
            List("horde_level", "LEVEL LENGTH", "How long a Horde level lasts at most before the next faction joins.",
                 { "30 S", "45 S", "60 S", "90 S", "120 S" }, { 30, 45, 60, 90, 120 }, 2);
            List("h_field", "ENEMIES AT ONCE", "The most enemies on the field together. Past 100 the game can stutter or hang.",
                 { "10", "16", "24", "32", "40", "50", "60", "80", "100" }, { 10, 16, 24, 32, 40, 50, 60, 80, 100 }, 4);
            List("h_elites", "ELITE ENEMIES", "How often an enemy is replaced by one of its faction's elite units.",
                 { "OFF", "RARE", "NORMAL", "OFTEN", "ALWAYS" }, { 0, 50, 100, 200, 400 }, 2);
            List("h_dmg_out", "YOUR DAMAGE", "How hard you hit.",
                 { "50%", "75%", "100%", "150%", "200%", "300%", "500%" }, { 50, 75, 100, 150, 200, 300, 500 }, 2);
            List("h_dmg_in", "DAMAGE YOU TAKE", "How hard enemies hit you.",
                 { "25%", "50%", "75%", "100%", "150%", "200%", "300%" }, { 25, 50, 75, 100, 150, 200, 300 }, 3);
            Sect(8);
            MixRows("h_", "HORDE ENEMY MIX");
            Sect(9);
            FactionRows("h_", false);

            if (g_rows.size() > kMaxRows) {
                LOG("settings: %zu rows exceeds the menu's %zu, truncating", g_rows.size(), kMaxRows);
                g_rows.resize(kMaxRows);
            }
            for (auto& r : g_rows)
                if (r.choices.size() > kMaxChoices) { r.choices.resize(kMaxChoices); if (r.values.size() > kMaxChoices) r.values.resize(kMaxChoices); }
        }

        void Save(size_t row) {   // marks the row for the writer thread: the menu must not wait for the disk
            if (g_rows[row].heading()) return;
            if (g_dirty.size() < g_rows.size()) g_dirty.assign(g_rows.size(), 0);
            g_dirty[row] = 1;
            g_last_change = GetTickCount64();
        }

        bool IsHorde(const std::string& key) { return key.rfind("h_", 0) == 0 || key == "horde_level"; }

        // The rows on screen (cut down so the settings screen stays quick); every other row is hidden but still exists: the ini, the presets and
        // the defaults reach it. Order on screen is the order they were built in.
        void ChooseShown() {
            static const char* const kCampaign[] = { "preset", "practice", "count", "field", "chapter", "bosswaves", "elites", "boons", "dmg_out", "dmg_in" };
            static const char* const kHorde[] = { "h_preset", "h_practice", "horde_level", "h_field", "h_elites", "h_dmg_out", "h_dmg_in" };
            for (auto& r : g_rows) {
                if (r.heading() || r.key == "section") continue;
                int t = -2;
                if (r.key.rfind("fac_", 0) == 0) t = 0;
                else if (r.key.rfind("h_fac_", 0) == 0) t = 1;
                for (const char* k : kCampaign) if (r.key == k) t = 0;
                for (const char* k : kHorde) if (r.key == k) t = 1;
                r.tab = t;
            }
        }

        std::once_flag g_once, g_writer_once;
        void Init() {
            Build();
            ChooseShown();
            for (size_t i = 0; i < g_rows.size(); i++) {
                g_idx[i] = g_rows[i].def;
                if (!g_rows[i].heading()) g_find.emplace(g_rows[i].key, (int)i);
            }
            g_dirty.assign(g_rows.size(), 0);
        }

        // Writes changed rows to the ini half a second after the last change.
        void WriterLoop() {
            for (;;) {
                Sleep(300);
                ULONGLONG last = g_last_change;
                if (!last || GetTickCount64() - last < 500) continue;
                std::vector<std::pair<std::string, int>> out;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    for (size_t i = 0; i < g_dirty.size(); i++)
                        if (g_dirty[i]) { out.push_back({ g_rows[i].key, g_idx[i] }); g_dirty[i] = 0; }
                    g_last_change = 0;
                }
                for (auto& [k, v] : out) {
                    char b[16];
                    snprintf(b, sizeof(b), "%d", v);
                    WritePrivateProfileStringA("settings", k.c_str(), b, Ini().c_str());
                }
            }
        }
    }

    static std::atomic<bool> g_rebuild{ false };
    bool TakeRebuild() { return g_rebuild.exchange(false); }

    const std::vector<Row>& Rows() {
        std::call_once(g_once, Init);
        return g_rows;
    }

    const char* SectionName(int section) {
        const auto& r = Rows()[0];   // the SHOWING row
        return section >= 0 && section < (int)r.choices.size() ? r.choices[(size_t)section].c_str() : "?";
    }
    int CurrentSection() { return Rows().empty() ? 0 : g_idx[0].load(); }

    int Find(const std::string& key) {
        Rows();
        auto it = g_find.find(key);
        return it == g_find.end() ? -1 : it->second;
    }

    int Index(size_t row) {
        Rows();
        return row < g_rows.size() ? g_idx[row].load() : 0;
    }

    void SetIndex(size_t row, int index) {
        Rows();
        int preset = 0;   // 1 the Campaign's preset row, 2 Horde's
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (row >= g_rows.size() || g_rows[row].heading()) return;
            int n = g_rows[row].choices.empty() ? 2 : (int)g_rows[row].choices.size();
            index = std::clamp(index, 0, n - 1);
            if (g_idx[row] == index) return;
            g_idx[row] = index;
            Save(row);
            preset = g_rows[row].key == "preset" ? 1 : g_rows[row].key == "h_preset" ? 2 : 0;
            if (g_rows[row].key == "section") g_rebuild = true;   // other rows are on screen now
        }
        std::call_once(g_writer_once, [] { std::thread(WriterLoop).detach(); });
        if (preset) ApplyPreset(index, preset == 2);
    }

    void ResetRow(size_t row) { Rows(); SetIndex(row, g_rows[row].def); }

    void ResetSection(int section) {
        Rows();
        for (size_t i = 0; i < g_rows.size(); i++)
            if (!g_rows[i].heading() && g_rows[i].tab != -1 && IsHorde(g_rows[i].key) == (section == 1)) SetIndex(i, g_rows[i].def);
    }

    void ResetAll() {
        Rows();
        for (size_t i = 0; i < g_rows.size(); i++) SetIndex(i, g_rows[i].def);
    }

    bool SetValue(const std::string& key, int value) {
        const int i = Find(key);
        if (i < 0) return false;
        const Row& r = g_rows[(size_t)i];
        if (r.choices.empty()) { SetIndex((size_t)i, value ? 1 : 0); return true; }
        for (size_t k = 0; k < r.choices.size(); k++) {
            int v = k < r.values.size() ? r.values[k] : (int)k;
            if (v == value) { SetIndex((size_t)i, (int)k); return true; }
        }
        LOG("settings: no choice %d in row %s", value, key.c_str());
        return false;
    }

    bool ApplyPreset(int index, bool horde) {
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
        for (size_t i = 0; i < rows.size(); i++) {
            const Row& r = rows[i];
            if (r.heading() || r.key == "preset" || r.key == "h_preset") continue;
            if (r.tab != -1 && IsHorde(r.key) == horde) SetIndex(i, r.def);   // only this mode's rows, hidden ones too
        }
        for (const auto& p : presets[(size_t)index]) SetValue(horde ? std::string("h_") + p.key : std::string(p.key), p.value);
        g_rebuild = true;
        LOG("settings: %s preset %d applied", horde ? "Horde" : "Campaign", index);
        return true;
    }

    void Load() {
        Rows();
        std::lock_guard<std::mutex> lock(g_mutex);
        for (size_t i = 0; i < g_rows.size(); i++) {
            const Row& r = g_rows[i];
            if (r.heading()) continue;
            int n = r.choices.empty() ? 2 : (int)r.choices.size();
            int v = (int)GetPrivateProfileIntA("settings", r.key.c_str(), r.def, Ini().c_str());
            g_idx[i] = std::clamp(v, 0, n - 1);
        }
        LOG("settings: loaded %zu rows from %s", g_rows.size(), Ini().c_str());
    }

    int Value(const std::string& key, int fallback) {
        const int row = Find(key);
        if (row < 0) return fallback;
        const Row& r = g_rows[(size_t)row];
        int i = g_idx[row];
        return i < (int)r.values.size() ? r.values[(size_t)i] : i;
    }

    bool On(const std::string& key) { return Value(key, 0) != 0; }

    bool Changed() {
        Rows();
        for (size_t i = 0; i < g_rows.size(); i++) if (!g_rows[i].heading() && g_idx[i] != g_rows[i].def) return true;
        return false;
    }

    void LogAll() {
        Rows();
        for (size_t i = 0; i < g_rows.size(); i++) {
            const Row& r = g_rows[i];
            if (r.heading()) continue;
            LOG("settings: %-14s = %s", r.key.c_str(), r.choices.empty() ? (g_idx[i] ? "ON" : "OFF") : r.choices[(size_t)g_idx[i]].c_str());
        }
    }
}
