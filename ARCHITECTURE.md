# direct_wheel — Architecture

This document is the contract the rest of the implementation is built against. It covers: plugin layout, the startup sequence, the hardware-to-game data path (Logitech SDK → `sources::` seam → vehicle detour + button dispatch), the config JSON schema, the redscript-facing native API, the button-binding model, and the G HUB coexistence model.

## One-line summary

A Cyberpunk 2077 RED4ext plugin that reads a Logitech G-series wheel via the official Logitech Steering Wheel SDK, injects its axes into the game through a hash-resolved detour on `vehicle::BaseObject::UpdateVehicleCameraInput`, dispatches wheel-button presses to in-game actions via `SendInput`, drives physics-aware force-feedback effects back out (centering, cornering, surface texture, collision jolts, slip-angle countersteer), reflects engine RPM and radio state on the wheel's rev-strip LEDs, and exposes a flat set of global native functions that six `.reds` files use to render a Settings page via [Mod Settings](https://github.com/jackhumbert/mod_settings), publish player-vehicle / RPM / radio state, and react to collision events.

## Components

```text
Logitech G-series wheel ──USB HID──▶ Logitech Steering Wheel SDK (via G HUB / LGS)
                                                     │
                                                     ▼
                           direct_wheel.dll  (red4ext plugin, 250 Hz pump)
                           ┌──────────────────────────────────────────┐
                           │   wheel::  ─────▶  sources::Frame        │
                           │                      │         │          │
                           │                      ▼         ▼          │
                           │      vehicle_hook (detour)   input_bindings │
                           │          │ inject axes          │ SendInput │
                           │          ▼                      ▼          │
                           │   vehicle::BaseObject     kbd_hook filters │
                           │                           G HUB ghosts     │
                           └──────────┬──────────────────┬──────────────┘
                                      │                  │
                                      ▼                  ▼
                                game vehicle        game input queue
                                                         │
                           redscript (r6/scripts/direct_wheel/*.reds)
                                      │
                                      ▼
                       Main Menu → Mod Settings → G-series Wheel
```

| File | Role |
| --- | --- |
| `direct_wheel/src/dllmain.cpp` | RED4ext `Main` / `Query` entry points. |
| `direct_wheel/src/plugin.{h,cpp}` | Lifecycle: 6-step `OnLoad` (config → rtti → wheel → vehicle_hook → kbd_hook → pump thread); owns the 250 Hz pump loop. |
| `direct_wheel/src/logging.{h,cpp}` | Thin wrapper over the RED4ext logger + optional local log file. |
| `direct_wheel/src/device_table.{h,cpp}` | Logitech G-series VID/PID table. |
| `direct_wheel/src/wheel.{h,cpp}` | Logitech SDK wrapper: init, `Pump`, `Snapshot` (steer/throttle/brake/clutch/buttons/POV), and FFB dispatch (`PlayConstant`/`PlayDamper`/`PlaySpring` + global strength). |
| `direct_wheel/src/sources.{h,cpp}` | Hardware-agnostic publish/read seam. `sources::Frame` = axes + digital + connected. Also carries `InVehicle()` control context (set by reds mount wrappers). |
| `direct_wheel/src/input_bindings.{h,cpp}` | Physical-input enum, per-device layout, edge detection, `SendInput` dispatch. Entry point: `OnTick(const sources::Frame&)`. Every input falls through to the user's Mod Settings binding; menu-nav is the default for D-pad + A but is user-overridable. |
| `direct_wheel/src/vehicle_hook.{h,cpp}` | RED4ext hash-resolved detour on `vehicle::BaseObject::UpdateVehicleCameraInput`. Gated on a cached player-vehicle pointer so we don't remote-drive parked cars. |
| `direct_wheel/src/kbd_hook.{h,cpp}` | Low-level `WH_KEYBOARD_LL` hook that suppresses G HUB's synthetic vehicle-key presses while on foot. Our own `SendInput` events tag `dwExtraInfo = kExtraInfoTag` ('dWHL') so the hook passes them through. |
| `direct_wheel/src/config.{h,cpp}` | Config struct, `Load()`, `ReadAsJson()`, per-field setters. Atomic double-buffered snapshot; every setter writes back to disk. |
| `direct_wheel/src/rtti.{h,cpp}` | Registers 28 global native functions exposed to redscript in `PostRegisterTypes`. |
| `direct_wheel/src/mod_settings_seed.cpp` | Writes hardware-capability flags to `red4ext/plugins/mod_settings/user.ini` at plugin load, before Mod Settings reads it, so the in-game Settings page hides sections for hardware the wheel doesn't have. |
| `direct_wheel/src/rtti_dump.{h,cpp}` | Debug-only RTTI dumper (disabled this build during a bisect). |
| `direct_wheel_reds/direct_wheel_natives.reds` | Declarations only — kept in lockstep with `rtti.cpp::PostRegisterTypes`. |
| `direct_wheel_reds/direct_wheel_settings.reds` | `DirectWheelSettings` Mod Settings class + `DirectWheelAction` enum; pushes values to plugin on every change. |
| `direct_wheel_reds/direct_wheel_mount.reds` | Wraps `VehicleComponent::OnVehicleFinishedMountingEvent` / `OnUnmountingEvent`; notifies plugin of the player's current vehicle pointer. |
| `direct_wheel_reds/direct_wheel_events.reds` | Wraps `VehicleObject::OnVehicleBumpEvent`; queues a transient FFB jolt (in player-frame world-right) on collision. |
| `direct_wheel_reds/direct_wheel_surface.reds` | 20 Hz downward raycast from chassis; pushes ground material CName transitions to the plugin (FFB mapping currently dormant). |
| `direct_wheel_reds/direct_wheel_vehicle_signals.reds` | Subscribes to vehicle Blackboard for RPMValue + VehRadioState; pushes normalized RPM and radio state to the plugin so the LED rev-strip / music-visualizer reflect real game state. |

## Supported hardware (device_table)

Only wheels under Logitech VID `0x046D` with PIDs in the table below are accepted. Anything else is refused with a log line.

```text
VID = 0x046D, entries in (PID, model_id, name, ffb_default, steering_range_deg):

0xC291  WINGMAN_FORMULA_FORCE       "WingMan Formula Force"        yes  240
0xC293  WINGMAN_FORMULA_FORCE_GP    "WingMan Formula Force GP"     yes  240
0xC294  DRIVING_FORCE               "Driving Force"                no   240
0xC295  MOMO_FORCE                  "Momo Force"                   yes  270
0xC298  DRIVING_FORCE_PRO           "Driving Force Pro"            yes  900
0xC299  G25                         "G25 Racing Wheel"             yes  900
0xC29A  DRIVING_FORCE_GT            "Driving Force GT"             yes  900
0xC29B  G27                         "G27 Racing Wheel"             yes  900
0xC24F  G29_NATIVE                  "G29 Driving Force"            yes  900
0xC260  G29_PS                      "G29 Driving Force (PS mode)"  yes  900
0xC261  G920_VARIANT                "G920 Driving Force"           yes  900
0xC262  G920                        "G920 Driving Force"           yes  900
0xC266  G923_XBOX                   "G923 (Xbox)"                  yes  900
0xC267  G923_PS_PC                  "G923 (PS/PC)"                 yes  900
0xC26D  G923_PS                     "G923 (PS mode)"               yes  900
0xC26E  G923                        "G923 (PC/USB)"                yes  900
0xCA03  MOMO_RACING                 "Momo Racing"                  yes  270
0xCA04  FORMULA_VFB                 "Formula Vibration Feedback"   yes  240
```

`ffb_default` is informational only. The authoritative check is the `hasFFB` flag on `wheel::Caps`, derived from `LogiHasForceFeedback` at bind time. This drives the `DirectWheel_HasFFB` native, which in turn disables the FFB section of the Settings UI for motorless wheels.

## Startup sequence

`plugin::OnLoad` runs six steps in order (see [plugin.cpp:63-95](direct_wheel/src/plugin.cpp#L63-L95)):

1. `config::Load()` — read `config.json` from the plugin install dir; fall back to defaults if missing / unparseable. Pushes initial bindings into `input_bindings::ReplaceAll`.
2. `rtti::Register()` — queue pre- and post-register callbacks with `CRTTISystem`; natives are registered in `PostRegisterTypes` once RTTI is built.
3. `wheel::Init()` — validate the Logitech SDK version and schedule a deferred `LogiSteeringInitialize` (the SDK refuses to bind until G HUB has enumerated the device).
4. `vehicle_hook::Init()` — resolve the UpdateVehicleCameraInput hash via RED4ext's `UniversalRelocBase::Resolve` and install the detour.
5. `kbd_hook::Install()` — install the low-level keyboard filter.
6. Start the 250 Hz pump thread.

## Wheel I/O (Logitech SDK) + sources seam

The pump thread (`plugin.cpp::PumpLoop`) runs at 250 Hz. Each tick:

1. `wheel::Pump()` → `LogiUpdate` + `LogiGetC` → `wheel::Snapshot`.
2. `BuildFrame(snapshot)` → `sources::Frame` (axes + digital + connected).
3. `sources::Publish(frame)`.
4. If `frame.connected`, `input_bindings::OnTick(frame)` — detects button/POV edges and dispatches bound actions via `SendInput`.

The `sources::` module is a hardware-agnostic seam. Today only `wheel.cpp` publishes into it; if we ever move digital input to RawInput (or merge readers), `BuildFrame` becomes the merge point and consumers don't change.

`sources::` also carries a control-context flag:

- `sources::InVehicle()` — flipped on/off by the redscript mount-event wrappers via `DirectWheel_Set/ClearPlayerVehicle`. `input_bindings::Dispatch` consults this to suppress vehicle-only actions (Handbrake, Headlights, Horn, camera cycles, etc.) while V is walking — the same keyboard keys mean different things on foot and we don't want the wheel firing Jump / Interact because a paddle is mapped to Handbrake.

The vehicle hook doesn't go through `sources::`; the detour fires on the game thread and reads `wheel::CurrentSnapshot()` directly to minimize latency into the input path.

## Vehicle input hook

Single target: `vehicle::BaseObject::UpdateVehicleCameraInput(self)`. Resolved via `RED4ext::UniversalRelocBase::Resolve(501486464u)` (hash sourced from Let There Be Flight, maintained in RED4ext's address database and updated per game patch by the RED4ext maintainers).

If the hash doesn't resolve — meaning RED4ext itself is behind the current game build — RED4ext terminates the process with its own MessageBox at load. The game won't launch at all until the RED4ext address database catches up.

The detour fires per-vehicle per-tick (not just for the player's car). A cached `g_playerVehicle` pointer, set by the redscript mount/unmount wrappers through `DirectWheel_SetPlayerVehicle` / `DirectWheel_ClearPlayerVehicle`, gates injection to the player's currently-mounted vehicle only. Without this gate, our writes would propagate to parked cars, AI traffic, and anything else with active camera updates.

Struct field offsets (CP2077 v2.31, build 5294808, found empirically 2026-04-21 — LTBF's SDK labels did not match):

| Offset | Field | Type | Range |
| --- | --- | --- | --- |
| `0x264` | throttle | `float` | 0..1 |
| `0x268` | brake | `float` | 0..1 (also drives reverse while stationary) |
| `0x278` | steer | `float` | -1..+1, + = right |

Re-probe if the game patches the `vehicle::BaseObject` struct. The detour also merges with vanilla keyboard/gamepad input (max-magnitude wins), so keyboard steering still works when the wheel isn't moving.

`vehicle_hook::FireCount()` is a monotonic tick counter surfaced through `DirectWheel_GetDeviceInfo` for live-hook confirmation.

## Native function surface (RTTI)

Registered as global static native functions (not a class; no `IScriptable` parent) in [direct_wheel/src/rtti.cpp](direct_wheel/src/rtti.cpp) `PostRegisterTypes` via `RED4ext::CGlobalFunction::Create(name, name, fn)` + `AddParam` + `SetReturnType` + `rtti->RegisterFunction(func)`.

Each function uses the canonical stack-frame shape:

```cpp
void Fn(RED4ext::IScriptable* aContext,
        RED4ext::CStackFrame* aFrame,
        OutT* aOut,
        int64_t /*unused*/);
```

Parameters are read sequentially with `RED4ext::GetParameter(aFrame, &arg)`, followed by `aFrame->code++;`. Return value is assigned via `if (aOut) *aOut = …;`.

### Read-only

| Native | Params | Returns | Purpose |
| --- | --- | --- | --- |
| `DirectWheel_GetVersion` | — | `String` | Plugin version string (`kVersionString`). |
| `DirectWheel_IsPluginReady` | — | `Bool` | `true` iff the Logitech SDK bound a device. |
| `DirectWheel_GetDeviceInfo` | — | `String` | Human-readable summary: product name, operating range, FFB flag, SDK version, hook state, fire count. |
| `DirectWheel_HasFFB` | — | `Bool` | `true` iff a device is bound and advertises force feedback. |
| `DirectWheel_DetectedHasFfbHardware` | — | `Bool` | Same as HasFFB; named distinctly so the Mod Settings capability-flag plumbing reads cleanly. |
| `DirectWheel_DetectedHasRevLeds` | — | `Bool` | `true` iff the bound wheel has the rev-strip LED bar (G25/G27/G29/G920/G923 etc.). |
| `DirectWheel_DetectedHasRightCluster` | — | `Bool` | `true` iff the bound wheel has the lower-cluster cluster (Plus/Minus/scroll; absent on G920). |
| `DirectWheel_DetectedModelName` | — | `String` | Wheel model name from the device table. |
| `DirectWheel_ReadConfig` | — | `String` | Current config serialized as JSON (same schema as `config.json`). |

### Config setters

Each setter atomically swaps the live snapshot and writes `config.json` to disk.

| Native | Params |
| --- | --- |
| `DirectWheel_SetInputEnabled` | `v: Bool` |
| `DirectWheel_SetClutchAsBrake` | `v: Bool` |
| `DirectWheel_SetFfbEnabled` | `v: Bool` |
| `DirectWheel_SetFfbDebugLogging` | `v: Bool` |
| `DirectWheel_SetFfbTorquePct` | `pct: Int32` (0–100) |
| `DirectWheel_SetStationaryThresholdMps` | `v: Float` (0.0–5.0) |
| `DirectWheel_SetYawFeedbackPct` | `pct: Int32` (0–100) |
| `DirectWheel_SetActiveTorqueStrengthPct` | `pct: Int32` (0–100) |
| `DirectWheel_SetHandshakePlayOnStart` | `v: Bool` |
| `DirectWheel_SetLedEnabled` | `v: Bool` |
| `DirectWheel_SetLedVisualizerWhileMusic` | `v: Bool` |

### Bindings

| Native | Params | Purpose |
| --- | --- | --- |
| `DirectWheel_SetInputBinding` | `inputId: Int32, action: Int32` | Map a PhysicalInput (0–19) to an Action (0–36). |

### Player-vehicle lifecycle

| Native | Params | Purpose |
| --- | --- | --- |
| `DirectWheel_SetPlayerVehicle` | `v: ref<VehicleObject>` | Cache on mount (also sets `sources::InVehicle(true)`). |
| `DirectWheel_ClearPlayerVehicle` | — | Clear on dismount (also `sources::InVehicle(false)`). |

### Vehicle telemetry (pushed from redscript)

| Native | Params | Purpose |
| --- | --- | --- |
| `DirectWheel_SetEngineRpmNormalized` | `v: Float` (0..1) | Pushed by `direct_wheel_vehicle_signals.reds` from the vehicle Blackboard's RPMValue / MaxRPM. Drives the LED rev strip. |
| `DirectWheel_SetRadioActive` | `v: Bool` | Pushed when the in-car radio is on. Switches the LED rev strip to the music visualizer. |

### Event natives

| Native | Params | Purpose |
| --- | --- | --- |
| `DirectWheel_OnVehicleBump` | `kick: Float` | Called from `direct_wheel_events.reds` `OnVehicleBumpEvent` wrap; queues a transient FFB jolt overlay. |
| `DirectWheel_OnVehicleHit` | `kick: Float` | Currently unused (legacy path; OnVehicleHit lives on the wrong class). |
| `DirectWheel_OnWheelMaterial` | `wheelIdx: Int32, mat: CName` | Called from `direct_wheel_surface.reds` raycast; surface-baseline mapping currently dormant. |

## Config JSON schema

Path: `<CP2077>/red4ext/plugins/direct_wheel/config.json`. Loaded at plugin load, written back on every setter. Schema version `4`. The FOMOD installer ships a minimal `{ "version": 4, "handshake": { "playOnStart": true|false } }` (every other field falls back to its in-code default until the user touches it in Mod Settings); the C++ side serializes the full schema on its first write.

```json
{
  "version": 4,

  "input": {
    "enabled": true,
    "clutchAsBrake": false,
    "responseCurve": "default"
  },

  "ffb": {
    "enabled": true,
    "debugLogging": true,
    "torquePct": 100,
    "stationaryThresholdMps": 0.5,
    "yawFeedbackPct": 50,
    "activeTorqueStrengthPct": 100
  },

  "handshake": {
    "playOnStart": true
  },

  "led": {
    "enabled": true,
    "visualizerWhileMusic": true
  },

  "music": {
    "processName": ""
  },

  "perVehicle": {
    "car":        { "steeringMultiplier": 1.0, "responseDelayMs": 20 },
    "motorcycle": { "steeringMultiplier": 1.2, "responseDelayMs": 10 },
    "truck":      { "steeringMultiplier": 0.8, "responseDelayMs": 40 },
    "van":        { "steeringMultiplier": 0.9, "responseDelayMs": 30 }
  },

  "bindings": [10, 10, 33, 34, 35, 36, 31, 3, 1, 4, 30, 24, 27, 19, 7, 2, 18, 13, 14, 5]
}
```

**Defaults** (constants in [direct_wheel/src/config.h](direct_wheel/src/config.h)):

| Field | Default | Range | Notes |
| --- | --- | --- | --- |
| `input.enabled` | `true` | — | Master toggle for wheel input. |
| `input.clutchAsBrake` | `false` | — | When true, `BuildFrame` publishes `brake = max(brake, clutch)` so the clutch pedal brakes. |
| `input.responseCurve` | `"default"` | `"default"` \| `"subdued"` \| `"sharp"` | Shapes axis response pre-game. (No Mod Settings UI yet.) |
| `ffb.enabled` | `true` | — | Master enable for all plugin-generated FFB. |
| `ffb.debugLogging` | `true` | — | Verbose log of every effect start/stop. |
| `ffb.torquePct` | `100` | 0 – 100 | Scales plugin effect magnitudes via `wheel::SetGlobalStrength`. |
| `ffb.stationaryThresholdMps` | `0.5` | 0.0 – 5.0 | Below this speed all centering forces are off; the wheel rests where the driver leaves it. |
| `ffb.yawFeedbackPct` | `50` | 0 – 100 | Additive cornering-spring stiffness during rotation, normalised against the car's own turn rate. |
| `ffb.activeTorqueStrengthPct` | `100` | 0 – 100 | Gain on the directional push-back constant force. |
| `handshake.playOnStart` | `true` | — | Pon pon shi greeting on game start. |
| `led.enabled` | `true` | — | Rev-strip LEDs. |
| `led.visualizerWhileMusic` | `true` | — | Switch rev strip to music visualizer when the in-car radio is on. |
| `music.processName` | `""` | string | Empty = capture this game's audio. Non-empty = capture the named process instead. |
| `perVehicle.*` | as shown | positive floats / ints | Per-vehicle tuning consumed inside the C++ FFB / steering math (no Mod Settings UI). |
| `bindings` | 20-element array | `Action` integers (0–36) | `PhysicalInput` index → `Action` value. See "Button bindings" below. |

Validation is per-setter inside `config::Set*`: each setter clamps/rejects out-of-range values before committing. There is no monolithic `ApplyConfig`.

## Button bindings

Wheel buttons map to in-game actions through a table driven by two enums that must stay in lockstep:

- `PhysicalInput` ([input_bindings.h:14-37](direct_wheel/src/input_bindings.h#L14-L37)) — 20 stable IDs for the controls on a modern G923-class wheel: `PaddleLeft`, `PaddleRight`, `DpadUp..Right`, `ButtonA..Y`, `Start`, `Select`, `LSB`, `RSB`, `Plus`, `Minus`, `ScrollClick`, `ScrollCW`, `ScrollCCW`, `Xbox`. Order is locked by the config.json `bindings` array and by the field order in `direct_wheel_settings.reds` — do not renumber; new controls append.
- `Action` ([direct_wheel/src/input_bindings.h](direct_wheel/src/input_bindings.h)) — 36 values (plus `None=0`) covering driving (horn, headlights, handbrake, autodrive, exit, call vehicle), camera (cycle forward, rear-view, reset), combat (shoot primary/secondary/tertiary), weapons (next/prev, slot 1/2, switch, holster), radio (menu, next), gameplay (consumable, iconic cyberware, quick save), menus (map, journal, inventory, phone, perks, crafting, pause), and menu nav (Confirm / Cancel / Up / Down / Left / Right). Indices must match `DirectWheelAction` in [direct_wheel_reds/direct_wheel_settings.reds](direct_wheel_reds/direct_wheel_settings.reds) so Mod Settings dropdown indices round-trip.

Each `Action` dispatches to a specific Windows virtual-key or mouse event via `SendInput`. Tap actions fire DOWN+UP on rising edge; Hold actions mirror the physical state.

Per-device layouts pick which DirectInput button index / POV value maps to each `PhysicalInput`:

| Device | Status |
| --- | --- |
| G923 Xbox | **Verified** empirically 2026-04-21 via `tools/input_probe`. Ground truth. |
| G923 PS / PC, G920, G29, G27 | Unverified — use G923 Xbox layout as best-guess fallback. Re-run `tools/input_probe` and update `input_bindings.cpp` when possible. |

Dispatch lifecycle each pump tick (`input_bindings::OnTick`):

1. For each of the 20 physical inputs, read `IsPhysicallyPressed(layout, input, buttons, pov)`.
2. Compare against the previous tick → rising / falling edge.
3. If an edge fired, `Dispatch(bindings[input], rising)`:
   - Suppress if the action is `VehicleOnly` and `sources::InVehicle()` is false.
   - Otherwise `SendInput` with `dwExtraInfo = kbd_hook::kExtraInfoTag` so our own LL keyboard hook doesn't filter the event.
4. There is no menu-state-aware override. An earlier design hard-overrode D-pad + A/B/X/Y to arrow keys / Enter / Escape while any menu was open, but CP2077's arrow keys are secondary vehicle controls (Up/Down = accelerate/decelerate, Left/Right = steer), so the override drove the car when the user pressed the D-pad even with binding=None. Menu nav is now just the user-visible default for D-pad + A in `direct_wheel_settings.reds`; the user can rebind to None to fully disable.

## FFB effect model

Layered Logitech SDK effects (`GUID_ConstantForce`, `GUID_Damper`, `GUID_Spring`, plus a road-surface SINE and an airborne shake). Created at bind time, reparameterized per event via `LogiPlay*Force*`, stopped via `LogiStop*`. Each effect's magnitude is scaled by `ffb.torquePct / 100` via `wheel::SetGlobalStrength`. If the bound wheel reports no FFB support, every FFB call becomes a no-op; callers don't need to branch.

The plugin computes its own per-tick FFB from real game state, not by translating game-side rumble (CP2077 has no vehicle rumble signal). Per-car parameters are derived from `WheeledPhysics` (turn rate → yaw reference, wheelbase → cruise speed and centering baseline) so the feel is consistent across vehicle classes.

A 200 ms watchdog tears down all effects when the vehicle detour stops firing (pause menu, unmount, crash); without it the last set effect would persist and the wheel would lock up at the last commanded torque.

## G HUB coexistence

| Knob | Owner |
| --- | --- |
| Rotation range (°) | G HUB. The mod reads it via `LogiGetOperatingRange` at 1 Hz and scales its FFB output to match. |
| Sensitivity curve | G HUB. |
| Centering spring | The mod's physics-derived centering torque. G HUB's own spring should be off in the wheel's profile. |
| Collision FFB | Mod (`direct_wheel_events.reds` + `wheel::TriggerJolt`). |
| Surface-texture FFB | Mod (envelope from chassis dynamics; surface-material baseline currently dormant). |
| Slip-angle countersteer | Mod (lateral-velocity dot world-right, gated on grip factor). |
| Per-vehicle response | Mod (derived from WheeledPhysics; user-tunable scalars in Mod Settings). |
| Rev-strip LEDs / music visualizer | Mod (driven from vehicle Blackboard via `direct_wheel_vehicle_signals.reds`). |
| Wheel button → keyboard | Mod (`input_bindings` + `kbd_hook`). |

The `kbd_hook` layer keeps this peaceful. G HUB's own Cyberpunk profile also synthesizes keyboard events; without filtering, bound controls would double-fire (wheel → `SendInput(Handbrake)` and G HUB → `Space`). The LL keyboard hook drops any non-tagged synthetic event matching a vehicle-only key while the player is on foot; the plugin's own events carry the `'dWHL'` `dwExtraInfo` tag to pass through.

The Logitech SDK's `LogiSetPreferredControllerProperties` API is broken on most G HUB builds and has been since at least 2023, so this mod does not write rotation range, sensitivity, or centering preferences back to the wheel. Per-effect magnitude scaling is the workaround.

## Developer tools

- [tools/input_probe/](tools/input_probe/) — standalone console tool. Enumerates connected wheels via the Logi SDK, polls at 16 ms, logs every button / POV edge and significant axis delta. Used to build per-device layout tables for `input_bindings.cpp`. Requires G HUB (or LGS) running and the wheel not claimed by another session (game closed).
- [tools/wheel_reset/](tools/wheel_reset/) — one-shot utility that pushes G HUB's current controller properties back to the wheel (or forces a specific operating-range override). Useful when firmware is stuck in an SDK-managed state and G HUB's GUI changes no longer reach the hardware.

## Build / deploy

- Build: `powershell -ExecutionPolicy Bypass -File build.ps1 -Config Release` (wraps CMake + MSVC; VS2022 + CMake 3.21+ required). Output at `build/direct_wheel/Release/direct_wheel.dll`.
- Deploy: `powershell -ExecutionPolicy Bypass -File deploy.ps1 [-Game <path>]`. Zip mode (default) produces `dist/direct_wheel-<version>.zip` laid out for the FOMOD installer (includes the patched `mod_settings.dll` from `vendor/mod_settings/build/Release/`). Direct mode (`-Game`) copies the DLL + six `.reds` files into `<CP2077>/red4ext/plugins/direct_wheel/` and `<CP2077>/r6/scripts/direct_wheel/`, overwrites `<CP2077>/red4ext/plugins/mod_settings/mod_settings.dll` with the patched build, and invalidates the redscript cache.
- Runtime deps: [RED4ext](https://github.com/WopsS/RED4ext), [redscript](https://github.com/jac3km4/redscript), [Mod Settings](https://github.com/jackhumbert/mod_settings), [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl).

## Sources

- [RED4ext documentation](https://docs.red4ext.com)
- [RED4ext & RED4ext.SDK on GitHub](https://github.com/WopsS/RED4ext)
- [Let There Be Flight (hash source for UpdateVehicleCameraInput)](https://github.com/jackhumbert/let_there_be_flight)
- [Mod Settings](https://github.com/jackhumbert/mod_settings) / [Nexus listing](https://www.nexusmods.com/cyberpunk2077/mods/4885)
- [redscript](https://github.com/jac3km4/redscript) / [redscript wiki](https://wiki.redmodding.org/redscript)
- [ArchiveXL](https://github.com/psiberx/cp2077-archive-xl)
- [Logitech Gaming Software SDK — Steering Wheel](https://www.logitechg.com/en-us/innovation/developer-lab.html)
- [the-sz USB ID database — Logitech 046D](https://the-sz.com/products/usbid/index.php?v=0x046D)
- [FOMOD ModuleConfig schema](https://fomod-docs.readthedocs.io/en/latest/_static/ModuleConfig.html)
