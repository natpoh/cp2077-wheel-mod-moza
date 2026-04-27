// Track the player's currently-mounted vehicle for the direct_wheel plugin.
//
// The plugin's C++ side hooks vehicle::BaseObject::UpdateVehicleCameraInput,
// which fires each tick for EVERY visible vehicle — not just the one the
// player is driving. Without a gate, the plugin's steer/throttle/brake
// injections write into parked cars, NPC traffic, etc., which produces
// "remote driving" of every car on screen. These wrappers catch the two
// mount lifecycle events we care about and notify the plugin which
// vehicle (if any) should actually receive wheel input — the plugin
// also flips its in-vehicle context flag off this signal so on-foot
// dispatches can suppress vehicle-centric actions.
//
// - OnVehicleFinishedMountingEvent fires after the mount animation
//   completes and the physics path is live — the right moment to engage
//   input injection.
// - OnUnmountingEvent fires at the start of the dismount sequence — we
//   disengage before V starts the exit animation so the last injection
//   tick doesn't carry over into the parked state.
//
// Both events also fire for NPCs mounting/dismounting AI vehicles; we
// filter to the local player via IsPlayer() on the mounting character.

@wrapMethod(VehicleComponent)
protected cb func OnVehicleFinishedMountingEvent(evt: ref<VehicleFinishedMountingEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);
  let character: ref<GameObject> = evt.character as GameObject;
  if IsDefined(character) && character.IsPlayer() && evt.isMounting {
    DirectWheel_SetPlayerVehicle(this.GetVehicle());
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
    DirectWheel_ClearPlayerVehicle();
  }
  return result;
}
