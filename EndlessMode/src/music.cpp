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
        }

        bool Enabled() { return settings::On("music"); }
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
