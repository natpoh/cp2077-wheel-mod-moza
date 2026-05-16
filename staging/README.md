# Direct Wheel — Moza & Direct Drive Racing Wheel Support for Cyberpunk 2077

Full racing wheel support for Cyberpunk 2077 v2.32 with **force feedback**, **speed-sensitive steering compensation**, and **TweakDB steering physics overrides**.

Tested with **Moza R5**. Should work with any DirectInput-compatible wheel.

### 📥 [Download direct_wheel-2.36.0.zip](https://github.com/natpoh/cp2077-wheel-mod-moza/raw/main/dist/direct_wheel-2.36.0.zip)

---

## Features

### 🎮 Wheel Input
- **Steering, Throttle, Brake, Clutch** — full axis mapping via Logitech SDK / DirectInput
- **Clutch-as-Brake** — use the softer clutch pedal as brake (toggle in settings)
- **Steering Linearization** — the game applies a quadratic curve internally (`actual_steer ≈ input²`), making small wheel angles feel dead. The mod automatically applies a √x (square root) inverse so your steering response is perfectly linear across the full wheel range
- **Speed Steering Boost** — compensates for the game's built-in steering reduction at high speed. Multiplies the linearized steering signal up to **2x** at 100 mph so the same physical wheel rotation produces consistent in-game turns regardless of velocity
- **Device Selector** — pick wheel and pedal devices independently for split-device setups (e.g. Moza wheel + Logitech pedals). Set by device index (0 = auto-detect, 1..N = specific device)
- **Axis Mapping** — choose throttle/brake axes from a dropdown in Mod Settings — no more editing `config.json`

### 💪 Force Feedback
- **Centering spring** — physics-based, scales with speed
- **Cornering feedback** — spring stiffens during turns
- **Friction** — road texture resistance, scales with speed
- **Sine vibration** — 25 Hz road surface buzz from suspension activity
- **Collision jolt** — sharp impact pulse on collision
- **All FFB effects** have individual strength sliders (0–100%)

### 💡 LED Support
- Rev-strip LED bar (G29/G920/G923)
- Optional WASAPI audio visualizer mode

---

## Requirements

1. **Cyberpunk 2077 v2.32** (latest patch)
2. **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)** — native plugin loader
3. **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** — in-game settings UI (patched version included)

---

## Installation

### Quick Install (zip)

1. Download `direct_wheel-2.36.0.zip`
2. Extract **directly into your Cyberpunk 2077 game folder**, for example:
   ```
   D:\SteamLibrary\steamapps\common\Cyberpunk 2077\
   ```
3. The zip merges into the existing folder structure:
   ```
   Cyberpunk 2077/
   ├── r6/scripts/direct_wheel/*.reds                                    ← Redscript
   └── red4ext/plugins/
       ├── direct_wheel/direct_wheel.dll                                 ← Main plugin
       └── mod_settings/mod_settings.dll                                 ← Settings UI
   ```
4. **Start G HUB** (or LGS) before launching the game
5. Launch Cyberpunk 2077

### First Launch
- First launch after install is slow (30–60 seconds) — Redscript needs to compile
- Your wheel should rumble briefly on load (handshake confirmation)
- Enter any vehicle to start driving with the wheel

---

## Configuration

All settings are in-game: **Main Menu → Settings → Mod Settings → G-series Wheel**

> **Note:** It is highly recommended to set your wheel's operating range (rotation angle) to **720 degrees** in your wheel software (Logitech G HUB, Moza Pit House, Fanatec Control Panel, etc.) for the best steering response.

### Wheel Input Section

| Slider | Default | Description |
|---|---|---|
| Enable wheel input | ON | Master toggle for steering/throttle/brake injection |
| Treat clutch as brake | ON | Use clutch pedal as brake (softer pedal) |
| Speed steering boost (%) | 45 | Compensates steering at high speed. 0=off, 50=1.5x at 100 mph, 100=2x max |
| Steering turn speed | 1 | Engine physics limit for turn speed. **For wheel play, set to 6+.** *(Requires game restart to apply)* |
| Steering re-center speed | 1 | Engine physics limit for return-to-center speed. **For wheel play, set to 6+.** *(Requires game restart to apply)* |

### Axis Mapping Section

| Slider | Default | Description |
|---|---|---|
| Wheel device (0 = Auto) | 0 | Which controller to use for steering + FFB. 0 = auto-detect, 1..N = specific device by index |
| Pedal device (0 = Same as wheel) | 0 | Separate USB pedal device for throttle/brake. 0 = use wheel device |
| Throttle axis | lZ | DirectInput axis for throttle. Logitech = lZ, some Moza = lY |
| Brake axis | lRz | DirectInput axis for brake |

> To see which device is at which index, run `tools/input_probe.exe` — it lists all devices with numbers.

### Force Feedback Section

| Slider | Default | Description |
|---|---|---|
| Enable force feedback | ON | Master FFB toggle |
| FFB strength (%) | 100 | Overall FFB magnitude |
| Cornering feedback (%) | 50 | Spring stiffness during turns |
| Friction force (%) | 30 | Road texture resistance |
| Road vibration (%) | 30 | Sine wave buzz from road surface |
| Collision jolt (%) | 50 | Impact pulse on collision |

---

## Tuning Guide

### Steering feels dead at high speed
Increase **Speed steering boost** (try 70–100%).

### Steering is too twitchy at high speed
Decrease **Speed steering boost** (try 20–40%).

### FFB is too strong / too weak
Adjust **FFB strength** first (overall), then tune individual effects.

### Wheel doesn't respond at all
1. Make sure **G HUB** (or LGS) is running
2. Check that RED4ext is installed correctly
3. Look at logs: `red4ext/logs/direct_wheel-*.log`

> Other wheels (Fanatec, Thrustmaster, etc.) - run `input_probe.exe` to check your axes, then select the correct axis in **Mod Settings → G-series Wheel → Axis mapping**.

---

## How It Works

The mod works via a **RED4ext plugin** (`direct_wheel.dll`) that hooks `vehicle::BaseObject::UpdateVehicleCameraInput` to inject wheel axis values each frame. It also runs FFB effects (centering spring, friction, sine, jolt) via DirectInput.

**Steering pipeline:** The game applies a quadratic curve to steering input (`actual_steer ≈ input²`), which makes small wheel angles feel dead (at 10° you only get 3.6% of the linear response). The mod applies the mathematical inverse — `√x` (square root) — before the game processes it, so `√x` → game squares it → `x`. This perfectly linearizes your steering across the full range. The **Speed Steering Boost** then multiplies this linearized signal to compensate for the game's speed-dependent steering attenuation.

---

## Uninstallation

Delete these folders:
```
red4ext/plugins/direct_wheel/
r6/scripts/direct_wheel/
```

---

## Credits

- Forked from [cp2077-wheel-mod](https://github.com/clevergrant/cp2077-wheel-mod) by clevergrant
- RED4ext SDK by [WopsS](https://github.com/WopsS/RED4ext.SDK)
- Cyber Engine Tweaks by [yamashi / maximegmd](https://github.com/maximegmd/CyberEngineTweaks)

## License

MIT
