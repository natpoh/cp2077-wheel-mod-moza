
public class DirectWheelFlightPoller {
    private let m_vehicle: wref<VehicleObject>;
    private let m_running: Bool;

    public func Start(vehicle: ref<VehicleObject>) {
        this.m_vehicle = vehicle;
        this.m_running = true;
        this.QueueNext();
    }

    public func Stop() {
        this.m_running = false;
    }

    private func QueueNext() {
        if !this.m_running { return; }
        if !IsDefined(this.m_vehicle) { return; }
        let req = new DirectWheelFlightPollerEvent();
        GameInstance.GetDelaySystem(this.m_vehicle.GetGame()).DelayScriptableSystemRequestNextFrame(n"DirectWheelFlightSystem", req);
    }

    public func Tick() {
        if !this.m_running { return; }
        if !IsDefined(this.m_vehicle) { return; }

        let fc = FlightController.GetInstance();
        if IsDefined(fc) {
            // Apply inputs if flight is active
            if fc.active {
                if DirectWheel_IsActionActive(38) { // Flight_ModeSwitchForward
                    fc.CycleMode(1);
                    fc.SetupActions();
                }

                if DirectWheel_IsActionActive(39) { // Flight_ModeSwitchBackward
                    fc.CycleMode(-1);
                    fc.SetupActions();
                }

                // Lift: default 0, override if button held
                let liftInput: Float = 0.0;
                if DirectWheel_IsActionActive(40) { // Flight_LiftUp
                    liftInput = 1.0;
                } else if DirectWheel_IsActionActive(41) { // Flight_LiftDown
                    liftInput = -1.0;
                }
                fc.lift.SetInput(liftInput);

                // Brakes: default 0, override if held
                let linBrake: Float = 0.0;
                if DirectWheel_IsActionActive(42) { // Flight_LinearBrake
                    linBrake = 1.0;
                }
                fc.linearBrake.SetInput(linBrake);

                let angBrake: Float = 0.0;
                if DirectWheel_IsActionActive(43) { // Flight_AngularBrake
                    angBrake = 1.0;
                }
                fc.angularBrake.SetInput(angBrake);

                // Pitch: default 0, override if held
                let pitchInput: Float = 0.0;
                if DirectWheel_IsActionActive(44) { // Flight_PitchForward
                    pitchInput = 1.0;
                } else if DirectWheel_IsActionActive(45) { // Flight_PitchBackward
                    pitchInput = -1.0;
                }
                fc.pitch.SetInput(pitchInput);

                // Roll: default 0, override if held
                let rollInput: Float = 0.0;
                if DirectWheel_IsActionActive(46) { // Flight_RollLeft
                    rollInput = -1.0;
                } else if DirectWheel_IsActionActive(47) { // Flight_RollRight
                    rollInput = 1.0;
                }
                fc.roll.SetInput(rollInput);

                // Yaw from steering wheel
                let steer: Float = DirectWheel_GetDebugRawSteer();
                fc.yaw.SetInput(steer);

                // Surge from pedals (throttle - brake)
                let throttle: Float = DirectWheel_GetRawThrottle();
                let brake: Float = DirectWheel_GetRawBrake();
                fc.surge.SetInput(throttle - brake);
            }
        }

        this.QueueNext();
    }
}

public class DirectWheelFlightSystem extends ScriptableSystem {
    private let m_poller: ref<DirectWheelFlightPoller>;

    private func OnFlightPollerEvent(request: ref<DirectWheelFlightPollerEvent>) {
        if IsDefined(this.m_poller) {
            this.m_poller.Tick();
        }
    }

    public func SetPoller(poller: ref<DirectWheelFlightPoller>) {
        this.m_poller = poller;
    }
}

public class DirectWheelFlightPollerEvent extends ScriptableSystemRequest {}

@addField(VehicleComponent)
private let m_direct_wheelFlightPoller: ref<DirectWheelFlightPoller>;

@wrapMethod(VehicleComponent)
protected cb func OnVehicleFinishedMountingEvent(evt: ref<VehicleFinishedMountingEvent>) -> Bool {
    let result: Bool = wrappedMethod(evt);
    let character: ref<GameObject> = evt.character as GameObject;
    if IsDefined(character) && character.IsPlayer() && evt.isMounting {
        if !IsDefined(this.m_direct_wheelFlightPoller) {
            this.m_direct_wheelFlightPoller = new DirectWheelFlightPoller();
            let sys = GameInstance.GetScriptableSystemsContainer(this.GetVehicle().GetGame()).Get(n"DirectWheelFlightSystem") as DirectWheelFlightSystem;
            if IsDefined(sys) {
                sys.SetPoller(this.m_direct_wheelFlightPoller);
            }
        }
        this.m_direct_wheelFlightPoller.Start(this.GetVehicle());
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
        if IsDefined(this.m_direct_wheelFlightPoller) {
            this.m_direct_wheelFlightPoller.Stop();
        }
    }
    return result;
}
