# Cyberpunk 2077 — Moza & Logitech Racing Wheel Support

**v2.31.7** for Cyberpunk 2077 game patch 2.31.

Full racing wheel support for Cyberpunk 2077. Drive with a **Moza** or **Logitech** steering wheel! Your wheel handles steering, throttle, brake, and clutch. Force feedback fires from in-game physics (centering as you turn, cornering load, surface texture, collision jolts, slip when you lose grip). 

There is no virtual gamepad to install, no XInput shim, no third-party driver. The mod hooks the game directly via DirectInput/Logitech SDK.

## Supported wheels

The mod was designed to work with DirectInput-compatible wheels. Tested and confirmed on:

**Moza Racing:**
- Moza R5
- Moza R3
*(Map your axes manually if needed in `config.json`)*

**Logitech:**
- G923 (Xbox, PS, PC variants)
- G920, G29, G27, G25
- Momo Force, Driving Force GT

The mod auto-detects what hardware your Logitech wheel actually has (force feedback motor, rev-strip LEDs, lower-cluster buttons) and only shows the relevant settings.

## New Features
- **Custom Steering Equalizer:** 3-point equalizer (25%, 50%, 75%) to customize your steering curve and perfectly counteract the game's internal exponential steering physics! 
- **Speed Steering Boost:** Compensates steering at speed (up to 3x multiplier) so you can still turn at 200 mph.

## Required mods

1. **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)**
1. **[redscript](https://www.nexusmods.com/cyberpunk2077/mods/1511)**
1. **[ArchiveXL](https://www.nexusmods.com/cyberpunk2077/mods/4198)**
1. **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** v0.2.21 or later

## Required drivers

- **[Logitech G HUB](https://www.logitech.com/innovation/g-hub.html)**

## Install

1. Click the **Vortex** download button on this page (the green button in the Files tab).
1. Vortex will ask about a file conflict on `mod_settings.dll`. Choose this mod to win the conflict (load after Mod Settings). The included file is a small patch on top of upstream Mod Settings.

(Manual Download is also available if you don't use Vortex, but Mod Manager Download is the supported path. If you go manual, you handle the file conflict on `mod_settings.dll` yourself by overwriting upstream Mod Settings' DLL with the one from this zip.)

## G HUB

This mod respects G HUB. Wheel rotation range, sensitivity, and centering spring stay with G HUB; the mod reads the rotation range and scales its force feedback to match. G HUB and this mod can both be running at the same time.

If you have wheel buttons configured in your G HUB Cyberpunk profile that overlap with the bindings here, you may get doubled keypresses. Clear the overlapping bindings from your G HUB Cyberpunk profile.

I recommend starting with an operating range of 180 to start, since the rotation will match the animation you see in 1st person view inside of a car. I tend to like it a bit wider to help mitigate fishtailing.

## Troubleshooting

- **Wheel detected but the car doesn't move.** Make sure RED4ext is installed and current.
- **Settings page is missing.** Mod Settings or ArchiveXL aren't installed correctly. The wheel will still work on defaults.
- **Three orphan toggles named `hasFfbHardware` / `hasRevLeds` / `hasRightCluster` show up in settings.** Vortex installed stock Mod Settings on top of the patched build that ships with this mod. Re-deploy in Vortex and let this mod win the file conflict.
- **Doubled keypresses on D-pad or A/B/X/Y.** G HUB and this mod are both binding the same keys. Reset the wheel-button entries in your G HUB Cyberpunk profile back to their default bindings.

## Credits

- [RED4ext](https://github.com/WopsS/RED4ext) by WopsS
- [redscript](https://github.com/jac3km4/redscript) by jac3km4
- [Mod Settings](https://github.com/jackhumbert/mod_settings) by jackhumbert
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl) by psiberx

## Reporting issues

Include your wheel model, Cyberpunk patch version, RED4ext version, and your most recent `red4ext/logs/direct_wheel-*.log` file.
