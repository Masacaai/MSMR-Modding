#include "music.h"

#include "audio.h"
#include "log.h"
#include "settings.h"

#include <Windows.h>
#include <atomic>
#include <thread>

namespace music {
    namespace {
        // State ids of I20_MUS_OW_A1, by what the game was doing when it set them (audio recording 2026-11-07).
        constexpr uint32_t kIdle = 0x03c3edb8;                                   // free roam, nothing happening
        constexpr uint32_t kLadder[3] = { 0x77f0d3b8, 0x14135cdd, 0xb3963542 };   // a hideout's first, second and later waves
        constexpr uint32_t kLastHit = 0x3ccb45df;                                 // the last enemy of an encounter falls
        constexpr uint32_t kCooldown1 = 0x8a303e2a, kCooldown2 = 0x942c7f38, kDefault = 0xbdf29433;
        constexpr uint64_t kObject = 0x45415f4d55534943ull;                       // our own Wwise game object ("EA_MUSIC")

        std::atomic<bool> g_on{ false };
        std::atomic<int> g_level{ -1 };
        uint32_t g_group = 0;
        void Watch();

        void Begin() {
            if (g_on.exchange(true)) return;
            audio::RegisterObject(kObject, "EndlessArena_Music");
            g_group = audio::Id("I20_MUS_OW_A1");
            audio::PostId(audio::Id("Set_GCT_TrainCombatValue_None"), kObject);
            audio::PostId(audio::Id("Stop_MUS_OW_A3"), kObject);
            audio::PostId(audio::Id("Play_MUS_UNMUTE_ALL"), kObject);
            audio::PostId(audio::Id("Play_I20_MUS_OW_A1"), kObject);
            audio::PostId(audio::Id("Stop_MUS_OW_A2"), kObject);
            g_level = -1;
            LOG("music: started");
            Watch();
        }

        bool Enabled() { return settings::On("music"); }

        // The game's music segments end by themselves (it re-triggers them as a fight goes on): if nothing is playing on our object while a run
        // is on, start it again at the current level. Seen 2026-11-07: the music stopped in the middle of a long wave.
        std::atomic<bool> g_watching{ false };
        void Watch() {
            if (g_watching.exchange(true)) return;
            std::thread([] {
                int last = -2, silent = 0;
                for (int tick = 0; g_on; tick++) {
                    Sleep(2000);
                    if (!g_on) break;
                    const int n = audio::PlayingCount(kObject);
                    if (n != last && tick < 200) LOG("music: %d sound(s) playing", n);
                    last = n;
                    if (n == 0) {
                        if (++silent >= 2) {   // two checks in a row
                            const int lv = g_level < 0 ? 0 : g_level.load();
                            audio::PostId(audio::Id("Play_I20_MUS_OW_A1"), kObject);
                            audio::SetStateId(g_group, kLadder[lv]);
                            audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
                            LOG("music: silent for 4 s, restarted at level %d", lv);
                            silent = 0;
                        }
                    } else {
                        silent = 0;
                    }
                }
                g_watching = false;
            }).detach();
        }
    }

    void WaveStart(int tier, bool boss) {
        if (!Enabled()) return;
        Begin();
        if (tier < 0) tier = 0;
        if (tier > 2) tier = 2;
        if (boss) tier = 2;
        audio::SetStateId(g_group, kLadder[tier]);
        audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
        if (boss) audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
        g_level = tier;
        LOG("music: wave tier %d%s", tier, boss ? " (boss)" : "");
    }

    void WaveCleared() {
        if (!g_on) return;
        audio::SetStateId(g_group, kLastHit);
        std::thread([] {
            Sleep(3000);
            if (g_on) { audio::SetStateId(g_group, kCooldown1); Sleep(500); audio::SetStateId(g_group, kCooldown2); }
        }).detach();
    }

    void Sting() {
        if (!g_on) return;
        audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
    }

    void End() {
        if (!g_on.exchange(false)) return;
        audio::SetStateId(g_group, kLastHit);
        std::thread([] {
            Sleep(2500);
            audio::SetStateId(g_group, kCooldown1);
            Sleep(500);
            audio::SetStateId(g_group, kCooldown2);
            Sleep(8000);
            audio::SetStateId(g_group, kDefault);
            Sleep(500);
            audio::SetStateId(g_group, 0);
            audio::PostId(audio::Id("Stop_MUS_all_switch_groups"), kObject);
            LOG("music: stopped");
        }).detach();
    }
}
