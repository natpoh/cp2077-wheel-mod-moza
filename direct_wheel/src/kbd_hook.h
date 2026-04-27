#pragma once

#include <cstdint>

// Low-level keyboard hook that prevents G HUB's (or any other injector's)
// synthetic keyboard events from reaching the game for vehicle-only keys
// when V is on foot. Without this, G HUB's Cyberpunk profile fires e.g.
// paddle → Space → V jumps, even though our plugin correctly suppresses
// the corresponding wheel-bound action. The plugin's own SendInput calls
// tag their events with kExtraInfoTag so the hook passes them through.

namespace direct_wheel::kbd_hook
{
    // Tag value stamped into INPUT.ki.dwExtraInfo / INPUT.mi.dwExtraInfo
    // on every event the plugin fires, so the LL hook can distinguish
    // our injections from third-party ones. Must not collide with any
    // well-known magic; "dWHL" as little-endian ASCII.
    constexpr uintptr_t kExtraInfoTag = 0x4C48'5767;  // 'dWHL'

    bool Install();
    void Uninstall();

    // True if the hook is currently installed and running.
    bool IsInstalled();
}
