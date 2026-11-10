// Manhattan Skies: time of day and weather control. A separate mod from Endless Arena and Manhattan Streets.
// Loaded by Overstrike's scripts proxy (LoadLibrary from scripts\, then script_enable()).
// Dev control: write commands to scripts\ManhattanSkies.cmd (one per line); results go to scripts\ManhattanSkies.log.
//
// S0: passive recorder (lighting-mode changes, atmosphere slot activity).  S1: `sky <index>` sets the lighting mode like the Settings menu.
// S2: the MANHATTAN SKIES tab in the shared MODS menu (menu.cpp, ModMenu/): a TIME OF DAY row with all seven modes.
#include "log.h"
#include "memory.h"
#include "menu.h"

#include "../../../ModMenu/modmenu.h"
#include "natives.h"
#include "recorder.h"
#include "scan.h"
#include "sky.h"
#include "watchpoint.h"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::string g_dir;

static std::string ModuleDir() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&ModuleDir, &self);
    char path[MAX_PATH];
    GetModuleFileNameA(self, path, MAX_PATH);
    std::string s(path);
    return s.substr(0, s.find_last_of("\\/"));
}

static void RunCommand(const std::string& line) {
    LOG("> %s", line.c_str());
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    if (cmd == "ping") {
        LOG("pong");
    } else if (cmd == "sky") {
        int index = -1;
        if (in >> index) recorder::SetMode(index); else recorder::Status();
    } else if (cmd == "peek") {
        std::string where; size_t len = 0x40;
        in >> where >> len;
        const uintptr_t a = where.rfind("exe+", 0) == 0 ? scan::FromRva(strtoull(where.c_str() + 4, nullptr, 16)) : strtoull(where.c_str(), nullptr, 16);
        if (a) recorder::Peek(a, len > 0x400 ? 0x400 : len); else LOG("peek: usage peek <addr|exe+rva> [len]");
    } else if (cmd == "findq") {
        std::string v; int max = 24;
        in >> v >> max;
        if (v.empty()) LOG("findq: usage findq <hex64> [max hits]"); else recorder::FindQ(strtoull(v.c_str(), nullptr, 16), max);
    } else if (cmd == "poke32" || cmd == "pokef") {
        std::string where, val;
        in >> where >> val;
        const uintptr_t a = where.rfind("exe+", 0) == 0 ? scan::FromRva(strtoull(where.c_str() + 4, nullptr, 16)) : strtoull(where.c_str(), nullptr, 16);
        if (!a || val.empty()) LOG("poke: usage %s <addr|exe+rva> <value>", cmd.c_str());
        else if (cmd == "poke32") recorder::Poke32(a, (uint32_t)strtoul(val.c_str(), nullptr, 16));
        else recorder::PokeF(a, strtof(val.c_str(), nullptr));
    } else if (cmd == "nofade") {   // nofade <ms>: swallow the lighting transition's fade for that long (then e.g. `sky 2`)
        unsigned ms = 5000;
        in >> ms;
        modmenu::SuppressFade(ms > 60000 ? 60000 : ms);
        LOG("nofade: lighting transition fade swallowed for %u ms", ms);
    } else if (cmd == "smoothfade") {   // smoothfade 0|1: the day-night cycle swallows the fade at its dawn/dusk switches
        int on = 1;
        in >> on;
        sky::SetSmoothFade(on != 0);
    } else if (cmd == "holdf") {   // holdf <ms> <addr|exe+rva> <float> [<addr> <float> ...]: rewrites floats every millisecond (for values the game rebuilds each frame)
        unsigned ms = 0;
        in >> ms;
        std::vector<std::pair<uintptr_t, float>> items;
        std::string where, val;
        while (in >> where >> val) {
            const uintptr_t a = where.rfind("exe+", 0) == 0 ? scan::FromRva(strtoull(where.c_str() + 4, nullptr, 16)) : strtoull(where.c_str(), nullptr, 16);
            if (a) items.emplace_back(a, strtof(val.c_str(), nullptr));
        }
        if (!ms || items.empty()) LOG("holdf: usage holdf <ms> <addr|exe+rva> <float> [...]");
        else {
            if (ms > 60000) ms = 60000;
            LOG("holdf: holding %zu value(s) for %u ms", items.size(), ms);
            std::thread([ms, items] {
                const ULONGLONG end = GetTickCount64() + ms;
                while (GetTickCount64() < end) {
                    for (const auto& it : items) memory::Write(it.first, it.second);
                    Sleep(1);
                }
                LOG("holdf: done");
            }).detach();
        }
    } else if (cmd == "loadtrace") {
        unsigned ms = 4000;
        in >> ms;
        recorder::LoadTrace(ms > 60000 ? 60000 : ms);
    } else if (cmd == "atmload") {
        std::string what, mg;
        in >> what >> mg;
        uintptr_t mgr = 0;
        if (!mg.empty()) mgr = mg.rfind("exe+", 0) == 0 ? scan::FromRva(strtoull(mg.c_str() + 4, nullptr, 16)) : strtoull(mg.c_str(), nullptr, 16);
        if (what.empty()) LOG("atmload: usage atmload <hex id | @path-substring> [manager addr|exe+rva]"); else recorder::AtmLoad(what, mgr);
    } else if (cmd == "atmlist") {
        std::string f;
        in >> f;
        recorder::AtmList(f);
    } else if (cmd == "atmrelease") {
        int slot = -1;
        in >> slot;
        recorder::ReleaseSlot(slot);
    } else if (cmd == "dumpdata") {
        std::string name;
        in >> name;
        if (name.empty()) LOG("dumpdata: usage dumpdata <name>"); else recorder::DumpData(g_dir + "\\dump_" + name + ".bin");
    } else if (cmd == "skyset") {   // skyset <setting index> <value>: 0 style, 1 variant, 2 dir, 3 height, 4 color, 5 bright, 6 fog, 7 fogdist, 8 haze, 9 exposure, 10 filter, 11 clouds, 12 fogcolor
        int which = -1, value = 0;
        in >> which >> value;
        if (which >= 0 && which < sky::kSettingCount) sky::Set((sky::Setting)which, value); else LOG("skyset: usage skyset <0-12> <value index>");
    } else if (cmd == "skystate") {
        sky::LogState();
    } else if (cmd == "skyreset") {
        sky::ResetAll();
    } else if (cmd == "look") {   // look <style 0-6> <variant>
        int style = 0, variant = 0;
        in >> style >> variant;
        sky::Set(sky::kStyle, style);
        sky::Set(sky::kVariant, variant);
    } else if (cmd == "sun") {    // sun <dir> <height> <color> <bright> (indices into the menu rows)
        int d = 0, h = 0, c = 0, b = 0, f = 0;
        in >> d >> h >> c >> b >> f;
        sky::Set(sky::kDir, d);
        sky::Set(sky::kHeight, h);
        sky::Set(sky::kColor, c);
        sky::Set(sky::kBright, b);
        sky::Set(sky::kFog, f);
    } else if (cmd == "adj") {
        int slot = -1, delta = 0;
        in >> slot >> delta;
        recorder::Adjust(slot, delta);
    } else if (cmd == "slotaddr") {
        int slot = 0;
        in >> slot;
        recorder::SlotAddr(slot);
    } else if (cmd == "wp") {
        std::string sub, where;
        in >> sub >> where;
        if (sub == "set") {
            const uintptr_t a = where.rfind("exe+", 0) == 0 ? scan::FromRva(strtoull(where.c_str() + 4, nullptr, 16)) : strtoull(where.c_str(), nullptr, 16);
            watchpoint::Set(a);
        } else if (sub == "dump") watchpoint::Dump();
        else if (sub == "clear") watchpoint::Clear();
        else LOG("wp: usage wp set <addr|exe+rva> | dump | clear");
    } else if (cmd == "recompute") {
        recorder::Recompute();
    } else if (cmd == "uitrace") {
        unsigned ms = 3000;
        in >> ms;
        modmenu::TraceUi(ms > 60000 ? 60000 : ms);   // then e.g. `sky 3` with the pause menu open: shows which UI call closes it
    } else if (cmd == "snap") {
        std::string name;
        in >> name;
        if (name.empty()) LOG("snap: usage snap <name>"); else recorder::Snap(name);
    } else if (cmd == "snapdiff") {
        std::string a, b, a2;
        in >> a >> b >> a2;
        recorder::SnapDiff(a, b, a2);
    } else if (cmd == "menu") {
        menu::LogStatus();
    } else if (cmd == "atm") {
        recorder::DumpAtmospheres();
    } else if (cmd == "mark") {
        std::string rest;
        std::getline(in, rest);
        LOG("==== mark:%s ====", rest.c_str());
    } else {
        LOG("unknown command (ping, sky [index], findq <hex64>, pokef <addr> <f>, poke32 <addr> <hex>, recompute, atmrelease <slot>, skystate, skyreset, look <style> <variant>, sun <dir> <height> <color> <bright> [fog], loadtrace [ms], atmload <id|@name> [mgr], atmlist [name], adj <slot> <delta>, slotaddr <n>, holdf <ms> <addr> <f> [...], wp set|dump|clear, uitrace [ms], snap <name>, snapdiff <a> <b> [<a2>], menu, atm, peek <addr|exe+rva> [len], mark <text>)");
    }
}

static void PollCommands() {
    const std::string path = g_dir + "\\ManhattanSkies.cmd";
    std::vector<std::string> lines;
    {
        std::ifstream f(path);
        if (!f.good()) return;
        for (std::string l; std::getline(f, l); ) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            if (!l.empty()) lines.push_back(l);
        }
    }
    if (lines.empty()) return;
    std::ofstream(path, std::ios::trunc).close();   // each command runs once
    for (auto& l : lines) RunCommand(l);
}

static void Main() {
    g_dir = ModuleDir();
    logging::Init(g_dir + "\\ManhattanSkies.log");
    LOG("Manhattan Skies (S2) loaded from %s", g_dir.c_str());
    const auto& m = scan::Game();
    LOG("game module base=%llx .text=%llx size=%llx", (unsigned long long)m.base, (unsigned long long)m.text,
        (unsigned long long)m.text_size);
    const int ok = natives::ResolveAll();
    LOG("%d/%d natives at expected addresses", ok, (int)natives::Count);
    sky::Load();
    recorder::SetAdjustFilter(&sky::Filter);   // before the first activation, so the base slot is tracked from the start
    recorder::Install();
    std::thread([] {   // the MODS menu host may wait for an older Endless Arena's own hooks first (ModMenu)
        const uint32_t code = menu::Install();
        if (code) LOG("menu: MODS tab not registered (%u)", code);
    }).detach();
    LOG("ready; write commands to %s\\ManhattanSkies.cmd", g_dir.c_str());
    for (;;) {
        PollCommands();
        recorder::Tick();
        sky::Tick();
        Sleep((DWORD)sky::TickMs());
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) {
    return TRUE;
}

extern "C" __declspec(dllexport) void script_enable() {
    std::thread(Main).detach();
}
