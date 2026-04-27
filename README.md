# Direct Wheel — Direct Drive & Logitech Racing Wheel Support for Cyberpunk 2077

Full racing wheel support for Cyberpunk 2077 v2.31 with **force feedback**, **speed-sensitive steering compensation**, and **TweakDB steering physics overrides**.

Tested with **Moza R5** (Direct Drive) and **Logitech G923**. Should work with any DirectInput-compatible wheel.

### 📥 [Download direct_wheel_moza_v2.31.7.zip](https://github.com/natpoh/cp2077-wheel-mod-moza/raw/main/dist/direct_wheel_moza_v2.31.7.zip)

---

## Features

### 🎮 Wheel Input
- **Steering, Throttle, Brake, Clutch** — full axis mapping via Logitech SDK / DirectInput
- **Clutch-as-Brake** — use the softer clutch pedal as brake (toggle in settings)
- **Speed Steering Boost** — compensates for the game's built-in steering reduction at high speed. Multiplies steering input up to **3x** at cruise speed so the same physical wheel rotation produces consistent in-game turns regardless of velocity

### 🔧 TweakDB Steering Physics (CET companion)
The game internally reduces steering effectiveness exponentially at speed through three TweakDB parameters. This mod overrides them in real-time:

| Parameter | What it does | How the mod compensates |
|---|---|---|
| `maxWheelTurnDeg` | Max wheel turn angle | Decreases with speed (configurable) |
| `wheelTurnMaxAddPerSecond` | How fast wheels respond to steering | **Increases** at speed — snappier response |
| `wheelTurnMaxSubPerSecond` | How fast wheels return to center | **Increases** at speed — faster centering |

All three parameters are tunable via **Mod Settings** sliders (no CET overlay needed).

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

1. **Cyberpunk 2077 v2.31** (latest patch)
2. **[RED4ext](https://www.nexusmods.com/cyberpunk2077/mods/2380)** — native plugin loader
3. **[Cyber Engine Tweaks (CET)](https://www.nexusmods.com/cyberpunk2077/mods/107)** v1.37+ — for TweakDB steering overrides
4. **[Mod Settings](https://www.nexusmods.com/cyberpunk2077/mods/4885)** — in-game settings UI (patched version included)
5. **Logitech G HUB** or **Logitech Gaming Software** — must be running (provides the steering SDK)

---

## Installation

### Quick Install (zip)

1. Download `direct_wheel_moza_v2.31.7.zip`
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

### Wheel Input Section

| Slider | Default | Description |
|---|---|---|
| Enable wheel input | ON | Master toggle for steering/throttle/brake injection |
| Treat clutch as brake | ON | Use clutch pedal as brake (softer pedal) |
| **Speed steering boost (%)** | 50 | Compensates steering at speed. 0=off, 50=2x, 100=3x at cruise |
| **Equalizer: 25% input** | 40 | Custom steering curve. Output at 25% physical rotation |
| **Equalizer: 50% input** | 70 | Custom steering curve. Output at 50% physical rotation |
| **Equalizer: 75% input** | 87 | Custom steering curve. Output at 75% physical rotation |
| **Turn angle speed factor** | 15 | How fast maxTurnDeg decreases with speed (×0.001). Higher = less angle |
| **Min turn angle (deg)** | 12 | Floor for wheel turn angle — never goes below this |
| **Wheel turn add boost** | 8 | How fast wheel response speeds up at speed (×0.001) |
| **Wheel turn sub boost** | 12 | How fast wheels return to center at speed (×0.001) |

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
Increase **Speed steering boost** (try 70–100%) and **Wheel turn add boost** (try 12–20).

### Steering is too twitchy at high speed
Decrease **Speed steering boost** (try 20–40%) and increase **Min turn angle** (try 15–20).

### FFB is too strong / too weak
Adjust **FFB strength** first (overall), then tune individual effects.

### Wheel doesn't respond at all
1. Make sure **G HUB** (or LGS) is running
2. Check that RED4ext is installed correctly
3. Look at logs: `red4ext/logs/direct_wheel-*.log`

### Non-Logitech Wheels (Moza, Fanatec, Thrustmaster, etc.)

Different wheel bases assign gas/brake/clutch to different DirectInput axes. If your wheel connects but pedals don't work, you need to remap the axes in `config.json`.

**Step 1 — Identify your axes** using `input_probe.exe` (included in `bin/`):

```
> bin\input_probe.exe
Found device: DirectInput Wheel (Moza)
Wheel connected and acquired. Reading axes (Press Ctrl+C to exit):
--------------------------------------------------------
X:32767  Y:0  Z:65535  Rx:0  Ry:0  Rz:65535  S0:65535  S1:0
```

Press each pedal one at a time and note which value changes:
- **Steering** = the value that moves when you turn the wheel (usually `X`)
- **Throttle** = the value that drops when you press gas
- **Brake** = the value that drops when you press brake
- **Clutch** = the value that drops when you press clutch

**Step 2 — Edit `config.json`** (in `red4ext/plugins/direct_wheel/`):

```json
{
  "axes": {
    "steer": "lX",
    "throttle": "lY",
    "brake": "lRz",
    "clutch": "slider0"
  }
}
```

Axis name mapping:
| input_probe label | config.json name |
|---|---|
| X | `lX` |
| Y | `lY` |
| Z | `lZ` |
| Rx | `lRx` |
| Ry | `lRy` |
| Rz | `lRz` |
| S0 | `slider0` |
| S1 | `slider1` |

**Default** (Logitech G29/G920/G923): `steer=lX, throttle=lZ, brake=lRz, clutch=slider0`

**Moza R3**: `steer=lX, throttle=lY, brake=lRz, clutch=slider0`

**Moza R5**: `steer=lX, throttle=lZ, brake=lRz, clutch=slider0` (same as Logitech)

> Other wheels (Fanatec, Thrustmaster, etc.) - run `input_probe.exe` to check your axes.

---

## How It Works

The mod uses a **two-layer** approach:

1. **RED4ext plugin** (`direct_wheel.dll`) — hooks `vehicle::BaseObject::UpdateVehicleCameraInput` to inject wheel axis values each frame. Also runs FFB effects (centering spring, friction, sine, jolt) via DirectInput. The **Speed Steering Boost** multiplies the raw steer input before injection.

2. **CET companion script** (`steering_debug/init.lua`) — reads tuning parameters from `config.json` every 2 seconds, then modifies TweakDB `driveModelData` records in real-time. This fights the game's internal exponential steering reduction that no amount of input boosting can overcome.

Both layers work together for the most realistic steering feel possible.

---

## Uninstallation

Delete these folders:
```
red4ext/plugins/direct_wheel/
r6/scripts/direct_wheel/
bin/x64/plugins/cyber_engine_tweaks/mods/steering_debug/
```

---

## Credits

- Based on [Logitech G-series Wheel Support](https://www.nexusmods.com/cyberpunk2077/mods/29172) by the original author
- RED4ext SDK by [WopsS](https://github.com/WopsS/RED4ext.SDK)
- Cyber Engine Tweaks by [yamashi / maximegmd](https://github.com/maximegmd/CyberEngineTweaks)

## License

MIT
