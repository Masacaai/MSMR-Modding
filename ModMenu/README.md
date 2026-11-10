# ModMenu

Puts one tab per installed script mod into the game's **Settings** screen, after the game's own tabs. (The earlier separate MODS entry on the pause menu's first page is still in `modmenu.cpp`, switched off by `kModsPageItem`; it is not maintained.)
Every mod compiles `modmenu.cpp` into its own DLL. The first mod to start becomes the host (it hooks the game's menu functions once and
builds every tab); the others find it through a named shared object (`Local\MSMR_ModMenu_v1_<pid>`) and hand over their rows and callbacks,
so load order does not matter.

API: see [modmenu.h](modmenu.h) (`Register`, `KeepMenuOpen`, `TraceUi`, `LogStatus`). Used by [Endless Arena](../EndlessArena/README.md)
and [Manhattan Skies](../ManhattanSkies/README.md). How it works and which addresses it uses: `ManhattanSkies/docs/RECON.md` Parts 6-9 and `docs/GAME_INTERNALS.md`.
It is an internal library for these mods, not a public base for other mods (see [../docs/ROADMAP.md](../docs/ROADMAP.md), item 1).

Method: SpidyVR (IlyaMezerowsky, GPL-3.0, https://github.com/IlyaMez/SpidyVR) found how to add a tab and a first-page button to the game's
Settings; see [../THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
