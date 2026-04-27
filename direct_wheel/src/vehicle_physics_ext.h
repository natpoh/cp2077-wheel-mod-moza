#pragma once

// Dynamic, RTTI-resolved accessors for vehicle physics state.
//
// Offsets are resolved at plugin load from the game's own CClass::GetProperty
// metadata (see rtti_offsets.{h,cpp}). That makes us patch-robust — whatever
// build CDPR ships, the field offsets come from the build itself. Hardcoded
// fallbacks (captured from LTBF's SDK fork, game 1.52-1.61) are used only
// when RTTI doesn't know a field, and every fallback is logged so we notice
// silent drift.

#include "rtti_offsets.h"

#include <cstdint>
#include <cstddef>

namespace direct_wheel::vehicle_ext
{
    // Hardcoded-fallback offsets. Outer offsets (physics / physicsData on
    // vehicleBaseObject) and PhysicsData inner offsets come from LTBF's SDK
    // fork (new-types branch, game 1.52-1.61) and have held steady into
    // 2.31 — the runtime back-pointer probe in rtti_offsets.cpp validates
    // them each session. Inner WheeledPhysics offsets drifted for 2.31 and
    // were re-discovered via the ProbeInnerWheeledFields scanner —
    // triangulated across car/bike/truck by matching each field to its
    // expected value domain (wheelbase ∈ [1,5]m, numDriveWheels ∈ {2,4},
    // etc.). If CDPR patches the struct layout again, re-enable the inner
    // probe on next vehicle mount and update these constants.
    namespace fallback
    {
        // Outer (vehicleBaseObject) — stable across 1.52 → 2.31.
        inline constexpr std::ptrdiff_t kVehicleBaseObject_physics      = 0x2C8;
        inline constexpr std::ptrdiff_t kVehicleBaseObject_physicsData  = 0x2D0;

        // PhysicsData (base Physics layout) — stable across 1.52 → 2.31.
        inline constexpr std::ptrdiff_t kPhysicsData_velocity           = 0x18;
        inline constexpr std::ptrdiff_t kPhysicsData_angularVelocity    = 0x24;

        // WheeledPhysics inner fields — triangulated for 2.31 (build 5294808).
        // Re-probe if these break on a future patch.
        inline constexpr std::ptrdiff_t kWheeledPhysics_numDriveWheels         = 0xE0;  // was 0xD8 in 1.52-fork
        inline constexpr std::ptrdiff_t kWheeledPhysics_frontBackWheelDistance = 0xE4;  // was 0xDC
        inline constexpr std::ptrdiff_t kWheeledPhysics_turnRate               = 0x114; // was 0xF4
        inline constexpr std::ptrdiff_t kWheeledPhysics_maxWheelTurnDeg        = 0x1A4; // was 0xC10
        inline constexpr std::ptrdiff_t kWheeledPhysics_slipAngleCurveScale    = 0xC78; // still valid; varied per-car in probe
    }

    // Return the runtime offset if RTTI resolved it, else the fallback.
    inline uint32_t ResolvedOr(uint32_t rttiOffset, std::ptrdiff_t fallback)
    {
        return rttiOffset ? rttiOffset : static_cast<uint32_t>(fallback);
    }

    // --- pointer-field accessors -------------------------------------------
    // physics and physicsData are both pointers in the vehicle struct. Read
    // them through the resolved offset. Can return nullptr if the vehicle's
    // physics chain isn't set up yet (brief window during mount).

    inline void* GetPhysicsRaw(void* vehicle)
    {
        const uint32_t off = ResolvedOr(rtti_offsets::VehicleBaseObject_physicsOffset(),
                                        fallback::kVehicleBaseObject_physics);
        return *reinterpret_cast<void**>(static_cast<char*>(vehicle) + off);
    }

    inline void* GetPhysicsDataRaw(void* vehicle)
    {
        const uint32_t off = ResolvedOr(rtti_offsets::VehicleBaseObject_physicsDataOffset(),
                                        fallback::kVehicleBaseObject_physicsData);
        return *reinterpret_cast<void**>(static_cast<char*>(vehicle) + off);
    }

    // --- PhysicsData scalar reads ------------------------------------------

    // Reads Vector3 velocity from physicsData into out[3]. Returns true on
    // success, false if physicsData is null.
    inline bool ReadVelocity(void* vehicle, float out[3])
    {
        void* pd = GetPhysicsDataRaw(vehicle);
        if (!pd) return false;
        const uint32_t off = ResolvedOr(rtti_offsets::PhysicsData_velocityOffset(),
                                        fallback::kPhysicsData_velocity);
        const float* src = reinterpret_cast<const float*>(static_cast<char*>(pd) + off);
        out[0] = src[0]; out[1] = src[1]; out[2] = src[2];
        return true;
    }

    inline bool ReadAngularVelocity(void* vehicle, float out[3])
    {
        void* pd = GetPhysicsDataRaw(vehicle);
        if (!pd) return false;
        const uint32_t off = ResolvedOr(rtti_offsets::PhysicsData_angularVelocityOffset(),
                                        fallback::kPhysicsData_angularVelocity);
        const float* src = reinterpret_cast<const float*>(static_cast<char*>(pd) + off);
        out[0] = src[0]; out[1] = src[1]; out[2] = src[2];
        return true;
    }

    // --- WheeledPhysics scalar reads ---------------------------------------

    struct WheeledPhysicsSnapshot
    {
        float    turnRate               = 0.f;
        float    maxWheelTurnDeg        = 0.f;
        float    slipAngleCurveScale    = 0.f;
        uint32_t numDriveWheels         = 0;
        float    frontBackWheelDistance = 0.f;
        bool     valid                  = false;
    };

    inline WheeledPhysicsSnapshot ReadWheeledPhysics(void* vehicle)
    {
        WheeledPhysicsSnapshot s;
        void* wp = GetPhysicsRaw(vehicle);
        if (!wp) return s;
        const auto at = [&](uint32_t off) -> const char* {
            return static_cast<const char*>(wp) + off;
        };
        s.turnRate               = *reinterpret_cast<const float*>(at(ResolvedOr(
            rtti_offsets::WheeledPhysics_turnRateOffset(),
            fallback::kWheeledPhysics_turnRate)));
        s.maxWheelTurnDeg        = *reinterpret_cast<const float*>(at(ResolvedOr(
            rtti_offsets::WheeledPhysics_maxWheelTurnDegOffset(),
            fallback::kWheeledPhysics_maxWheelTurnDeg)));
        s.slipAngleCurveScale    = *reinterpret_cast<const float*>(at(ResolvedOr(
            rtti_offsets::WheeledPhysics_slipAngleCurveScaleOffset(),
            fallback::kWheeledPhysics_slipAngleCurveScale)));
        s.numDriveWheels         = *reinterpret_cast<const uint32_t*>(at(ResolvedOr(
            rtti_offsets::WheeledPhysics_numDriveWheelsOffset(),
            fallback::kWheeledPhysics_numDriveWheels)));
        s.frontBackWheelDistance = *reinterpret_cast<const float*>(at(ResolvedOr(
            rtti_offsets::WheeledPhysics_frontBackWheelDistanceOffset(),
            fallback::kWheeledPhysics_frontBackWheelDistance)));
        s.valid = true;
        return s;
    }
}
