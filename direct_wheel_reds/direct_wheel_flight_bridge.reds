@if(ModuleExists("LetThereBeFlight"))
module DirectWheel.FlightBridge

import LetThereBeFlight.*

@addField(FlightController)
public let directWheelModeFwdWasActive: Bool;

@addField(FlightController)
public let directWheelModeBwdWasActive: Bool;

@wrapMethod(FlightController)
public func UpdateInputs(timeDelta: Float) {
    wrappedMethod(timeDelta);

    let isModeFwd = DirectWheel_IsActionActive(38); // Flight_ModeSwitchForward
    if isModeFwd && !this.directWheelModeFwdWasActive {
        this.CycleMode(1);
        this.SetupActions();
    }
    this.directWheelModeFwdWasActive = isModeFwd;

    let isModeBwd = DirectWheel_IsActionActive(39); // Flight_ModeSwitchBackward
    if isModeBwd && !this.directWheelModeBwdWasActive {
        this.CycleMode(-1);
        this.SetupActions();
    }
    this.directWheelModeBwdWasActive = isModeBwd;

    if DirectWheel_IsActionActive(40) { // Flight_LiftUp
        this.lift.SetInput(1.0);
    } else if DirectWheel_IsActionActive(41) { // Flight_LiftDown
        this.lift.SetInput(-1.0);
    }

    if DirectWheel_IsActionActive(42) { // Flight_LinearBrake
        this.linearBrake.SetInput(1.0);
    }
    
    if DirectWheel_IsActionActive(43) { // Flight_AngularBrake
        this.angularBrake.SetInput(1.0);
    }

    let steer: Float = DirectWheel_GetDebugRawSteer();
    let throttle: Float = DirectWheel_GetRawThrottle();
    let brake: Float = DirectWheel_GetRawBrake();

    // Map steering to Yaw (turning left/right).
    // In direct_wheel, steer < 0 is left, steer > 0 is right.
    // In LTBF, negative is left, positive is right. So they match.
    // But direct_wheel only exposes the debug raw steer. It usually doesn't have deadzone applied.
    // We can just use the raw steer.
    if AbsF(steer) > 0.05 {
        this.yaw.SetInput(steer);
    }
    
    // Throttle / Brake to Surge (forward / backward speed)
    let surgeInput: Float = throttle - brake;
    if AbsF(surgeInput) > 0.05 {
        this.surge.SetInput(surgeInput);
    }
}

// ----------------------------------------------------------------------------
// Flight_Toggle Hooks into State Machine Transitions
// ----------------------------------------------------------------------------

@if(ModuleExists("LetThereBeFlight"))
@wrapMethod(VehicleFlightEnabledDecisions)
public const func ToVehicleFlightActivating(const stateContext: ref<StateContext>, const scriptInterface: ref<StateGameScriptInterface>) -> Bool {
    if DirectWheel_IsActionActive(37) { // Flight_Toggle
        return true;
    }
    return wrappedMethod(stateContext, scriptInterface);
}

@if(ModuleExists("LetThereBeFlight"))
@wrapMethod(VehicleFlightActiveDecisions)
public const func ToVehicleFlightDeactivating(const stateContext: ref<StateContext>, const scriptInterface: ref<StateGameScriptInterface>) -> Bool {
    if DirectWheel_IsActionActive(37) { // Flight_Toggle
        return true;
    }
    return wrappedMethod(stateContext, scriptInterface);
}
