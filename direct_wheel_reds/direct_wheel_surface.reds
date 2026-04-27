// Per-vehicle road surface detection for the direct_wheel plugin.
//
// CP2077 exposes the live material-under-wheel via a physics raycast:
// TraceResult.material is a CName populated by
// SpatialQueriesSystem.SyncRaycastByCollisionGroup. We cast a single
// downward ray from the player vehicle's world position every ~50 ms,
// read the material CName, and forward transitions to C++ via
// DirectWheel_OnWheelMaterial.
//
// Why single-ray and not per-wheel: the 4 wheels sit within ~2 m of
// each other; they're on the same material 99% of the time. Going
// per-wheel would need SlotComponent + wheel-slot CName enumeration,
// and on VehicleObject the SlotComponent isn't directly reachable
// (it's exposed on ScriptedPuppet only). Single-ray is plenty for
// surface-class FFB categorisation and avoids the plumbing entirely.
//
// Why raycast at all, and not the persistence path: the
// vehiclePersistentDataPS.wheelRuntimeData.previousTouchedMaterial
// CName is save-state infrastructure — it's not updated per frame.
// Raycast against live collision geometry is what the engine itself
// uses for surface reactions.

@addField(VehicleComponent)
private let m_direct_wheelSurfacePoller: ref<DirectWheelSurfacePoller>;

public class DirectWheelSurfacePoller extends IScriptable {
  private let vehicle: wref<VehicleObject>;
  private let lastMaterial: CName;
  private let running: Bool;
  private let debugLayer1: Uint32;
  private let debugLayer2: Uint32;
  private let hasDrawnDebug: Bool;

  public func Start(v: ref<VehicleObject>) -> Void {
    this.vehicle = v;
    this.running = true;
    this.lastMaterial = n"";
    this.ScheduleNext();
  }

  public func Stop() -> Void {
    this.running = false;
    if this.hasDrawnDebug && IsDefined(this.vehicle) {
      GameInstance.GetDebugVisualizerSystem(this.vehicle.GetGame()).ClearLayer(this.debugLayer1);
      GameInstance.GetDebugVisualizerSystem(this.vehicle.GetGame()).ClearLayer(this.debugLayer2);
      this.hasDrawnDebug = false;
    }
  }

  private func ScheduleNext() -> Void {
    if !this.running { return; }
    let v = this.vehicle;
    if !IsDefined(v) { return; }
    GameInstance.GetDelaySystem(v.GetGame()).DelayCallback(
      DirectWheelSurfaceTickCallback.Create(this), 0.05);
  }

  public func Tick() -> Void {
    if !this.running { return; }
    let v = this.vehicle;
    if !IsDefined(v) { return; }

    let sqs = GameInstance.GetSpatialQueriesSystem(v.GetGame());
    if !IsDefined(sqs) {
      this.ScheduleNext();
      return;
    }

    if DirectWheel_IsDebugLoggingEnabled() {
      if this.hasDrawnDebug {
        GameInstance.GetDebugVisualizerSystem(v.GetGame()).ClearLayer(this.debugLayer1);
        GameInstance.GetDebugVisualizerSystem(v.GetGame()).ClearLayer(this.debugLayer2);
      }
      let rawSteer = DirectWheel_GetDebugRawSteer();
      let gameSteer = DirectWheel_GetDebugWheelSteer();
      this.debugLayer1 = GameInstance.GetDebugVisualizerSystem(v.GetGame()).DrawText(new Vector4(20.0, 20.0, 0.0, 0.0), "Steering (Wheel): " + FloatToString(rawSteer), gameDebugViewETextAlignment.Left, new Color(Cast<Uint8>(0), Cast<Uint8>(255), Cast<Uint8>(0), Cast<Uint8>(255)));
      GameInstance.GetDebugVisualizerSystem(v.GetGame()).SetScale(this.debugLayer1, new Vector4(1.0, 1.0, 0.0, 0.0));
      this.debugLayer2 = GameInstance.GetDebugVisualizerSystem(v.GetGame()).DrawText(new Vector4(20.0, 40.0, 0.0, 0.0), "Steering (Game):  " + FloatToString(gameSteer), gameDebugViewETextAlignment.Left, new Color(Cast<Uint8>(0), Cast<Uint8>(255), Cast<Uint8>(255), Cast<Uint8>(255)));
      GameInstance.GetDebugVisualizerSystem(v.GetGame()).SetScale(this.debugLayer2, new Vector4(1.0, 1.0, 0.0, 0.0));
      this.hasDrawnDebug = true;
    } else if this.hasDrawnDebug {
      GameInstance.GetDebugVisualizerSystem(v.GetGame()).ClearLayer(this.debugLayer1);
      GameInstance.GetDebugVisualizerSystem(v.GetGame()).ClearLayer(this.debugLayer2);
      this.hasDrawnDebug = false;
    }

    // Vehicle origin sits near the chassis centre. Starting the ray
    // above the origin means the first hit is often the car's own
    // body (default_material.physmat), producing an alternating
    // pattern in the log. Starting BELOW the chassis (pos.Z − 0.4)
    // clears the vehicle body for any sensible car geometry and ends
    // on the road surface directly.
    let pos: Vector4 = v.GetWorldPosition();
    let start: Vector4 = new Vector4(pos.X, pos.Y, pos.Z - 0.40, 1.0);
    let end:   Vector4 = new Vector4(pos.X, pos.Y, pos.Z - 2.50, 1.0);

    let result: TraceResult;
    if sqs.SyncRaycastByCollisionGroup(start, end, n"Static", result, true, false) {
      let mat: CName = result.material;
      // Treat `default_material.physmat` as a "no real data" hit —
      // it's what CDPR returns for meshes without an explicit physics
      // material, including the car's own body in edge cases. Keeps
      // the surface state pinned to the last known real material.
      if Equals(mat, n"default_material.physmat") {
        this.ScheduleNext();
        return;
      }
      if !Equals(mat, this.lastMaterial) {
        this.lastMaterial = mat;
        DirectWheel_OnWheelMaterial(0, mat);
      }
    }

    this.ScheduleNext();
  }
}

public class DirectWheelSurfaceTickCallback extends DelayCallback {
  private let poller: wref<DirectWheelSurfacePoller>;

  public static func Create(p: ref<DirectWheelSurfacePoller>) -> ref<DirectWheelSurfaceTickCallback> {
    let cb = new DirectWheelSurfaceTickCallback();
    cb.poller = p;
    return cb;
  }

  public func Call() -> Void {
    let p = this.poller;
    if IsDefined(p) {
      p.Tick();
    }
  }
}

@wrapMethod(VehicleComponent)
protected cb func OnVehicleFinishedMountingEvent(evt: ref<VehicleFinishedMountingEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let character: ref<GameObject> = evt.character as GameObject;
  if IsDefined(character) && character.IsPlayer() && evt.isMounting {
    if !IsDefined(this.m_direct_wheelSurfacePoller) {
      this.m_direct_wheelSurfacePoller = new DirectWheelSurfacePoller();
    }
    this.m_direct_wheelSurfacePoller.Start(this.GetVehicle());
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
    if IsDefined(this.m_direct_wheelSurfacePoller) {
      this.m_direct_wheelSurfacePoller.Stop();
    }
  }
  return result;
}
