# Lighting, atmospheres and time of day

## Lighting, atmospheres and the time of day (2026-11-09; found for Manhattan Skies, details in `docs/internals/atmosphere-and-menu-recon-log.md`)

All `[V]` = verified in game. Addresses are `exe+RVA`.

- **Seven lighting modes** [V]: index 0 Day, 2 Sunset, 5 Night, 1 Night variant, 3 and 6 Overcast, 4 Early Morning (the game's `kBlackout`; the opening mission's dawn).
  The setter is `exe+1f7f9e0(world+0xe10d0, index)`, the world getter `exe+1f707e0`, the current mode `world+0xe2040`. The game's own Settings option only offers Day,
  Sunset and Night. Each mode swaps a base **atmosphere** (sky, sun, reflected light, exposure, fog) and a world "levellight". The lit windows and street lamps follow
  the mode, not the atmosphere: nothing we tried (overlay zones, levellight headers, atmosphere fields, a Day/Night 0/1 switch at `exe+728390c`) changes them alone.
- **The game sets its remembered mode while a world loads** [V]: the setter is called with the mode field still unset (`-1`), once per load (story missions too, which is why a
  mode forced there applies to missions). **Never call the setter later during a load**: a call 3 s after the field became readable hung the loading screen for good.
  Swapping the index inside the game's own load-time call works with no transition.
- **Atmosphere pool** [V]: manager `exe+60430c0`; `AtmosphereAdjust` `exe+162f020(mgr, slot, delta, mask, min, max)` adds weights (base = 2; the map screen adds 1000);
  recompute `exe+162ff10(mgr)`; slots are 0x8c0 bytes each. Load any of the **124** atmospheres in the toc with `exe+1626380(manager, id, ...)`, release a slot with
  `exe+1624e80(manager, slot)`. Slot fields: refcount dword `+4`, asset id `+8`, sun block `+0x2a8..0x2bc`, fog distance `+0x588..0x594`, fog density `+0x598..0x5ac`,
  fog colour `+0x5b4`, haze `+0x5c0`, exposure `+0x484`, colour filter `+0x668/66c/670`, clouds `+0x81c`. `+0x47c/+0x480` give black or white screens: leave alone.
  The game's own activation of the next base atmosphere (`caller exe+1a1b59e`, delta > 0) must be swallowed while an override is on.
- **Rain** [?]: not located. Lighting-mode names (`kOvercastRain`, `kNightRain`) suggest it belongs to the mode; third-party atmosphere mods also change rain, so pairs can't be told apart on a modded install.

