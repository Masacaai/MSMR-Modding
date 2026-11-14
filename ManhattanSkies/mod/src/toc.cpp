#include "toc.h"

#include "log.h"
#include "memory.h"
#include "scan.h"

#include <MinHook.h>
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace toc {
namespace {
    // Section layout (tools/tocinfo.py prints the distances from the id table): ids 8 bytes each, offsets 8 bytes (archive u32, offset u32), sizes 12 bytes (1, size u32, index u32).

    // Scan one committed region for the 32-byte pattern; SEH-guarded (pages can disappear). No C++ objects here.
    const uint8_t* SearchRegion(const uint8_t* p, size_t n, const uint64_t pat[4]) {
        __try {
            for (size_t i = 0; i + 32 <= n; i += 8) {
                if (*reinterpret_cast<const uint64_t*>(p + i) != pat[0]) continue;
                if (std::memcmp(p + i, pat, 32) == 0) return p + i;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return nullptr;
    }

    // Same as SearchRegion for three consecutive 8-byte values.
    const uint8_t* SearchRegion3(const uint8_t* p, size_t n, const uint64_t pat[3]) {
        __try {
            for (size_t i = 0; i + 24 <= n; i += 8) {
                if (*reinterpret_cast<const uint64_t*>(p + i) != pat[0]) continue;
                if (*reinterpret_cast<const uint64_t*>(p + i + 8) == pat[1] && *reinterpret_cast<const uint64_t*>(p + i + 16) == pat[2]) return p + i;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return nullptr;
    }

    bool ReadBytes(uintptr_t a, void* out, size_t n) {
        __try { std::memcpy(out, reinterpret_cast<const void*>(a), n); return true; } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // ---- the game's in-memory asset table (a DAT1 buffer: header, then the id, offsets, sizes and archive-name sections) ----
    struct Table { uintptr_t base = 0, ids = 0, offs = 0, sizes = 0, archives = 0, spans = 0; size_t count = 0, archive_count = 0, span_count = 0; bool ok = false; } g_tab;

    struct Carrier {
        uint64_t id = 0;
        size_t index = 0;
        uint32_t old_archive = 0, old_offset = 0, old_size = 0;
        std::string archive_name;          // the archive file the entry points at, as the toc lists it (for example mods\mod166)
        uint32_t offset = 0, size = 0;     // what the entry says now (a made-up offset past the end of that file)
        std::wstring path;
        HANDLE file = INVALID_HANDLE_VALUE;
    };
    std::mutex g_car_mu;
    std::vector<Carrier> g_cars;
    std::atomic<bool> g_cars_active{ false };
    std::atomic<uint32_t> g_next_sentinel{ 0x7E000000u };

    // ---- read trace ----
    std::atomic<ULONGLONG> g_io_until{ 0 };
    std::atomic<int> g_io_budget{ 0 };
    std::atomic<unsigned long> g_io_other{ 0 };
    std::atomic<bool> g_io_hooked{ false };
    BOOL (WINAPI* g_read_orig)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = nullptr;
    std::mutex g_path_mu;
    std::unordered_map<HANDLE, std::string> g_paths;

    std::string PathOf(HANDLE h) {
        {
            std::lock_guard<std::mutex> lk(g_path_mu);
            auto it = g_paths.find(h);
            if (it != g_paths.end()) return it->second;
        }
        wchar_t w[512] = {};
        std::string out;
        const DWORD n = GetFinalPathNameByHandleW(h, w, 511, FILE_NAME_NORMALIZED);
        if (n && n < 512) {
            char a[512] = {};
            WideCharToMultiByte(CP_UTF8, 0, w, -1, a, 511, nullptr, nullptr);
            out = a;
        }
        std::lock_guard<std::mutex> lk(g_path_mu);
        g_paths[h] = out;
        return out;
    }

    BOOL WINAPI ReadFileDetour(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov) {
        if (ov && g_cars_active.load(std::memory_order_relaxed)) {
            const uint32_t off = ov->Offset;
            if (ov->OffsetHigh == 0 && off >= 0x7E000000u) {   // only the made-up offsets
                const std::string p = PathOf(h);
                HANDLE ours = INVALID_HANDLE_VALUE;
                {
                    std::lock_guard<std::mutex> lk(g_car_mu);
                    for (auto& c : g_cars) {
                        if (c.offset == off && c.size == n && p.size() >= c.archive_name.size() &&
                            _stricmp(p.c_str() + p.size() - c.archive_name.size(), c.archive_name.c_str()) == 0) {
                            if (c.file == INVALID_HANDLE_VALUE)   // the file may have been unpacked after the asset was added
                                c.file = CreateFileW(c.path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                            ours = c.file;
                            break;
                        }
                    }
                }
                if (ours == INVALID_HANDLE_VALUE) {
                    static std::atomic<int> missed{ 0 };
                    if (p.find("\\mods\\") != std::string::npos && missed++ < 12) LOG("carrier: read of %lu bytes at made-up offset 0x%x on '%s' matched no added asset (handle %p)", (unsigned long)n, off, p.c_str(), h);
                }
                if (ours != INVALID_HANDLE_VALUE) {
                    ov->Offset = 0; ov->OffsetHigh = 0;   // our own file holds exactly the asset's bytes
                    LOG("carrier: read of %lu bytes at made-up offset 0x%x served from our file", (unsigned long)n, off);
                    return g_read_orig(ours, buf, n, got, ov);
                }
            }
        }
        if (GetTickCount64() < g_io_until.load(std::memory_order_relaxed) && g_io_budget.load(std::memory_order_relaxed) > 0) {
            const std::string p = PathOf(h);
            if (p.find("asset_archive") != std::string::npos && p.find("\mods\\") == std::string::npos) g_io_other.fetch_add(1);   // the game's own archives stream constantly: count only
            else if (p.find("asset_archive") != std::string::npos) {
                g_io_budget.fetch_sub(1);
                const size_t slash = p.find_last_of('\\');
                const unsigned long long off = ov ? (((unsigned long long)ov->OffsetHigh << 32) | ov->Offset) : 0ull;
                LOG("io: %s %s offset 0x%llx len %lu caller exe+%llx", p.c_str() + (slash == std::string::npos ? 0 : slash + 1), ov ? "ovl" : "sync", off, (unsigned long)n,
                    (unsigned long long)scan::Rva((uintptr_t)_ReturnAddress()));
            }
        }
        return g_read_orig(h, buf, n, got, ov);
    }
}

    void Find(uint32_t index, const uint64_t pat[4], uintptr_t offsets_delta, uintptr_t sizes_delta) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
        const uintptr_t end = (uintptr_t)si.lpMaximumApplicationAddress;
        const ULONGLONG t0 = GetTickCount64();
        int hits = 0;
        size_t scanned = 0;
        MEMORY_BASIC_INFORMATION mbi;
        while (addr < end && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            const uintptr_t base = (uintptr_t)mbi.BaseAddress;
            const bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                                  (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
            // Private memory only, and only big regions: the toc is about 21 MB on the heap. File-backed views (the game maps its archives, tens of GB) must never be scanned.
            if (readable && mbi.Type == MEM_PRIVATE && mbi.RegionSize >= (8u << 20) && scanned < (6ull << 30)) {
                scanned += mbi.RegionSize;
                const uint8_t* p = reinterpret_cast<const uint8_t*>(base);
                size_t left = mbi.RegionSize;
                while (left >= 32) {
                    const uint8_t* h = SearchRegion(p, left, pat);
                    if (!h) break;
                    ++hits;
                    const uintptr_t at = (uintptr_t)h;
                    const uintptr_t ids_base = at - 8ull * index;
                    uint32_t arch = 0, off = 0, one = 0, size = 0, idx = 0;
                    const bool ok = ReadBytes(ids_base + offsets_delta + 8ull * index, &arch, 4) &&
                                    ReadBytes(ids_base + offsets_delta + 8ull * index + 4, &off, 4) &&
                                    ReadBytes(ids_base + sizes_delta + 12ull * index, &one, 4) &&
                                    ReadBytes(ids_base + sizes_delta + 12ull * index + 4, &size, 4) &&
                                    ReadBytes(ids_base + sizes_delta + 12ull * index + 8, &idx, 4);
                    LOG("tocfind: hit %d at %llx (region %llx size %llx type %lx prot %lx) -> id table base %llx; entry %u: archive %u offset 0x%x | sizes 1st %u size %u idx %u%s",
                        hits, (unsigned long long)at, (unsigned long long)base, (unsigned long long)mbi.RegionSize, (unsigned long)mbi.Type, (unsigned long)mbi.Protect,
                        (unsigned long long)ids_base, index, arch, off, one, size, idx, ok ? "" : " (unreadable)");
                    const size_t step = (size_t)(h - p) + 8;
                    p += step;
                    left -= step;
                    if (ok && arch < 256) { hits = 12; break; }   // a valid entry: this is the table
                    if (hits >= 12) break;
                }
            }
            if (hits >= 12) break;
            addr = base + mbi.RegionSize;
        }
        LOG("tocfind: %d hit(s); scanned %zu MB in %llu ms", hits, scanned >> 20, (unsigned long long)(GetTickCount64() - t0));
    }

    void IoTrace(unsigned ms) {
        if (!g_io_hooked.exchange(true)) {
            MH_Initialize();   // harmless if another module did it already
            if (MH_CreateHook(reinterpret_cast<void*>(&ReadFile), reinterpret_cast<void*>(&ReadFileDetour), reinterpret_cast<void**>(&g_read_orig)) != MH_OK ||
                MH_EnableHook(reinterpret_cast<void*>(&ReadFile)) != MH_OK) {
                LOG("iotrace: could not hook ReadFile");
                g_io_hooked = false;
                return;
            }
            LOG("iotrace: ReadFile hooked");
        }
        LOG("iotrace: %lu reads of the game's own archives since the last window (not logged)", g_io_other.exchange(0));
        g_io_budget = 400;
        g_io_until = GetTickCount64() + ms;
        LOG("iotrace: logging reads of asset_archive\mods (up to 400) for %u ms; other archive reads are only counted", ms);
    }
    // Finds the toc's DAT1 buffer in the game's memory: a big private region holding a DAT1 header whose sections include the id, offsets and sizes tables.
    bool Locate(bool quiet) {
        if (g_tab.ok) return true;
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
        const uintptr_t end = (uintptr_t)si.lpMaximumApplicationAddress;
        MEMORY_BASIC_INFORMATION mbi;
        while (addr < end && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            const uintptr_t base = (uintptr_t)mbi.BaseAddress;
            const bool readable = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                                  (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE));
            if (readable && mbi.RegionSize >= (8u << 20)) {
                for (uintptr_t a = base; a + 4096 < base + mbi.RegionSize; a += 16) {
                    uint32_t magic = 0;
                    if (!ReadBytes(a, &magic, 4)) break;
                    if (magic != 0x44415431) continue;
                    uint8_t hdr[16 + 12 * 16] = {};
                    if (!ReadBytes(a, hdr, sizeof(hdr))) continue;
                    const uint16_t nsec = *reinterpret_cast<const uint16_t*>(hdr + 12);
                    if (nsec < 3 || nsec > 16) continue;
                    Table t;
                    uint32_t ids_size = 0, sizes_size = 0, offs_size = 0, arch_size = 0;
                    for (int i = 0; i < nsec; ++i) {
                        const uint32_t tag = *reinterpret_cast<const uint32_t*>(hdr + 16 + 12 * i);
                        const uint32_t off = *reinterpret_cast<const uint32_t*>(hdr + 16 + 12 * i + 4);
                        const uint32_t sz = *reinterpret_cast<const uint32_t*>(hdr + 16 + 12 * i + 8);
                        if (tag == 0x506d7b8a) { t.ids = a + off; ids_size = sz; }
                        else if (tag == 0x65bcf461) { t.sizes = a + off; sizes_size = sz; }
                        else if (tag == 0xdcd720b5) { t.offs = a + off; offs_size = sz; }
                        else if (tag == 0x398abff0) { t.archives = a + off; arch_size = sz; }
                        else if (tag == 0xede8ada9) { t.spans = a + off; t.span_count = sz / 8; }
                    }
                    if (!t.ids || !t.sizes || !t.offs || ids_size % 8 || ids_size / 8 < 100000) continue;
                    if (offs_size / 8 != ids_size / 8 || sizes_size / 12 != ids_size / 8) continue;
                    t.base = a;
                    t.count = ids_size / 8;
                    t.archive_count = arch_size / 72;
                    t.ok = true;
                    g_tab = t;
                    LOG("toc: located at %llx: %zu assets, ids %llx offsets %llx sizes %llx, %zu archives", (unsigned long long)a, t.count, (unsigned long long)t.ids,
                        (unsigned long long)t.offs, (unsigned long long)t.sizes, t.archive_count);
                    return true;
                }
            }
            addr = base + mbi.RegionSize;
        }
        if (!quiet) LOG("toc: not found in memory");
        return false;
    }

    // The archive file name of an archive index, as the toc lists it (for example mods\mod166).
    static std::string ArchiveName(uint32_t index) {
        if (!g_tab.ok || !g_tab.archives || index >= g_tab.archive_count) return "";
        char name[72] = {};
        if (!ReadBytes(g_tab.archives + 72ull * index + 8, name, 64)) return "";
        name[64] = 0;
        return name;
    }

    // Makes `carrier_id` (an existing asset the game does not need) load from `path`: the asset's toc entry is pointed at a made-up offset in the archive
    // file of `use_archive` (a file the game already has open, for example a mod archive), with the size of our file, and reads of exactly that
    // (file, offset, length) are answered from our own file. `use_archive` is an archive index as tools/tocinfo.py prints it.
    bool AddCarrier(uint64_t carrier_id, uint32_t use_archive, const wchar_t* path) {
        if (!Locate()) return false;
        size_t index = (size_t)-1;
        for (size_t i = 0; i < g_tab.count; ++i) {
            uint64_t v = 0;
            if (!ReadBytes(g_tab.ids + 8 * i, &v, 8)) return false;
            if (v == carrier_id) { index = i; break; }
        }
        if (index == (size_t)-1) { LOG("carrier: %016llX is not in the toc", (unsigned long long)carrier_id); return false; }
        const std::string aname = ArchiveName(use_archive);
        if (aname.empty()) { LOG("carrier: archive index %u has no name", use_archive); return false; }
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (f == INVALID_HANDLE_VALUE) { LOG("carrier: cannot open the file (error %lu)", GetLastError()); return false; }
        LARGE_INTEGER sz{};
        GetFileSizeEx(f, &sz);
        Carrier c;
        c.id = carrier_id; c.index = index; c.archive_name = aname; c.path = path; c.file = f;
        c.size = (uint32_t)sz.QuadPart;
        c.offset = g_next_sentinel.fetch_add(0x10000u);
        ReadBytes(g_tab.offs + 8 * index, &c.old_archive, 4);
        ReadBytes(g_tab.offs + 8 * index + 4, &c.old_offset, 4);
        ReadBytes(g_tab.sizes + 12 * index + 4, &c.old_size, 4);
        {
            std::lock_guard<std::mutex> lk(g_car_mu);
            g_cars.push_back(c);   // the hook knows the redirect before the entry changes
        }
        g_cars_active = true;
        MH_Initialize();
        if (!g_io_hooked.exchange(true)) {
            if (MH_CreateHook(reinterpret_cast<void*>(&ReadFile), reinterpret_cast<void*>(&ReadFileDetour), reinterpret_cast<void**>(&g_read_orig)) != MH_OK ||
                MH_EnableHook(reinterpret_cast<void*>(&ReadFile)) != MH_OK) { LOG("carrier: could not hook ReadFile"); g_io_hooked = false; return false; }
        }
        const bool w = memory::Write(g_tab.offs + 8 * index, use_archive) && memory::Write(g_tab.offs + 8 * index + 4, c.offset) && memory::Write(g_tab.sizes + 12 * index + 4, c.size);
        LOG("carrier: %016llX (toc index %zu) was archive %u offset 0x%x size %u -> archive %u (%s) offset 0x%x size %u%s", (unsigned long long)carrier_id, index, c.old_archive, c.old_offset,
            c.old_size, use_archive, aname.c_str(), c.offset, c.size, w ? "" : " (WRITE FAILED)");
        return w;
    }

    void RestoreCarriers() {
        std::lock_guard<std::mutex> lk(g_car_mu);
        for (auto& c : g_cars) {
            memory::Write(g_tab.offs + 8 * c.index, c.old_archive);
            memory::Write(g_tab.offs + 8 * c.index + 4, c.old_offset);
            memory::Write(g_tab.sizes + 12 * c.index + 4, c.old_size);
            if (c.file != INVALID_HANDLE_VALUE) CloseHandle(c.file);
            LOG("carrier: %016llX restored", (unsigned long long)c.id);
        }
        g_cars.clear();
        g_cars_active = false;
    }
    // Where does the game keep pointers to the toc? Scans private memory and the exe's own image for 8-byte values equal to the DAT1 base or a section start,
    // and prints the first few hits with their surroundings, to see how the game's toc object holds the tables (research for adding new entries).
    void FindPointers() {
        if (!Locate()) return;
        struct Target { const char* name; uintptr_t value; };
        const Target targets[] = { { "dat1 base", g_tab.base }, { "ids", g_tab.ids }, { "offsets", g_tab.offs }, { "sizes", g_tab.sizes }, { "archives", g_tab.archives }, { "spans", g_tab.spans } };
        int hits[6] = {};
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
        const uintptr_t end = (uintptr_t)si.lpMaximumApplicationAddress;
        const uintptr_t exe = scan::FromRva(0);
        MEMORY_BASIC_INFORMATION mbi;
        size_t scanned = 0;
        while (addr < end && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            const uintptr_t base = (uintptr_t)mbi.BaseAddress;
            const bool in_exe = base >= exe && base < exe + 0x8000000;
            const bool ok = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_READONLY)) &&
                            (mbi.Type == MEM_PRIVATE || in_exe) && mbi.RegionSize <= (1ull << 29);
            if (ok) {
                scanned += mbi.RegionSize;
                for (uintptr_t a = base; a + 8 <= base + mbi.RegionSize; a += 8) {
                    uint64_t v = 0;
                    if (!ReadBytes(a, &v, 8)) break;
                    if (v < 0x10000) continue;
                    for (int t = 0; t < 6; ++t) {
                        if (v != targets[t].value || a == g_tab.ids || (a >= g_tab.ids && a < g_tab.ids + 8 * g_tab.count)) continue;
                        if (++hits[t] <= 4) {
                            uint8_t ctx[0x60] = {};
                            ReadBytes(a - 0x20, ctx, sizeof(ctx));
                            char hex[0x60 * 3 + 1] = {};
                            for (int i = 0; i < 0x60; ++i) snprintf(hex + i * 3, 4, "%02x ", ctx[i]);
                            LOG("tocptrs: %s pointer at %llx (%s, exe+%llx, region %llx size %llx): %s", targets[t].name, (unsigned long long)a, in_exe ? "exe image" : "private",
                                (unsigned long long)(in_exe ? a - exe : 0), (unsigned long long)base, (unsigned long long)mbi.RegionSize, hex);
                        }
                    }
                }
            }
            addr = base + mbi.RegionSize;
        }
        LOG("tocptrs: hits: base %d, ids %d, offsets %d, sizes %d, archives %d, spans %d (scanned %zu MB)", hits[0], hits[1], hits[2], hits[3], hits[4], hits[5], scanned >> 20);
        // the spans section itself: how asset ids are grouped
        for (size_t i = 0; i < g_tab.span_count && i < 8; ++i) {
            uint32_t b = 0, c = 0;
            ReadBytes(g_tab.spans + 8 * i, &b, 4);
            ReadBytes(g_tab.spans + 8 * i + 4, &c, 4);
            LOG("tocptrs: span %zu: first asset index %u, count %u", i, b, c);
        }
        uint32_t lb = 0, lc = 0;
        ReadBytes(g_tab.spans + 8 * (g_tab.span_count - 1), &lb, 4);
        ReadBytes(g_tab.spans + 8 * (g_tab.span_count - 1) + 4, &lc, 4);
        LOG("tocptrs: last span %zu: first asset index %u, count %u (of %zu assets)", g_tab.span_count - 1, lb, lc, g_tab.count);
    }
    // ---- adding genuinely new assets to the game's in-memory toc (no carrier, nothing hijacked) ----
    // The game finds an asset with exe+16d7990: it binary-searches ONE span (a contiguous run of the sorted id table, chosen by the caller as group*8+kind) of
    // the ids table and returns the matching 12-byte entry of the sizes table. Atmospheres and the textures they use all live in span 0 (group 0, kind 0).
    // So: make bigger copies of the ids, offsets and sizes tables with spare room; after the full old tables (every old asset keeps its index) copy span 0's
    // entries and append the new assets after them (their ids must be larger than every id in span 0); repoint the game's toc object at the copies; and
    // finally point span 0 at the copy with a single 8-byte store. Old lookups that cached an index or entry pointer still see valid, identical data.
    // Reads of the new assets are answered from loose files by the ReadFile redirect the carriers use.
    struct Grow {
        bool ready = false;
        uintptr_t obj = 0;
        uint64_t* ids = nullptr;
        uint8_t* offs = nullptr;    // 8 bytes: archive u32, offset u32
        uint8_t* sizes = nullptr;   // 12 bytes: 1, size u32, index u32
        size_t base_count = 0, span_count = 0, added = 0, cap = 0, copy_first = 0;
        uint64_t last_id = 0;
    } g_grow;
    constexpr size_t kSpare = 4096;

    static bool FindObject() {
        if (g_grow.obj) return true;
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
        const uintptr_t end = (uintptr_t)si.lpMaximumApplicationAddress;
        MEMORY_BASIC_INFORMATION mbi;
        while (addr < end && VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            const uintptr_t base = (uintptr_t)mbi.BaseAddress;
            const bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                            (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) && mbi.RegionSize >= (1u << 20) && mbi.RegionSize <= (1ull << 29);
            if (ok) {
                const uint64_t pat3[3] = { g_tab.base, g_tab.spans, g_tab.ids };
                const uint8_t* p = reinterpret_cast<const uint8_t*>(base);
                size_t left = mbi.RegionSize;
                while (left >= 0x60) {
                    const uint8_t* h = SearchRegion3(p, left, pat3);
                    if (!h) break;
                    const uintptr_t a = (uintptr_t)h;
                    uint64_t sizes_p = 0, offs_p = 0, count = 0;
                    ReadBytes(a + 0x28, &sizes_p, 8);
                    ReadBytes(a + 0x30, &offs_p, 8);
                    ReadBytes(a + 0x48, &count, 8);
                    if (sizes_p == g_tab.sizes && offs_p == g_tab.offs && count == g_tab.count) {   // the layout we expect; anything else is left alone
                        g_grow.obj = a;
                        LOG("toc: the game's toc object is at %llx (sizes/offsets pointers and the asset count %llu check out)", (unsigned long long)a, (unsigned long long)count);
                        return true;
                    }
                    const size_t step = (size_t)(h - p) + 8;
                    p += step;
                    left -= step;
                }
            }
            addr = base + mbi.RegionSize;
        }
        LOG("toc: the game's toc object was not found");
        return false;
    }

    static bool PrepareGrow() {
        if (g_grow.ready) return true;
        if (!Locate() || !FindObject()) return false;
        uint32_t first = 0, count = 0;
        ReadBytes(g_tab.spans, &first, 4);
        ReadBytes(g_tab.spans + 4, &count, 4);
        if (g_tab.span_count < 1 || count < 1000 || (size_t)first + count > g_tab.count) { LOG("toc: span 0 looks wrong (first %u count %u): not adding assets", first, count); return false; }
        g_grow.base_count = g_tab.count;
        g_grow.span_count = count;
        g_grow.copy_first = first;
        g_grow.cap = kSpare;
        const size_t n = g_tab.count + count + kSpare;
        g_grow.ids = static_cast<uint64_t*>(VirtualAlloc(nullptr, n * 8, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        g_grow.offs = static_cast<uint8_t*>(VirtualAlloc(nullptr, n * 8, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        g_grow.sizes = static_cast<uint8_t*>(VirtualAlloc(nullptr, n * 12, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_grow.ids || !g_grow.offs || !g_grow.sizes) { LOG("toc: could not allocate the grown tables"); return false; }
        if (!ReadBytes(g_tab.ids, g_grow.ids, g_tab.count * 8) || !ReadBytes(g_tab.offs, g_grow.offs, g_tab.count * 8) || !ReadBytes(g_tab.sizes, g_grow.sizes, g_tab.count * 12)) {
            LOG("toc: could not copy the tables");
            return false;
        }
        // span 0's entries again, right after the old tables, to be extended with the new assets
        std::memcpy(g_grow.ids + g_tab.count, g_grow.ids + first, (size_t)count * 8);
        std::memcpy(g_grow.offs + 8 * g_tab.count, g_grow.offs + 8 * (size_t)first, (size_t)count * 8);
        std::memcpy(g_grow.sizes + 12 * g_tab.count, g_grow.sizes + 12 * (size_t)first, (size_t)count * 12);
        g_grow.last_id = g_grow.ids[g_tab.count + count - 1];
        g_grow.ready = true;
        LOG("toc: grown tables ready: span 0 (%u assets, last id %016llX) copied behind the old tables, room for %zu more assets; nothing is repointed until the first asset is added", count,
            (unsigned long long)g_grow.last_id, kSpare);
        return true;
    }

    // Adds a new asset `id` (ids must ascend across calls) whose bytes are read from `path` as if they were stored in the archive file of `use_archive`.
    bool AddAsset(uint64_t id, uint32_t use_archive, const wchar_t* path, uint32_t size) {
        if (!PrepareGrow()) return false;
        if (g_grow.added >= g_grow.cap) { LOG("toc: no room for more assets"); return false; }
        if (id <= g_grow.last_id) { LOG("toc: new ids must ascend (%016llX after %016llX)", (unsigned long long)id, (unsigned long long)g_grow.last_id); return false; }
        const std::string aname = ArchiveName(use_archive);
        if (aname.empty()) { LOG("toc: archive index %u has no name", use_archive); return false; }
        HANDLE f = INVALID_HANDLE_VALUE;
        uint32_t fsize = size;
        if (!size) {   // no size given: the file must exist now
            f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (f == INVALID_HANDLE_VALUE) { LOG("toc: cannot open the file (error %lu)", GetLastError()); return false; }
            LARGE_INTEGER sz{};
            GetFileSizeEx(f, &sz);
            fsize = (uint32_t)sz.QuadPart;
        }
        const size_t index = g_grow.base_count + g_grow.span_count + g_grow.added;
        const uint32_t offset = g_next_sentinel.fetch_add(0x10000u);
        Carrier c;
        c.id = id; c.index = index; c.archive_name = aname; c.path = path; c.file = f; c.size = fsize; c.offset = offset;
        {
            std::lock_guard<std::mutex> lk(g_car_mu);
            g_cars.push_back(c);
        }
        g_cars_active = true;
        MH_Initialize();
        if (!g_io_hooked.exchange(true)) {
            if (MH_CreateHook(reinterpret_cast<void*>(&ReadFile), reinterpret_cast<void*>(&ReadFileDetour), reinterpret_cast<void**>(&g_read_orig)) != MH_OK ||
                MH_EnableHook(reinterpret_cast<void*>(&ReadFile)) != MH_OK) { LOG("toc: could not hook ReadFile"); g_io_hooked = false; return false; }
        }
        // the new entry, written into the spare room of the grown tables
        g_grow.ids[index] = id;
        const uint32_t e1[2] = { use_archive, offset };
        std::memcpy(g_grow.offs + 8 * index, e1, 8);
        const uint32_t e2[3] = { 1u, c.size, (uint32_t)index };
        std::memcpy(g_grow.sizes + 12 * index, e2, 12);
        g_grow.last_id = id;
        const bool first = g_grow.added == 0;
        ++g_grow.added;
        bool ok = true;
        if (first) {   // repoint the toc object at the grown tables, pointers before the count
            ok = memory::Write(g_grow.obj + 0x10, (uint64_t)(uintptr_t)g_grow.ids) && memory::Write(g_grow.obj + 0x28, (uint64_t)(uintptr_t)g_grow.sizes) &&
                 memory::Write(g_grow.obj + 0x30, (uint64_t)(uintptr_t)g_grow.offs);
        }
        // the count covers the whole table; then span 0 is pointed at the copy (first index, count) with one atomic 8-byte store
        const uint64_t total = (uint64_t)(g_grow.base_count + g_grow.span_count + g_grow.added);
        ok = ok && memory::Write(g_grow.obj + 0x48, total);
        const uint64_t span0 = (uint64_t)g_grow.base_count | ((uint64_t)(g_grow.span_count + g_grow.added) << 32);
        ok = ok && memory::Write(g_tab.spans, span0);
        LOG("toc: new asset %016llX = toc index %zu, %u bytes from %ls (as archive %u %s offset 0x%x)%s", (unsigned long long)id, index, c.size, path, use_archive, aname.c_str(), offset,
            ok ? "" : " (WRITE FAILED)");
        return ok;
    }
    // An archive index the new assets can borrow: a raw (chunk map 0) mod archive that Overstrike wrote (mods\modN) and that exists on disk. -1: none.
    int PickRawArchive() {
        if (!Locate()) return -1;
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir(exe);
        dir = dir.substr(0, dir.find_last_of(L'\\') + 1) + L"asset_archive\\";
        int best = -1;
        for (size_t i = 0; i < g_tab.archive_count; ++i) {
            const std::string n = ArchiveName((uint32_t)i);
            if (_strnicmp(n.c_str(), "mods\\mod", 8) != 0) continue;
            uint32_t chunkmap = 1;   // the archive entry's chunk map: 0 for a raw archive, nonzero for one the game reads through a chunk map (compressed)
            if (!ReadBytes(g_tab.archives + 72ull * i + 4, &chunkmap, 4) || chunkmap != 0) continue;
            const std::wstring w(n.begin(), n.end());
            if (GetFileAttributesW((dir + w).c_str()) != INVALID_FILE_ATTRIBUTES) best = (int)i;   // keep the last one that exists
        }
        if (best >= 0) LOG("toc: borrowing archive %d (%s) for the new assets", best, ArchiveName((uint32_t)best).c_str());
        return best;
    }
}
