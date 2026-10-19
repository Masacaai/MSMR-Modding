#pragma once

#include <cstdint>
#include <string>

// Whose side an actor is on (recon 2026-10-19, by disassembly; the dev command `alleg` tests it in the game).
//
// Every actor that can fight has a SimpleAllegiance component (vtable exe+4f89dc0): its team value is the int at +0x48. The
// mission action SetAllegianceScriptAction (handler exe+211f210) changes it with exe+2117e00 (this, value): it stores +0x48 and
// notifies the game's relation tables; then it calls the component's vtable slot 13 (+0x68). Values are the SimpleAllegiance enum,
// in the order of the names at exe+39470d8: 0 kNeutral, 1 kBlueTeam (the hero's), 2 kRedTeam, 3 kSuperFriends, 4 kBaseDestructables,
// 5 kHeroOnlyTarget, 6 kFreeForAll, 7 kPolice, 8 kInnerDemons, 9 kSableFriendly, 10 kSableEnemy, 11 kFisk, 12 kTaskmaster,
// 13 kPrisoner, 14 kHammerhead, 15 kMaggia. Who is hostile to whom comes from relation tables (AllegianceWithRelationPair) that
// are not read yet; the experiments will show it. Game thread (Set calls game code).
namespace allegiance {
    constexpr int kCount = 16;
    const char* Name(int value);
    int Parse(const std::string& s);                 // a number, or a name with or without the leading k; -1 if unknown
    bool Get(uintptr_t entity, int& value);          // guarded read
    bool Set(uintptr_t entity, int value);           // the game's own setter; false if the code is not as expected
}
