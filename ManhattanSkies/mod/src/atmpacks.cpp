#include "atmpacks.h"

#include "log.h"
#include "toc.h"

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <vector>

namespace atmpacks {
namespace {
    // The five atmospheres the game's lighting modes use (CRC64 of their paths; docs/RECON.md Part 10), and the slot each one belongs to.
    struct BaseAtm { uint64_t id; int slot; const char* tag; };
    const BaseAtm kBase[] = {
        { 0x882245A8C2B5BD1AULL, 0, "DAY" },
        { 0x8636707D50EACAB9ULL, 1, "EARLY MORNING" },
        { 0x98246BE74781784FULL, 2, "SUNSET" },
        { 0x9F4F5899144AF4FAULL, 3, "NIGHT" },
        { 0x8BEDB52FCD0301DFULL, 4, "OVERCAST" },
    };
    constexpr int kSlots = 5;

    // ---- a minimal zip reader (central directory; stored and deflate entries; no zip64) ----
    struct Entry { std::string name; uint16_t method = 0; uint32_t csize = 0, usize = 0, lho = 0; };

    struct Tex { uint64_t new_id; Entry entry; std::wstring path; };
    struct Pack {
        std::string name;
        int slot = 0;
        uint64_t id = 0;                 // the new asset id of the atmosphere
        std::wstring zip;                // the mod file
        std::vector<Tex> textures;       // unpacked into the cache the first time the pack is picked
        bool extracted = false;
    };
    std::mutex g_mu, g_prep_mu;
    std::vector<Pack> g_packs;
    std::vector<std::string> g_label_store[kSlots];
    std::vector<const char*> g_labels[kSlots];
    std::atomic<bool> g_started{ false }, g_ready{ false }, g_blocked{ false };   // blocked: mods were found but there is no archive to borrow

    // ---- libdeflate.dll, which the game ships (raw deflate, the zip's method 8) ----
    using AllocFn = void* (__cdecl*)();
    using DecompFn = int(__cdecl*)(void*, const void*, size_t, void*, size_t, size_t*);
    using FreeFn = void(__cdecl*)(void*);
    AllocFn g_alloc = nullptr;
    DecompFn g_decomp = nullptr;
    FreeFn g_free = nullptr;

    bool LoadDeflate() {
        if (g_decomp) return true;
        HMODULE m = GetModuleHandleW(L"libdeflate.dll");
        if (!m) {
            wchar_t exe[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::filesystem::path p = std::filesystem::path(exe).parent_path() / L"libdeflate.dll";
            m = LoadLibraryW(p.c_str());
        }
        if (!m) { LOG("atmpacks: libdeflate.dll not found"); return false; }
        g_alloc = reinterpret_cast<AllocFn>(GetProcAddress(m, "libdeflate_alloc_decompressor"));
        g_decomp = reinterpret_cast<DecompFn>(GetProcAddress(m, "libdeflate_deflate_decompress"));
        g_free = reinterpret_cast<FreeFn>(GetProcAddress(m, "libdeflate_free_decompressor"));
        if (!g_alloc || !g_decomp || !g_free) { LOG("atmpacks: libdeflate.dll lacks the functions needed"); g_decomp = nullptr; return false; }
        return true;
    }

    template <typename T>
    T Rd(const std::vector<uint8_t>& b, size_t at) { T v{}; if (at + sizeof(T) <= b.size()) std::memcpy(&v, b.data() + at, sizeof(T)); return v; }

    bool ReadZipDirectory(std::ifstream& f, std::vector<Entry>& out) {
        f.seekg(0, std::ios::end);
        const std::streamoff size = f.tellg();
        if (size < 22) return false;
        const std::streamoff tail = std::min<std::streamoff>(size, 70000);
        std::vector<uint8_t> t((size_t)tail);
        f.seekg(size - tail);
        f.read(reinterpret_cast<char*>(t.data()), tail);
        size_t eocd = std::string::npos;
        for (size_t i = t.size() >= 22 ? t.size() - 22 : 0;; --i) {
            if (Rd<uint32_t>(t, i) == 0x06054b50) { eocd = i; break; }
            if (i == 0) break;
        }
        if (eocd == std::string::npos) return false;
        const uint16_t count = Rd<uint16_t>(t, eocd + 10);
        const uint32_t cd_size = Rd<uint32_t>(t, eocd + 12), cd_off = Rd<uint32_t>(t, eocd + 16);
        if (cd_off == 0xFFFFFFFFu || cd_size > (64u << 20)) return false;
        std::vector<uint8_t> cd(cd_size);
        f.seekg(cd_off);
        f.read(reinterpret_cast<char*>(cd.data()), cd_size);
        size_t p = 0;
        for (uint16_t i = 0; i < count && p + 46 <= cd.size(); ++i) {
            if (Rd<uint32_t>(cd, p) != 0x02014b50) break;
            Entry e;
            e.method = Rd<uint16_t>(cd, p + 10);
            e.csize = Rd<uint32_t>(cd, p + 20);
            e.usize = Rd<uint32_t>(cd, p + 24);
            const uint16_t nlen = Rd<uint16_t>(cd, p + 28), xlen = Rd<uint16_t>(cd, p + 30), clen = Rd<uint16_t>(cd, p + 32);
            e.lho = Rd<uint32_t>(cd, p + 42);
            if (p + 46 + nlen <= cd.size()) e.name.assign(reinterpret_cast<const char*>(cd.data() + p + 46), nlen);
            out.push_back(e);
            p += 46 + nlen + xlen + clen;
        }
        return true;
    }

    bool Extract(std::ifstream& f, const Entry& e, std::vector<uint8_t>& out) {
        if (e.csize == 0xFFFFFFFFu || e.usize == 0xFFFFFFFFu) return false;
        uint8_t lh[30];
        f.seekg(e.lho);
        f.read(reinterpret_cast<char*>(lh), 30);
        if (!f || *reinterpret_cast<uint32_t*>(lh) != 0x04034b50) return false;
        const uint16_t nlen = *reinterpret_cast<uint16_t*>(lh + 26), xlen = *reinterpret_cast<uint16_t*>(lh + 28);
        f.seekg((std::streamoff)e.lho + 30 + nlen + xlen);
        std::vector<uint8_t> raw(e.csize);
        f.read(reinterpret_cast<char*>(raw.data()), e.csize);
        if (!f) return false;
        if (e.method == 0) { out = std::move(raw); return true; }
        if (e.method != 8 || !LoadDeflate()) return false;
        out.assign(e.usize, 0);
        void* d = g_alloc();
        size_t actual = 0;
        const int rc = d ? g_decomp(d, raw.data(), raw.size(), out.data(), out.size(), &actual) : -1;
        if (d) g_free(d);
        return rc == 0 && actual == e.usize;
    }

    uint64_t Fnv(const std::string& s, uint64_t h = 1469598103934665603ULL) {
        for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
        return h;
    }
    // Deterministic, so the cached files stay valid: above every id the game has (E1...), different per mod file and asset.
    uint64_t NewId(const std::string& mod_key, uint64_t asset) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%016llX", (unsigned long long)asset);
        return 0xE100000000000000ULL | (Fnv(mod_key + buf) & 0x00FFFFFFFFFFFFFFULL);
    }

    std::filesystem::path DataDir() {
        wchar_t docs[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, docs)) || !docs[0]) return {};
        return std::filesystem::path(docs) / L"Marvel's Spider-Man Remastered" / L"ManhattanSkies";
    }

    // The game's asset id of a path: ALERT's CRC64 (reflected, polynomial C96C5795D7870F42, start value = the polynomial, lower-case, forward slashes, no
    // doubled slashes), shifted right by 2 with the top bit set.
    uint64_t PathId(const std::string& path) {
        static uint64_t table[256];
        static bool init = false;
        if (!init) {
            for (uint64_t i = 0; i < 256; ++i) { uint64_t c = i; for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xC96C5795D7870F42ULL : c >> 1; table[i] = c; }
            init = true;
        }
        uint64_t v = 0xC96C5795D7870F42ULL;
        bool had_slash = false;
        for (unsigned char ch : path) {
            char c = (char)tolower(ch);
            if (c == '\\') c = '/';
            if (c == '/') { if (had_slash) continue; had_slash = true; } else had_slash = false;
            v = (v >> 8) ^ table[(uint8_t)(v ^ (uint8_t)c)];
        }
        return (v >> 2) | 0x8000000000000000ULL;
    }
    // Every JSON string value in a text, in order (enough to read the layout of a .modular's info.json: ["", "Label", "path.stage"]).
    std::vector<std::string> JsonStrings(const std::string& t) {
        std::vector<std::string> out;
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] != '"') continue;
            std::string cur;
            for (++i; i < t.size() && t[i] != '"'; ++i) {
                if (t[i] == '\\' && i + 1 < t.size()) ++i;
                cur += t[i];
            }
            out.push_back(cur);
        }
        return out;
    }

    std::string Upper(std::string s) { for (auto& c : s) c = (char)toupper((unsigned char)c); return s; }

    struct NewAsset { uint64_t id; std::wstring path; uint32_t size; };

    void Run() {
        const auto data = DataDir();
        if (data.empty()) return;
        std::error_code ec;
        const auto folder = data / L"Atmospheres", cache = data / L"cache";
        std::filesystem::create_directories(folder, ec);
        std::filesystem::create_directories(cache, ec);
        std::vector<std::filesystem::path> mods;
        for (const auto& e : std::filesystem::directory_iterator(folder, ec))
            if (e.is_regular_file() && Upper(e.path().extension().string()) == ".SMPCMOD" || Upper(e.path().extension().string()) == ".MODULAR") mods.push_back(e.path());
        std::sort(mods.begin(), mods.end());
        if (mods.empty()) { LOG("atmpacks: no .smpcmod or .modular files in %s", folder.string().c_str()); g_ready = true; return; }
        const ULONGLONG t0 = GetTickCount64();

        std::vector<Pack> packs;
        std::vector<NewAsset> assets;
        // One source = a zip of game files keyed by asset id: an .smpcmod, or one option (a .stage) of a .modular.
        auto process = [&](const std::filesystem::path& zippath, std::map<uint64_t, Entry>& byid, const std::string& key, const std::string& base, const std::string& display) {
            std::ifstream f(zippath, std::ios::binary);
            int found = 0;
            for (const auto& b : kBase) found += byid.count(b.id) ? 1 : 0;
            if (!found) { LOG("atmpacks: %s replaces none of the five open-world atmospheres: skipped", display.c_str()); return; }
            for (const auto& b : kBase) {
                auto it = byid.find(b.id);
                if (it == byid.end()) continue;
                std::vector<uint8_t> atm;
                if (!Extract(f, it->second, atm) || atm.size() < 0x600) { LOG("atmpacks: %s: cannot extract its %s atmosphere", display.c_str(), b.tag); continue; }
                // the textures this atmosphere references that the mod ships (their bytes are unpacked later, when the pack is first picked)
                std::map<uint64_t, std::vector<size_t>> refs;
                for (size_t off = 0; off + 8 <= atm.size(); off += 4) {
                    uint64_t v;
                    std::memcpy(&v, atm.data() + off, 8);
                    if (v != b.id && byid.count(v)) {
                        bool is_base = false;
                        for (const auto& bb : kBase) is_base = is_base || bb.id == v;
                        if (!is_base) refs[v].push_back(off);
                    }
                }
                Pack pk;
                pk.name = found > 1 ? base + " (" + b.tag + ")" : base;
                pk.slot = b.slot;
                pk.id = NewId(key, b.id);
                pk.zip = zippath.wstring();
                for (const auto& r : refs) {
                    Tex t;
                    t.new_id = NewId(key, r.first);
                    t.entry = byid[r.first];
                    t.path = (cache / (std::to_wstring(t.new_id) + L".bin")).wstring();
                    for (size_t off : r.second) std::memcpy(atm.data() + off, &t.new_id, 8);   // the atmosphere now points at the new texture ids
                    assets.push_back({ t.new_id, t.path, t.entry.usize });
                    pk.textures.push_back(t);
                }
                const auto apath = cache / (std::to_wstring(pk.id) + L".bin");   // the rewritten atmosphere is small: written now
                {
                    std::ofstream o(apath, std::ios::binary | std::ios::trunc);
                    o.write(reinterpret_cast<const char*>(atm.data()), (std::streamsize)atm.size());
                }
                assets.push_back({ pk.id, apath.wstring(), (uint32_t)atm.size() });
                packs.push_back(pk);
                LOG("atmpacks: %s -> %s slot (%zu texture(s)), atmosphere id %016llX", display.c_str(), b.tag, refs.size(), (unsigned long long)pk.id);
            }
        };
        for (const auto& mp : mods) {
            std::ifstream f(mp, std::ios::binary);
            std::vector<Entry> entries;
            if (!f || !ReadZipDirectory(f, entries)) { LOG("atmpacks: cannot read %s as a zip", mp.filename().string().c_str()); continue; }
            const std::string key = mp.filename().string() + std::to_string(std::filesystem::file_size(mp, ec)) + std::to_string((long long)std::filesystem::last_write_time(mp, ec).time_since_epoch().count());
            const std::string stem = mp.stem().string();
            if (Upper(mp.extension().string()) == ".SMPCMOD") {
                std::map<uint64_t, Entry> byid;
                for (const auto& e : entries) {
                    if (e.name.rfind("ModFiles/", 0) != 0 || e.name.back() == '/') continue;
                    const size_t us = e.name.find_last_of('_');
                    if (us == std::string::npos) continue;
                    byid[strtoull(e.name.c_str() + us + 1, nullptr, 16)] = e;
                }
                process(mp, byid, key, Upper(stem), mp.filename().string());
                continue;
            }
            // .modular: info.json lists the options of each module, each one a .stage zip of game files named by path ("<span>/atmosphere/x.atmosphere")
            std::map<std::string, std::string> label_of;   // stage path -> option label
            for (const auto& e : entries) {
                if (e.name != "info.json") continue;
                std::vector<uint8_t> raw;
                if (!Extract(f, e, raw)) break;
                const auto toks = JsonStrings(std::string(raw.begin(), raw.end()));
                for (size_t i = 1; i < toks.size(); ++i) if (toks[i].size() > 6 && toks[i].compare(toks[i].size() - 6, 6, ".stage") == 0) label_of[toks[i]] = toks[i - 1];
            }
            int options = 0;
            for (const auto& e : entries) {
                if (e.name.size() < 7 || e.name.compare(e.name.size() - 6, 6, ".stage") != 0) continue;
                if (e.usize > (400u << 20)) { LOG("atmpacks: %s: %s is too large: skipped", mp.filename().string().c_str(), e.name.c_str()); continue; }
                const std::string skey = key + e.name;
                const auto spath = cache / (L"stage_" + std::to_wstring(Fnv(skey)) + L".zip");
                if (!(std::filesystem::exists(spath, ec) && std::filesystem::file_size(spath, ec) == e.usize)) {   // the option's zip is unpacked once into the cache
                    std::vector<uint8_t> stage;
                    if (!Extract(f, e, stage)) { LOG("atmpacks: %s: cannot unpack %s", mp.filename().string().c_str(), e.name.c_str()); continue; }
                    std::ofstream o(spath, std::ios::binary | std::ios::trunc);
                    o.write(reinterpret_cast<const char*>(stage.data()), (std::streamsize)stage.size());
                }
                std::ifstream sf(spath, std::ios::binary);
                std::vector<Entry> sentries;
                if (!sf || !ReadZipDirectory(sf, sentries)) { LOG("atmpacks: %s: %s is not a readable zip", mp.filename().string().c_str(), e.name.c_str()); continue; }
                std::map<uint64_t, Entry> byid;
                for (const auto& se : sentries) {
                    const size_t sl = se.name.find('/');
                    if (sl == std::string::npos || sl == 0 || se.name.back() == '/') continue;   // info.json and folders
                    bool digits = true;
                    for (size_t i = 0; i < sl; ++i) digits = digits && isdigit((unsigned char)se.name[i]);
                    if (digits) byid[PathId(se.name.substr(sl + 1))] = se;
                }
                const auto lit = label_of.find(e.name);
                std::string label = lit != label_of.end() && !lit->second.empty() ? lit->second : std::filesystem::path(e.name).stem().string();
                process(spath, byid, skey, Upper(stem) + " - " + Upper(label), mp.filename().string() + " / " + label);
                ++options;
            }
            if (!options) LOG("atmpacks: %s has no usable options", mp.filename().string().c_str());
        }
        if (packs.empty()) { g_ready = true; return; }
        // ids must ascend when added; unique
        std::sort(assets.begin(), assets.end(), [](const NewAsset& a, const NewAsset& b) { return a.id < b.id; });
        assets.erase(std::unique(assets.begin(), assets.end(), [](const NewAsset& a, const NewAsset& b) { return a.id == b.id; }), assets.end());
        // the game's asset table appears in memory shortly after the game starts: wait for it (up to 3 minutes)
        bool located = false;
        for (int i = 0; i < 90 && !located; ++i) { located = toc::Locate(true); if (!located) Sleep(2000); }
        if (!located) { LOG("atmpacks: the game's asset table was not found; no atmosphere mods this session"); return; }
        const bool pretend = std::filesystem::exists(data / L"force_blocked.txt", ec);   // test switch: behave as if there were no Overstrike archive
        const int arch = pretend ? -1 : toc::PickRawArchive();
        if (arch < 0) {
            g_blocked = true;
            g_ready = true;
            LOG("atmpacks: %s: install any .smpcmod mod with Overstrike (and press Install mods); no atmosphere mods this session", pretend ? "force_blocked.txt exists (test switch)" : "no raw mod archive to borrow");
            return;
        }
        size_t added = 0;
        for (const auto& a : assets) {
            if (!toc::AddAsset(a.id, (uint32_t)arch, a.path.c_str(), a.size)) { LOG("atmpacks: adding assets failed after %zu; stopping", added); return; }
            ++added;
        }
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_packs = packs;
            for (int s = 0; s < kSlots; ++s) {
                g_label_store[s].assign(1, "DEFAULT");
                for (const auto& p : g_packs) if (p.slot == s) g_label_store[s].push_back(p.name);
                g_labels[s].clear();
                for (const auto& l : g_label_store[s]) g_labels[s].push_back(l.c_str());
            }
        }
        LOG("atmpacks: %zu atmosphere(s) from %zu mod file(s) ready in %llu ms (%zu assets added to the game's table; textures are unpacked when a mod is first picked)", packs.size(), mods.size(),
            (unsigned long long)(GetTickCount64() - t0), added);
        g_ready = true;
    }
}

    void Start() {
        if (g_started.exchange(true)) return;
        std::thread([] { Run(); }).detach();
    }
    bool Ready() { return g_ready; }
    bool IsMod(uint64_t id) { if (!g_ready) return false; std::lock_guard<std::mutex> lk(g_mu); for (const auto& p : g_packs) if (p.id == id) return true; return false; }
    bool Blocked() { return g_blocked; }
    bool AnyMods() { if (!g_ready) return false; std::lock_guard<std::mutex> lk(g_mu); return !g_packs.empty(); }

    // Unpacks a mod's textures into the cache the first time it is needed (a second or less); the game reads them through the redirect in toc.cpp.
    bool Prepare(uint64_t atmosphere_id) {
        if (!g_ready) return false;
        Pack* pk = nullptr;
        std::lock_guard<std::mutex> lk(g_prep_mu);
        {
            std::lock_guard<std::mutex> lk2(g_mu);
            for (auto& p : g_packs) if (p.id == atmosphere_id) { pk = &p; break; }
        }
        if (!pk) return true;   // not an atmosphere mod's id: nothing to prepare
        if (pk->extracted) return true;
        const ULONGLONG t0 = GetTickCount64();
        std::ifstream f{ std::filesystem::path(pk->zip), std::ios::binary };
        std::error_code ec;
        if (!f) { LOG("atmpacks: cannot open %ls", pk->zip.c_str()); return false; }
        for (const auto& t : pk->textures) {
            if (std::filesystem::exists(t.path, ec) && std::filesystem::file_size(t.path, ec) == t.entry.usize) continue;
            std::vector<uint8_t> tex;
            if (!Extract(f, t.entry, tex)) { LOG("atmpacks: cannot unpack a texture of %s", pk->name.c_str()); return false; }
            std::ofstream o{ std::filesystem::path(t.path), std::ios::binary | std::ios::trunc };
            o.write(reinterpret_cast<const char*>(tex.data()), (std::streamsize)tex.size());
        }
        pk->extracted = true;
        LOG("atmpacks: %s unpacked in %llu ms", pk->name.c_str(), (unsigned long long)(GetTickCount64() - t0));
        return true;
    }

    int SlotOfMode(int mode) {
        switch (mode) { case 0: return 0; case 4: return 1; case 2: return 2; case 5: case 1: return 3; case 3: case 6: return 4; default: return -1; }
    }
    int LabelCount(int slot) {
        if (!g_ready || slot < 0 || slot >= kSlots) return 1;
        std::lock_guard<std::mutex> lk(g_mu);
        return (int)g_labels[slot].size() ? (int)g_labels[slot].size() : 1;
    }
    const char* const* Labels(int slot) {
        static const char* const kDefault[] = { "DEFAULT" };
        if (!g_ready || slot < 0 || slot >= kSlots) return kDefault;
        std::lock_guard<std::mutex> lk(g_mu);
        return g_labels[slot].empty() ? kDefault : g_labels[slot].data();
    }
    std::string NameAt(int slot, int index) {
        if (!g_ready || slot < 0 || slot >= kSlots || index < 1) return "";
        std::lock_guard<std::mutex> lk(g_mu);
        return (size_t)index < g_label_store[slot].size() ? g_label_store[slot][(size_t)index] : "";
    }
    uint64_t IdFor(int slot, const std::string& name) {
        if (!g_ready || name.empty()) return 0;
        std::lock_guard<std::mutex> lk(g_mu);
        for (const auto& p : g_packs) if (p.slot == slot && p.name == name) return p.id;
        return 0;
    }
    void LogStatus() {
        std::lock_guard<std::mutex> lk(g_mu);
        LOG("atmpacks: started=%d ready=%d, %zu atmosphere(s)", (int)g_started.load(), (int)g_ready.load(), g_packs.size());
    }
}
