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

        // A state's music runs out after 40-80 s and then goes quiet while the sound still counts as playing (seen 2026-11-07: it stopped in the
        // middle of a long wave; re-posting the start event did nothing, but the next wave's state change brought it back). The game keeps a
        // hideout going by changing state and stinging every so often, so a wave that goes on does the same: every kRefreshMs it steps up the
        // ladder, and at the top alternates the two highest states.
        constexpr DWORD kRefreshMs = 35000;
        std::atomic<bool> g_watching{ false }, g_wave{ false };
        std::atomic<DWORD> g_changed{ 0 };
        void Watch() {
            if (g_watching.exchange(true)) return;
            std::thread([] {
                while (g_on) {
                    Sleep(1000);
                    if (!g_on) break;
                    if (!g_wave || GetTickCount() - g_changed < kRefreshMs) continue;
                    // 0 -> 1 -> 2 -> 1 -> 2 ...: always a real change (a refresh to the same state did nothing: seen as "2 -> 2")
                    const int cur = g_level < 0 ? 0 : g_level.load();
                    const int next = cur == 0 ? 1 : cur == 1 ? 2 : 1;
                    audio::SetStateId(g_group, kLadder[next]);
                    audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
                    LOG("music: refresh, level %d -> %d", g_level.load(), next);
                    g_level = next;
                    g_changed = GetTickCount();
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
        g_wave = true;
        g_changed = GetTickCount();
        LOG("music: wave tier %d%s", tier, boss ? " (boss)" : "");
    }

    void WaveCleared() {
        if (!g_on) return;
        g_wave = false;
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
        g_wave = false;
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
