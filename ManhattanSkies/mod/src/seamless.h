#pragma once

// SEAMLESS lighting switches (experimental; docs/RECON.md Part 18 and 25).
// A lighting-mode change sends LightingModeTransitionStartedEvent; its load-screen handler (DynamicLoadScreenSystem::OnLightingTransitionStarted, exe+710970)
// starts a load screen (the loading icon, the fade, the frozen hero, and, through the fast-travel "no vignette" path, the ended crime). The handler already returns
// at once for a transition its config marks SkipLoadScreen; this hook does the same for the transitions this mod makes: while Arm() is active the handler is skipped.
// Off by default (a row in the tab turns it on). Nothing is hooked until it is turned on.
namespace seamless {
    void SetEnabled(bool on);   // installs the hook the first time it is turned on (verifies the game's bytes first)
    bool Enabled();
    void Arm(unsigned ms);      // call right before the setter: the next transition-started event within `ms` is not allowed to start a load screen
    void LogStatus();
}
