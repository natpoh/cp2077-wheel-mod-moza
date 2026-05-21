// Collision / bump feedback for the direct_wheel plugin.
//
// @wrapMethod(VehicleObject) OnVehicleBumpEvent is CDPR's only
// VehicleObject-owned collision event (see
// scripts/core/gameplay/vehicles.script:715). It fires on scrapes /
// nudges / traffic bumps / full-speed crashes, and the event carries:
//   impactVelocityChange : Float    — Δv magnitude for this contact
//   hitNormal            : Vector3  — world-space contact normal
//   hitVehicle           : weak<VehicleObject>  — the OTHER vehicle
//   isInTraffic          : Bool
//
// Empirical finding (2026-04-23 log): CDPR dispatches this event to
// the vehicle that got STRUCK, not the one doing the striking. So
// when the player rams an NPC, `this` inside the handler is the
// NPC's VehicleObject — not the player's — and `evt.hitVehicle` is
// the player's. We forward the event to C++ whenever either side
// matches the player's currently-mounted vehicle.
//
// The kick has to be computed in the PLAYER'S world frame (since
// hitNormal is world-space and the wheel responds to
// left/right-relative-to-the-driver), not in `this`'s frame. If the
// player is the striker (hitVehicle=player), we flip the normal
// because the game reports it relative to the struck vehicle.

@wrapMethod(VehicleObject)
protected cb func OnVehicleBumpEvent(evt: ref<VehicleBumpEvent>) -> Bool {
  let result: Bool = wrappedMethod(evt);

  let intensity: Float = ClampF(AbsF(evt.impactVelocityChange) / 20.0, 0.0, 1.0);
  if intensity <= 0.0 {
    return result;
  }

  // Resolve the player's currently-mounted vehicle. If the player is
  // on-foot or in a non-VehicleObject (tank?), skip.
  let player: ref<PlayerPuppet> = GetPlayer(this.GetGame());
  if !IsDefined(player) {
    return result;
  }
  let playerVeh: ref<VehicleObject> = player.GetMountedVehicle() as VehicleObject;
  if !IsDefined(playerVeh) {
    return result;
  }

  // Figure out which side the player is on: striker or struck.
  // hitNormal is reported in the frame of `this` (the struck entity),
  // pointing out of the surface; if the player is the striker, the
  // normal already faces the player correctly. If the player IS the
  // struck, the normal faces away from the player and we flip it.
  let normalSign: Float = 0.0;
  let other: ref<VehicleObject> = evt.hitVehicle as VehicleObject;
  if Equals(this, playerVeh) {
    normalSign = -1.0;                 // `this` = player = struck; flip
  } else if IsDefined(other) && Equals(other, playerVeh) {
    normalSign = 1.0;                  // `this` = NPC struck by player
  } else {
    return result;                     // NPC ↔ NPC traffic, discard
  }

  // Kick is computed in the PLAYER'S frame regardless of which
  // VehicleObject received the event. Negative dot = impact on the
  // player's left → wheel rim jerks to the right (chassis rotates
  // around the contact, front tyres scrub away from the impact).
  let pr: Vector4 = playerVeh.GetWorldRight();
  let lateral: Float = pr.X * evt.hitNormal.X
                     + pr.Y * evt.hitNormal.Y
                     + pr.Z * evt.hitNormal.Z;
  let kick: Float = ClampF(-normalSign * lateral * intensity, -1.0, 1.0);
  DirectWheel_OnVehicleBump(playerVeh, kick);

  return result;
}
