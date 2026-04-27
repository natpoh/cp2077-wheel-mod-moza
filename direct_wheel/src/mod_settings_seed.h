#pragma once

namespace direct_wheel::mod_settings_seed
{
    // Probe attached HID devices for a Logitech wheel and write the three
    // hidden capability flags (hasFfbHardware / hasRevLeds / hasRightCluster)
    // into mod_settings' user.ini before mod_settings reads it during
    // ProcessScriptData. With the values pre-seeded, mod_settings' built-in
    // dependency evaluator hides every section that depends on a capability
    // the current wheel doesn't have (or hides everything when no wheel is
    // attached).
    //
    // Also mirrors config.json's handshake.playOnStart into [DirectWheelSettings]
    // handshakePlayOnStart so the in-game Mod Settings menu reflects the
    // FOMOD installer's handshake choice on first launch (and stays in sync
    // with the C++ side thereafter, since redscript Push() rewrites
    // config.json from the listener instance).
    //
    // Must be called from plugin OnLoad. Plugin OnLoads complete before
    // RED4ext processes script data, so our writes are always in place when
    // mod_settings parses the file. Probe is synchronous, ~tens of ms,
    // independent of the deferred Logitech SDK init in wheel::Init.
    //
    // Plug/unplug a wheel = restart the game once for the UI to catch up;
    // the file is read once at process start.
    void Run();
}
