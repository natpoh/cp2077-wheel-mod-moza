#include "rtti.h"
#include "config.h"
#include "wheel.h"
#include "input_bindings.h"
#include "vehicle_hook.h"
#include "plugin.h"
#include "logging.h"
#include "rtti_dump.h"
#include "rtti_offsets.h"
#include "sources.h"
#include "device_table.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/TweakDB.hpp>

#include <cstdio>
#include <string>

namespace direct_wheel::rtti
{
    namespace
    {
        std::string ReadString(RED4ext::CStackFrame* aFrame)
        {
            RED4ext::CString s;
            RED4ext::GetParameter(aFrame, &s);
            return std::string(s.c_str());
        }

        // -------- Read-only natives -----------------------------------------

        void GetVersion(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(kVersionString);
        }

        void IsPluginReady(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = wheel::IsReady();
        }

        void GetDeviceInfo(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (!aOut) return;
            if (!wheel::IsReady())
            {
                *aOut = RED4ext::CString("no wheel connected (Logitech SDK has not bound a device yet)");
                return;
            }
            const auto& caps = wheel::GetCaps();
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "%s (%d deg, FFB=%s, SDK=%d.%d.%d) -> hook:%s fireCount=%llu",
                          caps.productName,
                          caps.operatingRangeDeg,
                          caps.hasFFB ? "yes" : "no",
                          caps.sdkMajor, caps.sdkMinor, caps.sdkBuild,
                          vehicle_hook::IsInstalled() ? "installed" : "not-installed",
                          static_cast<unsigned long long>(vehicle_hook::FireCount()));
            *aOut = RED4ext::CString(buf);
        }

        void HasFFB(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = wheel::IsReady() && wheel::GetCaps().hasFFB;
        }

        // Wheel-model auto-discovery natives. Read by direct_wheel_settings.reds at
        // OnGameAttached and again at PauseMenuGameController.OnInitialize to
        // drive ModSettings module registration. Strict default: returns FALSE
        // when no wheel is bound, so the settings page hides every wheel-
        // specific section until a Logitech wheel binds via the SDK. Unknown
        // (non-tabled) PIDs DO get the permissive treatment so the UI doesn't
        // collapse when a new Logitech wheel ships that we haven't catalogued.

        bool DetectedHasRightClusterImpl()
        {
            if (!wheel::IsReady()) return false;
            const auto& caps = wheel::GetCaps();
            const auto* info = LookupByPid(caps.pid);
            if (!info) return true;  // unknown PID: permissive
            return info->has_right_cluster;
        }

        bool DetectedHasFfbHardwareImpl()
        {
            if (!wheel::IsReady()) return false;
            const auto& caps = wheel::GetCaps();
            // Prefer the live SDK report (caps.hasFFB) over the table flag.
            // The SDK knows whether THIS bound device's firmware reported FFB
            // capability; the table is the historical default. Fall back to
            // the table for unknown PIDs where the SDK wasn't queried.
            if (caps.hasFFB) return true;
            const auto* info = LookupByPid(caps.pid);
            if (!info) return true;  // unknown PID: permissive
            return info->ffb_default;
        }

        bool DetectedHasRevLedsImpl()
        {
            if (!wheel::IsReady()) return false;
            const auto& caps = wheel::GetCaps();
            const auto* info = LookupByPid(caps.pid);
            if (!info) return true;  // unknown PID: permissive
            return info->has_rev_leds;
        }

        void DetectedHasRightCluster(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasRightClusterImpl();
        }

        void DetectedHasFfbHardware(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasFfbHardwareImpl();
        }

        void DetectedHasRevLeds(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = DetectedHasRevLedsImpl();
        }

        void DetectedModelName(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (!aOut) return;
            if (!wheel::IsReady()) { *aOut = RED4ext::CString("(no wheel bound)"); return; }
            const auto& caps = wheel::GetCaps();
            const auto* info = LookupByPid(caps.pid);
            if (!info) { *aOut = RED4ext::CString(caps.productName); return; }
            *aOut = RED4ext::CString(std::string(info->name).c_str());
        }

        void ReadConfig(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(config::ReadAsJson().c_str());
        }

        // -------- Menu lifecycle (no-op stubs) --------------------------------
        // direct_wheel_menu.reds calls these when gameplay-blocking menus
        // open/close. Currently used only for logging; the plugin does not
        // suppress input while in menus (the mount/unmount gate is sufficient).

        void MenuOpen(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto tag = ReadString(aFrame); aFrame->code++;
            log::InfoF("[direct_wheel:menu] open: %s", tag.c_str());
            if (aOut) *aOut = true;
        }

        void MenuClose(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto tag = ReadString(aFrame); aFrame->code++;
            log::InfoF("[direct_wheel:menu] close: %s", tag.c_str());
            if (aOut) *aOut = true;
        }

        // -------- Config setters --------------------------------------------

        void SetInputEnabled(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            bool v = false; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            config::SetInputEnabled(v);
            if (aOut) *aOut = true;
        }

        template <void (*Fn)(int32_t)>
        void SetInt(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t v = 0; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            Fn(v);
            if (aOut) *aOut = true;
        }

        template <void (*Fn)(float)>
        void SetFloat(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            float v = 0.f; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            Fn(v);
            if (aOut) *aOut = true;
        }

        template <void (*Fn)(bool)>
        void SetBool(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            bool v = false; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            Fn(v);
            if (aOut) *aOut = true;
        }

        // -------- Input bindings --------------------------------------------

        void SetInputBinding(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t inputId = -1;
            int32_t action = 0;
            RED4ext::GetParameter(aFrame, &inputId);
            RED4ext::GetParameter(aFrame, &action);
            aFrame->code++;
            config::SetInputBinding(inputId, action);
            if (aOut) *aOut = true;
        }

        // -------- Player-vehicle mount tracking -----------------------------
        //
        // Called from redscript VehicleComponent mount/unmount wrappers so
        // the hook knows which vehicle is "the one the player is driving"
        // and doesn't write inputs into all the other vehicles the
        // UpdateVehicleCameraInput detour fires on each tick.

        void SetPlayerVehicle(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::ISerializable> handle;
            RED4ext::GetParameter(aFrame, &handle);
            aFrame->code++;
            void* ptr = static_cast<void*>(handle.instance);
            vehicle_hook::SetPlayerVehicle(ptr);
            if (aOut) *aOut = true;
        }

        void ClearPlayerVehicle(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            vehicle_hook::SetPlayerVehicle(nullptr);
            if (aOut) *aOut = true;
        }

        // Vehicle telemetry pushed from redscript Blackboard listeners.
        // See direct_wheel_vehicle_signals.reds.
        void SetEngineRpmNormalized(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            float v = 0.f;
            RED4ext::GetParameter(aFrame, &v);
            aFrame->code++;
            sources::SetEngineRpmNormalized(v);
            if (aOut) *aOut = true;
        }

        void SetRadioActive(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            bool v = false;
            RED4ext::GetParameter(aFrame, &v);
            aFrame->code++;
            sources::SetRadioActive(v);
            if (aOut) *aOut = true;
        }

        // -------- Collision / bump feedback natives -------------------------
        //
        // Called from redscript @wrapMethod handlers on VehicleObject
        // collision events (see direct_wheel_reds/direct_wheel_events.reds). Each
        // native receives the vehicle handle + a signed lateral kick
        // in [-1..+1] (negative = left-side hit, positive = right-side).
        // The vehicle handle lets us filter out events from NPC cars /
        // traffic — only the player's vehicle feeds the wheel.

        void OnVehicleBump(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::ISerializable> handle;
            float kick = 0.f;
            RED4ext::GetParameter(aFrame, &handle);
            RED4ext::GetParameter(aFrame, &kick);
            aFrame->code++;
            // Always log + always trigger — the .reds wrapper already
            // filters for player vehicle. Ensure kick has minimum magnitude.
            float absKick = std::max(std::abs(kick), 0.5f);
            float signedKick = kick >= 0.f ? absKick : -absKick;
            log::InfoF("[direct_wheel:evt] bump: kick=%+.3f (boosted=%+.3f)", kick, signedKick);
            wheel::TriggerJolt(signedKick, 200);
            if (aOut) *aOut = true;
        }

        void OnVehicleHit(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            RED4ext::Handle<RED4ext::ISerializable> handle;
            float kick = 0.f;
            RED4ext::GetParameter(aFrame, &handle);
            RED4ext::GetParameter(aFrame, &kick);
            aFrame->code++;
            float absKick = std::max(std::abs(kick), 0.7f);
            float signedKick = kick >= 0.f ? absKick : -absKick;
            log::InfoF("[direct_wheel:evt] hit: kick=%+.3f (boosted=%+.3f)", kick, signedKick);
            wheel::TriggerJolt(signedKick, 400);
            if (aOut) *aOut = true;
        }

        // Map a material CName hash to a baseline road-surface SINE
        // magnitude (0..1). Hashes captured from 2026-04-24 log.
        //
        // All-zero for now. A non-zero baseline plays a constant 5.5 Hz
        // SINE regardless of physics activity, which feels like the wheel
        // is oscillating nonstop — confirmed by user on 2026-04-24 after
        // first pass with metal=0.12 / unknown=0.10. Re-introduce values
        // only when we've (a) captured dirt / gravel CNames to tune
        // against and (b) got reliable per-tick material reads (no more
        // alternating hits on the car body).
        float SurfaceBaselineForMaterial(uint64_t /*hash*/)
        {
            return 0.f;
        }

        // Per-wheel material report from the redscript raycast poller
        // (see direct_wheel_reds/direct_wheel_surface.reds). wheelIdx is 0..3
        // (FL/FR/RL/RR by convention), material is the CName of the
        // physics material under that wheel's raycast hit. Called
        // on transitions only.
        void OnWheelMaterial(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t idx = -1;
            RED4ext::CName mat;
            RED4ext::GetParameter(aFrame, &idx);
            RED4ext::GetParameter(aFrame, &mat);
            aFrame->code++;
            const char* name = RED4ext::CNamePool::Get(mat);
            const float baseline = SurfaceBaselineForMaterial(mat.hash);
            wheel::SetSurfaceBaselineMag(baseline);
            log::InfoF("[direct_wheel:surface] wheel[%d] material: %s (hash=0x%016llX) baseline=%.2f",
                       idx,
                       name ? name : "(null)",
                       static_cast<unsigned long long>(mat.hash),
                       baseline);
            if (aOut) *aOut = true;
        }

        // -------- Registration ----------------------------------------------

        using FuncFlags = RED4ext::CBaseFunction::Flags;

        void RegisterTypes() {}

        void RegisterGlobal(RED4ext::CRTTISystem* rtti,
                            const char* name,
                            RED4ext::ScriptingFunction_t<void*> fn,
                            const char* returnType,
                            std::initializer_list<std::pair<const char*, const char*>> params)
        {
            auto func = RED4ext::CGlobalFunction::Create(name, name, fn);
            func->flags = FuncFlags{ .isNative = true, .isStatic = true };
            if (returnType && *returnType) func->SetReturnType(returnType);
            for (const auto& p : params) func->AddParam(p.first, p.second);
            rtti->RegisterFunction(func);
        }

        void PostRegisterTypes()
        {
            auto rtti = RED4ext::CRTTISystem::Get();

            RegisterGlobal(rtti, "DirectWheel_GetVersion",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetVersion),
                           "String", {});
            RegisterGlobal(rtti, "DirectWheel_IsPluginReady",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&IsPluginReady),
                           "Bool", {});
            RegisterGlobal(rtti, "DirectWheel_GetDeviceInfo",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetDeviceInfo),
                           "String", {});
            RegisterGlobal(rtti, "DirectWheel_HasFFB",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&HasFFB),
                           "Bool", {});
            RegisterGlobal(rtti, "DirectWheel_DetectedHasRightCluster",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasRightCluster),
                           "Bool", {});
            RegisterGlobal(rtti, "DirectWheel_DetectedHasFfbHardware",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasFfbHardware),
                           "Bool", {});
            RegisterGlobal(rtti, "DirectWheel_DetectedHasRevLeds",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedHasRevLeds),
                           "Bool", {});
            RegisterGlobal(rtti, "DirectWheel_DetectedModelName",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DetectedModelName),
                           "String", {});
            RegisterGlobal(rtti, "DirectWheel_ReadConfig",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ReadConfig),
                           "String", {});

            RegisterGlobal(rtti, "DirectWheel_SetInputEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputEnabled),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetClutchAsBrake",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetClutchAsBrake>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "DirectWheel_SetFfbEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetFfbDebugLogging",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetFfbDebugLogging>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetFfbTorquePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetFfbTorquePct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetHandshakePlayOnStart",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetHandshakePlayOnStart>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "DirectWheel_SetStationaryThresholdMps",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetFloat<&config::SetStationaryThresholdMps>),
                           "Bool", {{ "Float", "mps" }});
            RegisterGlobal(rtti, "DirectWheel_SetYawFeedbackPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetYawFeedbackPct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetActiveTorqueStrengthPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetActiveTorqueStrengthPct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetConstantForcePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetConstantForcePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetSpringForcePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSpringForcePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetDamperForcePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetDamperForcePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetFrictionForcePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetFrictionForcePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetSineForcePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSineForcePct>),
                           "Bool", {{ "Int32", "pct" }});
            RegisterGlobal(rtti, "DirectWheel_SetJoltForcePct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetJoltForcePct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetSpeedSensitiveSteeringPct",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSpeedSensitiveSteeringPct>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetSteeringSpeedFactor",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringSpeedFactor>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetSteeringMinTurn",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringMinTurn>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetSteeringAddBoost",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringAddBoost>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetSteeringSubBoost",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringSubBoost>),
                           "Bool", {{ "Int32", "pct" }});

            RegisterGlobal(rtti, "DirectWheel_SetLedEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLedEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetLedVisualizerWhileMusic",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLedVisualizerWhileMusic>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "DirectWheel_SetInputBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputBinding),
                           "Bool", {{ "Int32", "inputId" }, { "Int32", "action" }});

            RegisterGlobal(rtti, "DirectWheel_MenuOpen",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&MenuOpen),
                           "Bool", {{ "String", "tag" }});
            RegisterGlobal(rtti, "DirectWheel_MenuClose",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&MenuClose),
                           "Bool", {{ "String", "tag" }});

            RegisterGlobal(rtti, "DirectWheel_SetPlayerVehicle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetPlayerVehicle),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }});
            RegisterGlobal(rtti, "DirectWheel_ClearPlayerVehicle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ClearPlayerVehicle),
                           "Bool", {});

            RegisterGlobal(rtti, "DirectWheel_SetEngineRpmNormalized",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetEngineRpmNormalized),
                           "Bool", {{ "Float", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetRadioActive",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetRadioActive),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "DirectWheel_OnVehicleBump",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnVehicleBump),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }, { "Float", "lateralKick" }});
            RegisterGlobal(rtti, "DirectWheel_OnVehicleHit",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnVehicleHit),
                           "Bool", {{ "handle:vehicleBaseObject", "v" }, { "Float", "lateralKick" }});

            RegisterGlobal(rtti, "DirectWheel_OnWheelMaterial",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&OnWheelMaterial),
                           "Bool", {{ "Int32", "wheelIdx" }, { "CName", "material" }});


            log::Info("[direct_wheel] native functions registered for redscript");

            // Resolve vehicle struct offsets dynamically from RTTI — this
            // sidesteps the "offsets drifted between game patches" problem
            // by asking the game itself where each field lives. See
            // rtti_offsets.cpp for what gets resolved.
            rtti_offsets::Init();

            // BISECT: RTTI dump disabled. Still registering natives above;
            // just skipping the full class enumeration pass.
            log::Info("[direct_wheel] RTTI dump DISABLED this build (bisect)");
        }
    }

    void Register()
    {
        auto rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Error("[direct_wheel] CRTTISystem::Get() returned null - native functions will not be registered.");
            return;
        }
        rtti->AddRegisterCallback(RegisterTypes);
        rtti->AddPostRegisterCallback(PostRegisterTypes);
        log::Debug("[direct_wheel] RTTI register callbacks queued");
    }
}
