#include "hero.h"

#include "entity.h"
#include "memory.h"
#include "natives.h"
#include "scan.h"

#include <cstring>

namespace hero {
    Info Read() {
        Info h;
        const uintptr_t system = scan::FromRva(natives::kHeroSystemRva);
        auto get_actor = (uintptr_t(*)(void*))natives::Get(natives::GetActor);
        if (!get_actor || !memory::Read(system + 0x1C, h.handle) || h.handle == 0) return h;
        if (!memory::SafeCall(get_actor, &h.handle, h.actor) || !h.actor) return h;
        if (!memory::Read(h.actor, h.transform) || !h.transform) return h;
        if (!memory::SafeRead(h.transform + 0x30, h.pos, sizeof(h.pos))) return h;
        if (!memory::SafeRead(h.transform + 0x20, h.fwd, sizeof(h.fwd))) return h;
        h.ok = true;
        return h;
    }

    uintptr_t Component(uintptr_t entity, const char* suffix, std::string* full_name) {
        uintptr_t found = 0;
        const size_t n = strlen(suffix);
        entity::ForEachComponent(entity, [&](const std::string& name, uintptr_t inst) {
            if (!found && name.size() >= n && name.compare(name.size() - n, n, suffix) == 0) {
                found = inst;
                if (full_name) *full_name = name;
            }
        });
        return found;
    }
}
