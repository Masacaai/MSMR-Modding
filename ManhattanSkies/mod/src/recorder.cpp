#include "recorder.h"
#include "sky.h"

#include "gamethread.h"
#include "log.h"
#include "memory.h"
#include "natives.h"
#include "scan.h"

#include <MinHook.h>
#include <Windows.h>
#include <intrin.h>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

#include "atm_ids.inc"

namespace recorder {
    using SetLightingFn = uintptr_t (*)(uintptr_t self, int index);
    using AdjustFn = void (*)(uintptr_t mgr, uintptr_t slot, int delta, int mask, int min, int max);

    // AtmosphereManager layout (from exe+162f020): slot count +0x70, slot array pointer +0x110 (stride 0x8c0),
    // counter array pointer +0x120 (4 ints per slot).
    constexpr uintptr_t kSlotCount = 0x70, kSlots = 0x110, kCounters = 0x120, kSlotStride = 0x8c0;

    static SetLightingFn g_set_orig = nullptr;
    static AdjustFn g_adjust_orig = nullptr;
    static std::atomic<bool> g_installed{ false };
    static std::atomic<uintptr_t> g_mgr{ 0 };
    static int g_prev_lm = -99;
    static AdjustFilter g_filter = nullptr;
    static std::atomic<long long> g_max_slot{ -1 };   // highest slot index seen (the +0x70 field is not the slot count)

    static std::mutex g_mutex;   // guards the two containers below
    static std::set<long long> g_seen_slots;
    static std::map<std::tuple<uintptr_t, long long, int, int>, unsigned> g_silent;   // (caller, slot, delta, mask) -> repeats not logged

    static std::string Backtrace() {
        void* frames[10] = {};
        const USHORT n = RtlCaptureStackBackTrace(2, 10, frames, nullptr);
        const uintptr_t base = scan::Game().base;
        std::string s;
        for (USHORT i = 0; i < n; i++) {
            const uintptr_t f = (uintptr_t)frames[i];
            char part[24];
            if (f >= base && f < base + 0x8000000) snprintf(part, sizeof(part), " %llx", (unsigned long long)(f - base));
            else snprintf(part, sizeof(part), " ?");
            s += part;
        }
        return s;
    }

    static int ReadLightingMode() {
        const uintptr_t getter = natives::Get(natives::WorldGetter);
        if (!getter) return -98;
        uintptr_t w = 0;
        if (!memory::SafeCall((uintptr_t (*)(void*))getter, nullptr, w) || !w) return -97;
        int v = -96;
        return memory::Read(w + 0xe2040, v) ? v : -96;
    }

    static uintptr_t SetDetour(uintptr_t self, int index) {
        LOG("ls: lighting setter index=%d mode_before=%d caller=exe+%llx bt:%s", index, ReadLightingMode(),
            (unsigned long long)scan::Rva((uintptr_t)_ReturnAddress()), Backtrace().c_str());
        // The game sets its own remembered time of day while a world loads (mode field still unset). Swapping in the time picked in our menu
        // there means no change during the load (a setter call mid-load hung the loading screen in testing).
        if (ReadLightingMode() < 0) {
            const int saved = sky::SavedMode();
            if (saved >= 0 && saved <= 6 && saved != index) { LOG("ls: load-time mode %d replaced by the saved %d", index, saved); index = saved; }
        }
        return g_set_orig(self, index);
    }

    static long long SlotIndex(uintptr_t mgr, uintptr_t slot) {
        uintptr_t base = 0;
        if (!memory::Read(mgr + kSlots, base) || !base || slot < base) return -1;
        return (long long)((slot - base) / kSlotStride);
    }

    static bool ReadCounters(uintptr_t mgr, long long idx, int (&c)[4]) {
        uintptr_t arr = 0;
        return idx >= 0 && memory::Read(mgr + kCounters, arr) && arr && memory::SafeRead(arr + (uintptr_t)idx * 16, c, sizeof(c));
    }

    static std::string Hex(uintptr_t addr, size_t len) {
        unsigned char buf[0x40] = {};
        if (len > sizeof(buf)) len = sizeof(buf);
        if (!memory::SafeRead(addr, buf, len)) return "(unreadable)";
        std::string s;
        char b[4];
        for (size_t i = 0; i < len; i++) { snprintf(b, sizeof(b), "%02x%s", buf[i], (i % 8 == 7) ? "  " : " "); s += b; }
        return s;
    }

    static void AdjustDetour(uintptr_t mgr, uintptr_t slot, int delta, int mask, int min, int max) {
        const uintptr_t caller = scan::Rva((uintptr_t)_ReturnAddress());
        if (g_mgr.exchange(mgr) != mgr) LOG("at: AtmosphereManager at %llx", (unsigned long long)mgr);
        const long long idx = SlotIndex(mgr, slot);
        for (long long m = g_max_slot.load(); idx > m && !g_max_slot.compare_exchange_weak(m, idx); ) {}
        uint64_t asset = 0;
        memory::Read(slot + 8, asset);   // the slot's atmosphere asset id (CRC64 of the path); slots are reused, so log it every time
        // A mod may swallow a call (the sky override keeps its look on top by skipping the game's own activation of a base atmosphere).
        if (g_filter && g_filter(caller, (int)idx, asset, delta)) {
            static std::atomic<int> logged{ 0 };
            if (logged++ < 12) LOG("at: SWALLOWED slot=%lld asset=%016llX delta=%d caller=exe+%llx", idx, (unsigned long long)asset, delta, (unsigned long long)caller);
            return;
        }
        int before[4] = {}, after[4] = {};
        const bool had = ReadCounters(mgr, idx, before);
        g_adjust_orig(mgr, slot, delta, mask, min, max);
        const bool have = ReadCounters(mgr, idx, after);
        const bool changed = !had || !have || memcmp(before, after, sizeof(before)) != 0;

        bool first_sight = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            first_sight = g_seen_slots.insert(idx).second;
            if (!changed) { g_silent[{ caller, idx, delta, mask }]++; }
        }
        if (first_sight) LOG("as: slot %lld at %llx: %s", idx, (unsigned long long)slot, Hex(slot, 0x40).c_str());
        if (changed) {
            LOG("at: slot=%lld asset=%016llX delta=%d mask=%x clamp=[%d,%d] counters %d,%d,%d,%d -> %d,%d,%d,%d caller=exe+%llx bt:%s", idx, (unsigned long long)asset, delta, mask, min, max,
                before[0], before[1], before[2], before[3], after[0], after[1], after[2], after[3], (unsigned long long)caller, Backtrace().c_str());
        }
    }

    static bool Hook(natives::Id id, void* detour, void** orig, const char* what) {
        const uintptr_t target = natives::Get(id);
        if (!target) { LOG("recorder: %s not resolved, not hooked", what); return false; }
        if (MH_CreateHook((void*)target, detour, orig) != MH_OK || MH_EnableHook((void*)target) != MH_OK) {
            LOG("recorder: hooking %s failed", what);
            return false;
        }
        LOG("recorder: hooked %s exe+%llx", what, (unsigned long long)scan::Rva(target));
        return true;
    }

    bool Install() {
        if (g_installed) return true;
        const MH_STATUS init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) { LOG("recorder: MinHook init failed (%d)", (int)init); return false; }
        const bool a = Hook(natives::LightingSet, (void*)&SetDetour, (void**)&g_set_orig, "lighting setter");
        const bool b = Hook(natives::AtmosphereAdjust, (void*)&AdjustDetour, (void**)&g_adjust_orig, "AtmosphereAdjust");
        g_installed = a || b;
        g_prev_lm = ReadLightingMode();
        LOG("recorder: lighting mode at start %d", g_prev_lm);
        return a && b;
    }

    void Tick() {
        const int lm = ReadLightingMode();
        if (lm != g_prev_lm) { LOG("lm: lighting mode field %d -> %d", g_prev_lm, lm); g_prev_lm = lm; }
    }

    void Status() {
        LOG("sky: lighting mode %d, hooks %s, AtmosphereManager %llx", ReadLightingMode(), g_installed ? "on" : "off",
            (unsigned long long)g_mgr.load());
    }

    int CurrentMode() { return ReadLightingMode(); }

    bool ApplyMode(int index) {
        const uintptr_t set = natives::Get(natives::LightingSet), getter = natives::Get(natives::WorldGetter);
        if (!set || !getter) return false;
        const uintptr_t w = ((uintptr_t (*)())getter)();
        if (!w) return false;
        ((SetLightingFn)set)(w + 0xe10d0, index);
        return true;
    }

    void SetMode(int index) {
        const uintptr_t set = natives::Get(natives::LightingSet), getter = natives::Get(natives::WorldGetter);
        if (!set || !getter) { LOG("sky: setter or world getter not resolved"); return; }
        if (!gamethread::Installed() && !gamethread::Install()) { LOG("sky: game-thread hook failed; not calling the setter off-thread"); return; }
        // Same call as the Settings menu (exe+735d0b): setter(world + 0xe10d0, index). It goes through our hook, so an ls: line follows.
        const bool ran = gamethread::Run([=] { if (!ApplyMode(index)) LOG("sky: no world"); });
        LOG("sky: set index %d %s", index, ran ? "(ran on the game thread)" : "(timed out waiting for the game thread)");
    }

    // ---- world-state snapshots: find the variables that differ between lighting modes (rain, wetness, wind...) ----
    // The world singleton is one big struct; the lighting object sits at +0xe10d0 and the mode field at +0xe2040, so weather state is likely nearby.
    constexpr uintptr_t kSnapStart = 0xd0000, kSnapLen = 0x30000;
    static std::map<std::string, std::vector<unsigned char>> g_snaps;

    void Snap(const std::string& name) {
        const uintptr_t getter = natives::Get(natives::WorldGetter);
        uintptr_t w = 0;
        if (!getter || !memory::SafeCall((uintptr_t (*)(void*))getter, nullptr, w) || !w) { LOG("snap: no world"); return; }
        std::vector<unsigned char> buf(kSnapLen);
        size_t bad = 0;
        for (size_t off = 0; off < kSnapLen; off += 0x1000)
            if (!memory::SafeRead(w + kSnapStart + off, buf.data() + off, 0x1000)) { std::fill(buf.begin() + off, buf.begin() + off + 0x1000, 0); ++bad; }
        g_snaps[name] = std::move(buf);
        LOG("snap: '%s' taken (world %llx +0x%llx..+0x%llx, mode %d, %zu unreadable pages)", name.c_str(), (unsigned long long)w,
            (unsigned long long)kSnapStart, (unsigned long long)(kSnapStart + kSnapLen), ReadLightingMode(), bad);
    }

    // Dwords that differ between a and b. With a2 (a second snapshot of the same state as a), fields that also differ between a and a2 are noise and are skipped.
    void SnapDiff(const std::string& a, const std::string& b, const std::string& a2) {
        const auto ia = g_snaps.find(a), ib = g_snaps.find(b);
        if (ia == g_snaps.end() || ib == g_snaps.end()) { LOG("snapdiff: unknown snapshot"); return; }
        const std::vector<unsigned char>* noise = nullptr;
        if (!a2.empty()) { const auto in = g_snaps.find(a2); if (in != g_snaps.end()) noise = &in->second; }
        int shown = 0, total = 0;
        for (size_t off = 0; off + 4 <= kSnapLen; off += 4) {
            uint32_t x, y, z = 0;
            memcpy(&x, &ia->second[off], 4);
            memcpy(&y, &ib->second[off], 4);
            if (x == y) continue;
            if (noise) { memcpy(&z, &(*noise)[off], 4); if (z != x) continue; }
            ++total;
            if (shown >= 300) continue;
            float fx, fy;
            memcpy(&fx, &x, 4); memcpy(&fy, &y, 4);
            LOG("snapdiff: world+0x%llx  %u (0x%x, %g) -> %u (0x%x, %g)", (unsigned long long)(kSnapStart + off), x, x, fx, y, y, fy);
            ++shown;
        }
        LOG("snapdiff: %s -> %s: %d differing dwords%s, %d shown", a.c_str(), b.c_str(), total, noise ? " (noise removed)" : "", shown);
    }

    using LoadFn = uintptr_t (*)(uintptr_t mgr, uint64_t hash, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

    // ---- slot access for the sky module ----
    void SetAdjustFilter(AdjustFilter f) { g_filter = f; }
    uintptr_t Manager() { return g_mgr.load(); }
    uintptr_t SlotAddress(int idx) {
        uintptr_t base = 0;
        const uintptr_t mgr = g_mgr.load();
        if (!mgr || idx < 0 || idx > 32 || !memory::Read(mgr + kSlots, base) || !base) return 0;
        return base + (uintptr_t)idx * kSlotStride;
    }
    bool SlotInfo(int idx, uint64_t& asset, int& weight) {
        const uintptr_t a = SlotAddress(idx);
        int c[4] = {};
        asset = 0; weight = 0;
        if (!a || !memory::Read(a + 8, asset) || !ReadCounters(g_mgr.load(), idx, c)) return false;
        weight = c[0];
        return true;
    }
    int FindSlot(uint64_t asset) {
        for (int i = 0; i < 16; ++i) {
            uint64_t a; int w;
            if (SlotInfo(i, a, w) && a == asset) return i;
        }
        return -1;
    }
    uint64_t AtmIdBySuffix(const char* file) {   // "atm_ibl_lgt_day_02.atmosphere" -> the id of the path that ends with it
        const size_t n = strlen(file);
        for (const auto& e : kAtmosphereIds) {
            const size_t m = strlen(e.path);
            if (m >= n && strcmp(e.path + (m - n), file) == 0 && (m == n || e.path[m - n - 1] == '/')) return e.id;
        }
        return 0;
    }
    // The same call the game makes to bring an atmosphere into the pool (LoadAssetByHash with the AtmosphereManager). Game thread.
    bool LoadAtmosphere(uint64_t id) {
        const uintptr_t fn = natives::Get(natives::LoadAssetByHash), mgr = g_mgr.load();
        if (!fn || !mgr) return false;
        if (!gamethread::Installed() && !gamethread::Install()) return false;
        return gamethread::Run([=] { ((LoadFn)fn)(mgr, id, 0, 0, 0, 0, 0); });
    }
    // Drops one reference from a slot the sky module loaded and queues it for the asset manager's per-frame update, which destroys a queued slot
    // whose reference count is 0. The game's own release does the same (docs/RECON.md Part 10): the header dword at slot+4 is the reference count
    // (low 24 bits, 0xff above), exe+1624e80(manager, slot) appends the slot to the manager's pending ring. Only for slots with weight 0. Game thread.
    bool ReleaseSlot(int idx) {
        const uintptr_t mgr = g_mgr.load(), slot = SlotAddress(idx), fn = scan::FromRva(0x1624e80);
        static const unsigned char kPrologue[] = { 0x48, 0x8b, 0xc2, 0x4c, 0x8b, 0xc9, 0x48, 0x2b, 0x41, 0x40 };   // mov rax,rdx; mov r9,rcx; sub rax,[rcx+0x40]
        unsigned char got[sizeof(kPrologue)] = {};
        if (!mgr || !slot) return false;
        if (!memory::SafeRead(fn, got, sizeof(got)) || memcmp(got, kPrologue, sizeof(got)) != 0) { LOG("release: exe+1624e80 is not the expected function"); return false; }
        uintptr_t base = 0, stride = 0, slots = 0;
        memory::Read(mgr + 0x40, base); memory::Read(mgr + 0x48, stride); memory::Read(mgr + kSlots, slots);
        if (base != slots || stride != kSlotStride) { LOG("release: manager layout differs (+0x40 %llx +0x48 %llx slots %llx)", (unsigned long long)base, (unsigned long long)stride, (unsigned long long)slots); return false; }
        uint64_t asset; int weight;
        if (!SlotInfo(idx, asset, weight) || !asset || weight > 0) { LOG("release: slot %d is empty or active (weight %d)", idx, weight); return false; }
        if (!gamethread::Installed() && !gamethread::Install()) return false;
        auto result = std::make_shared<int>(-1);
        const bool ran = gamethread::Run([=] {
            uint32_t d = 0;
            if (!memory::Read(slot + 4, d)) return;
            const uint32_t refs = d & 0xffffff;
            if (refs == 0) { *result = 0; return; }
            memory::Write<uint32_t>(slot + 4, (d & 0xff000000) | (refs - 1));
            ((void (*)(uintptr_t, uintptr_t))fn)(mgr, slot);
            *result = (int)refs - 1;
        });
        LOG("release: slot %d asset %016llX %s, references left %d", idx, (unsigned long long)asset, ran ? "released" : "timed out", *result);
        return ran && *result >= 0;
    }

    // The six fog densities of a slot (+0x598..+0x5ac; the four distance bands before them at +0x588). Lower them and the fog thins: tested live.
    bool SlotFog(int idx, float out[6], bool write) {
        const uintptr_t a = SlotAddress(idx);
        if (!a) return false;
        if (write) return memory::SafeWrite(a + 0x598, out, 6 * sizeof(float));
        return memory::SafeRead(a + 0x598, out, 6 * sizeof(float));
    }

    // Reads or writes n floats at a byte offset inside a slot (the probe in tools/fieldprobe.py found what the offsets do: docs/RECON.md Part 12).
    bool SlotFloats(int idx, uintptr_t offset, float* v, size_t n, bool write) {
        const uintptr_t a = SlotAddress(idx);
        if (!a || offset + n * sizeof(float) > kSlotStride) return false;
        return write ? memory::SafeWrite(a + offset, v, n * sizeof(float)) : memory::SafeRead(a + offset, v, n * sizeof(float));
    }

    // Writes floats into a slot's sun block: R,G,B,intensity,rotation,elevation at +0x2a8.
    bool SlotSun(int idx, float out[6], bool write) {
        const uintptr_t a = SlotAddress(idx);
        if (!a) return false;
        if (write) return memory::SafeWrite(a + 0x2a8, out, 6 * sizeof(float));
        return memory::SafeRead(a + 0x2a8, out, 6 * sizeof(float));
    }

    // ---- asset loading: how an atmosphere gets into a slot ----
    static const char* AtmName(uint64_t id) {
        size_t lo = 0, hi = sizeof(kAtmosphereIds) / sizeof(kAtmosphereIds[0]);
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (kAtmosphereIds[mid].id == id) return kAtmosphereIds[mid].path;
            if (kAtmosphereIds[mid].id < id) lo = mid + 1; else hi = mid;
        }
        return nullptr;
    }
    static LoadFn g_load_orig = nullptr;
    static std::atomic<ULONGLONG> g_loadtrace_until{ 0 };
    static std::atomic<int> g_load_lines{ 0 };

    static std::string Where(uintptr_t a) {
        char b[48];
        const uintptr_t base = scan::Game().base;
        if (a >= base && a < base + 0x8000000) snprintf(b, sizeof(b), "exe+%llx", (unsigned long long)(a - base));
        else snprintf(b, sizeof(b), "%llx", (unsigned long long)a);
        return b;
    }
    static uintptr_t LoadDetour(uintptr_t mgr, uint64_t hash, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e) {
        const char* name = AtmName(hash);
        if ((name || GetTickCount64() < g_loadtrace_until) && g_load_lines < 400) {
            ++g_load_lines;
            LOG("ld: manager=%s asset=%016llX %s caller=%s args %llx %llx %llx %llx %llx", Where(mgr).c_str(), (unsigned long long)hash,
                name ? name : "", Where((uintptr_t)_ReturnAddress()).c_str(), (unsigned long long)a, (unsigned long long)b, (unsigned long long)c,
                (unsigned long long)d, (unsigned long long)e);
        }
        return g_load_orig(mgr, hash, a, b, c, d, e);
    }
    static bool EnsureLoadHook() {
        if (g_load_orig) return true;
        const uintptr_t t = natives::Get(natives::LoadAssetByHash);
        if (!t) { LOG("loadtrace: LoadAssetByHash not resolved"); return false; }
        if (MH_CreateHook((void*)t, (void*)&LoadDetour, (void**)&g_load_orig) != MH_OK || MH_EnableHook((void*)t) != MH_OK) { LOG("loadtrace: hook failed"); g_load_orig = nullptr; return false; }
        LOG("loadtrace: hooked LoadAssetByHash exe+%llx (every atmosphere-id load is logged from now on)", (unsigned long long)scan::Rva(t));
        return true;
    }
    void LoadTrace(unsigned ms) {
        if (!EnsureLoadHook()) return;
        g_load_lines = 0;
        g_loadtrace_until = GetTickCount64() + ms;
        LOG("loadtrace: logging every asset load for %u ms", ms);
    }
    // `atmload <hex id | @path-substring> [manager]`: the same call the game makes, from the game thread. manager defaults to the AtmosphereManager.
    void AtmLoad(const std::string& what, uintptr_t manager) {
        if (!EnsureLoadHook()) return;
        uint64_t id = 0;
        const char* name = nullptr;
        if (!what.empty() && what[0] == '@') {
            for (const auto& e : kAtmosphereIds) if (strstr(e.path, what.c_str() + 1)) { id = e.id; name = e.path; break; }
            if (!id) { LOG("atmload: nothing matches %s", what.c_str()); return; }
        } else id = strtoull(what.c_str(), nullptr, 16);
        if (!manager) manager = g_mgr.load();
        if (!manager) { LOG("atmload: no manager (give one, or wait for an atmosphere to be seen)"); return; }
        if (!gamethread::Installed() && !gamethread::Install()) { LOG("atmload: game-thread hook failed"); return; }
        auto result = std::make_shared<uintptr_t>(0);
        const bool ran = gamethread::Run([=] { *result = g_load_orig(manager, id, 0, 0, 0, 0, 0); });
        LOG("atmload: manager=%s asset=%016llX %s -> %s, returned %llx", Where(manager).c_str(), (unsigned long long)id, name ? name : "", ran ? "ran" : "timed out", (unsigned long long)*result);
    }
    void AtmList(const std::string& filter) {
        int n = 0;
        for (const auto& e : kAtmosphereIds)
            if (filter.empty() || strstr(e.path, filter.c_str())) { LOG("atmlist: %016llX %s", (unsigned long long)e.id, e.path); ++n; }
        LOG("atmlist: %d of %zu", n, sizeof(kAtmosphereIds) / sizeof(kAtmosphereIds[0]));
    }

    // ---- exploration tools ----
    // Copies the exe's .data section (the game's static globals, 32 MB) to a file so two moments can be diffed offline (tools/datadiff.py).
    // File layout: u32 rva, u32 size, then size bytes (an unreadable 64 KB chunk is written as zeros).
    void DumpData(const std::string& path) {
        const uintptr_t base = scan::Game().base;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
            if (memcmp(sec->Name, ".data", 5) != 0) continue;
            const uint32_t rva = sec->VirtualAddress, size = sec->Misc.VirtualSize;
            FILE* f = nullptr;
            if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) { LOG("dumpdata: cannot open %s", path.c_str()); return; }
            fwrite(&rva, 4, 1, f);
            fwrite(&size, 4, 1, f);
            std::vector<unsigned char> buf(0x10000);
            size_t bad = 0;
            for (uint32_t off = 0; off < size; off += 0x10000) {
                const size_t n = size - off < 0x10000 ? size - off : 0x10000;
                if (!memory::SafeRead(base + rva + off, buf.data(), n)) { std::fill(buf.begin(), buf.end(), 0); ++bad; }
                fwrite(buf.data(), 1, n, f);
            }
            fclose(f);
            LOG("dumpdata: .data rva %x size %u written to %s (%zu unreadable chunks), mode %d", rva, size, path.c_str(), bad, ReadLightingMode());
            return;
        }
        LOG("dumpdata: no .data section found");
    }

    // Scans one readable region for an aligned 8-byte value. No C++ objects (SEH).
    static size_t ScanRegion(uintptr_t base, size_t size, uint64_t value, uintptr_t* out, size_t max) {
        size_t n = 0;
        __try {
            const uint64_t* p = reinterpret_cast<const uint64_t*>(base);
            const size_t count = size / 8;
            for (size_t i = 0; i < count && n < max; ++i)
                if (p[i] == value) out[n++] = base + i * 8;
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return n;
    }

    void FindQ(uint64_t value, int max_hits) {
        const uintptr_t game = scan::Game().base;
        uintptr_t addr = 0;
        MEMORY_BASIC_INFORMATION mbi;
        int hits = 0;
        size_t scanned = 0;
        while (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == sizeof(mbi) && hits < max_hits) {
            const uintptr_t base = (uintptr_t)mbi.BaseAddress;
            const bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
                                  (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
            const bool too_big_mapped = mbi.Type == MEM_MAPPED && mbi.RegionSize > (64u << 20);   // GPU-visible mappings: skip
            if (readable && !too_big_mapped && mbi.RegionSize <= (1ull << 30)) {
                uintptr_t found[16];
                const size_t n = ScanRegion(base, mbi.RegionSize, value, found, 16);
                scanned += mbi.RegionSize;
                for (size_t i = 0; i < n && hits < max_hits; ++i, ++hits) {
                    const bool in_exe = found[i] >= game && found[i] < game + 0x8000000;
                    char where[64];
                    if (in_exe) snprintf(where, sizeof(where), "exe+%llx", (unsigned long long)(found[i] - game));
                    else snprintf(where, sizeof(where), "region %llx+%llx %s", (unsigned long long)base, (unsigned long long)(found[i] - base),
                                  mbi.Type == MEM_IMAGE ? "image" : mbi.Type == MEM_MAPPED ? "mapped" : "private");
                    LOG("findq: %016llX at %llx (%s)  next: %s", (unsigned long long)value, (unsigned long long)found[i], where, Hex(found[i], 0x20).c_str());
                }
            }
            addr = base + mbi.RegionSize;
            if (addr < base) break;
        }
        LOG("findq: %016llX: %d hits, %zu MB scanned", (unsigned long long)value, hits, scanned >> 20);
    }

    void PokeRaw(uintptr_t addr, const void* data, size_t len) {
        unsigned char before[8] = {}, after[8] = {};
        memory::SafeRead(addr, before, len);
        const bool ok = memory::SafeWrite(addr, data, len);
        memory::SafeRead(addr, after, len);
        std::string b, a;
        char t[4];
        for (size_t i = 0; i < len; ++i) { snprintf(t, sizeof(t), "%02x ", before[i]); b += t; snprintf(t, sizeof(t), "%02x ", after[i]); a += t; }
        LOG("poke: %llx %s: %s-> %s", (unsigned long long)addr, ok ? "ok" : "FAILED", b.c_str(), a.c_str());
    }
    void Poke32(uintptr_t addr, uint32_t v) { PokeRaw(addr, &v, 4); }
    void PokeF(uintptr_t addr, float v) { PokeRaw(addr, &v, 4); }

    // Calls the manager's recompute (exe+162ff10): re-blends every slot's weighted counters. Game thread.
    void Recompute() {
        const uintptr_t mgr = g_mgr.load();
        const uintptr_t fn = scan::FromRva(0x162ff10);
        static const unsigned char kPrologue[] = { 0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x6c, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57 };
        unsigned char got[sizeof(kPrologue)] = {};
        if (!mgr) { LOG("recompute: no AtmosphereManager seen yet"); return; }
        if (!memory::SafeRead(fn, got, sizeof(got)) || memcmp(got, kPrologue, sizeof(got)) != 0) { LOG("recompute: exe+162ff10 is not the expected function"); return; }
        if (!gamethread::Installed() && !gamethread::Install()) { LOG("recompute: game-thread hook failed"); return; }
        const bool ran = gamethread::Run([=] { ((void (*)(uintptr_t))fn)(mgr); });
        LOG("recompute: %s", ran ? "ran on the game thread" : "timed out");
    }

    // Calls AtmosphereAdjust (exe+162f020) on a slot by index, on the game thread: delta is added to the slot's weight counters (see Part 1 of RECON).
    // With the base atmosphere at weight 2, this is how the map screen puts its own look on top (+1000); here to see how weights blend.
    void Adjust(int slot_index, int delta) {
        const uintptr_t mgr = g_mgr.load(), fn = natives::Get(natives::AtmosphereAdjust);
        uintptr_t slots = 0;
        if (!mgr || !fn || !memory::Read(mgr + kSlots, slots) || !slots || slot_index < 0 || slot_index > 64) { LOG("adj: no manager/slots"); return; }
        if (!gamethread::Installed() && !gamethread::Install()) { LOG("adj: game-thread hook failed"); return; }
        const uintptr_t slot = slots + (uintptr_t)slot_index * kSlotStride;
        const bool ran = gamethread::Run([=] { ((void (*)(uintptr_t, uintptr_t, int, int, int, int))fn)(mgr, slot, delta, -1, 0, 0x7fffffff); });
        LOG("adj: slot %d (%llx) delta %d %s", slot_index, (unsigned long long)slot, delta, ran ? "applied on the game thread" : "timed out");
    }

    // The address of a slot (index) so commands can name slot memory: `slotaddr 2` then poke/peek.
    void SlotAddr(int slot_index) {
        uintptr_t slots = 0;
        const uintptr_t mgr = g_mgr.load();
        if (!mgr || !memory::Read(mgr + kSlots, slots)) { LOG("slotaddr: no manager"); return; }
        LOG("slotaddr: slot %d at %llx (sun rgba +2a8, rot/elev +2b8, ambient +300)", slot_index, (unsigned long long)(slots + (uintptr_t)slot_index * kSlotStride));
    }

    void Peek(uintptr_t addr, size_t len) {
        for (size_t off = 0; off < len; off += 0x20) LOG("peek: %llx  %s", (unsigned long long)(addr + off), Hex(addr + off, len - off < 0x20 ? len - off : 0x20).c_str());
    }

    void DumpAtmospheres() {
        const uintptr_t mgr = g_mgr.load();
        if (!mgr) { LOG("atm: no AtmosphereManager seen yet (no AtmosphereAdjust call so far)"); return; }
        int field70 = -1;
        uintptr_t slots = 0;
        memory::Read(mgr + kSlotCount, field70);
        memory::Read(mgr + kSlots, slots);
        const long long top = g_max_slot.load();
        LOG("atm: manager %llx, slots at %llx, +0x70=%d, highest slot seen %lld", (unsigned long long)mgr, (unsigned long long)slots, field70, top);
        for (long long i = 0; slots && i <= top + 2 && i < 512; i++) {
            int c[4] = {};
            ReadCounters(mgr, i, c);
            uint64_t asset = 0;
            memory::Read(slots + (uintptr_t)i * kSlotStride + 8, asset);
            LOG("atm: [%3lld] asset=%016llX counters %d,%d,%d,%d  %s", i, (unsigned long long)asset, c[0], c[1], c[2], c[3],
                Hex(slots + (uintptr_t)i * kSlotStride, 0x20).c_str());
        }
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& [key, n] : g_silent) {
            LOG("atm: silent repeats: caller=exe+%llx slot=%lld delta=%d mask=%x x%u", (unsigned long long)std::get<0>(key), std::get<1>(key),
                std::get<2>(key), std::get<3>(key), n);
        }
    }
}
