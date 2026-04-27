#pragma once

// Self-discovering struct offsets via the game's own RTTI system.
//
// (Header includes std::initializer_list for the dump helpers below.)
//
// CP2077's scripting RTTI registers every script-visible field on every
// script-visible class as a CProperty with a valueOffset. Instead of
// hardcoding offsets (which drift between patches and across SDK forks),
// we look them up by name at plugin load: `vehicleBaseObject.physicsData`,
// `vehicleWheeledPhysics.turnRate`, etc. The results are cached in
// statics and exposed via small accessors.
//
// Missing offsets (e.g., a field that isn't RTTI-exposed on this build)
// return 0 from the accessor; callers should gate on that and fall back
// to whatever behaviour makes sense when the signal is unavailable.

#include <cstdint>
#include <initializer_list>

namespace direct_wheel::rtti_offsets
{
    // Resolve every offset we care about by querying the RTTI system.
    // Safe to call multiple times; subsequent calls are no-ops. Must be
    // called after the RTTI registration phase has completed (typically
    // from a PostRegisterTypes callback). Logs every lookup result.
    void Init();

    // True once Init() has resolved at least the critical offsets.
    bool IsReady();

    // vehicle::BaseObject fields — parent of every ground vehicle.
    uint32_t VehicleBaseObject_physicsOffset();     // Physics* (WheeledPhysics*/CarPhysics*/BikePhysics*/TankPhysics*)
    uint32_t VehicleBaseObject_physicsDataOffset(); // PhysicsData*

    // vehicle::PhysicsData fields — world-space physics state of the vehicle.
    uint32_t PhysicsData_velocityOffset();         // Vector3 linear velocity (m/s)
    uint32_t PhysicsData_angularVelocityOffset();  // Vector3 angular velocity (rad/s)

    // vehicle::WheeledPhysics fields — per-car handling tuning values.
    uint32_t WheeledPhysics_turnRateOffset();            // float — steering responsiveness
    uint32_t WheeledPhysics_maxWheelTurnDegOffset();     // float — max wheel turn angle (degrees)
    uint32_t WheeledPhysics_slipAngleCurveScaleOffset(); // float — slip curve scale
    uint32_t WheeledPhysics_numDriveWheelsOffset();      // uint32_t — drive wheel count
    uint32_t WheeledPhysics_frontBackWheelDistanceOffset(); // float — wheelbase (m)

    // Dump the full property list of a class to the log, one line per field,
    // with the resolved offset + type. Useful on first-vehicle encounter to
    // discover what's actually exposed on the current build.
    void DumpClassProperties(const char* className);

    // Dump the full method (function) list of a class to the log, one line
    // per method, with name + parameter / return types. Used to discover
    // direct-invoke entry points that bypass the keyboard layer (e.g. an
    // "OnVehicleReverseCam" callback we can call by RTTI name).
    void DumpClassFunctions(const char* className);

    // Sweep the global RTTI namespace and log every class whose name
    // contains *any* of the given case-insensitive substrings. For each
    // match, log the class name + size. Used to discover camera-related
    // and input-action-related classes whose names we don't already know.
    void DumpClassesContaining(std::initializer_list<const char*> substrings);

    // Sweep the global RTTI namespace and log every *function* (method on
    // any class) whose name contains any of the given substrings. Catches
    // OnVehicleReverseCam-style handlers we couldn't find via class lookup.
    void DumpFunctionsContaining(std::initializer_list<const char*> substrings);

    // --- Runtime struct-offset probe ---------------------------------------
    //
    // Backfills the physics / physicsData pointer offsets + their inner
    // back-pointer offsets by correlation from a live `vehicleBaseObject*`.
    //
    // Strategy: scan pointer-aligned slots in vehicleBaseObject looking for
    // candidates that dereference to a region which *itself* contains a
    // pointer back to the same vehicleBaseObject. That doubly-linked
    // structure is how PhysicsData and Physics are wired to their owner —
    // self-validating against any layout the game ships.
    //
    // Idempotent: first success sets the cached offsets, subsequent calls
    // are no-ops. Returns true once the correlation has succeeded at least
    // once for any vehicle.
    //
    // Must be called from the main thread (or any thread that can safely
    // dereference game heap pointers). Failure is logged but non-fatal —
    // the fallback offsets from LTBF's SDK fork remain in use.
    bool ProbeStructOffsets(void* vehicle);

    // True once ProbeStructOffsets has succeeded at least once.
    bool StructProbeDone();

    // --- Inner WheeledPhysics field probe -----------------------------------
    //
    // Scans the WheeledPhysics struct for values matching the semantic
    // expectations of fields we want to read:
    //   numDriveWheels — uint32 equal to 2 or 4
    //   wheelbase      — float in [1.0, 5.0] (meters)
    //   turnRate       — float in [0.5, 100.0]
    //
    // Logs every candidate offset. Across multiple vehicles the offset that
    // consistently produces a semantically valid value is the real field.
    // This is a scanning/diagnostic pass — it doesn't mutate any cached
    // offsets. Callers (or operators reading the log) identify consistent
    // offsets and hardcode them afterwards.
    //
    // Called on first-tick per unique vehicle so a short drive across
    // several rides yields enough evidence to triangulate.
    void ProbeInnerWheeledFields(void* wheeledPhysics);
}
