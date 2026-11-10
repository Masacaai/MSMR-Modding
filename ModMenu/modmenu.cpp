#include "modmenu.h"

#include <MinHook.h>
#include <Windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Every participating DLL exports this, so a host can tell a participant from an older script that hooks the menu on its own.
extern "C" __declspec(dllexport) uint32_t MSMR_ModMenu_Participant = modmenu::kProtocol;

// Error codes: 9301+i the code at hook i is not this game's; 94xx MinHook; 9601 a host exists but never published itself; 9602 the
// host speaks an older protocol; 9603 too many mods; 9604 shared object failed.
namespace modmenu {
namespace {
    // ---- game addresses (Spider-Man.exe 4.0630.0.0) ----
    constexpr uintptr_t kSystem = 0x5d96dd0;          // settings system; +0x20 config: tabs +0x168 (0x40 each), count +0x170
    constexpr uintptr_t kOpenOptions = 0x7dd9f0;      // builds every tab, calls Flash OpenOptions(tabs, flag); also how SETTINGS opens
    constexpr uintptr_t kMovie = 0x1d29690;           // the UI movie Flash objects are made in
    constexpr uintptr_t kOpenOptionsName = 0x389f4c0; // the "OpenOptions" literal kOpenOptions passes to the UI call wrapper
    constexpr uintptr_t kSelectHash = 0x6d8ed44;      // callback hashes (set at start-up from their names)
    constexpr uintptr_t kCloseOptionsHash = 0x6d8ed7c;
    constexpr uintptr_t kResetAllHash = 0x6d8ed88;
    constexpr uintptr_t kPauseCloseReturn = 0x7dc06b; // where the pause menu's update gets back from its CloseMenu call
    constexpr uintptr_t kPageItemReturn = 0x7dd8ae;   // where the first page's item loop gets back from AddItem
    constexpr uintptr_t kSettingType = 0x3a7d6e8, kHeadingType = 0x3a7d838;
    constexpr uint32_t kSettingsItem = 5;             // the first page's SETTINGS (its Select case calls kOpenOptions)
    constexpr bool kModsPageItem = false;             // 2026-11-09: no separate MODS entry; the tabs sit in Settings (set true for the old MODS screen, which also needs BuildTabHook's old filter)
    constexpr uint32_t kModsItem = 0x3ff;             // our MODS item (the game's Select switch ignores numbers above 0x1d)
    constexpr uint32_t kFirstTabId = 94;              // game tabs 2-10, Endless Arena's old own tab 93
    constexpr int kFirstSetting = 0x400;              // game settings 0-122, Endless Arena's old own rows 0x200+; mod m row id r -> 0x400 + m*0x100 + r
    constexpr int kMaxMods = 8, kMaxVisible = 160;
    constexpr size_t kTabBytes = 0x40, kItemBytes = 0x88, kChoiceBytes = 0x50;

    LogFn g_log{};
    void Log(const char* fmt, ...) {
        if (!g_log) return;
        char buf[512];
        va_list a;
        va_start(a, fmt);
        vsnprintf(buf, sizeof(buf), fmt, a);
        va_end(a);
        g_log(buf);
    }

    // ---- the shared object: the host publishes its entry points here ----
    struct HostApi {
        uint32_t protocol, size;
        uint32_t (*register_mod)(const Mod* mod);
        void (*log_status)();
        void (*trace_ui)(unsigned ms);   // added later in protocol 1; check size
        void (*keep_open)(unsigned ms);  // likewise
        void (*suppress_fade)(unsigned ms);   // likewise
    };
    struct Shared { HostApi* api; };

    // ---- host state ----
    struct ObjectInterface { void** vtable; };
    struct GfxValue {
        unsigned char head[16];
        ObjectInterface* objects;
        uint32_t type, reserved;
        uint64_t data, extra;
    };
    static_assert(sizeof(GfxValue) == 0x30);
    constexpr uint32_t kTypeMask = 0x8f, kManaged = 0x40, kInt = 3, kUInt = 4, kArray = 9;

    struct Text { std::string tag, text; uint32_t hash = 0; };
    struct Entry {
        Mod mod{};
        // Built per opening; kept until the next build of this tab (Flash reads the item memory while it builds).
        std::unique_ptr<unsigned char[]> items, choices;
        alignas(16) unsigned char tab[kTabBytes]{};
        std::vector<Text> texts;
        std::vector<std::string> strings;   // titles/helps/choices copied from the mod for this build
    };
    std::array<Entry, kMaxMods> g_mods;
    std::atomic<int> g_count{ 0 };
    std::mutex g_register_mutex;
    HostApi g_api{};
    bool g_is_host = false;
    std::atomic<bool> g_hooked{ false };
    uintptr_t g_base{};

    // Templates copied from the game's own config at the first opening.
    alignas(16) unsigned char g_tpl_tab[kTabBytes], g_tpl_setting[kItemBytes], g_tpl_heading[kItemBytes], g_tpl_choice[kChoiceBytes];
    std::atomic<int> g_tpl{ 0 };   // 0 not yet, 1 ready, 2 impossible
    alignas(16) unsigned char g_mods_item[kItemBytes];
    Text g_mods_title, g_mods_help, g_tab_texts[kMaxMods];

    std::atomic<bool> g_mods_screen{ false };   // the options screen now open is the MODS screen
    std::atomic<uint64_t> g_open_count{ 0 }, g_tabs_built{ 0 }, g_pages{ 0 }, g_changes{ 0 };
    // The last fault caught around a call into the game's UI code: the exception code, where it happened (image offset if in Spider-Man.exe), the address it touched.
    volatile unsigned long g_fault_code = 0;
    volatile uintptr_t g_fault_rip = 0, g_fault_addr = 0;
    std::atomic<ULONGLONG> g_trace_until{ 0 }, g_keep_until{ 0 }, g_nofade_until{ 0 };
    std::atomic<uint64_t> g_fades_blocked{ 0 };
    std::atomic<uint64_t> g_closes_blocked{ 0 };
    std::atomic<bool> g_page_off{ false };   // after one fault adding MODS to the first page, never again (a half-built Scaleform value is left behind)
    int CaptureFault(EXCEPTION_POINTERS* ep) {
        g_fault_code = ep->ExceptionRecord->ExceptionCode;
        g_fault_rip = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
        g_fault_addr = ep->ExceptionRecord->NumberParameters > 1 ? (uintptr_t)ep->ExceptionRecord->ExceptionInformation[1] : 0;
        return EXCEPTION_EXECUTE_HANDLER;
    }
    std::atomic<uint64_t> g_page_calls{ 0 }, g_page_settings{ 0 }, g_page_fail{ 0 }, g_selects{ 0 };   // diagnostics for the first-page item
    thread_local bool t_building = false;
    thread_local const std::unordered_map<uint32_t, const char*>* t_texts = nullptr;

    using InvokeFn = bool(__fastcall*)(void*, const char*, GfxValue*, uint32_t, uint64_t, float, uint32_t, uint64_t);
    using CallbackFn = void(__fastcall*)(void*, uint32_t, GfxValue*, uint32_t);
    using TypeFn = int(__fastcall*)(void*, int);
    using ValueFn = float(__fastcall*)(void*, int);
    using ActiveFn = bool(__fastcall*)(void*, int);
    using SetFn = void(__fastcall*)(void*, int, float, bool, bool);
    using ResetFn = void(__fastcall*)(void*, int, bool);
    using TextFn = const char*(__fastcall*)(uint32_t, const char*);
    using HasTextFn = bool(__fastcall*)(uint32_t);
    using BuildTabFn = bool(__fastcall*)(GfxValue*, void*, const void*);
    using AddItemFn = bool(__fastcall*)(GfxValue*, void*, const void*, bool);
    InvokeFn g_invoke{};
    CallbackFn g_callback{};
    TypeFn g_type{}, g_default_choice{};
    ValueFn g_value{}, g_default{};
    ActiveFn g_active{};
    SetFn g_set{};
    ResetFn g_reset{};
    TextFn g_text{};
    HasTextFn g_has_text{};
    BuildTabFn g_build_tab{};
    AddItemFn g_add_item{};

    uint32_t Crc32(std::string_view s) {
        uint32_t c = 0xffffffff;
        for (unsigned char ch : s) {
            c ^= ch;
            for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320 & (0u - (c & 1)));
        }
        return ~c;
    }
    template <class T> void Put(unsigned char* at, size_t offset, T value) { std::memcpy(at + offset, &value, sizeof(T)); }
    template <class T> T Get(uintptr_t at) { T v; std::memcpy(&v, reinterpret_cast<const void*>(at), sizeof(T)); return v; }
    void PutTag(unsigned char* at, size_t offset, const Text* t) {   // a tag: +0 pointer, +8 length, +0xc hash
        Put<uint64_t>(at, offset, t ? (uint64_t)t->tag.c_str() : 0);
        Put<uint32_t>(at, offset + 8, t ? (uint32_t)t->tag.size() : 0);
        Put<uint32_t>(at, offset + 0xc, t ? t->hash : 0);
    }
    void SetText(Text& t, std::string tag, std::string text) {
        t.tag = std::move(tag);
        t.text = std::move(text);
        t.hash = Crc32(t.tag);
    }

    void ReleaseValue(GfxValue& v) {
        if ((v.type & kManaged) && v.objects)
            reinterpret_cast<void(__fastcall*)(ObjectInterface*, GfxValue*, uint64_t)>(v.objects->vtable[2])(v.objects, &v, v.data);
    }
    bool PushBack(const GfxValue& array, const GfxValue& v) {
        return reinterpret_cast<bool(__fastcall*)(ObjectInterface*, uint64_t, const GfxValue*)>(array.objects->vtable[15])(array.objects, array.data, &v);
    }
    void Reopen(void* ui) { reinterpret_cast<void(__fastcall*)(void*)>(g_base + kOpenOptions)(ui); }

    // Copies a tab, a setting (with its first choice) and a heading out of the game's loaded config. Game thread.
    bool Templates() {
        if (g_tpl) return g_tpl == 1;
        __try {
            const auto config = Get<uintptr_t>(g_base + kSystem + 0x20);
            if (!config) return false;   // not loaded yet: try again at the next opening
            const auto tabs = Get<uintptr_t>(config + 0x168);
            const auto tab_count = Get<uint32_t>(config + 0x170);
            if (!tabs || !tab_count || tab_count > 64) return false;
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
            if (!menu_tab || !setting || !heading) { g_tpl = 2; Log("modmenu: the game's config has no tab/setting/heading to copy; MODS off"); return false; }
            std::memcpy(g_tpl_tab, (const void*)menu_tab, kTabBytes);
            std::memcpy(g_tpl_setting, (const void*)setting, kItemBytes);
            std::memcpy(g_tpl_heading, (const void*)heading, kItemBytes);
            std::memcpy(g_tpl_choice, (const void*)Get<uintptr_t>(setting + 0x70), kChoiceBytes);
            g_tpl = 1;
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // The game's builder on one of our tabs, then onto the array going to Flash. No C++ objects here (SEH).
    bool BuildAndPush(const unsigned char* tab, GfxValue& tabs, const std::unordered_map<uint32_t, const char*>* texts) {
        __try {
            void* movie = reinterpret_cast<void*(__fastcall*)()>(g_base + kMovie)();
            if (!movie) return true;
            GfxValue value{};
            std::memset(value.head, 0xff, sizeof(value.head));
            t_building = true;
            t_texts = texts;
            const bool made = g_build_tab(&value, movie, tab);
            t_building = false;
            t_texts = nullptr;
            if (made && PushBack(tabs, value)) ++g_tabs_built;
            ReleaseValue(value);
            return true;
        } __except (CaptureFault(GetExceptionInformation())) {
            t_building = false;
            t_texts = nullptr;
            return false;
        }
    }

    // Builds mod m's tab with the game's builder and appends it to the tabs going to Flash. Game thread.
    void AddModTab(int m, GfxValue& tabs) {
        Entry& e = g_mods[(size_t)m];
        const Mod& mod = e.mod;
        int ids[kMaxVisible];
        int n = mod.rows(mod.ctx, ids, kMaxVisible);
        if (n < 0) n = 0;
        if (n > kMaxVisible) n = kMaxVisible;
        e.texts.clear();
        e.texts.reserve((size_t)n * (2 + kMaxChoices) + 2);
        std::unordered_map<uint32_t, const char*> lookup;
        auto add = [&](std::string tag, const char* text) -> const Text* {
            if (!text) return nullptr;
            Text t;
            SetText(t, std::move(tag), text);
            e.texts.push_back(std::move(t));
            const Text* p = &e.texts.back();
            lookup[p->hash] = p->text.c_str();
            return p;
        };
        char tag[32];
        snprintf(tag, sizeof(tag), "MMOD%d_TAB", m);
        const Text* tab_name = add(tag, mod.name);
        for (int i = 0; i < n; ++i) {
            const int id = ids[i];
            if (id < 0 || id > kMaxRowId) continue;
            Row r{};
            mod.row(mod.ctx, id, &r);
            unsigned char* item = e.items.get() + (size_t)i * kItemBytes;
            std::memcpy(item, r.heading ? g_tpl_heading : g_tpl_setting, kItemBytes);
            Put<uint64_t>(item, 8, (uint64_t)(kFirstSetting + m * 0x100 + id));
            snprintf(tag, sizeof(tag), "MMOD%d_%d", m, id);
            PutTag(item, 0x10, add(tag, r.title ? r.title : ""));
            snprintf(tag, sizeof(tag), "MMOD%d_%d_H", m, id);
            PutTag(item, 0x20, r.help && *r.help ? add(tag, r.help) : nullptr);
            const int count = r.heading || !r.choices ? 0 : (r.choice_count > kMaxChoices ? kMaxChoices : r.choice_count);
            unsigned char* choices = e.choices.get() + (size_t)i * kMaxChoices * kChoiceBytes;
            Put<uint64_t>(item, 0x70, count ? (uint64_t)choices : 0);   // none: the game's own OFF/ON
            Put<uint32_t>(item, 0x78, (uint32_t)count);
            Put<uint32_t>(item, 0x7c, (uint32_t)count);
            for (int k = 0; k < count; ++k) {
                unsigned char* c = choices + (size_t)k * kChoiceBytes;
                std::memcpy(c, g_tpl_choice, kChoiceBytes);
                snprintf(tag, sizeof(tag), "MMOD%d_%d_%d", m, id, k);
                const Text* t = add(tag, r.choices[k] ? r.choices[k] : "");
                Put<uint64_t>(c, 8, (uint64_t)t->tag.c_str());
                Put<uint32_t>(c, 0x10, (uint32_t)t->tag.size());
                Put<uint32_t>(c, 0x14, t->hash);
                Put<uint64_t>(c, 0x18, 0);   // no help or preview image of its own
                Put<uint64_t>(c, 0x20, 0);
                Put<uint64_t>(c, 0x28, 0);
                Put<uint64_t>(c, 0x30, 0);
            }
        }
        std::memcpy(e.tab, g_tpl_tab, kTabBytes);
        Put<uint32_t>(e.tab, 8, kFirstTabId + (uint32_t)m);
        PutTag(e.tab, 0x10, tab_name);
        PutTag(e.tab, 0x20, tab_name);
        Put<uint64_t>(e.tab, 0x30, (uint64_t)e.items.get());
        Put<uint32_t>(e.tab, 0x38, (uint32_t)n);
        Put<uint32_t>(e.tab, 0x3c, (uint32_t)n);
        if (!BuildAndPush(e.tab, tabs, &lookup))
            Log("modmenu: building the %s tab FAULTED: code %lx at exe+%llx, touching %llx", mod.name, g_fault_code, (unsigned long long)(g_fault_rip - g_base), (unsigned long long)g_fault_addr);
    }

    // A setting number -> (mod, row id), or false for the game's (and anyone else's) settings.
    bool RowOf(int setting, int& m, int& id) {
        const int s = setting - kFirstSetting;
        if (s < 0) return false;
        m = s >> 8;
        id = s & 0xff;
        return m < g_count.load() && g_tpl == 1;
    }
    Row RowInfo(int m, int id) {
        Row r{};
        g_mods[(size_t)m].mod.row(g_mods[(size_t)m].mod.ctx, id, &r);
        return r;
    }

    // The MODS screen was left: tell the mods once (they may apply deferred changes).
    void NotifyClosed() {
        if (!g_mods_screen.exchange(false)) return;
        Log("modmenu: MODS screen closed");
        for (int m = 0; m < g_count.load(); ++m) {
            const Mod& mod = g_mods[(size_t)m].mod;
            if (mod.size >= sizeof(Mod) && mod.closed) mod.closed(mod.ctx);
        }
    }

    // ---- hooks ----
    bool __fastcall InvokeHook(void* ui, const char* name, GfxValue* args, uint32_t count, uint64_t a, float b, uint32_t c, uint64_t d) {
        if (GetTickCount64() < g_trace_until && name) {
            void* frames[7] = {};
            const USHORT n = RtlCaptureStackBackTrace(1, 7, frames, nullptr);
            char where[160] = {};
            size_t used = 0;
            for (USHORT i = 0; i < n && used + 12 < sizeof(where); ++i) {
                const uintptr_t f = (uintptr_t)frames[i];
                used += (size_t)snprintf(where + used, sizeof(where) - used, " %llx", (unsigned long long)(f >= g_base && f < g_base + 0x8000000 ? f - g_base : 0));
            }
            Log("modmenu: ui '%.40s' (%u args) from%s", name, count, where);
        }
        // A mod asked for a lighting change without the fade: swallow the transition's Flash FadeOut / FadeIn for a few seconds.
        if (name && GetTickCount64() < g_nofade_until && (std::strcmp(name, "FadeOut") == 0 || std::strcmp(name, "FadeIn") == 0)) {
            if (g_fades_blocked++ < 8) Log("modmenu: %s swallowed (%u args, first arg type %u data %llx)", name, count, args && count ? (unsigned)args[0].type : 0u, args && count ? (unsigned long long)args[0].data : 0ull);
            return true;
        }
        // The pause menu closes itself when the world goes into a lighting transition: swallow that one request while a mod's menu change is under way.
        if (name && g_mods_screen && GetTickCount64() < g_keep_until && _ReturnAddress() == reinterpret_cast<void*>(g_base + kPauseCloseReturn) &&
            std::strcmp(name, "CloseMenu") == 0) {
            if (g_closes_blocked++ < 3) Log("modmenu: kept the pause menu open (CloseMenu swallowed)");
            return true;
        }
        if (name == reinterpret_cast<const char*>(g_base + kOpenOptionsName) && args && count >= 1 && (args[0].type & kTypeMask) == kArray && args[0].objects) {
            ++g_open_count;
            // Settings screen: the mods' tabs go after the game's own (the pause menu's Flash/options screen is the game's).
            if (g_count.load() && Templates()) {
                g_mods_screen = true;   // an options screen with our tabs is open
                for (int m = 0; m < g_count.load(); ++m) AddModTab(m, args[0]);
            }
        }
        return g_invoke(ui, name, args, count, a, b, c, d);
    }
    // While the MODS screen is being built, the game's own tabs (and an older script's tab) come back unbuilt, so the game skips them.
    bool __fastcall BuildTabHook(GfxValue* out, void* movie, const void* tab) {
        return g_build_tab(out, movie, tab);
    }
    std::unordered_map<uint32_t, const char*> g_page_texts;   // MODS and its help, filled at install

    // After the page's SETTINGS item, a copy of it as MODS. No C++ objects here (SEH).
    void AddModsItem(GfxValue* array, void* movie, const void* item) {
        __try {
            if (Get<uint32_t>((uintptr_t)item + 8) != kSettingsItem) return;
            ++g_page_settings;
            std::memcpy(g_mods_item, item, kItemBytes);
            Put<uint64_t>(g_mods_item, 8, (uint64_t)kModsItem);
            PutTag(g_mods_item, 0x10, &g_mods_title);
            PutTag(g_mods_item, 0x20, &g_mods_help);
            t_building = true;
            t_texts = &g_page_texts;
            const bool made = g_add_item(array, movie, g_mods_item, false);
            t_building = false;
            t_texts = nullptr;
            if (made && g_pages++ == 0) Log("modmenu: MODS added to the pause menu");
            if (!made) ++g_page_fail;
        } __except (CaptureFault(GetExceptionInformation())) {
            t_building = false;
            t_texts = nullptr;
            g_page_off = true;
            ++g_page_fail;
            Log("modmenu: adding MODS to the first page FAULTED: code %lx at %llx (exe+%llx), touching %llx; MODS item off for this session", g_fault_code,
                (unsigned long long)g_fault_rip, (unsigned long long)(g_fault_rip - g_base), (unsigned long long)g_fault_addr);
        }
    }

    // The pause menu's first page: MODS right under SETTINGS, built by the same code as the page's own items.
    bool __fastcall AddItemHook(GfxValue* array, void* movie, const void* item, bool flag) {
        const bool made = g_add_item(array, movie, item, flag);
        if (_ReturnAddress() != reinterpret_cast<void*>(g_base + kPageItemReturn)) return made;
        ++g_page_calls;
        NotifyClosed();   // the first page is being built: no options screen is open
        if (!kModsPageItem || !g_count.load() || t_building || g_page_off) return made;
        AddModsItem(array, movie, item);
        return made;
    }
    bool IntArg(const GfxValue* args, uint32_t count, uint32_t& out) {
        if (!args || count < 1) return false;
        const uint32_t t = args[0].type & kTypeMask;
        if (t != kInt && t != kUInt) return false;
        out = (uint32_t)args[0].data;
        return true;
    }
    void __fastcall CallbackHook(void* ui, uint32_t hash, GfxValue* args, uint32_t count) {
        uint32_t arg = 0;
        const bool has_int = IntArg(args, count, arg);
        if (hash == Get<uint32_t>(g_base + kSelectHash) && has_int) {
            if (g_selects++ < 20) Log("modmenu: Select %u", arg);
            if (arg == kModsItem) {   // A on MODS: the options screen with only the mods' tabs
                g_mods_screen = true;
                Reopen(ui);
                return;
            }
            if (arg == kSettingsItem) NotifyClosed();   // the game's own SETTINGS: its own tabs
        }
        if (hash == Get<uint32_t>(g_base + kResetAllHash) && has_int && arg >= kFirstTabId && arg < kFirstTabId + (uint32_t)g_count.load()) {
            const Mod& mod = g_mods[arg - kFirstTabId].mod;
            mod.reset_all(mod.ctx);
            ++g_changes;
            Reopen(ui);   // every row shows its value again
            return;
        }
        const uint64_t opened_before = g_open_count;
        g_callback(ui, hash, args, count);
        if (hash == Get<uint32_t>(g_base + kCloseOptionsHash)) { NotifyClosed(); return; }
        // A mod's rows on screen changed (a section switch, a preset): build the tabs again, unless the game just did.
        if (g_mods_screen) {
            bool rebuild = false;
            for (int m = 0; m < g_count.load(); ++m) rebuild |= g_mods[(size_t)m].mod.take_rebuild(g_mods[(size_t)m].mod.ctx) != 0;
            if (rebuild && g_open_count == opened_before) Reopen(ui);
        }
    }
    int __fastcall TypeHook(void* sys, int setting) {   // 0 a list, 5 a heading
        int m, id;
        if (setting == (int)kModsItem) return 0;
        if (!RowOf(setting, m, id)) return g_type(sys, setting);
        return RowInfo(m, id).heading ? 5 : 0;
    }
    float __fastcall ValueHook(void* sys, int setting) {
        int m, id;
        if (setting == (int)kModsItem) return 0.f;
        if (!RowOf(setting, m, id)) return g_value(sys, setting);
        const Mod& mod = g_mods[(size_t)m].mod;
        return (float)mod.value(mod.ctx, id);
    }
    bool __fastcall ActiveHook(void* sys, int setting) {
        int m, id;
        if (setting == (int)kModsItem) return true;
        return RowOf(setting, m, id) ? true : g_active(sys, setting);
    }
    float __fastcall DefaultHook(void* sys, int setting) {
        int m, id;
        if (setting == (int)kModsItem) return 0.f;
        if (!RowOf(setting, m, id)) return g_default(sys, setting);
        const Mod& mod = g_mods[(size_t)m].mod;
        return (float)mod.default_choice(mod.ctx, id);
    }
    int __fastcall DefaultChoiceHook(void* sys, int setting) {
        int m, id;
        if (setting == (int)kModsItem) return 0;
        if (!RowOf(setting, m, id)) return g_default_choice(sys, setting);
        const Mod& mod = g_mods[(size_t)m].mod;
        return mod.default_choice(mod.ctx, id);
    }
    void __fastcall SetHook(void* sys, int setting, float value, bool apply, bool force) {
        int m, id;
        if (setting == (int)kModsItem) return;
        if (!RowOf(setting, m, id)) return g_set(sys, setting, value, apply, force);
        if (std::isfinite(value) && std::fabs(value) < 64) {
            const Mod& mod = g_mods[(size_t)m].mod;
            mod.set(mod.ctx, id, (int)std::lround(value));
            ++g_changes;
        }
    }
    void __fastcall ResetHook(void* sys, int setting, bool apply) {
        int m, id;
        if (setting == (int)kModsItem) return;
        if (!RowOf(setting, m, id)) return g_reset(sys, setting, apply);
        const Mod& mod = g_mods[(size_t)m].mod;
        mod.reset(mod.ctx, id);
        ++g_changes;
    }
    const char* TextFor(uint32_t hash) {
        if (!t_building || !t_texts) return nullptr;
        const auto it = t_texts->find(hash);
        return it == t_texts->end() ? nullptr : it->second;
    }
    const char* __fastcall TextHook(uint32_t hash, const char* fallback) {
        if (const char* text = TextFor(hash)) return text;
        return g_text(hash, fallback);
    }
    bool __fastcall HasTextHook(uint32_t hash) {
        return TextFor(hash) != nullptr || g_has_text(hash);
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
        { 0x807740, (void*)&BuildTabHook, (void**)&g_build_tab, { 0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18 }, 10 },
        { 0x80a220, (void*)&AddItemHook, (void**)&g_add_item, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10 }, 10 },
    };

    // 1: the game's bytes; 2: a jmp another script's hook put there (we chain on top); 0: something else.
    int CheckTarget(uintptr_t at, const unsigned char* bytes, size_t count) {
        __try {
            if (std::memcmp(reinterpret_cast<const void*>(at), bytes, count) == 0) return 1;
            return *reinterpret_cast<const unsigned char*>(at) == 0xe9 ? 2 : 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    // An Endless Arena from before the shared menu (0.2.0-beta.1) hooks 11 of these targets ~2 s after start and refuses any that already
    // hold a jmp. Let it go first, then chain on top.
    void WaitForOlderScripts() {
        HMODULE ea = GetModuleHandleA("EndlessArena.dll");
        if (!ea || GetProcAddress(ea, "MSMR_ModMenu_Participant")) return;
        const ULONGLONG until = GetTickCount64() + 30000;
        bool all = false;
        while (!all && GetTickCount64() < until) {
            all = true;
            for (size_t i = 0; i < 11; ++i) all = all && CheckTarget(g_base + kHooks[i].rva, kHooks[i].bytes, kHooks[i].count) == 2;
            if (!all) Sleep(200);
        }
        Log("modmenu: an older Endless Arena (own tab) is loaded; %s", all ? "its hooks are in, chaining on top" : "its hooks did not appear in 30 s, hooking anyway");
    }

    uint32_t InstallHooks() {
        g_base = (uintptr_t)GetModuleHandleW(nullptr);
        WaitForOlderScripts();
        int chained = 0;
        for (size_t i = 0; i < std::size(kHooks); ++i) {
            const int how = CheckTarget(g_base + kHooks[i].rva, kHooks[i].bytes, kHooks[i].count);
            if (!how) {
                Log("modmenu: code at exe+%llx is not this game's (hook %zu); MODS off", (unsigned long long)kHooks[i].rva, i);
                return 9301 + (uint32_t)i;
            }
            chained += how == 2;
        }
        SetText(g_mods_title, "MMOD_PAGE", "MODS");
        SetText(g_mods_help, "MMOD_PAGE_H", "Settings of the installed script mods.");
        g_page_texts = { { g_mods_title.hash, g_mods_title.text.c_str() }, { g_mods_help.hash, g_mods_help.text.c_str() } };
        auto st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) return 9400 + st;
        for (size_t i = 0; i < std::size(kHooks); ++i)
            if ((st = MH_CreateHook((void*)(g_base + kHooks[i].rva), kHooks[i].detour, kHooks[i].original)) != MH_OK) {
                while (i--) MH_RemoveHook((void*)(g_base + kHooks[i].rva));
                Log("modmenu: MH_CreateHook failed (%d)", st);
                return 9420 + st;
            }
        for (const auto& h : kHooks)   // all at once: nothing reaches the game before the answers about its rows
            if ((st = MH_QueueEnableHook((void*)(g_base + h.rva))) != MH_OK) return 9440 + st;
        if ((st = MH_ApplyQueued()) != MH_OK) return 9460 + st;
        g_hooked = true;
        Log("modmenu: hosting the MODS menu (%zu hooks, %d chained on another script's)", std::size(kHooks), chained);
        return 0;
    }

    // Host side of Register (also what clients call through the shared object).
    uint32_t HostRegister(const Mod* mod) {
        std::lock_guard<std::mutex> lock(g_register_mutex);
        const int m = g_count.load();
        if (m >= kMaxMods) return 9603;
        Entry& e = g_mods[(size_t)m];
        e.mod = {};
        std::memcpy(&e.mod, mod, mod->size < sizeof(Mod) ? mod->size : sizeof(Mod));
        e.items.reset(new unsigned char[(size_t)kMaxVisible * kItemBytes]());
        e.choices.reset(new unsigned char[(size_t)kMaxVisible * kMaxChoices * kChoiceBytes]());
        g_count = m + 1;   // published last: the hooks read entries [0, count)
        Log("modmenu: %s registered (tab %u)", e.mod.name, kFirstTabId + (uint32_t)m);
        return 0;
    }
    void HostTraceUi(unsigned ms) { g_trace_until = GetTickCount64() + ms; Log("modmenu: tracing UI calls for %u ms", ms); }
    void HostKeepOpen(unsigned ms) { g_keep_until = GetTickCount64() + ms; }
    void HostSuppressFade(unsigned ms) { g_nofade_until = GetTickCount64() + ms; }
    void HostLogStatus() {
        Log("modmenu: host, %d mods, hooked=%d templates=%d mods-screen=%d openings=%llu tabs=%llu pages=%llu changes=%llu | first page: calls=%llu settings-items=%llu fail=%llu selects=%llu | closes swallowed=%llu",
            g_count.load(), (int)g_hooked, g_tpl.load(), (int)g_mods_screen, (unsigned long long)g_open_count, (unsigned long long)g_tabs_built,
            (unsigned long long)g_pages, (unsigned long long)g_changes, (unsigned long long)g_page_calls, (unsigned long long)g_page_settings,
            (unsigned long long)g_page_fail, (unsigned long long)g_selects, (unsigned long long)g_closes_blocked);
    }

    HostApi* g_remote = nullptr;   // the host's entry points, when another script hosts
    std::once_flag g_elect_once;
    uint32_t g_elect_code = 0;

    // The first script to get here in this process creates the shared object and hosts; the others read the host's entry points from it.
    void Elect() {
        wchar_t name[64];
        swprintf_s(name, L"Local\\MSMR_ModMenu_v1_%lu", GetCurrentProcessId());
        HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Shared), name);
        if (!map) { g_elect_code = 9604; return; }
        const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;
        auto* shared = static_cast<Shared*>(MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
        if (!shared) { g_elect_code = 9604; return; }
        // The handle and view stay open for the life of the process.
        if (!existed) {
            g_is_host = true;
            g_api = { kProtocol, sizeof(HostApi), &HostRegister, &HostLogStatus, &HostTraceUi, &HostKeepOpen, &HostSuppressFade };
            InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&shared->api), &g_api);
            g_elect_code = InstallHooks();
            return;
        }
        for (int i = 0; i < 100 && !shared->api; ++i) Sleep(50);   // the host publishes right after creating the object
        g_remote = static_cast<HostApi*>(InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(&shared->api), nullptr, nullptr));
        if (!g_remote) { g_elect_code = 9601; return; }
        if (g_remote->protocol < kProtocol) { g_elect_code = 9602; g_remote = nullptr; return; }
        Log("modmenu: another script hosts the MODS menu (protocol %u)", g_remote->protocol);
    }
}

    uint32_t Register(const Mod& mod, LogFn log) {
        if (!g_log) g_log = log;
        std::call_once(g_elect_once, Elect);
        if (g_elect_code) { Log("modmenu: %s not registered (%u)", mod.name, g_elect_code); return g_elect_code; }
        const uint32_t code = g_is_host ? HostRegister(&mod) : g_remote->register_mod(&mod);
        if (code) Log("modmenu: %s not registered (%u)", mod.name, code);
        return code;
    }

    void KeepMenuOpen(unsigned ms) {
        if (g_is_host) HostKeepOpen(ms);
        else if (g_remote && g_remote->size >= sizeof(HostApi)) g_remote->keep_open(ms);
    }

    void SuppressFade(unsigned ms) {
        if (g_is_host) HostSuppressFade(ms);
        else if (g_remote && g_remote->size >= sizeof(HostApi)) g_remote->suppress_fade(ms);
    }

    void TraceUi(unsigned ms) {
        if (g_is_host) HostTraceUi(ms);
        else if (g_remote && g_remote->size >= sizeof(HostApi)) g_remote->trace_ui(ms);
    }

    void LogStatus() {
        if (g_is_host) HostLogStatus();
        else if (g_remote) { Log("modmenu: client of another script's host"); g_remote->log_status(); }
        else Log("modmenu: not started (%u)", g_elect_code);
    }
}
