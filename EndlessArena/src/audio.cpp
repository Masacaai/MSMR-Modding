#include "audio.h"

#include "log.h"
#include "paths.h"
#include "scan.h"

#include <MinHook.h>
#include <Windows.h>
#include <atomic>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <map>
#include <mutex>

namespace audio {
namespace {
    // The exported Wwise API (decorated C++ names, as the exe exports them).
    constexpr const char* kPostEventId = "?PostEvent@SoundEngine@AK@@YAKK_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z";
    constexpr const char* kPostEventStr = "?PostEvent@SoundEngine@AK@@YAKPEBD_KKP6AXW4AkCallbackType@@PEAUAkCallbackInfo@@@ZPEAXKPEAUAkExternalSourceInfo@@K@Z";
    constexpr const char* kSetStateId = "?SetState@SoundEngine@AK@@YA?AW4AKRESULT@@KK@Z";
    constexpr const char* kSetStateStr = "?SetState@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0@Z";
    constexpr const char* kSetSwitchId = "?SetSwitch@SoundEngine@AK@@YA?AW4AKRESULT@@KK_K@Z";
    constexpr const char* kSetSwitchStr = "?SetSwitch@SoundEngine@AK@@YA?AW4AKRESULT@@PEBD0_K@Z";
    constexpr const char* kSetRtpcId = "?SetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@KM_KJW4AkCurveInterpolation@@_N@Z";
    constexpr const char* kSetRtpcStr = "?SetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@PEBDM_KJW4AkCurveInterpolation@@_N@Z";
    constexpr const char* kResetRtpcId = "?ResetRTPCValue@SoundEngine@AK@@YA?AW4AKRESULT@@K_KJW4AkCurveInterpolation@@_N@Z";
    constexpr const char* kExecuteId = "?ExecuteActionOnEvent@SoundEngine@AK@@YA?AW4AKRESULT@@KW4AkActionOnEventType@12@_KJW4AkCurveInterpolation@@K@Z";
    constexpr const char* kStopPlayingId = "?StopPlayingID@SoundEngine@AK@@YAXKJW4AkCurveInterpolation@@@Z";
    constexpr const char* kPostTriggerId = "?PostTrigger@SoundEngine@AK@@YA?AW4AKRESULT@@K_K@Z";
    constexpr const char* kStopAll = "?StopAll@SoundEngine@AK@@YAX_K@Z";

    using PostIdFn = uint32_t (*)(uint32_t, uint64_t, uint32_t, void*, void*, uint32_t, void*, uint32_t);
    using PostStrFn = uint32_t (*)(const char*, uint64_t, uint32_t, void*, void*, uint32_t, void*, uint32_t);
    using StateIdFn = int (*)(uint32_t, uint32_t);
    using StateStrFn = int (*)(const char*, const char*);
    using SwitchIdFn = int (*)(uint32_t, uint32_t, uint64_t);
    using SwitchStrFn = int (*)(const char*, const char*, uint64_t);
    using RtpcIdFn = int (*)(uint32_t, float, uint64_t, int32_t, int, bool);
    using RtpcStrFn = int (*)(const char*, float, uint64_t, int32_t, int, bool);
    using ResetRtpcFn = int (*)(uint32_t, uint64_t, int32_t, int, bool);
    using ExecuteFn = int (*)(uint32_t, int, uint64_t, int32_t, int, uint32_t);
    using StopPlayingFn = void (*)(uint32_t, int32_t, int);
    using PostTriggerFn = int (*)(uint32_t, uint64_t);
    using StopAllFn = void (*)(uint64_t);

    PostIdFn o_post_id; PostStrFn o_post_str; StateIdFn o_state_id; StateStrFn o_state_str; SwitchIdFn o_switch_id;
    SwitchStrFn o_switch_str; RtpcIdFn o_rtpc_id; RtpcStrFn o_rtpc_str; ResetRtpcFn o_reset_rtpc; ExecuteFn o_execute;
    StopPlayingFn o_stop_playing; PostTriggerFn o_trigger; StopAllFn o_stop_all;

    std::atomic<bool> g_recording{ false }, g_hooked{ false };
    std::mutex g_mutex;
    FILE* g_file = nullptr;
    ULONGLONG g_start = 0;
    thread_local bool t_ours = false;   // a call the mod makes itself

    uintptr_t Rva(void* ret) { return (uintptr_t)ret - scan::Game().base; }

    void Line(const char* fmt, ...) {
        if (!g_recording) return;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_file) return;
        fprintf(g_file, "%8llu ", (unsigned long long)(GetTickCount64() - g_start));
        va_list ap;
        va_start(ap, fmt);
        vfprintf(g_file, fmt, ap);
        va_end(ap);
        fputc('\n', g_file);
        fflush(g_file);
    }

    uint32_t PostIdDetour(uint32_t id, uint64_t obj, uint32_t flags, void* cb, void* cookie, uint32_t ne, void* ext, uint32_t pid) {
        void* ret = _ReturnAddress();
        uint32_t r = o_post_id(id, obj, flags, cb, cookie, ne, ext, pid);
        Line("PostEvent   id=%08x obj=%llx flags=%x cb=%d -> play=%u %s @%llx", id, (unsigned long long)obj, flags, cb != nullptr, r,
             t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    uint32_t PostStrDetour(const char* name, uint64_t obj, uint32_t flags, void* cb, void* cookie, uint32_t ne, void* ext, uint32_t pid) {
        void* ret = _ReturnAddress();
        uint32_t r = o_post_str(name, obj, flags, cb, cookie, ne, ext, pid);
        Line("PostEvent   name=%s obj=%llx flags=%x -> play=%u %s @%llx", name ? name : "?", (unsigned long long)obj, flags, r,
             t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    int StateIdDetour(uint32_t g, uint32_t s) {
        void* ret = _ReturnAddress();
        int r = o_state_id(g, s);
        Line("SetState    group=%08x state=%08x -> %d %s @%llx", g, s, r, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    int StateStrDetour(const char* g, const char* s) {
        void* ret = _ReturnAddress();
        int r = o_state_str(g, s);
        Line("SetState    group=%s state=%s -> %d %s @%llx", g ? g : "?", s ? s : "?", r, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    int SwitchIdDetour(uint32_t g, uint32_t s, uint64_t obj) {
        void* ret = _ReturnAddress();
        int r = o_switch_id(g, s, obj);
        Line("SetSwitch   group=%08x switch=%08x obj=%llx -> %d %s @%llx", g, s, (unsigned long long)obj, r, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    int SwitchStrDetour(const char* g, const char* s, uint64_t obj) {
        void* ret = _ReturnAddress();
        int r = o_switch_str(g, s, obj);
        Line("SetSwitch   group=%s switch=%s obj=%llx -> %d %s @%llx", g ? g : "?", s ? s : "?", (unsigned long long)obj, r, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }

    // RTPC values can change every frame: logged when they moved by more than 0.5% of 100 or after a second.
    bool RtpcWorthLogging(uint32_t id, uint64_t obj, float value) {
        static std::map<std::pair<uint32_t, uint64_t>, std::pair<float, ULONGLONG>> last;
        std::lock_guard<std::mutex> lock(g_mutex);
        auto& e = last[{ id, obj }];
        ULONGLONG now = GetTickCount64();
        if (std::fabs(e.first - value) < 0.5f && now - e.second < 1000 && e.second) return false;
        e = { value, now };
        return true;
    }
    int RtpcIdDetour(uint32_t id, float v, uint64_t obj, int32_t ms, int curve, bool bypass) {
        void* ret = _ReturnAddress();
        int r = o_rtpc_id(id, v, obj, ms, curve, bypass);
        if (g_recording && RtpcWorthLogging(id, obj, v))
            Line("SetRTPC     id=%08x value=%.3f obj=%llx fade=%dms -> %d %s @%llx", id, v, (unsigned long long)obj, ms, r, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    int RtpcStrDetour(const char* name, float v, uint64_t obj, int32_t ms, int curve, bool bypass) {
        void* ret = _ReturnAddress();
        int r = o_rtpc_str(name, v, obj, ms, curve, bypass);
        Line("SetRTPC     name=%s value=%.3f obj=%llx fade=%dms -> %d %s @%llx", name ? name : "?", v, (unsigned long long)obj, ms, r, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    int ResetRtpcDetour(uint32_t id, uint64_t obj, int32_t ms, int curve, bool bypass) {
        void* ret = _ReturnAddress();
        int r = o_reset_rtpc(id, obj, ms, curve, bypass);
        Line("ResetRTPC   id=%08x obj=%llx fade=%dms -> %d @%llx", id, (unsigned long long)obj, ms, r, (unsigned long long)Rva(ret));
        return r;
    }
    int ExecuteDetour(uint32_t id, int action, uint64_t obj, int32_t ms, int curve, uint32_t pid) {
        void* ret = _ReturnAddress();
        int r = o_execute(id, action, obj, ms, curve, pid);
        Line("ExecuteAction event=%08x action=%d obj=%llx fade=%dms play=%u -> %d %s @%llx", id, action, (unsigned long long)obj, ms, pid, r,
             t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
        return r;
    }
    void StopPlayingDetour(uint32_t pid, int32_t ms, int curve) {
        void* ret = _ReturnAddress();
        o_stop_playing(pid, ms, curve);
        Line("StopPlaying play=%u fade=%dms %s @%llx", pid, ms, t_ours ? "[mod]" : "", (unsigned long long)Rva(ret));
    }
    int TriggerDetour(uint32_t id, uint64_t obj) {
        void* ret = _ReturnAddress();
        int r = o_trigger(id, obj);
        Line("PostTrigger id=%08x obj=%llx -> %d @%llx", id, (unsigned long long)obj, r, (unsigned long long)Rva(ret));
        return r;
    }
    void StopAllDetour(uint64_t obj) {
        void* ret = _ReturnAddress();
        o_stop_all(obj);
        Line("StopAll     obj=%llx @%llx", (unsigned long long)obj, (unsigned long long)Rva(ret));
    }

    void* Export(const char* name) { return (void*)GetProcAddress(GetModuleHandleA(nullptr), name); }

    bool Hook(const char* name, void* detour, void** original) {
        void* target = Export(name);
        if (!target) { LOG("audio: export not found: %.60s", name); return false; }
        if (MH_CreateHook(target, detour, original) != MH_OK || MH_EnableHook(target) != MH_OK) { LOG("audio: hook failed: %.60s", name); return false; }
        return true;
    }

    // The unhooked calls, for posting (the hooks' trampolines when hooked, else the exports).
    template <class F> F Fn(F hooked, const char* name) { return hooked ? hooked : (F)Export(name); }
}

    uint32_t Id(const char* name) {
        uint32_t h = 2166136261u;
        for (const unsigned char* p = (const unsigned char*)name; *p; ++p) {
            unsigned char c = *p;
            if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
            h = (h * 16777619u) ^ c;
        }
        return h;
    }

    bool Install() {
        if (g_hooked) return true;
        auto st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return false;
        int ok = 0;
        ok += Hook(kPostEventId, (void*)&PostIdDetour, (void**)&o_post_id);
        ok += Hook(kPostEventStr, (void*)&PostStrDetour, (void**)&o_post_str);
        ok += Hook(kSetStateId, (void*)&StateIdDetour, (void**)&o_state_id);
        ok += Hook(kSetStateStr, (void*)&StateStrDetour, (void**)&o_state_str);
        ok += Hook(kSetSwitchId, (void*)&SwitchIdDetour, (void**)&o_switch_id);
        ok += Hook(kSetSwitchStr, (void*)&SwitchStrDetour, (void**)&o_switch_str);
        ok += Hook(kSetRtpcId, (void*)&RtpcIdDetour, (void**)&o_rtpc_id);
        ok += Hook(kSetRtpcStr, (void*)&RtpcStrDetour, (void**)&o_rtpc_str);
        ok += Hook(kResetRtpcId, (void*)&ResetRtpcDetour, (void**)&o_reset_rtpc);
        ok += Hook(kExecuteId, (void*)&ExecuteDetour, (void**)&o_execute);
        ok += Hook(kStopPlayingId, (void*)&StopPlayingDetour, (void**)&o_stop_playing);
        ok += Hook(kPostTriggerId, (void*)&TriggerDetour, (void**)&o_trigger);
        ok += Hook(kStopAll, (void*)&StopAllDetour, (void**)&o_stop_all);
        LOG("audio: %d of 13 Wwise calls hooked", ok);
        g_hooked = ok > 0;
        return g_hooked;
    }

    void Record(bool on) {
        if (on) {
            if (!Install()) { LOG("audio: cannot record (Wwise exports not found)"); return; }
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_file) fclose(g_file);
            g_file = fopen((DataDir() + "\\EndlessMode_audio.txt").c_str(), "w");
            g_start = GetTickCount64();
            g_recording = g_file != nullptr;
            if (g_file) fprintf(g_file, "# time(ms) call ids... [mod] = made by EndlessArena; @ = caller (exe+rva). Names: tools/assets/wwise_names.py\n");
            LOG("audio: recording to %s", (DataDir() + "\\EndlessMode_audio.txt").c_str());
        } else {
            g_recording = false;
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_file) { fclose(g_file); g_file = nullptr; }
            LOG("audio: recording stopped");
        }
    }

    bool Recording() { return g_recording; }

    void Mark(const std::string& text) { Line("# MARK %s", text.c_str()); }

    uint32_t Post(const char* event_name, uint64_t obj) {
        auto fn = Fn(o_post_id, kPostEventId);
        if (!fn) return 0;
        t_ours = true;
        uint32_t r = fn(Id(event_name), obj, 0, nullptr, nullptr, 0, nullptr, 0);
        t_ours = false;
        return r;
    }
    bool SetState(const char* group, const char* state) {
        auto fn = Fn(o_state_id, kSetStateId);
        if (!fn) return false;
        t_ours = true;
        int r = fn(Id(group), Id(state));
        t_ours = false;
        return r == 1;   // AK_Success
    }
    bool SetSwitch(const char* group, const char* sw, uint64_t obj) {
        auto fn = Fn(o_switch_id, kSetSwitchId);
        if (!fn) return false;
        t_ours = true;
        int r = fn(Id(group), Id(sw), obj);
        t_ours = false;
        return r == 1;
    }
    bool SetRtpc(const char* name, float value, uint64_t obj, int fade_ms) {
        auto fn = Fn(o_rtpc_id, kSetRtpcId);
        if (!fn) return false;
        t_ours = true;
        int r = fn(Id(name), value, obj, fade_ms, 4 /*linear*/, false);
        t_ours = false;
        return r == 1;
    }
    void StopPlaying(uint32_t playing_id, int fade_ms) {
        auto fn = Fn(o_stop_playing, kStopPlayingId);
        if (!fn || !playing_id) return;
        t_ours = true;
        fn(playing_id, fade_ms, 4);
        t_ours = false;
    }

    uint32_t PostId(uint32_t event_id, uint64_t obj) {
        auto fn = Fn(o_post_id, kPostEventId);
        if (!fn) return 0;
        t_ours = true;
        uint32_t r = fn(event_id, obj, 0, nullptr, nullptr, 0, nullptr, 0);
        t_ours = false;
        return r;
    }
    bool SetStateId(uint32_t group_id, uint32_t state_id) {
        auto fn = Fn(o_state_id, kSetStateId);
        if (!fn) return false;
        t_ours = true;
        int r = fn(group_id, state_id);
        t_ours = false;
        return r == 1;
    }
    bool RegisterObject(uint64_t obj, const char* name) {
        using RegFn = int (*)(uint64_t, const char*);
        auto fn = (RegFn)Export("?RegisterGameObj@SoundEngine@AK@@YA?AW4AKRESULT@@_KPEBD@Z");
        if (!fn) return false;
        return fn(obj, name) == 1;
    }

    int PlayingCount(uint64_t obj) {
        using CountFn = int (*)(uint64_t, uint32_t&, uint32_t*);
        auto fn = (CountFn)Export("?GetPlayingIDsFromGameObject@Query@SoundEngine@AK@@YA?AW4AKRESULT@@_KAEAKPEAK@Z");
        if (!fn) return -1;
        uint32_t n = 0;
        return fn(obj, n, nullptr) == 1 ? (int)n : -1;
    }
}
