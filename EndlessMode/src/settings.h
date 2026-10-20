#pragma once

#include <string>
#include <vector>

// Player settings for the Arena, shown as an ENDLESS ARENA tab in the game's own Settings menu (menu.cpp) and kept in
// <Documents>\Marvel's Spider-Man Remastered\EndlessMode\EndlessArena_settings.ini. Every setting is a row with a list
// of choices (a row with no choices is an ON/OFF switch) or a section heading. The rest of the mod reads values by key.
namespace settings {
    struct Row {
        std::string key;                    // ini key and lookup name ("" for a heading)
        std::string title;                  // upper case, like the game's own rows
        std::string help;                   // shown beside the row while it is selected
        std::vector<std::string> choices;   // empty: an ON/OFF switch (0 off, 1 on)
        std::vector<int> values;            // what each choice means; empty: the index itself
        int def = 0;                        // default choice
        int tab = 0;                        // which tab of the menu shows it (see TabName)
        bool heading() const { return key.empty(); }
    };

    const std::vector<Row>& Rows();
    constexpr size_t kMaxRows = 128, kMaxChoices = 14;

    // The menu has three tabs, so each stays short (a long list made the game's settings screen lag): 0 the Campaign's run, difficulty,
    // rewards and curses; 1 the Campaign's enemies (mix, factions, bosses); 2 Horde, which has its own copy of what applies to it (keys "h_...").
    constexpr int kTabs = 3;
    const char* TabName(int tab);
    int TabOf(size_t row);
    void ResetTab(int tab);

    // Loads the ini (call once at start-up; building the table needs the roster).
    void Load();

    int Index(size_t row);                    // the current choice of a row
    void SetIndex(size_t row, int index);     // a change from the menu: kept and saved
    void ResetRow(size_t row);
    void ResetAll();
    int Find(const std::string& key);         // row of a key, -1 if none

    int Value(const std::string& key, int fallback = 0);   // the current choice's value
    bool On(const std::string& key);                       // Value() != 0
    bool Changed();                                        // anything differs from the defaults

    void LogAll();

    // Presets: a PRESET row whose choice sets many other rows at once (the settings you changed since are kept as you left them).
    // Choosing one applies it; the menu rebuilds so every row shows its new value. 0 PICK ONE (nothing happens) ... see settings.cpp.
    bool ApplyPreset(int index, bool horde = false);   // false for 0 or an unknown preset; horde: the Horde tab's rows
    bool SetValue(const std::string& key, int value);   // the choice whose value is `value`
    bool TakeRebuild();                       // a preset was applied: the menu should be rebuilt (true once)
}
