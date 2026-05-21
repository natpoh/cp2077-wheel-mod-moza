// Declarations for native functions registered by direct_wheel.dll.
// Keep this file in sync with direct_wheel/src/rtti.cpp::PostRegisterTypes.

public static native func DirectWheel_GetVersion() -> String;
public static native func DirectWheel_IsPluginReady() -> Bool;
public static native func DirectWheel_GetDeviceInfo() -> String;
public static native func DirectWheel_HasFFB() -> Bool;
public static native func DirectWheel_IsDebugLoggingEnabled() -> Bool;
public static native func DirectWheel_GetDebugRawSteer() -> Float;
public static native func DirectWheel_GetDebugWheelSteer() -> Float;
public static native func DirectWheel_ReadConfig() -> String;

// Wheel-model auto-discovery. Read at OnGameAttached to populate dependency
// targets that hide controls the bound wheel doesn't physically have.
// Permissive default (true / "(no wheel bound)") when the SDK hasn't bound
// yet, so the settings page shows everything until detection is ready.
public static native func DirectWheel_DetectedHasRightCluster() -> Bool;
public static native func DirectWheel_DetectedHasFfbHardware() -> Bool;
public static native func DirectWheel_DetectedHasRevLeds() -> Bool;
public static native func DirectWheel_DetectedModelName() -> String;

public static native func DirectWheel_SetInputEnabled(v: Bool) -> Bool;
public static native func DirectWheel_SetClutchAsBrake(v: Bool) -> Bool;
public static native func DirectWheel_SetInvertSteering(v: Bool) -> Bool;
public static native func DirectWheel_SetInvertThrottle(v: Bool) -> Bool;
public static native func DirectWheel_SetInvertBrake(v: Bool) -> Bool;

public static native func DirectWheel_SetFfbEnabled(v: Bool) -> Bool;
public static native func DirectWheel_SetFfbDebugLogging(v: Bool) -> Bool;
public static native func DirectWheel_SetFfbTorquePct(pct: Int32) -> Bool;

// Startup handshake (LED sweep + 4 triplets + centering breath) on wheel bind.
// Effective at next wheel bind — toggling mid-session does not retroactively
// play or cancel a handshake that already fired this session.
public static native func DirectWheel_SetHandshakePlayOnStart(v: Bool) -> Bool;

// Phase-1 physics FFB: speed-gated self-centering spring with yaw-rate bonus.
public static native func DirectWheel_SetStationaryThresholdMps(mps: Float) -> Bool;
public static native func DirectWheel_SetYawFeedbackPct(pct: Int32) -> Bool;
public static native func DirectWheel_SetActiveTorqueStrengthPct(pct: Int32) -> Bool;
public static native func DirectWheel_SetConstantForcePct(pct: Int32) -> Bool;
public static native func DirectWheel_SetSpringForcePct(pct: Int32) -> Bool;
public static native func DirectWheel_SetDamperForcePct(pct: Int32) -> Bool;
public static native func DirectWheel_SetFrictionForcePct(pct: Int32) -> Bool;
public static native func DirectWheel_SetSineForcePct(pct: Int32) -> Bool;
public static native func DirectWheel_SetJoltForcePct(pct: Int32) -> Bool;
public static native func DirectWheel_SetSpeedSensitiveSteeringPct(pct: Int32) -> Bool;
public static native func DirectWheel_SetSteeringCurve25(pct: Int32) -> Bool;
public static native func DirectWheel_SetSteeringCurve50(pct: Int32) -> Bool;
public static native func DirectWheel_SetSteeringCurve75(pct: Int32) -> Bool;


// Rev-strip LED bar on top of the wheel (G29/G920/G923). VisualizerWhileMusic
// swaps the speed-driven rev bar for a WASAPI-loopback audio visualizer
// whenever system audio is playing; falls back to rev-strip on silence.
public static native func DirectWheel_SetLedEnabled(v: Bool) -> Bool;
public static native func DirectWheel_SetLedVisualizerWhileMusic(v: Bool) -> Bool;

// Per-physical-input action binding. inputId is one of the stable IDs in
// direct_wheel/src/input_bindings.h (0 = PaddleLeft, 1 = PaddleRight, etc.).
// action is a DirectWheelAction enum value cast to Int32; the plugin dispatches
// it as a Windows SendInput event on rising/falling edges.
public static native func DirectWheel_SetInputBinding(inputId: Int32, action: Int32) -> Bool;

// Axis mapping — axis name is one of: lX, lY, lZ, lRx, lRy, lRz, slider0, slider1
public static native func DirectWheel_SetAxisSteer(axis: String) -> Bool;
public static native func DirectWheel_SetAxisThrottle(axis: String) -> Bool;
public static native func DirectWheel_SetAxisBrake(axis: String) -> Bool;
public static native func DirectWheel_SetAxisClutch(axis: String) -> Bool;

// Tracks the player's currently-mounted vehicle. The plugin's vehicle-
// input detour fires for every visible vehicle each tick; without this
// filter, our steering/throttle/brake writes would propagate to all of
// them (remote-driving parked cars, etc.). Call SetPlayerVehicle on
// mount and ClearPlayerVehicle on dismount from VehicleComponent event
// wrappers.
// Menu lifecycle signals. The direct_wheel_menu.reds wrappers call these
// when gameplay-blocking menus open/close so the plugin can suppress
// input injection while in menus.
public static native func DirectWheel_MenuOpen(tag: String) -> Bool;
public static native func DirectWheel_MenuClose(tag: String) -> Bool;

public static native func DirectWheel_SetPlayerVehicle(v: ref<VehicleObject>) -> Bool;
public static native func DirectWheel_ClearPlayerVehicle() -> Bool;

// Vehicle telemetry pushed from Blackboard listeners (direct_wheel_vehicle_signals.reds).
// Normalized engine RPM in [0..1] = RPMValue / VehEngineData.MaxRPM().
// Radio state mirrors VehicleDef.VehRadioState.
public static native func DirectWheel_SetEngineRpmNormalized(v: Float) -> Bool;
public static native func DirectWheel_SetRadioActive(v: Bool) -> Bool;

// Collision / bump feedback. lateralKick is the world-space hit direction
// dotted with the vehicle's right vector, signed in [-1..+1] (negative =
// struck on left). The plugin filters out events from non-player vehicles
// using the handle; we still forward every event for simplicity.
public static native func DirectWheel_OnVehicleBump(v: ref<VehicleObject>, lateralKick: Float) -> Bool;
public static native func DirectWheel_OnVehicleHit(v: ref<VehicleObject>, lateralKick: Float) -> Bool;

// Per-wheel road material report. Called from the redscript raycast
// poller (see direct_wheel_surface.reds) when a wheel's material CName
// changes. wheelIdx is 0..3, material is the physics material CName
// (e.g. `asphalt`, `dirt`, `metal`). C++ logs the transition and
// eventually drives surface-aware FFB off the category.
public static native func DirectWheel_OnWheelMaterial(wheelIdx: Int32, material: CName) -> Bool;

// Device picker — returns pipe-separated list of attached controllers.
public static native func DirectWheel_GetConnectedDeviceList() -> String;
public static native func DirectWheel_SetWheelDeviceName(name: String) -> Bool;
public static native func DirectWheel_SetPedalDeviceName(name: String) -> Bool;
public static native func DirectWheel_SetWheelDeviceIndex(idx: Int32) -> Bool;
public static native func DirectWheel_SetPedalDeviceIndex(idx: Int32) -> Bool;
public static native func DirectWheel_GetDeviceCount() -> Int32;
public static native func DirectWheel_ResetDevices() -> Bool;

// Axis auto-binding: target 0=throttle, 1=brake, 2=clutch.
public static native func DirectWheel_BeginAxisBinding(target: Int32) -> Bool;
