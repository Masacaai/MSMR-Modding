#include "menu.h"

#include "log.h"
#include "scan.h"
#include "settings.h"

#include <MinHook.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string_view>

// All addresses are image offsets in Spider-Man.exe 4.0630.0.0 and are checked against the bytes the functions start with
// before anything is hooked (a mismatch leaves the Settings untouched). See docs/NEXT_RELEASE_RECON.md, section 3.
namespace menu {
namespace {
    // The settings system (static); +0x20 its config, whose tabs are at +0x168 (0x40 bytes each), +0x170 counts them.
    constexpr uintptr_t kSystem = 0x5d96dd0;
    constexpr uintptr_t kOpenOptions = 0x7dd9f0;     // pause menu: builds every tab, calls Flash OpenOptions(tabs, flag)
    constexpr uintptr_t kBuildTab = 0x807740;        // (out value, movie, tab) -> a tab's Flash object, rows and all
    constexpr uintptr_t kMovie = 0x1d29690;          // the UI movie Flash objects are made in
    constexpr uintptr_t kOpenOptionsName = 0x389f4c0, kResetAllHash = 0x6d8ed88;
    constexpr uintptr_t kSettingType = 0x3a7d6e8, kHeadingType = 0x3a7d838;   // option-type vtables
    constexpr uint32_t kTabId = 93;                  // the game's tabs are 2-10; its RESET ALL ignores anything above
    constexpr int kFirstSetting = 0x200;             // the game's settings are 0-122
    constexpr size_t kTabBytes = 0x40, kItemBytes = 0x88, kChoiceBytes = 0x50;
    constexpr size_t kRows = settings::kMaxRows, kChoices = settings::kMaxChoices;

    // A Scaleform GFx::Value as the game lays it out: 16 bytes of 0xff, the object interface, the type, the data.
    struct ObjectInterface { void** vtable; };
    struct GfxValue {
        unsigned char head[16];
        ObjectInterface* objects;
        uint32_t type, reserved;
        uint64_t data, extra;
    };
    static_assert(sizeof(GfxValue) == 0x30);
    constexpr uint32_t kTypeMask = 0x8f, kManaged = 0x40, kInt = 3, kArray = 9;

    void ReleaseValue(GfxValue& v) {
        if ((v.type & kManaged) && v.objects)
            reinterpret_cast<void(__fastcall*)(ObjectInterface*, GfxValue*, uint64_t)>(v.objects->vtable[2])(v.objects, &v, v.data);
    }
    bool PushBack(const GfxValue& array, const GfxValue& v) {
        return reinterpret_cast<bool(__fastcall*)(ObjectInterface*, uint64_t, const GfxValue*)>(array.objects->vtable[15])(
            array.objects, array.data, &v);
    }

    using InvokeFn = bool(__fastcall*)(void*, const char*, GfxValue*, uint32_t, uint64_t, float, uint32_t, uint64_t);
    using CallbackFn = void(__fastcall*)(void*, uint32_t, GfxValue*, uint32_t);
    using TypeFn = int(__fastcall*)(void*, int);
    using ValueFn = float(__fastcall*)(void*, int);
    using ActiveFn = bool(__fastcall*)(void*, int);
    using SetFn = void(__fastcall*)(void*, int, float, bool, bool);
    using ResetFn = void(__fastcall*)(void*, int, bool);
    using TextFn = const char*(__fastcall*)(uint32_t, const char*);
    using HasTextFn = bool(__fastcall*)(uint32_t);
    InvokeFn g_invoke{};
    CallbackFn g_callback{};
    TypeFn g_type{}, g_default_choice{};
    ValueFn g_value{}, g_default{};
    ActiveFn g_active{};
    SetFn g_set{};
    ResetFn g_reset{};
    TextFn g_text{};
    HasTextFn g_has_text{};

    uintptr_t g_base{};
    std::atomic<ULONGLONG> g_open_at{ 0 };
    std::atomic<ULONGLONG> g_set_at{ 0 };      // when one of our rows last changed (for the timeline below)
    std::atomic<uint32_t> g_calls[8];            // hook calls since then: type, value, active, default, default choice, text, has-text, set
    std::atomic<int> g_timeline_lines{ 0 };   // when the settings screen was last opened (OpenOptions)
    std::atomic<uint64_t> g_tabs_built{}, g_changes{};
    std::atomic<uint32_t> g_failure{};
    std::atomic<bool> g_hooked{};
    std::atomic<int> g_built{};   // the tab's items: 0 not yet, 1 ready, 2 cannot be built
    thread_local bool t_building = false;

    alignas(16) unsigned char g_tab[kTabBytes];
    alignas(16) unsigned char g_items[kRows][kItemBytes];
    alignas(16) unsigned char g_tab_items[kRows][kItemBytes];   // the items on screen: the SHOWING row and the current section's rows (copies of g_items)
    alignas(16) unsigned char g_choice[kRows][kChoices][kChoiceBytes];

    // Texts by the hash the game looks them up with: CRC-32 of a tag of our own. They answer only while our tab is being
    // built, so a game text sharing a hash is never replaced.
    struct Text { char tag[24]; uint32_t hash; const char* text; };
    std::array<Text, 8 + kRows * (2 + kChoices)> g_texts{};
    std::array<uint16_t, 8 + kRows * (2 + kChoices)> g_by_hash{};
    size_t g_text_count = 0;
    const Text *g_tab_name{}, *g_tab_heading{};
    const Text *g_titles[kRows]{}, *g_helps[kRows]{};
    const Text* g_choice_texts[kRows][kChoices]{};

    uint32_t Crc32(std::string_view s) {
        uint32_t c = 0xffffffff;
        for (unsigned char ch : s) {
            c ^= ch;
            for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320 & (0u - (c & 1)));
        }
        return ~c;
    }
    const Text* AddText(const char* tag, const char* text) {
        Text& t = g_texts[g_text_count];
        strncpy_s(t.tag, tag, _TRUNCATE);
        t.hash = Crc32(t.tag);
        t.text = text;
        g_by_hash[g_text_count] = (uint16_t)g_text_count;
        ++g_text_count;
        return &t;
    }
    void AddTexts() {
        if (g_text_count) return;
        const auto& rows = settings::Rows();
        g_tab_name = AddText("EA_TAB", "ENDLESS ARENA");
        g_tab_heading = AddText("EA_HEAD", "ENDLESS ARENA SETTINGS");
        char tag[24];
        for (size_t r = 0; r < rows.size(); ++r) {
            snprintf(tag, sizeof(tag), "EA_%zu", r);
            g_titles[r] = AddText(tag, rows[r].title.c_str());
            if (!rows[r].help.empty()) {
                snprintf(tag, sizeof(tag), "EA_%zu_H", r);
                g_helps[r] = AddText(tag, rows[r].help.c_str());
            }
            for (size_t k = 0; k < rows[r].choices.size() && k < kChoices; ++k) {
                snprintf(tag, sizeof(tag), "EA_%zu_%zu", r, k);
                g_choice_texts[r][k] = AddText(tag, rows[r].choices[k].c_str());
            }
        }
        std::sort(g_by_hash.begin(), g_by_hash.begin() + (std::ptrdiff_t)g_text_count,
                  [](uint16_t a, uint16_t b) { return g_texts[a].hash < g_texts[b].hash; });
    }
    const char* TextFor(uint32_t hash) {
        const auto end = g_by_hash.begin() + (std::ptrdiff_t)g_text_count;
        const auto at = std::lower_bound(g_by_hash.begin(), end, hash, [](uint16_t i, uint32_t h) { return g_texts[i].hash < h; });
        return at != end && g_texts[*at].hash == hash ? g_texts[*at].text : nullptr;
    }

    template <class T> void Put(unsigned char* at, size_t offset, T value) { std::memcpy(at + offset, &value, sizeof(T)); }
    template <class T> T Get(uintptr_t at) { T v; std::memcpy(&v, reinterpret_cast<const void*>(at), sizeof(T)); return v; }
    // A tag the game keeps beside its hash (+0 pointer, +8 length, +0xc hash).
    void PutTag(unsigned char* at, size_t offset, const Text* t) {
        Put<uint64_t>(at, offset, t ? (uint64_t)t->tag : 0);
        Put<uint32_t>(at, offset + 8, t ? (uint32_t)std::strlen(t->tag) : 0);
        Put<uint32_t>(at, offset + 0xc, t ? t->hash : 0);
    }

    // Copies a tab, a setting and a heading out of the game's config and builds our items from them. Game thread.
    uint32_t BuildItems() {
        __try {
            const auto config = Get<uintptr_t>(g_base + kSystem + 0x20);
            if (!config) return 9501;
            const auto tabs = Get<uintptr_t>(config + 0x168);
            const auto tab_count = Get<uint32_t>(config + 0x170);
            if (!tabs || !tab_count || tab_count > 64) return 9501;
            uintptr_t menu_tab{}, setting{}, heading{};
            for (uint32_t t = 0; t < tab_count; ++t) {
                const uintptr_t at = tabs + t * kTabBytes;
                const auto list = Get<uintptr_t>(at + 0x30);
                const auto count = Get<uint32_t>(at + 0x38);
                if (!list || !count || count > 256) continue;
                if (!menu_tab || Get<uint32_t>(at + 8) == 4) menu_tab = at;   // the GAME tab, else the first with items
                for (uint32_t i = 0; i < count; ++i) {
                    const uintptr_t item = list + i * kItemBytes;
                    const auto option = Get<uintptr_t>(item + 0x48);
                    const auto type = option ? Get<uintptr_t>(option) - g_base : 0;
                    if (!setting && type == kSettingType && Get<uintptr_t>(item + 0x70) && Get<uint32_t>(item + 0x78)) setting = item;
                    if (!heading && type == kHeadingType) heading = item;
                }
            }
            if (!menu_tab || !setting || !heading) return 9502;
            const auto choice_template = Get<uintptr_t>(setting + 0x70);
            const auto& rows = settings::Rows();
            for (size_t r = 0; r < rows.size(); ++r) {
                unsigned char* item = g_items[r];
                std::memcpy(item, reinterpret_cast<const void*>(rows[r].heading() ? heading : setting), kItemBytes);
                Put<uint64_t>(item, 8, (uint64_t)(kFirstSetting + r));
                PutTag(item, 0x10, g_titles[r]);
                PutTag(item, 0x20, g_helps[r]);
                const auto count = (uint32_t)std::min(rows[r].choices.size(), kChoices);   // none: the game's own OFF and ON
                Put<uint64_t>(item, 0x70, count ? (uint64_t)g_choice[r] : 0);
                Put<uint32_t>(item, 0x78, count);
                Put<uint32_t>(item, 0x7c, count);
                for (uint32_t k = 0; k < count; ++k) {
                    unsigned char* c = g_choice[r][k];
                    std::memcpy(c, reinterpret_cast<const void*>(choice_template), kChoiceBytes);
                    Put<uint64_t>(c, 8, (uint64_t)g_choice_texts[r][k]->tag);
                    Put<uint32_t>(c, 0x10, (uint32_t)std::strlen(g_choice_texts[r][k]->tag));
                    Put<uint32_t>(c, 0x14, g_choice_texts[r][k]->hash);
                    Put<uint64_t>(c, 0x18, 0);   // no help or preview image of its own (the row's help shows)
                    Put<uint64_t>(c, 0x20, 0);
                    Put<uint64_t>(c, 0x28, 0);
                }
            }
            std::memcpy(g_tab, reinterpret_cast<const void*>(menu_tab), kTabBytes);
            Put<uint32_t>(g_tab, 8, kTabId);
            PutTag(g_tab, 0x10, g_tab_name);
            PutTag(g_tab, 0x20, g_tab_heading);
            Put<uint64_t>(g_tab, 0x30, (uint64_t)g_tab_items);
            return 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 9501;
        }
    }
    bool Ready() {
        if (!g_built) {
            const auto code = BuildItems();
            g_failure = code;
            g_built = !code ? 1 : code == 9501 ? 0 : 2;   // a config not loaded yet is looked for again at the next opening
        }
        return g_built == 1;
    }

    // Adds our tab, built by the game's own code, to the tabs the pause menu hands to Flash.
    void AddTab(GfxValue& tabs) {
        if (!Ready()) return;
        LARGE_INTEGER t0, t1, freq;
        QueryPerformanceCounter(&t0);
        // Only the SHOWING row and the rows of the section it is on go to the screen.
        const auto& rows = settings::Rows();
        const int section = settings::CurrentSection();
        uint32_t n = 0;
        for (size_t r = 0; r < rows.size(); ++r)
            if (rows[r].tab == -1 || rows[r].tab == section) std::memcpy(g_tab_items[n++], g_items[r], kItemBytes);
        Put<uint32_t>(g_tab, 0x38, n);
        Put<uint32_t>(g_tab, 0x3c, n);
        __try {
            void* movie = reinterpret_cast<void*(__fastcall*)()>(g_base + kMovie)();
            if (!movie) return;
            GfxValue value{};
            std::memset(value.head, 0xff, sizeof(value.head));
            t_building = true;
            const bool made = reinterpret_cast<bool(__fastcall*)(GfxValue*, void*, const void*)>(g_base + kBuildTab)(&value, movie, g_tab);
            t_building = false;
            if (made && PushBack(tabs, value)) ++g_tabs_built;
            ReleaseValue(value);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            t_building = false;   // not again this session; the game's own tabs go on
            g_failure = 9503;
            g_built = 2;
        }
        QueryPerformanceCounter(&t1);
        QueryPerformanceFrequency(&freq);
        static int logged = 0;
        if (logged++ < 8) LOG("menu: built the tab (section %d, %u rows) in %.1f ms", section, n, 1000.0 * (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart);
    }

    int RowOf(int setting) {
        const int r = setting - kFirstSetting;
        return r >= 0 && r < (int)settings::Rows().size() && g_built == 1 ? r : -1;
    }

    // ---- hooks ----
    bool __fastcall InvokeHook(void* ui, const char* name, GfxValue* args, uint32_t count, uint64_t a, float b, uint32_t c, uint64_t d) {
        if (name == reinterpret_cast<const char*>(g_base + kOpenOptionsName) && args && count >= 1 &&
            (args[0].type & kTypeMask) == kArray && args[0].objects) {
            g_open_at = GetTickCount64();
            AddTab(args[0]);
        }
        // ActionScript runs inside this call: time it, to see where a slow settings screen spends its time.
        LARGE_INTEGER i0, i1, f;
        QueryPerformanceCounter(&i0);
        const ULONGLONG since_set = g_set_at ? GetTickCount64() - g_set_at : 0;
        const bool r = g_invoke(ui, name, args, count, a, b, c, d);
        QueryPerformanceCounter(&i1);
        QueryPerformanceFrequency(&f);
        const double ms = 1000.0 * (double)(i1.QuadPart - i0.QuadPart) / (double)f.QuadPart;
        static int slow = 0;
        if (ms > 15.0 && GetTickCount64() - g_open_at < 120000 && slow++ < 40)
            LOG("menu: UI call %.60s took %.1f ms", name ? name : "?", ms);
        // After one of our rows changed: every UI call the game makes in the next 1.5 s, with when and how long, then the hook call counts.
        if (g_set_at && since_set < 1500 && g_timeline_lines < 300) {
            ++g_timeline_lines;
            LOG("menu: +%llu ms UI call %.50s (%.2f ms)", (unsigned long long)since_set, name ? name : "?", ms);
        } else if (g_set_at && since_set >= 1500) {
            LOG("menu: after the change, hook calls: type %u value %u active %u default %u defchoice %u text %u hastext %u set %u", g_calls[0].load(), g_calls[1].load(),
                g_calls[2].load(), g_calls[3].load(), g_calls[4].load(), g_calls[5].load(), g_calls[6].load(), g_calls[7].load());
            g_set_at = 0;
        }
        return r;
    }
    void __fastcall CallbackHook(void* ui, uint32_t hash, GfxValue* args, uint32_t count) {
        // Y RESET ALL on our tab: the game's RESET ALL takes only its own tabs.
        if (g_built == 1 && args && count >= 1 && (args[0].type & kTypeMask) == kInt && (uint32_t)args[0].data == kTabId &&
            hash == Get<uint32_t>(g_base + kResetAllHash)) {
            settings::ResetAll();
            ++g_changes;
            reinterpret_cast<void(__fastcall*)(void*)>(g_base + kOpenOptions)(ui);   // every tab shows its values again
            return;
        }
        LARGE_INTEGER c0, c1, cf;
        QueryPerformanceCounter(&c0);
        const uint64_t built_before = g_tabs_built;
        g_callback(ui, hash, args, count);
        // A preset or a new section changed which rows are on screen: build the tabs again (unless the game just did, as it does after a change).
        if (g_built == 1 && settings::TakeRebuild() && g_tabs_built == built_before)
            reinterpret_cast<void(__fastcall*)(void*)>(g_base + kOpenOptions)(ui);
        QueryPerformanceCounter(&c1);
        QueryPerformanceFrequency(&cf);
        const double ms = 1000.0 * (double)(c1.QuadPart - c0.QuadPart) / (double)cf.QuadPart;
        static int slow = 0;
        if (ms > 8.0 && slow++ < 20) LOG("menu: the game took %.1f ms for callback %08x (tabs built %llu -> %llu)", ms, hash, (unsigned long long)built_before, (unsigned long long)g_tabs_built.load());
    }
    int __fastcall TypeHook(void* sys, int setting) {   // 0 a list, 5 a heading
        if (g_set_at) ++g_calls[0];
        const int r = RowOf(setting);
        if (r < 0) return g_type(sys, setting);
        return settings::Rows()[(size_t)r].heading() ? 5 : 0;
    }
    float __fastcall ValueHook(void* sys, int setting) {
        if (g_set_at) ++g_calls[1];
        const int r = RowOf(setting);
        return r < 0 ? g_value(sys, setting) : (float)settings::Index((size_t)r);
    }
    bool __fastcall ActiveHook(void* sys, int setting) {
        if (g_set_at) ++g_calls[2];
        const int r = RowOf(setting);
        return r < 0 ? g_active(sys, setting) : true;
    }
    float __fastcall DefaultHook(void* sys, int setting) {
        if (g_set_at) ++g_calls[3];
        const int r = RowOf(setting);
        return r < 0 ? g_default(sys, setting) : (float)settings::Rows()[(size_t)r].def;
    }
    int __fastcall DefaultChoiceHook(void* sys, int setting) {
        if (g_set_at) ++g_calls[4];
        const int r = RowOf(setting);
        return r < 0 ? g_default_choice(sys, setting) : settings::Rows()[(size_t)r].def;
    }
    void __fastcall SetHook(void* sys, int setting, float value, bool apply, bool force) {
        const int r = RowOf(setting);
        if (r < 0) return g_set(sys, setting, value, apply, force);
        g_timeline_lines = 0;
        for (auto& c : g_calls) c = 0;
        g_set_at = GetTickCount64();
        ++g_calls[7];
        if (std::isfinite(value) && std::fabs(value) < 64) {
            settings::SetIndex((size_t)r, (int)std::lround(value));
            ++g_changes;
        }
    }
    void __fastcall ResetHook(void* sys, int setting, bool apply) {
        const int r = RowOf(setting);
        if (r < 0) return g_reset(sys, setting, apply);
        settings::ResetRow((size_t)r);
        ++g_changes;
    }
    const char* __fastcall TextHook(uint32_t hash, const char* fallback) {
        if (g_set_at) ++g_calls[5];
        if (t_building)
            if (const char* text = TextFor(hash)) return text;
        return g_text(hash, fallback);
    }
    bool __fastcall HasTextHook(uint32_t hash) {
        if (g_set_at) ++g_calls[6];
        return (t_building && TextFor(hash)) || g_has_text(hash);
    }

    struct Hook {
        uintptr_t rva;
        void* detour;
        void** original;
        unsigned char bytes[10];   // the function's first bytes in the supported game
        size_t count;
    };
    const Hook kHooks[] = {
        { 0x1d1cf30, (void*)&InvokeHook, (void**)&g_invoke, { 0x80, 0xb9, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x75, 0x03 }, 9 },
        { 0x7dc400, (void*)&CallbackHook, (void**)&g_callback, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10 }, 10 },
        { 0x72d640, (void*)&TypeHook, (void**)&g_type, { 0x48, 0x63, 0xc2, 0x48, 0x8d, 0x14, 0x80, 0x8b, 0x84, 0xd1 }, 10 },
        { 0x72d650, (void*)&ValueHook, (void**)&g_value, { 0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18 }, 10 },
        { 0x72e1d0, (void*)&ActiveHook, (void**)&g_active, { 0x48, 0x63, 0xc2, 0x48, 0x8d, 0x14, 0x80, 0x0f, 0xb6, 0x84 }, 10 },
        { 0x72d5f0, (void*)&DefaultHook, (void**)&g_default, { 0x48, 0x63, 0xc2, 0x48, 0x8d, 0x14, 0x80, 0xf3, 0x0f, 0x10 }, 10 },
        { 0x72d610, (void*)&DefaultChoiceHook, (void**)&g_default_choice, { 0x48, 0x83, 0xec, 0x28, 0x48, 0x63, 0xc2, 0x0f, 0x57, 0xc9 }, 10 },
        { 0x730ee0, (void*)&SetHook, (void**)&g_set, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10 }, 10 },
        { 0x730ae0, (void*)&ResetHook, (void**)&g_reset, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10 }, 10 },
        { 0x1749970, (void*)&TextHook, (void**)&g_text, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10 }, 10 },
        { 0x1749ae0, (void*)&HasTextHook, (void**)&g_has_text, { 0x48, 0x83, 0xec, 0x28, 0x8b, 0xd1, 0x48, 0x8b, 0x0d }, 9 },
    };

    bool BytesMatch(uintptr_t at, const unsigned char* bytes, size_t count) {
        __try { return std::memcmp(reinterpret_cast<const void*>(at), bytes, count) == 0; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

    uint32_t Install() {
        if (g_hooked) return 0;
        g_base = scan::Game().base;
        settings::Load();
        for (size_t i = 0; i < std::size(kHooks); ++i)
            if (!BytesMatch(g_base + kHooks[i].rva, kHooks[i].bytes, kHooks[i].count)) {
                LOG("menu: code at exe+%llx is not this game's (hook %zu), settings tab off", (unsigned long long)kHooks[i].rva, i);
                return 9301 + (uint32_t)i;
            }
        AddTexts();
        auto st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return 9400 + st;
        for (size_t i = 0; i < std::size(kHooks); ++i)
            if ((st = MH_CreateHook((void*)(g_base + kHooks[i].rva), kHooks[i].detour, kHooks[i].original)) != MH_OK) {
                while (i--) MH_RemoveHook((void*)(g_base + kHooks[i].rva));
                LOG("menu: MH_CreateHook failed (%d)", st);
                return 9420 + st;
            }
        // All at once: the tab never reaches the game before the answers about its rows.
        for (const auto& h : kHooks)
            if ((st = MH_QueueEnableHook((void*)(g_base + h.rva))) != MH_OK) return 9440 + st;
        if ((st = MH_ApplyQueued()) != MH_OK) return 9460 + st;
        g_hooked = true;
        LOG("menu: settings tab hooked (%zu rows)", settings::Rows().size());
        return 0;
    }

    void LogStatus() {
        LOG("menu: hooked=%d built=%d status=%u tabs=%llu changes=%llu", (int)g_hooked, (int)g_built, g_failure.load(),
            (unsigned long long)g_tabs_built, (unsigned long long)g_changes);
    }
}
