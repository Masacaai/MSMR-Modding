## Manhattan Skies 1.0.0 (DRAFT: not built for release, not yet played end to end)

Adds **atmosphere mods from a folder**: drop `.smpcmod` and `.modular` atmosphere mods into one folder and pick between them in the game, several at once, for each time of day.

> Back up your saves before you try any mod.

### What is new

- **Atmospheres folder.** Put atmosphere mods (`.smpcmod`, and `.modular` files whose options replace the game's Day, Early Morning, Sunset, Night or Overcast atmosphere) in `Documents\Marvel's Spider-Man Remastered\ManhattanSkies\Atmospheres\`. They are read when the game starts; nothing in the game's files is replaced, so mods that replace the same atmosphere no longer fight each other.
- **ATMOSPHERE row** under TIME OF DAY: the game's own, or any mod found for that time of day. It switches the sky together with the world lighting (streets, buildings, sun). One choice is kept per time of day.
- **CYCLE ATMOSPHERES** under DYNAMIC CYCLE: when on, the cycle also picks among the atmosphere mods for each time of day, each as the mod ships it. An ATMOSPHERE you picked yourself still wins over the random pick.
- **The cycle stands still during crimes, hideouts, mission objectives and fights**, and never switches the lighting then (a switch is a mini-load that ended a crime in testing). It reads the game's own HUD calls (the crime pop-up, wave counter, objective line, progress bar, combo counter).
- **The cycle varies more:** each phase also gets a random SUN BRIGHTNESS, FOG DENSITY, FOG DISTANCE and HAZE (a value you set in that row wins), blended in with the colors before the switch.
- **SUN BRIGHTNESS can be OFF** (last choice of the row): no direct sunlight at all.
- **Transitions no longer touch the sun.** Only fog, haze, exposure, filter and fog color blend toward the next phase; each phase brings its own sun.
- **No black fade** when you change TIME OF DAY from the menu, like the cycle.
- **The cycle remembers where it was** and continues from there on the next launch, and starts a few seconds after the game is playable instead of 30 seconds after a load.
- **Early Morning after Night** no longer shows a red light overhead: the dawn sun rises from below the horizon, orange and faint, and eases to the sky's own color.

### Requirements

As 0.1.0, plus: **at least one mod installed through Overstrike that replaces game files** (a `.smpcmod`, for example). The mod borrows the archive file Overstrike creates for those mods; with none installed there is nothing to borrow and the Atmospheres folder is ignored (the log says so).

### Known Issues

- Everything in 0.1.0's list still applies, except the 30-second wait.
- A mod's textures are unpacked into a `cache` folder next to the Atmospheres folder the first time you pick it (under a second), and `.modular` options are unpacked once at start-up. The cache can be deleted; it is rebuilt. Plan for roughly the size of your atmosphere mods in free disk space.
- A mod that ships a shared helper asset (the sun-flare texture, one material) keeps the game's own version of it: those two details stay vanilla.
- A row holds 31 mods per time of day.
- Only the five open-world atmospheres are supported (the ones the time-of-day modes use). Mods that replace only other atmospheres are skipped, and the log says which.

### Untested at release

Pending a play-through: `.modular` options, CYCLE ATMOSPHERES, the cycle resuming, the earlier cycle start, the new dawn.

### Files

SHA-256 of ManhattanSkies.script: __SHA256__
