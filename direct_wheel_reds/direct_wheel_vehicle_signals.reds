// Push real vehicle telemetry (engine RPM, radio on/off) from the game's
// own Blackboard into the direct_wheel plugin so the LED rev strip can show
// the actual simulated RPM instead of a throttle-derived bluff, and so
// the music-visualizer mode fires from the game's authoritative radio
// state rather than audio-amplitude heuristics.
//
// Blackboard fields used (VehicleDef, scripts/core/blackboard/blackboardDefinitions.script):
//   RPMValue       : Float   — current engine RPM, driven by the audio/
//                              gameplay system (same value car_hud.script
//                              feeds its RPM gauge widget).
//   VehRadioState  : Bool    — true while the in-car radio receiver is on.
//
// RPM is normalised against VehicleRecord.VehEngineData().MaxRPM() —
// per-vehicle, read once per mount.
//
// Lifecycles that must all converge to an attached listener set:
//   - Fresh mount (VehicleFinishedMountingEvent) — happens on entering
//     a vehicle normally
//   - Save load while inside a vehicle (PlayerPuppet.OnGameAttached) —
//     the mount event doesn't replay post-load, so we re-attach here
//   - Unmount (UnmountingEvent) — tears down listeners and clears state

@addField(VehicleComponent)
public let m_direct_wheelRpmMax: Float;

@addField(VehicleComponent)
public let m_direct_wheelRpmBbId: ref<CallbackHandle>;

@addField(VehicleComponent)
public let m_direct_wheelRadioBbId: ref<CallbackHandle>;

// Resolve max-RPM, register Blackboard listeners, seed initial state.
// Idempotent: if the listeners are already attached (m_direct_wheelRpmBbId
// is set) this short-circuits. Safe to call from any lifecycle event
// that might be the "first" one to observe the mounted vehicle.
@addMethod(VehicleComponent)
public func DirectWheelAttach() -> Void {
  if IsDefined(this.m_direct_wheelRpmBbId) {
    return;  // already attached for this vehicle instance
  }

  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if !IsDefined(vehicle) {
    return;
  }

  // Resolve this vehicle's max RPM once. VehEngineData is a static
  // record attached to every VehicleObject; falling back to 8000 is
  // defensive for tanks / oddball vehicles where the record lookup
  // fails (an arbitrary "reasonable redline" that prevents div-by-0).
  let record: wref<Vehicle_Record> = vehicle.GetRecord();
  let maxRpm: Float = 8000.0;
  if IsDefined(record) {
    let engineData: wref<VehicleEngineData_Record> = record.VehEngineData();
    if IsDefined(engineData) {
      let recordedMax: Float = engineData.MaxRPM();
      if recordedMax > 0.0 {
        maxRpm = recordedMax;
      }
    }
  }
  this.m_direct_wheelRpmMax = maxRpm;

  let bb: ref<IBlackboard> = vehicle.GetBlackboard();
  if !IsDefined(bb) {
    return;
  }

  // Seed initial values. For radio we OR two sources because either
  // can momentarily disagree with reality on save-load:
  //   - The native IsRadioReceiverActive() can return false for a
  //     beat after attach while the radio component is still spinning
  //     up (the symptom: load-into-car-with-radio-on shows rev strip
  //     instead of music viz until the user toggles off/on).
  //   - The Blackboard field VehRadioState reflects the saved state
  //     immediately but the listener we register below only fires on
  //     CHANGES, so a true-at-load value would never be delivered.
  // OR-ing them ensures we pick up "radio is on" from whichever
  // source already knows it. Subsequent state changes still flow
  // through the listener registered just below.
  let initialRpm: Float = bb.GetFloat(GetAllBlackboardDefs().Vehicle.RPMValue);
  DirectWheel_SetEngineRpmNormalized(initialRpm / this.m_direct_wheelRpmMax);
  let radioOn: Bool = vehicle.IsRadioReceiverActive()
                      || bb.GetBool(GetAllBlackboardDefs().Vehicle.VehRadioState);
  DirectWheel_SetRadioActive(radioOn);

  // Subscribe to change events.
  this.m_direct_wheelRpmBbId = bb.RegisterListenerFloat(
    GetAllBlackboardDefs().Vehicle.RPMValue, this, n"OnGwheelRpmChanged");
  this.m_direct_wheelRadioBbId = bb.RegisterListenerBool(
    GetAllBlackboardDefs().Vehicle.VehRadioState, this, n"OnGwheelRadioChanged");

  // The C++ plugin tracks in-vehicle context via
  // DirectWheel_Set/ClearPlayerVehicle, normally set by the mount event
  // wrappers in direct_wheel_mount.reds. Save-load-in-vehicle skips those,
  // so explicitly assert the cached pointer here as a safety net.
  DirectWheel_SetPlayerVehicle(vehicle);

  // Save-load-with-radio-on recovery. At this exact moment both the
  // native IsRadioReceiverActive() and the Blackboard's VehRadioState
  // can read false because the radio component hasn't fully spun up
  // yet — the false→true transition that follows happens before our
  // listener (registered just above) fires for the first time, so we
  // never get notified of the saved state. Schedule a few delayed
  // re-reads to catch the radio after it settles. Cheap to do
  // unconditionally; the LED controller already gates on
  // visualizerWhileMusic so a false reading is a no-op.
  let delaySys: ref<DelaySystem> = GameInstance.GetDelaySystem(vehicle.GetGame());
  if IsDefined(delaySys) {
    delaySys.DelayCallback(DirectWheelRadioReseedCallback.Create(this), 0.5);
    delaySys.DelayCallback(DirectWheelRadioReseedCallback.Create(this), 1.5);
    delaySys.DelayCallback(DirectWheelRadioReseedCallback.Create(this), 3.0);
  }
}

// Re-seeds radio state from whichever source has the truth right
// now. Run via DelayCallback at progressively later offsets after
// DirectWheelAttach to bridge the post-load gap before the radio
// component finishes spinning up. Idempotent — if the listener has
// already pushed a fresh state, this just confirms it.
@addMethod(VehicleComponent)
public func DirectWheelReseedRadio() -> Void {
  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if !IsDefined(vehicle) { return; }
  let bb: ref<IBlackboard> = vehicle.GetBlackboard();
  if !IsDefined(bb) { return; }
  let radioOn: Bool = vehicle.IsRadioReceiverActive()
                      || bb.GetBool(GetAllBlackboardDefs().Vehicle.VehRadioState);
  DirectWheel_SetRadioActive(radioOn);
}

public class DirectWheelRadioReseedCallback extends DelayCallback {
  private let vc: wref<VehicleComponent>;

  public static func Create(v: ref<VehicleComponent>) -> ref<DirectWheelRadioReseedCallback> {
    let cb = new DirectWheelRadioReseedCallback();
    cb.vc = v;
    return cb;
  }

  public func Call() -> Void {
    let v = this.vc;
    if IsDefined(v) {
      v.DirectWheelReseedRadio();
    }
  }
}

// Tear down listeners + clear plugin state. Idempotent.
@addMethod(VehicleComponent)
public func DirectWheelDetach() -> Void {
  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if IsDefined(vehicle) {
    let bb: ref<IBlackboard> = vehicle.GetBlackboard();
    if IsDefined(bb) {
      if IsDefined(this.m_direct_wheelRpmBbId) {
        bb.UnregisterListenerFloat(GetAllBlackboardDefs().Vehicle.RPMValue, this.m_direct_wheelRpmBbId);
      }
      if IsDefined(this.m_direct_wheelRadioBbId) {
        bb.UnregisterListenerBool(GetAllBlackboardDefs().Vehicle.VehRadioState, this.m_direct_wheelRadioBbId);
      }
    }
  }
  this.m_direct_wheelRpmBbId = null;
  this.m_direct_wheelRadioBbId = null;
  this.m_direct_wheelRpmMax = 0.0;

  DirectWheel_SetEngineRpmNormalized(0.0);
  DirectWheel_SetRadioActive(false);
}

@wrapMethod(VehicleComponent)
protected cb func OnVehicleFinishedMountingEvent(evt: ref<VehicleFinishedMountingEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let character: ref<GameObject> = evt.character as GameObject;
  if IsDefined(character) && character.IsPlayer() && evt.isMounting {
    this.DirectWheelAttach();
  }
  return result;
}

@wrapMethod(VehicleComponent)
protected cb func OnUnmountingEvent(evt: ref<UnmountingEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let game: GameInstance = this.GetVehicle().GetGame();
  let childId: EntityID = evt.request.lowLevelMountingInfo.childId;
  let mountChild: ref<GameObject> = GameInstance.FindEntityByID(game, childId) as GameObject;
  if IsDefined(mountChild) && mountChild.IsPlayer() {
    this.DirectWheelDetach();
  }
  return result;
}

// Callbacks must live on VehicleComponent so `this` binds correctly
// when RegisterListenerFloat / RegisterListenerBool dispatch them.
// Top-level `cb func` without @addMethod is treated as static and
// fails to compile with UNEXPECTED_THIS.
@addMethod(VehicleComponent)
protected cb func OnGwheelRpmChanged(value: Float) -> Bool {
  if this.m_direct_wheelRpmMax > 0.0 {
    DirectWheel_SetEngineRpmNormalized(value / this.m_direct_wheelRpmMax);
  }
  return true;
}

@addMethod(VehicleComponent)
protected cb func OnGwheelRadioChanged(value: Bool) -> Bool {
  DirectWheel_SetRadioActive(value);
  return true;
}

// Belt-and-suspenders: the Blackboard listener should catch every radio
// on/off transition, but if the game ever routes a toggle through a
// code path that doesn't update the Blackboard field, this wrap on the
// explicit event guarantees the plugin sees it. Re-read via the native
// method (state may settle async inside wrappedMethod's handler).
@wrapMethod(VehicleComponent)
protected cb func OnRadioToggleEvent(evt: ref<RadioToggleEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let vehicle: ref<VehicleObject> = this.GetVehicle();
  if IsDefined(vehicle) {
    DirectWheel_SetRadioActive(vehicle.IsRadioReceiverActive());
  }
  return result;
}

// Save-load-in-vehicle recovery. PlayerPuppet.OnGameAttached fires after
// every save load; VehicleFinishedMountingEvent does NOT replay for a
// vehicle that was already mounted when the save was written. If V is
// currently mounted at attach time, run the same attach path we'd have
// run on a fresh mount.
@wrapMethod(PlayerPuppet)
protected cb func OnGameAttached() -> Bool {
  let result: Bool = wrappedMethod();
  let vehicle: wref<VehicleObject> = GetMountedVehicle(this);
  if IsDefined(vehicle) {
    let vc: ref<VehicleComponent> = vehicle.GetVehicleComponent();
    if IsDefined(vc) {
      vc.DirectWheelAttach();
    }
  }
  return result;
}
