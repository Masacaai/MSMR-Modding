#include "music.h"

#include "audio.h"
#include "gamethread.h"
#include "log.h"
#include "settings.h"

#include <Windows.h>
#include <atomic>
#include <thread>

namespace music {
    namespace {
        // State ids of I20_MUS_OW_A1, by what the game was doing when it set them (audio recording 2026-11-07).
        constexpr uint32_t kLadder[3] = { 0x77f0d3b8, 0x14135cdd, 0xb3963542 };   // a hideout's first, second and later waves
        constexpr uint32_t kLastHit = 0x3ccb45df;                                 // the last enemy of an encounter falls
        constexpr uint32_t kDefault = 0xbdf29433;                                 // the state the game leaves the group in when it is done
        constexpr uint64_t kObject = 0x45415f4d55534943ull;                       // our own Wwise game object ("EA_MUSIC")

        std::atomic<bool> g_on{ false }, g_wave{ false }, g_watching{ false };
        std::atomic<int> g_level{ -1 };
        std::atomic<DWORD> g_changed{ 0 };
        uint32_t g_group = 0;

        // Every call into the audio engine runs on the game thread, in its own tick - none from the director's or a helper thread, and none on a
        // timer after a run is over (a crash 11 s after a run ended, 2026-11-07, matched the old delayed shutdown thread).

        // A state's music runs out after 40-80 s and then goes quiet while the sound still counts as playing (seen 2026-11-07: it stopped in the
        // middle of a long wave; re-posting the start event did nothing, but the next wave's state change brought it back). The game keeps a
        // hideout going by changing state and stinging every so often, so a wave that goes on does the same: every kRefreshMs it goes 0 -> 1 -> 2 -> 1
        // -> 2 ... (always a real change).
        constexpr DWORD kRefreshMs = 35000;
        void Watch() {
            if (g_watching.exchange(true)) return;
            std::thread([] {   // only sleeps and decides; the audio calls are posted to the game thread
                while (g_on) {
                    Sleep(1000);
                    if (!g_on) break;
                    if (!g_wave || GetTickCount() - g_changed < kRefreshMs) continue;
                    const int cur = g_level < 0 ? 0 : g_level.load();
                    const int next = cur == 0 ? 1 : cur == 1 ? 2 : 1;
                    g_level = next;
                    g_changed = GetTickCount();
                    LOG("music: refresh, level %d -> %d", cur, next);
                    gamethread::Post([next] {
                        if (!g_on) return;
                        audio::SetStateId(g_group, kLadder[next]);
                        audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
                    });
                }
                g_watching = false;
            }).detach();
        }

        void Begin() {
            if (g_on.exchange(true)) return;
            g_level = -1;
            gamethread::Post([] {
                audio::RegisterObject(kObject, "EndlessArena_Music");
                g_group = audio::Id("I20_MUS_OW_A1");
                audio::PostId(audio::Id("Set_GCT_TrainCombatValue_None"), kObject);
                audio::PostId(audio::Id("Stop_MUS_OW_A3"), kObject);
                audio::PostId(audio::Id("Play_MUS_UNMUTE_ALL"), kObject);
                audio::PostId(audio::Id("Play_I20_MUS_OW_A1"), kObject);
                audio::PostId(audio::Id("Stop_MUS_OW_A2"), kObject);
            });
            LOG("music: started");
            Watch();
        }

        bool Enabled() { return settings::On("music"); }
    }

    void WaveStart(int tier, bool boss) {
        if (!Enabled()) return;
        Begin();
        if (tier < 0) tier = 0;
        if (tier > 2) tier = 2;
        if (boss) tier = 2;
        g_level = tier;
        g_wave = true;
        g_changed = GetTickCount();
        gamethread::Post([tier, boss] {
            if (!g_on) return;
            audio::SetStateId(g_group, kLadder[tier]);
            audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
            if (boss) audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject);
        });
        LOG("music: wave tier %d%s", tier, boss ? " (boss)" : "");
    }

    void WaveCleared() {
        if (!g_on) return;
        g_wave = false;
        gamethread::Post([] { if (g_on) audio::SetStateId(g_group, kLastHit); });
    }

    void Sting() {
        if (!g_on) return;
        gamethread::Post([] { if (g_on) audio::PostId(audio::Id("Play_MUS_combat_stinger"), kObject); });
    }

    // One tick, no timers: the group goes to the game's "done" state and everything music stops.
    void End() {
        if (!g_on.exchange(false)) return;
        g_wave = false;
        gamethread::Post([] {
            audio::SetStateId(g_group, kDefault);
            audio::PostId(audio::Id("Stop_MUS_all_switch_groups"), kObject);
            LOG("music: stopped");
        });
    }
}
