# Cyberpunk 2077 — Moza & Direct Drive Wheel Support

**v2.37.0** for Cyberpunk 2077 game patch 2.32.

Full racing wheel support for Cyberpunk 2077. Drive with a **Moza** or any other **Direct Drive** steering wheel! Your wheel handles steering, throttle, brake, and clutch. Force feedback fires from in-game physics (centering as you turn, cornering load, surface texture, collision jolts, slip when you lose grip). 

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
- **Automatic Steering Linearization:** The game applies a quadratic curve to steering (actual_steer ≈ input²), which kills small-angle precision — at 10° wheel turn you only get 3.6% of the expected response. The mod applies the mathematical inverse (√x) before the game processes it, so √x → game squares it → x. Your steering is perfectly linear across the full wheel range, no manual tuning needed.
- **Speed Steering Boost:** Compensates steering at speed by amplifying the linearized signal up to a 2x multiplier at 100 mph, so you can still turn perfectly at high speeds.
- **Global Steering Mults:** Two new sliders, "Steering turn speed" and "Steering re-center speed". These allow you to push the engine's steering physical limits. For wheel play, it is recommended to set both to **6+** for accurate and fast steering responsiveness. *(Requires game restart to apply)*
- **Invert Pedals:** Separate settings to invert throttle and brake pedals directly in the mod menu.
- **Device & Axis Mapping:** Select wheel and pedal devices independently (for split-device setups), and pick throttle/brake axes from a dropdown — no more editing `config.json`. Use the included `input_probe.exe` to identify your device numbers and axes.

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

I highly recommend setting your operating range / rotation angle to **720 degrees** in your wheel software for the best driving experience and steering response.

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

## Source Code

Source code is available on [GitHub](https://github.com/natpoh/cp2077-wheel-mod-moza). Pull requests and feedback are welcome!

## Reporting issues

Include your wheel model, Cyberpunk patch version, RED4ext version, and your most recent `red4ext/logs/direct_wheel-*.log` file.
