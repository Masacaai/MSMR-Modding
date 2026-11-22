#include "mute.h"

#include "log.h"

#include <MinHook.h>
#include <Windows.h>
#include <atomic>
#include <cstdio>

namespace mute {
namespace {
    constexpr const char* kPostEventId = "?PostEvent@SoundEngine@AK@@YAKK_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z";
    constexpr const char* kPostEventStr = "?PostEvent@SoundEngine@AK@@YAKPEBD_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z";
    using PostIdFn = uint32_t (*)(uint32_t, uint64_t, uint32_t, void*, void*, uint32_t, void*, uint32_t);
    using PostStrFn = uint32_t (*)(const char*, uint64_t, uint32_t, void*, void*, uint32_t, void*, uint32_t);
    PostIdFn o_id = nullptr;
    PostStrFn o_str = nullptr;
    std::atomic<uint64_t> g_obj[4] = {};
    std::atomic<bool> g_hooked{ false }, g_tried{ false };
    std::atomic<int> g_logged{ 0 }, g_dropped{ 0 };
    std::atomic<ULONGLONG> g_window_until{ 0 };

    // Wwise event ids are the FNV-1 hash of the lower-cased name. The decoy's voice lines are the events SPID_GEN_HOLO_DECOY_001..022 (the tags in its dialog
    // component; all 22 names exist in the game's event lookup), posted on whichever object the speaker uses: dropped by id, wherever they are posted.
    uint32_t Fnv(const char* name) {
        uint32_t h = 2166136261u;
        for (const unsigned char* p = (const unsigned char*)name; *p; ++p) { unsigned char c = *p; if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32); h = (h * 16777619u) ^ c; }
        return h;
    }
    uint32_t g_voice[32];
    int g_voice_n = 0;
    void InitVoice() {
        if (g_voice_n) return;
        for (int i = 1; i <= 22; i++) { char n[40]; snprintf(n, sizeof(n), "SPID_GEN_HOLO_DECOY_%03d", i); g_voice[g_voice_n++] = Fnv(n); }
    }
    bool VoiceId(uint32_t id) { for (int i = 0; i < g_voice_n; i++) if (g_voice[i] == id) return true; return false; }
    bool VoiceName(const char* n) { return n && _strnicmp(n, "SPID_GEN_HOLO_DECOY", 19) == 0; }

    bool Muted(uint64_t obj) {
        if (!obj) return false;
        for (auto& o : g_obj) if (o.load() == obj) return true;
        return false;
    }

    uint32_t IdDetour(uint32_t id, uint64_t obj, uint32_t flags, void* cb, void* cookie, uint32_t ne, void* ext, uint32_t pid) {
        const bool drop = Muted(obj) || VoiceId(id);
        if (drop || (GetTickCount64() < g_window_until && g_logged++ < 60)) LOG("mute: PostEvent id=%08x obj=%llx %s", id, (unsigned long long)obj, drop ? "DROPPED" : "(kept)");
        if (drop) { g_dropped++; return 0; }
        return o_id(id, obj, flags, cb, cookie, ne, ext, pid);
    }
    uint32_t StrDetour(const char* name, uint64_t obj, uint32_t flags, void* cb, void* cookie, uint32_t ne, void* ext, uint32_t pid) {
        const bool drop = Muted(obj) || VoiceName(name);
        if (drop || (GetTickCount64() < g_window_until && g_logged++ < 60)) LOG("mute: PostEvent name=%s obj=%llx %s", name ? name : "?", (unsigned long long)obj, drop ? "DROPPED" : "(kept)");
        if (drop) { g_dropped++; return 0; }
        return o_str(name, obj, flags, cb, cookie, ne, ext, pid);
    }

    bool Hook(const char* name, void* detour, void** original) {
        void* target = (void*)GetProcAddress(GetModuleHandleA(nullptr), name);
        if (!target) { LOG("mute: export not found: %.60s", name); return false; }
        if (MH_CreateHook(target, detour, original) != MH_OK || MH_EnableHook(target) != MH_OK) { LOG("mute: hook failed: %.60s", name); return false; }
        return true;
    }

    void EnsureHooked() {
        if (g_tried.exchange(true)) return;
        InitVoice();
        const MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) { LOG("mute: MH_Initialize failed (%d)", st); return; }
        const bool a = Hook(kPostEventId, (void*)&IdDetour, (void**)&o_id);
        const bool b = Hook(kPostEventStr, (void*)&StrDetour, (void**)&o_str);
        g_hooked = a || b;
        LOG("mute: PostEvent hooks %s", g_hooked ? "ok" : "unavailable");
    }
}

    void SetObjects(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
        EnsureHooked();
        g_obj[0] = a; g_obj[1] = b; g_obj[2] = c; g_obj[3] = d;
        g_logged = 0;
        g_window_until = GetTickCount64() + 6000;   // log every sound posted in the next 6 s, kept or dropped, so a miss can be traced
        LOG("mute: silencing game objects %llx %llx %llx %llx", (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d);
    }

    void Clear() {
        for (auto& o : g_obj) o = 0;
        if (g_dropped) LOG("mute: %d sound(s) dropped in all", g_dropped.load());
    }
}
