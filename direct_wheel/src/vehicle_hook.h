#pragma once

#include <cstdint>

namespace direct_wheel::vehicle_hook
{
    // Attach the vehicle-input detour. Uses RED4ext's UniversalReloc hash
    // resolver (address database maintained in RED4ext.dll, updated by the
    // RED4ext maintainers per game patch). Returns true iff the hook
    // attached successfully; returns false if RED4ext's hooking API is
    // unavailable or Attach failed. A failed hash resolution terminates
    // the game process with a RED4ext-authored MessageBox - the game
    // won't launch at all until sigs / hashes are correct for the build.
    bool Init();

    // Detach the hook.
    void Shutdown();

    // True iff the detour is currently attached.
    bool IsInstalled();

    // Monotonic counter of how many times the detour has fired since
    // attach. Useful for confirming the hook is live in-car.
    uint64_t FireCount();

    // Set the pointer of the player's currently-mounted vehicle. The
    // detour only writes into `self` when it matches this pointer —
    // without this filter, UpdateVehicleCameraInput fires for every
    // visible vehicle each tick and our writes propagate to all of
    // them. Called from the mount/unmount redscript event wrappers
    // via the DirectWheel_Set/ClearPlayerVehicle natives. Passing nullptr
    // disables all injection (no player-driven vehicle).
    void SetPlayerVehicle(void* p);

    // True iff `p` is the currently-cached player vehicle. Used by the
    // collision/bump natives to gate jolts — the events fire for every
    // visible vehicle (e.g. NPC traffic crashing off-screen) but only
    // the player's hits should reach the wheel.
    bool IsPlayerVehicle(void* p);

    // Read accessor for the cached player-vehicle pointer. Used by the
    // direct-action dispatcher in input_bindings to invoke vehicleBaseObject
    // methods (ToggleRadioReceiver, NextRadioReceiverStation, ToggleSiren,
    // etc.) without going through CP2077's keyboard binding layer. nullptr
    // when not in a vehicle.
    void* GetPlayerVehicle();

    // Called by the DirectWheel_SampleWheelMaterials native (redscript-side
    // PersistencySystem polling forwards the `vehiclePersistentDataPS`
    // handle each tick). Reads the 4 per-wheel CName hashes at offset
    // 0x78 and logs transitions. `psPointer` is the raw instance ptr
    // from the redscript Handle; nullptr is ignored.
    void OnPersistentStateSample(void* psPointer);
}
