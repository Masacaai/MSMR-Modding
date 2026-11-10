// Offline checks of the late-run chapter (src/SECRET.md) over many seeds and settings, printing only a summary and the first few failures.
// Build/run: tests\run_sinister.cmd
#include "../src/plan.h"
#include "../src/roster.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <set>
#include <string>

static int g_fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { if (g_fail++ < 12) { printf("FAIL %s:%d: ", #cond, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

int main() {
    long configs = 0, waves = 0;
    const int pers[] = { 2, 3, 4, 5, 6, 8, 10 };
    for (uint32_t seed = 1; seed <= 150; seed++) {
        for (int per : pers) {
            for (int variant = 0; variant < 4; variant++) {
                std::mt19937 rng(seed);
                plan::Config cfg;
                cfg.order = plan::CampaignOrder(seed % 2 == 0, rng);
                const int F = (int)cfg.order.size();
                cfg.waves_per_chapter = per;
                cfg.allow_unverified_bosses = true;
                cfg.horde_max = 40 + 10 * (seed % 7);
                if (variant == 1) { cfg.count_mult = 0.5f; cfg.crew_mult = 0.0f; }
                if (variant == 2) { cfg.count_mult = 3.0f; cfg.crew_mult = 3.0f; cfg.boss_count = 2; }
                if (variant == 3) { cfg.role_mult[(int)roster::Role::Pistol] = 0; cfg.role_mult[(int)roster::Role::Rifle] = 0; cfg.elite_mult = 0; cfg.riot = (seed % 3 == 0); }
                plan::BuildSinister(cfg, seed % 5 != 0);   // one run in five has no Kingpin
                const size_t want_bosses = seed % 5 != 0 ? 6 : 5;
                for (int at : { 1, 2, F, F + 1 }) {
                    cfg.sinister_at = at;
                    configs++;
                    std::set<std::string> crowd_paths;
                    for (const auto& u : cfg.sinister->units) crowd_paths.insert(u.path);
                    CHECK(crowd_paths.size() <= 30 && crowd_paths.size() >= 12, "crowd of %zu unit types", crowd_paths.size());
                    for (int w = 1; w <= per * (F + 3); w++) {
                        waves++;
                        std::mt19937 r2(seed * 7 + w);
                        plan::WavePlan p = plan::Make(cfg, w, r2);
                        const int raw = (w - 1) / per, step = (w - 1) % per;
                        CHECK(p.faction != nullptr, "w%d no faction", w);
                        CHECK(p.sinister == (raw == at), "w%d sinister flag %d (raw chapter %d, at %d)", w, p.sinister, raw, at);
                        if (!p.faction) continue;
                        if (raw == at) {
                            CHECK(p.step == step, "w%d step %d != %d", w, p.step, step);
                            if (step == per - 1) {
                                const bool crewless = cfg.crew_mult <= 0;
                                CHECK(p.bosses.size() == (variant == 2 ? want_bosses : want_bosses), "w%d finale has %zu bosses, want %zu", w, p.bosses.size(), want_bosses);
                                std::set<std::string> names;
                                for (auto* b : p.bosses) names.insert(b->name);
                                CHECK(names.size() == p.bosses.size(), "w%d duplicate boss", w);
                                CHECK(p.boss_wave, "w%d finale is not a boss wave", w);
                                CHECK(crewless ? p.count == 0 : p.count >= 18, "w%d finale crew %d", w, p.count);
                                CHECK(p.max_alive >= (int)p.bosses.size() + 4, "w%d finale max_alive %d", w, p.max_alive);
                                CHECK(p.max_alive <= std::max(cfg.horde_max, (int)p.bosses.size() + 12), "w%d finale max_alive %d over the cap %d", w, p.max_alive, cfg.horde_max);
                            } else {
                                CHECK(p.bosses.empty(), "w%d boss in a goon wave", w);
                                CHECK(p.count >= 1, "w%d empty wave", w);
                                CHECK(p.max_alive >= 8 && p.max_alive <= std::max(8, cfg.horde_max), "w%d max_alive %d (cap %d)", w, p.max_alive, cfg.horde_max);
                            }
                            // every goon draw finds a unit from the crowd
                            std::array<int, plan::kRoles> alive{};
                            for (int k = 0; k < 60; k++) {
                                const roster::Unit* u = plan::PickUnit(p, alive, r2);
                                CHECK(u != nullptr, "w%d PickUnit null", w);
                                if (u) CHECK(crowd_paths.count(u->path), "w%d unit %s not from the crowd", w, u->path);
                            }
                        } else {
                            const int k = raw > at ? raw - 1 : raw;   // chapters after the event carry on as if it never happened
                            CHECK(p.faction == cfg.order[k % F], "w%d faction %s, want %s (chapter %d)", w, p.faction->id, cfg.order[k % F]->id, k);
                            CHECK(p.step == step, "w%d step %d != %d", w, p.step, step);
                        }
                    }
                }
            }
        }
    }
    printf("%ld configs, %ld waves checked, %d failures\n", configs, waves, g_fail);

    // The chance counter: 20% at the start, x(1.02 + 0.0025 * wave) per cleared wave, one roll per chapter boundary (4 waves a chapter), a run
    // dies at wave D; the counter persists between runs. P(met in the first run) and the mean runs until it is met.
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    printf("died at wave   P(met in first run)   mean runs until met\n");
    for (int D : { 8, 12, 16, 24, 32, 48 }) {
        const int trials = 20000;
        int first = 0;
        double runs = 0;
        for (int t = 0; t < trials; t++) {
            double p = 0.2;
            int r = 0;
            for (;;) {
                r++;
                bool met = false;
                for (int w = 1; w <= D && !met; w++) {
                    if ((w - 1) % 4 == 0 && w > 1 && u(rng) < p) met = true;   // roll at a chapter boundary
                    else p = std::min(1.0, p * (1.0 + 0.02 + 0.0025 * w));
                }
                if (met) { if (r == 1) first++; break; }
                if (r > 200) break;
            }
            runs += r;
        }
        printf("%6d        %6.1f%%              %.2f\n", D, 100.0 * first / trials, runs / trials);
    }
    return g_fail ? 1 : 0;
}
