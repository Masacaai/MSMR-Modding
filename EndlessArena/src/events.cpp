#include "events.h"

#include "gamethread.h"
#include "log.h"
#include "natives.h"

#include <MinHook.h>
#include <Windows.h>
#include <atomic>
#include <cstdlib>
#include <map>

namespace events {
    using SendEventFn = uintptr_t (*)(uintptr_t sys, uint32_t type, uint32_t target, const uint32_t* source,
                                      int32_t a5, uint8_t a6, uint64_t a7, uint32_t a8, uint8_t a9, float f1,
                                      float f2, const void* payload, uint64_t payload_size);
    using GroupAlertFn = void (*)(uintptr_t group, uint32_t alerter, uint8_t flag);

    uint32_t TypeHash(const std::string& s) {
        static const std::map<std::string, uint32_t> known = {
            { "ScriptedGroupAlertEvent", 0x3A57F5AD },
            { "BotAlertGroupEvent", 0xF9C1D45E },
            { "BotAlertedEvent", 0xD8ABB208 },
            { "GroupAlertedEvent", 0x0FD8F64B },
            { "HeroPositionTriangulatedEvent", 0x2C438ACA },
            { "BotHeardLoudSoundEvent", 0x3D05C385 },
            { "DeathSeenEvent", 0x3E9BA834 },
            { "WeaponFiredEvent", 0xF6607648 },
            { "HeroSetAlertedEvent", 0x80B26DFF },
        };
        auto it = known.find(s);
        if (it != known.end()) return it->second;
        return (uint32_t)strtoul(s.c_str(), nullptr, 16);
    }

    bool Send(uint32_t type, uint32_t target, uint32_t source, float f1, float f2) {
        auto send = (SendEventFn)natives::Get(natives::SendEvent);
        uintptr_t sys = natives::Get(natives::EventSys);
        if (!send || !sys || !gamethread::Installed()) { LOG("event: not ready"); return false; }
        gamethread::Post([=] {
            uint32_t src = source;
            uintptr_t ev = send(sys, type, target, source ? &src : nullptr, 1, 0, 0, 0, 1, f1, f2, nullptr, 0);
            LOG("event: sent %08x to %08x (source %08x) -> %llx", type, target, source, (unsigned long long)ev);
        });
        return true;
    }

    bool AlertGroup(uintptr_t group, uint32_t alerter) {
        auto alert = (GroupAlertFn)natives::Get(natives::GroupAlert);
        if (!alert || !group || !gamethread::Installed()) { LOG("grpalert: not ready"); return false; }
        gamethread::Post([=] {
            alert(group, alerter, 1);
            LOG("grpalert: group %llx alerted by %08x", (unsigned long long)group, alerter);
        });
        return true;
    }

    // ---- recording ----
    static constexpr size_t kSlots = 8192;
    static std::atomic<uint32_t> g_keys[kSlots];
    static std::atomic<uint32_t> g_counts[kSlots];
    static bool g_logged[kSlots];
    static SendEventFn g_orig = nullptr;
    static std::atomic<bool> g_record{ false }, g_mission{ false };
    static std::atomic<uint32_t> g_mission_sys{ 0 };
    constexpr uint32_t kMissionStarted = 0x6E588F6D, kObjectiveStarted = 0xD358823E, kMissionComplete = 0xCC580B11, kCheckpointLoaded = 0x414AD37A,
                       kLevelLoaded = 0xFBE1B646, kSetPrimaryMission = 0xAE97E680;

    static uintptr_t Detour(uintptr_t sys, uint32_t type, uint32_t target, const uint32_t* source, int32_t a5, uint8_t a6, uint64_t a7, uint32_t a8,
                            uint8_t a9, float f1, float f2, const void* payload, uint64_t payload_size) {
        // Recon: every occurrence of the mission-related events, with target, source and the start of the payload (what a mid-mission load announces).
        if (g_record) {
            static const uint32_t watch[] = { 0x0504bf5e, 0x0f1b34e7, 0xdb953346, 0x520014d4, 0xae97e680, 0x5166b564, 0xd358823e, 0x6e588f6d, 0xcc580b11,
                                              0x24b2e9a7, 0x414ad37a, 0xfbe1b646, 0x84fc9200, 0x1169aecb, 0x3e870b52, 0x35d6494a };
            static std::atomic<int> lines{ 0 };
            for (uint32_t w : watch)
                if (w == type && lines++ < 150) {
                    char hex[100] = {};
                    int len = 0;
                    const unsigned char* pl = (const unsigned char*)payload;
                    for (uint64_t i = 0; pl && i < payload_size && i < 32; i++) len += snprintf(hex + len, sizeof(hex) - (size_t)len, "%02x", pl[i]);
                    LOG("events: mission event %08x target %08x source %08x payload(%llu) %s", type, target, source ? *source : 0, (unsigned long long)payload_size, hex);
                    break;
                }
        }
        if (type == 0x24b2e9a7) g_mission_sys = target;   // MissionSystemLoaded
        switch (type) {
        case kMissionStarted: case kObjectiveStarted: case kSetPrimaryMission:   // the last one is what a mid-mission checkpoint load announces (2026-10-25)
            if (!g_mission.exchange(true)) LOG("missions: a story mission is active (event %08x)", type);
            break;
        case kMissionComplete: case kCheckpointLoaded: case kLevelLoaded:
            if (g_mission.exchange(false)) LOG("missions: no story mission is active (event %08x)", type);
            break;
        default: break;
        }
        size_t i = (type * 2654435761u) & (kSlots - 1);
        for (size_t n = 0; g_record && n < kSlots; n++, i = (i + 1) & (kSlots - 1)) {
            uint32_t k = g_keys[i].load();
            if (k == type) { g_counts[i]++; break; }
            if (k == 0) {
                uint32_t expect = 0;
                if (g_keys[i].compare_exchange_strong(expect, type) || expect == type) { g_counts[i]++; break; }
            }
        }
        return g_orig(sys, type, target, source, a5, a6, a7, a8, a9, f1, f2, payload, payload_size);
    }

    bool StartTracking(bool record_types) {
        g_record = record_types;
        if (g_orig) return true;
        void* send = (void*)natives::Get(natives::SendEvent);
        if (!send) return false;
        MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return false;
        if (MH_CreateHook(send, (void*)&Detour, (void**)&g_orig) != MH_OK || MH_EnableHook(send) != MH_OK) return false;
        LOG("events: tracking story missions%s", record_types ? " and recording the event types" : "");
        return true;
    }

    bool StartRecording() { return StartTracking(true); }
    bool MissionActive() { return g_mission.load(); }
    uint32_t MissionSystemHandle() { return g_mission_sys.load(); }

    void LogNew() {
        for (size_t i = 0; i < kSlots; i++) {
            uint32_t k = g_keys[i].load();
            if (k && !g_logged[i]) { g_logged[i] = true; LOG("events: new %08x", k); }
        }
    }
}
