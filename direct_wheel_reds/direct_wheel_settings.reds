// In-game settings page for the direct_wheel mod.
//
// Single DirectWheelSettings class. Categories (`ModSettings.category` +
// `ModSettings.category.order`) provide visual grouping on the scrolling
// settings panel.
//
// Capability auto-detection (how the section-hide actually works):
//
//   1. Three hidden Bool fields (hasFfbHardware, hasRevLeds, hasRightCluster)
//      carry the runtime-property `ModSettings.hidden = "true"`. With our
//      patched mod_settings build they are excluded from the UI list but
//      still registered as variables.
//
//   2. Other settings reference these fields via `ModSettings.dependency`.
//      mod_settings hides the dependent setting whenever the dependency
//      target's stored value is not "true".
//
//   3. mod_settings reads its stored values from
//      `red4ext/plugins/mod_settings/user.ini` once at process start. So to
//      drive the three capability flags from real hardware state we have to
//      write that file BEFORE mod_settings reads it. That happens in
//      direct_wheel.dll's RED4ext OnLoad — see direct_wheel/src/mod_settings_seed.cpp.
//      Plugin OnLoads complete before script-data processing, so the
//      ordering is safe regardless of plugin load order.
//
//   4. Important consequence: the UI reflects the wheel state captured at
//      the moment of process start. Plug or unplug the wheel = restart the
//      game once for the section list to catch up.
//
// `ModSettings.hidden` requires our patched build of mod_settings (a
// small patch on top of jackhumbert/mod_settings v0.2.21, see
// vendor/mod_settings/). Plain upstream mod_settings ignores the hidden
// runtime property and the three capability flags would render as visible
// Bool toggles. deploy.ps1 ships our patched DLL alongside direct_wheel.dll.
//
// mod_settings dependency evaluation reads from its OWN internal
// RuntimeVariable<T> storage, NOT from the instance passed to
// RegisterListenerToClass. Setting capability flags on the listener
// instance from script does nothing for visibility — the only way to
// drive them is via user.ini (above). Verified against mod_settings
// source 2026-04-25.

public class DirectWheelSettings extends IScriptable {

  // ---- Hidden capability flags (auto-set in OnGameAttached) --------------
  //
  // ModSettings.hidden = "true" makes these invisible in the settings UI.
  // Requires our patched mod_settings build (vendor/mod_settings/). Other
  // fields reference these as `ModSettings.dependency` targets to hide
  // entire sections on wheels without the relevant hardware.

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasFfbHardware: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRevLeds: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.hidden", "true")
  let hasRightCluster: Bool = true;

  // ---- Wheel input --------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Enable wheel input")
  let inputEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Wheel input")
  @runtimeProperty("ModSettings.category.order", "100")
  @runtimeProperty("ModSettings.displayName", "Treat clutch as brake")
  @runtimeProperty("ModSettings.description", "The clutch acts just like the brake pedal. Useful for stiff brake pedals, since there's no gear system in the game.")
  @runtimeProperty("ModSettings.dependency", "inputEnabled")
  let clutchAsBrake: Bool = true;

  // ---- Force feedback -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Enable force feedback")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "FFB strength (%)")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let ffbTorquePct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Cornering feedback (%)")
  @runtimeProperty("ModSettings.description", "Adds spring stiffness while the car is rotating.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let yawFeedbackPct: Int32 = 50;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Active torque (%)")
  @runtimeProperty("ModSettings.description", "How hard the wheel pushes toward center, scaled by speed and steering angle.")
  @runtimeProperty("ModSettings.min", "0")
  @runtimeProperty("ModSettings.max", "100")
  @runtimeProperty("ModSettings.step", "5")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let activeTorqueStrengthPct: Int32 = 100;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Force feedback")
  @runtimeProperty("ModSettings.category.order", "200")
  @runtimeProperty("ModSettings.displayName", "Stationary threshold (m/s)")
  @runtimeProperty("ModSettings.description", "Below this speed the wheel has no centering force.")
  @runtimeProperty("ModSettings.min", "0.0")
  @runtimeProperty("ModSettings.max", "5.0")
  @runtimeProperty("ModSettings.step", "0.1")
  @runtimeProperty("ModSettings.dependency", "hasFfbHardware")
  let stationaryThresholdMps: Float = 0.5;

  // ---- Rev-strip LEDs -----------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rev-strip LEDs")
  @runtimeProperty("ModSettings.category.order", "300")
  @runtimeProperty("ModSettings.displayName", "Enable rev-strip LEDs")
  @runtimeProperty("ModSettings.dependency", "hasRevLeds")
  let ledEnabled: Bool = true;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Rev-strip LEDs")
  @runtimeProperty("ModSettings.category.order", "300")
  @runtimeProperty("ModSettings.displayName", "Rev strip as visualizer while music is playing")
  @runtimeProperty("ModSettings.dependency", "hasRevLeds")
  let ledVisualizerWhileMusic: Bool = true;

  // ---- Button bindings (15 controls present on every G-series wheel) -----
  //
  // Every binding here is user-controlled, with no hidden overrides. The
  // D-pad + A defaults are Menu-nav (Up/Down/Left/Right arrow keys and
  // Enter) so the wheel navigates pause/map/inventory menus like a
  // controller out of the box. If you'd rather those wheel controls stay
  // inert while driving, set them to None — CP2077's arrow keys are
  // secondary vehicle controls (Up/Down = accelerate/decelerate, Left/Right
  // = steer), so binding the D-pad to Menu-nav and then pressing the D-pad
  // while driving will nudge the car.
  //
  // IMPORTANT: clear the D-pad and A/B/X/Y keyboard bindings in G HUB's
  // Cyberpunk profile. Otherwise G HUB + plugin both fire keyboard
  // events and you'll get doubled keypresses.

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Left paddle shifter")
  let bindPaddleLeft: DirectWheelAction = DirectWheelAction.ShootPrimary;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Right paddle shifter")
  let bindPaddleRight: DirectWheelAction = DirectWheelAction.ShootPrimary;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Up")
  let bindDpadUp: DirectWheelAction = DirectWheelAction.MenuUp;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Down")
  let bindDpadDown: DirectWheelAction = DirectWheelAction.MenuDown;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Left")
  let bindDpadLeft: DirectWheelAction = DirectWheelAction.MenuLeft;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "D-pad Right")
  let bindDpadRight: DirectWheelAction = DirectWheelAction.MenuRight;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "A button")
  let bindButtonA: DirectWheelAction = DirectWheelAction.MenuConfirm;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "B button (in-vehicle)")
  let bindButtonB: DirectWheelAction = DirectWheelAction.Handbrake;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "X button (in-vehicle)")
  let bindButtonX: DirectWheelAction = DirectWheelAction.Horn;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Y button (in-vehicle)")
  let bindButtonY: DirectWheelAction = DirectWheelAction.Autodrive;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Start button")
  let bindStart: DirectWheelAction = DirectWheelAction.Pause;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Select / View button")
  let bindSelect: DirectWheelAction = DirectWheelAction.OpenMap;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "LSB (left stick click)")
  let bindLSB: DirectWheelAction = DirectWheelAction.OpenPhone;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "RSB (right stick click)")
  let bindRSB: DirectWheelAction = DirectWheelAction.RadioMenu;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Button bindings")
  @runtimeProperty("ModSettings.category.order", "400")
  @runtimeProperty("ModSettings.displayName", "Xbox / Guide button")
  let bindXbox: DirectWheelAction = DirectWheelAction.ExitVehicle;

  // ---- Lower-cluster bindings (G29 / G923 / G PRO; absent on G920) -------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Lower-cluster bindings")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Plus (+) button")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindPlus: DirectWheelAction = DirectWheelAction.CameraCycleForward;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Lower-cluster bindings")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Minus (-) button")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindMinus: DirectWheelAction = DirectWheelAction.Headlights;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Lower-cluster bindings")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Scroll click (Return)")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindScrollClick: DirectWheelAction = DirectWheelAction.HolsterWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Lower-cluster bindings")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Scroll clockwise")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindScrollCW: DirectWheelAction = DirectWheelAction.NextWeapon;

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Lower-cluster bindings")
  @runtimeProperty("ModSettings.category.order", "500")
  @runtimeProperty("ModSettings.displayName", "Scroll counter-clockwise")
  @runtimeProperty("ModSettings.dependency", "hasRightCluster")
  let bindScrollCCW: DirectWheelAction = DirectWheelAction.PrevWeapon;

  // ---- Startup ------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Startup")
  @runtimeProperty("ModSettings.category.order", "600")
  @runtimeProperty("ModSettings.displayName", "Pon pon shi greeting")
  @runtimeProperty("ModSettings.description", "Make the wheel dance when the game starts.")
  let handshakePlayOnStart: Bool = false;

  // ---- Debug --------------------------------------------------------------

  @runtimeProperty("ModSettings.mod", "G-series Wheel")
  @runtimeProperty("ModSettings.category", "Debug")
  @runtimeProperty("ModSettings.category.order", "700")
  @runtimeProperty("ModSettings.displayName", "Debug logging")
  @runtimeProperty("ModSettings.description", "Logs to red4ext/logs/direct_wheel-*.log.")
  let ffbDebugLogging: Bool = false;

  // ---- Listener callbacks (invoked by Mod Settings, NOT cb funcs) --------

  public func OnModSettingsChange() -> Void {
    this.Push();
  }

  public func Push() -> Void {
    DirectWheel_SetInputEnabled(this.inputEnabled);
    DirectWheel_SetClutchAsBrake(this.clutchAsBrake);

    DirectWheel_SetFfbEnabled(this.ffbEnabled);
    DirectWheel_SetFfbDebugLogging(this.ffbDebugLogging);
    DirectWheel_SetFfbTorquePct(this.ffbTorquePct);

    DirectWheel_SetStationaryThresholdMps(this.stationaryThresholdMps);
    DirectWheel_SetYawFeedbackPct(this.yawFeedbackPct);
    DirectWheel_SetActiveTorqueStrengthPct(this.activeTorqueStrengthPct);

    DirectWheel_SetHandshakePlayOnStart(this.handshakePlayOnStart);

    DirectWheel_SetLedEnabled(this.ledEnabled);
    DirectWheel_SetLedVisualizerWhileMusic(this.ledVisualizerWhileMusic);

    // Input IDs match the PhysicalInput enum in
    // direct_wheel/src/input_bindings.h. D-pad + A/B/X/Y (ids 2-9) are
    // in-vehicle bindings here; the plugin overrides them with
    // gamepad-nav whenever a menu is open.
    DirectWheel_SetInputBinding(0,  EnumInt(this.bindPaddleLeft));
    DirectWheel_SetInputBinding(1,  EnumInt(this.bindPaddleRight));
    DirectWheel_SetInputBinding(2,  EnumInt(this.bindDpadUp));
    DirectWheel_SetInputBinding(3,  EnumInt(this.bindDpadDown));
    DirectWheel_SetInputBinding(4,  EnumInt(this.bindDpadLeft));
    DirectWheel_SetInputBinding(5,  EnumInt(this.bindDpadRight));
    DirectWheel_SetInputBinding(6,  EnumInt(this.bindButtonA));
    DirectWheel_SetInputBinding(7,  EnumInt(this.bindButtonB));
    DirectWheel_SetInputBinding(8,  EnumInt(this.bindButtonX));
    DirectWheel_SetInputBinding(9,  EnumInt(this.bindButtonY));
    DirectWheel_SetInputBinding(10, EnumInt(this.bindStart));
    DirectWheel_SetInputBinding(11, EnumInt(this.bindSelect));
    DirectWheel_SetInputBinding(12, EnumInt(this.bindLSB));
    DirectWheel_SetInputBinding(13, EnumInt(this.bindRSB));
    DirectWheel_SetInputBinding(14, EnumInt(this.bindPlus));
    DirectWheel_SetInputBinding(15, EnumInt(this.bindMinus));
    DirectWheel_SetInputBinding(16, EnumInt(this.bindScrollClick));
    DirectWheel_SetInputBinding(17, EnumInt(this.bindScrollCW));
    DirectWheel_SetInputBinding(18, EnumInt(this.bindScrollCCW));
    DirectWheel_SetInputBinding(19, EnumInt(this.bindXbox));
  }
}

// Actions the plugin knows how to dispatch. Indices must match the Action
// enum in direct_wheel/src/input_bindings.h — same values, same order.
// Grouped by category (driving / camera / combat / weapons / radio /
// gameplay / menus / menu-nav) so Mod Settings' scroll-list shows similar
// actions adjacent. MUST stay in lockstep with the C++ `Action` enum in
// direct_wheel/src/input_bindings.h — same names, same integer values, same
// order. Mod Settings persists each binding by integer value, so any
// reorder shifts users' saved bindings (one-time forced re-bind).
//
// `RearViewCamera` is the redscript-side spelling of CameraCycleBackward;
// the dropdown label inherits the redscript identifier so we use the
// gameplay-meaningful name here even though the C++ side keeps the older
// "Backward" naming for historical reasons.

enum DirectWheelAction {
  None = 0,

  // Driving
  Horn = 1,
  Headlights = 2,
  Handbrake = 3,
  Autodrive = 4,
  ExitVehicle = 5,
  CallVehicle = 6,

  // Camera
  CameraCycleForward = 7,
  RearViewCamera = 8,
  CameraReset = 9,

  // Combat
  ShootPrimary = 10,
  ShootSecondary = 11,
  ShootTertiary = 12,

  // Weapons
  NextWeapon = 13,
  PrevWeapon = 14,
  WeaponSlot1 = 15,
  WeaponSlot2 = 16,
  SwitchWeapons = 17,
  HolsterWeapon = 18,

  // Radio
  RadioMenu = 19,
  RadioNext = 20,

  // Gameplay misc
  UseConsumable = 21,
  IconicCyberware = 22,
  QuickSave = 23,

  // Menus
  OpenMap = 24,
  OpenJournal = 25,
  OpenInventory = 26,
  OpenPhone = 27,
  OpenPerks = 28,
  OpenCrafting = 29,
  Pause = 30,

  // Menu navigation
  MenuConfirm = 31,
  MenuCancel = 32,
  MenuUp = 33,
  MenuDown = 34,
  MenuLeft = 35,
  MenuRight = 36,
}

// Attach our settings instance to the player puppet so it lives for the
// session and register it as a listener with Mod Settings. The listener
// receives OnModSettingsChange callbacks when the user clicks Apply in the
// settings menu — we use that to push the new values into the C++ plugin
// via the DirectWheel_Set* natives.
//
// The hidden capability flags (hasFfbHardware / hasRevLeds /
// hasRightCluster) are not seeded here. Mod Settings ignores writes to the
// listener instance for dependency / visibility purposes. Those flags are
// driven directly into mod_settings/user.ini from the C++ side at plugin
// load (see direct_wheel/src/mod_settings_seed.cpp).

@addField(PlayerPuppet)
public let m_direct_wheelSettings: ref<DirectWheelSettings>;

@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  if !IsDefined(this.m_direct_wheelSettings) {
    this.m_direct_wheelSettings = new DirectWheelSettings();
    ModSettings.RegisterListenerToClass(this.m_direct_wheelSettings);
    ModSettings.RegisterListenerToModifications(this.m_direct_wheelSettings);
    this.m_direct_wheelSettings.Push();
  }
  return result;
}
