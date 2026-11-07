#include "boons.h"

#include "entity.h"
#include "gamethread.h"
#include "hud.h"
#include "input.h"
#include "inventory.h"
#include "log.h"
#include "memory.h"
#include "natives.h"
#include "runloadout.h"
#include "scan.h"
#include "settings.h"
#include "paths.h"
#include "stats.h"
#include "timescale.h"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>

namespace boons {
    struct Item { uint64_t id; const char* internal; const char* display; };
    static const Item kItems[] = {
#include "boon_items.inc"
    };

    // ---- stat boons (levers we control directly) ----
    enum class Stat { Power, Toughness, Vampire, Recovery, SpiderSense, Machinery, BlackSuit, Wheatcakes, SlowBurn, FullTank };
    struct StatDef { Stat stat; const char* name; const char* desc; int max_stacks; };
    static const StatDef kStats[] = {
        { Stat::Power, "Heavy Hitter", "+20% damage dealt", 3 },
        { Stat::Toughness, "Reinforced Suit", "-15% damage taken", 3 },
        { Stat::Vampire, "Adrenaline Rush", "heal 4% of max health per kill", 3 },
        { Stat::Recovery, "Second Wind", "heal 30% of max health after each wave", 2 },
        { Stat::SpiderSense, "Peter Tingle", "the first hit you take each wave does nothing", 2 },
        { Stat::Machinery, "Under the Machinery", "below 25% health: -40% damage taken and a slow heal", 1 },
        { Stat::BlackSuit, "Black Suit", "+4% damage dealt per kill in a row (up to +80%), lost when hit", 2 },
        { Stat::Wheatcakes, "Wheatcakes", "heal 50% of max health after a boss wave", 2 },
        { Stat::SlowBurn, "Slow Burn", "Focus fills 30% faster", 2 },
        { Stat::FullTank, "Full Tank", "every wave starts with a full Focus meter", 1 },
    };

    // ---- bittersweet curses (user: always an upside AND a downside; downsides never add enemy health) ----
    enum class Curse { GlassCannon, RiotCall, OpenSeason, BloodPrice, ParkerLuck, RentIsDue, CloneSaga, OnePunch, DrySpell };
    struct CurseDef { Curse curse; const char* name; const char* up; const char* down; };
    static const CurseDef kCurses[] = {
        { Curse::GlassCannon, "Glass Cannon", "+40% damage dealt", "+30% damage taken" },
        { Curse::RiotCall, "Riot Call", "4 choices on every reward", "waves bring 40% more enemies" },
        { Curse::OpenSeason, "Open Season", "heal 25% after each wave", "snipers and rockets from the first wave" },
        { Curse::BloodPrice, "Blood Price", "heal 6% per kill", "elites twice as often" },
        { Curse::ParkerLuck, "Parker Luck", "+25% damage dealt", "one fewer choice on every reward" },
        { Curse::RentIsDue, "Rent Is Due", "one extra choice on every reward", "every cleared wave costs 8% of your health" },
        { Curse::CloneSaga, "Clone Saga", "an extra reward after every wave", "every boss wave brings a second copy of its boss" },
        { Curse::DrySpell, "Dry Spell", "+25% damage dealt", "Focus builds 40% slower" },
        { Curse::OnePunch, "One-Punch Spider", "everything goes down in one hit, bosses too", "so do you (damage x20 both ways)" },
    };

    // ---- state ----
    enum class Kind { Power, Mod, Stat, Curse, Decline };
    struct Option { Kind kind; uint64_t id = 0; int index = 0; std::string label; };
    struct Offer { std::string title; std::vector<Option> options; bool curse = false; bool repeat = false; DWORD shown_at = 0; DWORD ready_at = 0; };
    constexpr DWORD kOfferDelayMs = 3500;   // after the wave clears, let finishers and animations play out (user)

    static std::mutex g_mutex;
    static std::atomic<bool> g_active = false;
    static std::mt19937 g_rng;
    static std::vector<uint64_t> g_powers, g_mods;   // owned, by the game's own category
    static int g_stacks[12] = {};
    static std::atomic<int> g_tingle_left = 0, g_streak = 0;
    static std::atomic<bool> g_sandbox = false;   // debug: gifts given by command; a run's Begin/End keep them
    static float FocusRate();
    static std::atomic<bool> g_fill_pending = false;
    constexpr float kOnePunchMult = 20.0f;
    static std::vector<Curse> g_curses;
    static std::vector<std::string> g_taken;          // history for the loadout view
    static std::deque<Offer> g_queue;
    static std::atomic<bool> g_offering = false;
    static std::atomic<bool> g_frozen = false;          // world time scale held near 0 while an offer is up
    static DWORD g_last_refresh = 0;
    constexpr DWORD kOfferTimeoutMs = 45000;          // AFK: take a random option (curses: decline)

    static std::atomic<bool> g_ps = false;   // follow the player's settings
    static int S(const char* key, int def) { return g_ps ? settings::Value(key, def) : def; }
    static std::string g_pfx;   // "h_" in Horde: the damage rows have a copy per mode
    void SetPlayerSettings(bool on, bool horde) { g_ps = on; g_pfx.clear(); (void)horde; }
    static int SP(const char* key, int def) { return g_ps ? settings::Value(g_pfx + key, def) : def; }
    static const char* const kStatKeys[] = { "stat_power", "stat_tough", "stat_vamp", "stat_second", "stat_sense", "stat_machinery", "stat_blacksuit", "stat_wheat", "stat_burn", "stat_tank" };
    static const char* const kCurseKeys[] = { "curse_glass", "curse_riot", "curse_open", "curse_blood", "curse_parker", "curse_rent", "curse_clone", "curse_dry", "curse_punch" };
    static float CurseStrength() { return S("curse_strength", 100) / 100.0f; }
    static int RewardChoices() { return S("reward_choices", 3); }

    static bool HasCurse(Curse c) { return std::find(g_curses.begin(), g_curses.end(), c) != g_curses.end(); }
    static int Choices() {
        int n = RewardChoices() + (HasCurse(Curse::RiotCall) ? 1 : 0) + (HasCurse(Curse::RentIsDue) ? 1 : 0) - (HasCurse(Curse::ParkerLuck) ? 1 : 0);
        return std::max(1, std::min(4, n));
    }

    // The game's own name and description for an item (cached), so rewards read exactly as in the suit menu, including a
    // user's renamed or modded powers. Falls back to our table.
    static const inventory::ItemText& GameText(uint64_t id) {
        static std::unordered_map<uint64_t, inventory::ItemText> cache;
        auto it = cache.find(id);
        if (it == cache.end()) it = cache.emplace(id, inventory::Text(id)).first;
        return it->second;
    }

    static const char* TableName(uint64_t id);
    static std::string Label(uint64_t id) {
        const auto& t = GameText(id);
        return t.ok ? t.name : std::string(TableName(id));
    }
    // "Name: description" when the game has a description.
    static std::string Described(uint64_t id) {
        const auto& t = GameText(id);
        if (!t.ok) return TableName(id);
        return t.desc.empty() || t.desc == "INVALID" ? t.name : t.name + ": " + t.desc;
    }

    static const char* Name(uint64_t id) { return TableName(id); }
    static const char* TableName(uint64_t id) {
        for (const auto& it : kItems)
            if (it.id == id) {
                if (*it.display) return it.display;
                static thread_local char buf[40];
                snprintf(buf, sizeof(buf), "Item %04llX", (unsigned long long)(id & 0xFFFF));
                return buf;
            }
        return "?";
    }

    float OutgoingMult() {
        const float base = SP("dmg_out", 100) / 100.0f;
        if (!g_active) return base;
        float m = base * (1.0f + 0.20f * g_stacks[(int)Stat::Power]);
        if (HasCurse(Curse::GlassCannon)) m *= 1.40f;
        if (HasCurse(Curse::ParkerLuck)) m *= 1.25f;
        if (HasCurse(Curse::DrySpell)) m *= 1.25f;
        if (int bs = g_stacks[(int)Stat::BlackSuit]) m *= 1.0f + std::min(0.80f, 0.04f * bs * g_streak.load());
        if (HasCurse(Curse::OnePunch)) m *= kOnePunchMult;
        return m;
    }

    float IncomingMult() {
        const float base = SP("dmg_in", 100) / 100.0f;
        if (!g_active) return base;
        float m = base * std::pow(0.85f, (float)g_stacks[(int)Stat::Toughness]);
        if (HasCurse(Curse::GlassCannon)) m *= 1.0f + 0.30f * CurseStrength();
        if (HasCurse(Curse::OnePunch)) m *= kOnePunchMult;
        return m;
    }

    // The multiplier for one hit on the hero (stats hook): IncomingMult plus the per-hit effects. hp/max_hp: his health before the hit.
    float IncomingHitMult(float hp, float max_hp) {
        if (!g_active) return IncomingMult();
        if (g_tingle_left.load() > 0 && g_tingle_left.fetch_sub(1) > 0) return 0.0f;   // Peter Tingle
        g_streak = 0;                                                                    // Black Suit: a hit ends the streak
        float m = IncomingMult();
        if (g_stacks[(int)Stat::Machinery] && max_hp > 0 && hp < 0.25f * max_hp) m *= 0.60f;
        return m;
    }

    // Heal the hero by a fraction of max health (game thread; Health::SetHealth clamps to max).
    // frac < 0 hurts (never below 1 HP). below: only act while health is under that fraction of max.
    static void HealHero(float frac, float below = 1.01f) {
        if (frac == 0) return;
        gamethread::Post([frac, below] {
            using SetFn = void (*)(uintptr_t, float);
            auto set = (SetFn)natives::Get(natives::HealthSet);
            auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
            uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
            uint32_t handle = 0;
            if (!set || !get_actor || !memory::Read(system + 0x1C, handle) || !handle) return;
            uintptr_t actor = get_actor(&handle);
            uintptr_t c = actor ? entity::FindComponent(actor, "HeroHealth") : 0;
            float max_hp = 0, hp = 0;
            if (!c || !memory::Read(c + 0x80, max_hp) || !memory::Read(c + 0x88, hp) || max_hp <= 0 || hp <= 0) return;
            if (hp >= below * max_hp) return;
            set(c, std::max(1.0f, std::min(max_hp, hp + max_hp * frac)));
        });
    }

    void Begin(uint32_t seed) {
        if (g_sandbox && g_active) { LOG("boons: debug sandbox keeps its boons, curses and loadout"); return; }
        // The game's own item category decides what's a power (1) or a mod (2); skills never qualify.
        auto pools = std::make_shared<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>>();
        gamethread::Run([pools] {
            for (const auto& it : kItems) {
                inventory::ItemInfo info = inventory::Info(it.id);
                if (!info.found || !info.owned || info.locked) continue;
                if (info.category == 1) pools->first.push_back(it.id);
                else if (info.category == 2) pools->second.push_back(it.id);
            }
        }, 5000);
        // Curated pools (2026-10-15, from the suit menu texts in tools/assets/perk_text.json): leave out suit powers and mods that do
        // nothing useful in the arena. Matched by the game's own display name (so a renamed mod isn't filtered by accident, and
        // the English names are the ones the suit menu uses).
        static const char* const kBlocked[] = {
            // no effect against enemies that are already engaged / not combat
            "Equalizer",                // "Everyone goes down in one hit, including you": a curse, not a reward
            "Quips", "Sound Of Silence", "Blur Projector",
            "Combat Analyzer", "Proximity Sensor", "Long Range Scanner", "Silent Step", "Bio Scanner",
            // need gadgets, which are locked during runs
            "Resupply", "Battle Tech", "Scavenger", "Metabolic Converter",
        };
        // The player's own list: EndlessArena_pools.txt in the data folder, one item name per line (# starts a comment).
        std::vector<std::string> mine;
        if (g_ps) {
            std::ifstream in(DataDir() + "\\EndlessArena_pools.txt");
            for (std::string line; std::getline(in, line);) {
                size_t h = line.find('#');
                if (h != std::string::npos) line.resize(h);
                while (!line.empty() && isspace((unsigned char)line.back())) line.pop_back();
                size_t b = 0;
                while (b < line.size() && isspace((unsigned char)line[b])) b++;
                if (b < line.size()) mine.push_back(line.substr(b));
            }
        }
        const bool curated = S("curated", 0) != 0;
        auto blocked = [&](uint64_t id) {
            const inventory::ItemText& t = GameText(id);
            if (!t.ok) return false;
            if (curated) for (const char* b : kBlocked) if (_stricmp(t.name.c_str(), b) == 0) return true;
            for (const auto& b : mine) if (_stricmp(t.name.c_str(), b.c_str()) == 0) return true;
            return false;
        };
        // OFF for now (user, 2026-10-15): everything is included until the technical work is done; the user has the final say
        // on curation. Flip kCuratePools to apply the list above.
        size_t before_p = pools->first.size(), before_m = pools->second.size();
        if (curated || !mine.empty()) {
            std::erase_if(pools->first, blocked);
            std::erase_if(pools->second, blocked);
        }
        LOG("boons: curated pools: %zu -> %zu suit powers, %zu -> %zu suit mods", before_p, pools->first.size(), before_m, pools->second.size());
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_rng.seed(seed ^ 0xB0057u);
            g_powers = pools->first;
            g_mods = pools->second;
            std::fill(std::begin(g_stacks), std::end(g_stacks), 0);
            g_tingle_left = 0;
            g_streak = 0;
            g_curses.clear();
            g_taken.clear();
            g_queue.clear();
            g_offering = false;
        }
        LOG("boons: pools: %zu suit powers, %zu suit mods (owned, by game category)", g_powers.size(), g_mods.size());
        runloadout::Begin();   // snapshot, clear power and mods, lock the menu (blocking game-thread work)
        g_active = true;
    }

    void End() {
        if (!g_active) return;
        if (g_sandbox) {   // debug: keep what was given; only the run's offers go
            std::lock_guard<std::mutex> lock(g_mutex);
            g_queue.clear();
            g_offering = false;
            if (g_frozen.exchange(false)) timescale::Unfreeze();
            hud::Clear();
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_active = false;
            g_queue.clear();
            g_offering = false;
        }
        if (g_frozen.exchange(false)) timescale::Unfreeze();
        hud::Clear();
        runloadout::End();
    }

    bool Active() { return g_active; }

    // ---- offers ----

    static Offer MakeRewardOffer() {
        Offer o;
        o.title = "Choose a reward";
        int n = Choices();
        std::vector<Option> pool;
        uint64_t cur[5];
        inventory::Loadout(cur);
        bool power_empty = cur[1] == 0;
        if (runloadout::FreeModSlot() >= 0 && S("boon_mods", 1))
            for (uint64_t id : g_mods)
                if (!runloadout::HasItem(id)) pool.push_back({ Kind::Mod, id, 0, Described(id) });
        for (int i = 0; i < (int)std::size(kStats); i++)
            if (g_stacks[i] < (S("stat_stacks", 0) > 0 ? S("stat_stacks", 0) : kStats[i].max_stacks) && S(kStatKeys[i], 1))
                pool.push_back({ Kind::Stat, 0, i, std::string(kStats[i].name) + " (" + kStats[i].desc + ")" });
        std::shuffle(pool.begin(), pool.end(), g_rng);
        // Until the run has a suit power, one option is always a power.
        if (power_empty && !g_powers.empty() && S("boon_powers", 1)) {
            uint64_t id = g_powers[std::uniform_int_distribution<size_t>(0, g_powers.size() - 1)(g_rng)];
            o.options.push_back({ Kind::Power, id, 0, Described(id) });
        }
        for (auto& op : pool) {
            if ((int)o.options.size() >= n) break;
            o.options.push_back(op);
        }
        return o;
    }

    static Offer MakePowerOffer() {
        Offer o;
        o.title = "Boss down! Choose a suit power";
        uint64_t cur[5];
        inventory::Loadout(cur);
        std::vector<uint64_t> pool;
        for (uint64_t id : g_powers)
            if (id != cur[1]) pool.push_back(id);
        std::shuffle(pool.begin(), pool.end(), g_rng);
        int n = Choices();
        for (int i = 0; i < (int)pool.size() && i < n; i++)
            o.options.push_back({ Kind::Power, pool[i], 0, Described(pool[i]) + (cur[1] ? " (replaces your power)" : "") });
        return o;
    }

    static Offer MakeCurseOffer() {
        Offer o;
        o.title = "A bittersweet deal? (optional)";
        o.curse = true;
        std::vector<int> pool;
        for (int i = 0; i < (int)std::size(kCurses); i++)
            if (!HasCurse(kCurses[i].curse) && S(kCurseKeys[i], 1)) pool.push_back(i);
        std::shuffle(pool.begin(), pool.end(), g_rng);
        for (int i = 0; i < (int)pool.size() && i < S("curse_choices", 2); i++) {
            const CurseDef& c = kCurses[pool[i]];
            o.options.push_back({ Kind::Curse, 0, pool[i], std::string(c.name) + ": " + c.up + ", but " + c.down });
        }
        if (!o.options.empty() && !S("curse_forced", 0)) o.options.push_back({ Kind::Decline, 0, 0, "No deal" });
        return o;
    }

    void OnWaveCleared(int wave, bool boss_wave) {
        const float base_heal = S("wave_heal", 0) / 100.0f;
        if (!g_active) { HealHero(base_heal); return; }
        // Heals first, so they're felt during the pick.
        float heal = base_heal + 0.30f * g_stacks[(int)Stat::Recovery] + (HasCurse(Curse::OpenSeason) ? 0.25f : 0.0f);
        if (boss_wave) heal += 0.50f * g_stacks[(int)Stat::Wheatcakes];
        HealHero(heal);
        if (HasCurse(Curse::RentIsDue)) HealHero(-0.08f * CurseStrength());
        std::lock_guard<std::mutex> lock(g_mutex);
        const int every = S("reward_every", 1), curse_when = S("curse_when", 0);
        if (boss_wave) {
            if (S("boss_power", 1) && S("boon_powers", 1)) {
                Offer p = MakePowerOffer();
                if (!p.options.empty()) { p.repeat = HasCurse(Curse::CloneSaga); g_queue.push_back(p); }
            }
        } else if (every > 0 && wave % every == 0) {
            Offer r = MakeRewardOffer();
            if (!r.options.empty()) { r.repeat = HasCurse(Curse::CloneSaga); g_queue.push_back(r); }
        }
        if (curse_when == 1 || (curse_when == 0 && boss_wave)) {
            Offer c = MakeCurseOffer();
            if (!c.options.empty()) g_queue.push_back(c);
        }
        if (!g_queue.empty()) g_queue.front().ready_at = GetTickCount() + kOfferDelayMs;
        g_offering = !g_queue.empty();
        g_last_refresh = 0;
        LOG("boons: wave %d cleared, %zu offer(s) queued", wave, g_queue.size());
    }

    void OnKill() {
        if (!g_active) return;
        float frac = 0.04f * g_stacks[(int)Stat::Vampire] + (HasCurse(Curse::BloodPrice) ? 0.06f : 0.0f);
        HealHero(frac);
        g_streak++;
    }

    // Director loop, every tick: Under the Machinery heals 1% of max health a second while he is below a quarter.
    // The Focus meter ("assault" in the game's names): HeroSpiderAssaultManager, three segments as float pairs {fill, cap} from +0x110
    // (found in game 2026-11-07: filling the meter changed them 0 -> 100, and writing 0 emptied the third segment on screen).
    // Rate effects need no hook: each tick the change in total fill is scaled and written back, filling the segments in order.
    static float FocusRate() {
        float r = 1.0f + 0.30f * g_stacks[(int)Stat::SlowBurn];
        if (HasCurse(Curse::DrySpell)) r *= 0.60f;
        return r;
    }
    static void FocusTick() {
        const float rate = FocusRate();
        const bool full = g_fill_pending.exchange(false);
        if (std::fabs(rate - 1.0f) < 0.01f && !full) return;
        gamethread::Post([rate, full] {
            static float last = -1.0f;
            auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
            uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
            uint32_t handle = 0;
            if (!get_actor || !memory::Read(system + 0x1C, handle) || !handle) return;
            uintptr_t actor = get_actor(&handle);
            uintptr_t m = actor ? entity::FindComponent(actor, "HeroSpiderAssaultManager") : 0;
            uint32_t n = 0;
            if (!m || !memory::Read(m + 0x128, n) || n < 1 || n > 3) return;
            float fill[3], cap[3];
            float total = 0, cap_total = 0;
            for (uint32_t i = 0; i < n; i++) {
                if (!memory::Read(m + 0x110 + 8 * i, fill[i]) || !memory::Read(m + 0x114 + 8 * i, cap[i]) || cap[i] <= 0) return;
                total += fill[i];
                cap_total += cap[i];
            }
            float target = total;
            if (full) target = cap_total;
            else if (last >= 0 && total > last) target = std::min(cap_total, last + (total - last) * rate);
            if (std::fabs(target - total) > 0.01f) {
                float left = target;
                for (uint32_t i = 0; i < n; i++) {
                    float v = std::max(0.0f, std::min(cap[i], left));
                    left -= cap[i];
                    memory::Write<float>(m + 0x110 + 8 * i, v);
                }
            }
            last = target;
        });
    }


    // ---- debug mode (docs/HOW_TO.md): everything by command; `dbg ...` ----

    static uintptr_t FocusManager() {   // game thread
        auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
        uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
        uint32_t handle = 0;
        if (!get_actor || !memory::Read(system + 0x1C, handle) || !handle) return 0;
        uintptr_t actor = get_actor(&handle);
        return actor ? entity::FindComponent(actor, "HeroSpiderAssaultManager") : 0;
    }

    static bool Has(const std::string& hay, const std::string& needle) {
        std::string a = hay, b = needle;
        for (auto& c : a) c = (char)tolower((unsigned char)c);
        for (auto& c : b) c = (char)tolower((unsigned char)c);
        return a.find(b) != std::string::npos;
    }

    static void DebugRun(std::string args) {
        std::string cmd;
        { size_t sp = args.find(' '); cmd = args.substr(0, sp); args = sp == std::string::npos ? "" : args.substr(sp + 1); }
        while (!args.empty() && args.back() == ' ') args.pop_back();
        auto begin = [] {
            if (g_active) { g_sandbox = true; return; }
            SetPlayerSettings(true);
            g_sandbox = true;
            Begin(GetTickCount());
            LOG("dbg: sandbox on (loadout cleared like a run; `dbg end` restores it)");
        };
        if (cmd == "begin") { begin(); return; }
        if (cmd == "end") { g_sandbox = false; End(); SetPlayerSettings(false); LOG("dbg: sandbox off, loadout restored"); return; }
        if (cmd == "list") {
            for (const auto& d : kStats) LOG("dbg: boon   %s (max %d): %s", d.name, d.max_stacks, d.desc);
            for (const auto& d : kCurses) LOG("dbg: curse  %s: %s, but %s", d.name, d.up, d.down);
            for (uint64_t id : g_powers) LOG("dbg: power  %s", Label(id).c_str());
            for (uint64_t id : g_mods) LOG("dbg: mod    %s", Label(id).c_str());
            return;
        }
        if (cmd == "status") {
            for (const auto& line : Summary()) LOG("dbg:   %s", line.c_str());
            LOG("dbg: active=%d sandbox=%d dmg out x%.2f in x%.2f, focus rate x%.2f", (int)g_active, (int)g_sandbox, OutgoingMult(), IncomingMult(), FocusRate());
            return;
        }
        if (cmd == "give" || cmd == "curse" || cmd == "power" || cmd == "mod") {
            begin();
            std::lock_guard<std::mutex> lock(g_mutex);
            if (cmd == "give") {
                for (int i = 0; i < (int)std::size(kStats); i++)
                    if (Has(kStats[i].name, args)) {
                        g_stacks[i] = std::min(kStats[i].max_stacks, g_stacks[i] + 1);
                        if (kStats[i].stat == Stat::FullTank) g_fill_pending = true;
                        LOG("dbg: %s %d/%d", kStats[i].name, g_stacks[i], kStats[i].max_stacks);
                        return;
                    }
            } else if (cmd == "curse") {
                for (const auto& d : kCurses)
                    if (Has(d.name, args)) {
                        if (!HasCurse(d.curse)) g_curses.push_back(d.curse);
                        LOG("dbg: curse %s", d.name);
                        return;
                    }
            } else {
                for (uint64_t id : cmd == "power" ? g_powers : g_mods)
                    if (Has(Label(id), args)) {
                        if (cmd == "power") runloadout::SetPower(id); else runloadout::AddMod(id);
                        LOG("dbg: %s %s", cmd.c_str(), Label(id).c_str());
                        return;
                    }
            }
            LOG("dbg: nothing matches '%s' (dbg list)", args.c_str());
            return;
        }
        if (cmd == "clear") {
            std::lock_guard<std::mutex> lock(g_mutex);
            std::fill(std::begin(g_stacks), std::end(g_stacks), 0);
            g_curses.clear();
            g_taken.clear();
            LOG("dbg: boons and curses cleared (power and mods stay; `dbg end` restores yours)");
            return;
        }
        if (cmd == "heal") { HealHero((float)atof(args.c_str())); LOG("dbg: heal %s of max (negative hurts, never below 1 HP)", args.c_str()); return; }
        if (cmd == "cleared") {   // dbg cleared [wave] [boss]: the reward screens a cleared wave brings, driven here (no run needed)
            begin();
            int wave = atoi(args.c_str());
            OnWaveCleared(wave > 0 ? wave : 1, Has(args, "boss"));
            while (Offering()) { Tick(); Sleep(100); }
            return;
        }
        if (cmd == "focus") {   // dbg focus [full|empty|<0-300>]: show or set the Focus meter
            float v = -1;
            if (args == "full") v = 300; else if (args == "empty") v = 0; else if (!args.empty()) v = (float)atof(args.c_str());
            gamethread::Run([v] {
                uintptr_t m = FocusManager();
                uint32_t n = 0;
                if (!m || !memory::Read(m + 0x128, n)) { LOG("dbg: no Focus component"); return; }
                for (uint32_t i = 0; i < n && i < 3; i++) {
                    float f = 0, cap = 0;
                    memory::Read(m + 0x110 + 8 * i, f);
                    memory::Read(m + 0x114 + 8 * i, cap);
                    if (v >= 0) { f = std::max(0.0f, std::min(cap, v - 100.0f * i)); memory::Write<float>(m + 0x110 + 8 * i, f); }
                    LOG("dbg: focus segment %u: %.1f / %.1f", i, f, cap);
                }
            }, 5000);
            return;
        }
        LOG("dbg: give <boon> | curse <name> | power <name> | mod <name> | clear | list | status | cleared [wave] [boss] | heal <frac> | focus [full|empty|N] | begin | end");
    }

    void Debug(const std::string& args) { std::thread([args] { DebugRun(args); }).detach(); }

    void Update() {
        if (!g_active) return;
        static DWORD last = 0, last_focus = 0;
        DWORD now = GetTickCount();
        if (now - last_focus >= 50) { last_focus = now; FocusTick(); }
        if (!g_stacks[(int)Stat::Machinery] || now - last < 1000) return;
        last = now;
        HealHero(0.01f, 0.25f);
    }

    bool Offering() { return g_offering; }

    static void Show(const Offer& o) {
        static const input::Action acts[] = { input::Action::Choice1, input::Action::Choice2, input::Action::Choice3,
                                              input::Action::Choice4 };
        // One message, one option per line (HUD slots 2-6 share one spot on screen and overwrite each other, and a
        // single line wraps unevenly: user, 2026-10-13). Controllers get the game's button icons, keyboards "[1]".
        // (Padding with blank lines got the box cut off: 2026-10-13. A proper choice screen is a UI-hunt item.)
        std::string text = o.title;
        for (size_t i = 0; i < o.options.size() && i < 4; i++)
            text += std::string("\n") + input::Glyph(acts[i]) + "  " + o.options[i].label;
        hud::Show(text, 0.0f);   // stays until the pick clears it (user: it went away too soon)
    }

    static void Apply(const Option& op) {
        std::string what;
        switch (op.kind) {
        case Kind::Power:
            runloadout::SetPower(op.id);
            what = std::string("Suit power: ") + Label(op.id);
            break;
        case Kind::Mod:
            runloadout::AddMod(op.id);
            what = std::string("Suit mod: ") + Label(op.id);
            break;
        case Kind::Stat:
            g_stacks[op.index]++;
            what = std::string(kStats[op.index].name) + " " + std::to_string(g_stacks[op.index]) + "/" +
                   std::to_string(kStats[op.index].max_stacks);
            break;
        case Kind::Curse:
            g_curses.push_back(kCurses[op.index].curse);
            what = std::string("Curse: ") + kCurses[op.index].name;
            break;
        case Kind::Decline:
            what = "No deal";
            break;
        }
        if (op.kind != Kind::Decline) g_taken.push_back(what);
        stats::Event("boon", what);
        LOG("boons: took %s", what.c_str());
        hud::Show(what, 3.0f);
    }

    void Tick() {
        if (!g_offering) return;
        static const input::Action acts[] = { input::Action::Choice1, input::Action::Choice2, input::Action::Choice3,
                                              input::Action::Choice4 };
        input::Update();
        Option chosen{ Kind::Decline };
        bool have = false, repeat = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_queue.empty()) { g_offering = false; return; }
            Offer& o = g_queue.front();
            DWORD now = GetTickCount();
            if (o.ready_at && (int)(now - o.ready_at) < 0) return;   // still the post-wave breather
            if (!o.shown_at) {
                o.shown_at = now;
                if (!g_frozen) { g_frozen = true; timescale::Freeze(); }   // like the pause menu: the fight waits
            }
            for (size_t i = 0; i < o.options.size() && i < 4; i++)
                if (input::Pressed(acts[i])) { chosen = o.options[i]; have = true; break; }
            if (!have && now - o.shown_at > kOfferTimeoutMs) {
                // Nobody's picking (AFK): curses default to "No deal", rewards to a random option.
                chosen = (o.curse && o.options.back().kind == Kind::Decline) ? o.options.back()
                                 : o.options[std::uniform_int_distribution<size_t>(0, o.options.size() - 1)(g_rng)];
                have = true;
                LOG("boons: offer timed out");
            }
            if (!have && (!g_last_refresh || now - g_last_refresh > 8000)) {   // shown once; re-shown rarely in case
                g_last_refresh = now;                                       // something else replaced it
                Show(o);
            }
            if (have) { repeat = g_queue.front().repeat; g_queue.pop_front(); }
        }
        if (have) {
            hud::Clear();
            Apply(chosen);   // may block on game-thread work (equips)
            std::lock_guard<std::mutex> lock(g_mutex);
            if (repeat) {   // Clone Saga: a second, fresh reward
                Offer again = MakeRewardOffer();
                if (!again.options.empty()) { again.title = "Clone Saga: a second reward"; g_queue.push_front(again); }
            }
            g_offering = !g_queue.empty();
            g_last_refresh = 0;
            if (!g_offering && g_frozen) { g_frozen = false; timescale::Unfreeze(); }
        }
    }

    void AdjustWave(plan::WavePlan& p, int horde_max) {
        if (!g_active) return;
        g_tingle_left = g_stacks[(int)Stat::SpiderSense];
        if (g_stacks[(int)Stat::FullTank]) g_fill_pending = true;
        if (HasCurse(Curse::CloneSaga) && !p.bosses.empty()) p.bosses.push_back(p.bosses.front());   // a second copy
        if (HasCurse(Curse::RiotCall)) {
            p.count = (int)std::ceil(p.count * (1.0f + 0.4f * CurseStrength()));
            p.max_alive = std::min(p.max_alive + 4, std::max(8, horde_max));
        }
        if (HasCurse(Curse::OpenSeason)) {
            using roster::Role;
            p.weights[(int)Role::Sniper] = std::max(p.weights[(int)Role::Sniper], 1.0f);
            p.weights[(int)Role::Rocket] = std::max(p.weights[(int)Role::Rocket], 1.0f);
            p.caps[(int)Role::Sniper] = std::max(p.caps[(int)Role::Sniper], 2);
            p.caps[(int)Role::Rocket] = std::max(p.caps[(int)Role::Rocket], 2);
        }
        if (HasCurse(Curse::BloodPrice)) p.elite_chance = std::min(0.9f, std::max(0.1f, p.elite_chance * (1.0f + CurseStrength())));
    }

    std::vector<std::string> Summary() {
        std::lock_guard<std::mutex> lock(g_mutex);
        std::vector<std::string> out;
        uint64_t cur[5];
        inventory::Loadout(cur);
        out.push_back(std::string("Power: ") + (cur[1] ? Label(cur[1]) : "none"));
        for (int s = 2; s <= 4; s++) out.push_back(std::string("Mod ") + std::to_string(s - 1) + ": " + (cur[s] ? Label(cur[s]) : "empty"));
        for (int i = 0; i < (int)std::size(kStats); i++)
            if (g_stacks[i]) out.push_back(std::string(kStats[i].name) + " x" + std::to_string(g_stacks[i]));
        for (Curse c : g_curses)
            for (const auto& d : kCurses)
                if (d.curse == c) out.push_back(std::string("Curse: ") + d.name);
        return out;
    }
}
