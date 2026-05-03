#include "rtti.h"
#include "config.h"
#include "wheel.h"
#include "input_bindings.h"
#include "vehicle_hook.h"
#include "debug_steer.h"
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

        void GetDebugRawSteer(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = g_debugRawSteer.load(std::memory_order_relaxed);
        }

        void GetDebugWheelSteer(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, float* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = g_debugWheelSteer.load(std::memory_order_relaxed);
        }

        void IsDebugLoggingEnabled(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = config::Current().ffb.debugLogging;
        }

        // -------- Steering response getters ---------------------------------
        // Used by the mount event handler in redscript to read the current
        // slider values for TweakDB application.

        void GetSteeringTurnSpeedIdxNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, int32_t* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = config::Current().input.steeringTurnSpeedIdx;
        }

        void GetSteeringRecenterSpeedIdxNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, int32_t* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = config::Current().input.steeringRecenterSpeedIdx;
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

        // -------- Axis mapping natives ----------------------------------------
        void SetAxisSteerNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto v = ReadString(aFrame); aFrame->code++;
            config::SetAxisSteer(v);
            if (aOut) *aOut = true;
        }
        void SetAxisThrottleNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto v = ReadString(aFrame); aFrame->code++;
            config::SetAxisThrottle(v);
            if (aOut) *aOut = true;
        }
        void SetAxisBrakeNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto v = ReadString(aFrame); aFrame->code++;
            config::SetAxisBrake(v);
            if (aOut) *aOut = true;
        }
        void SetAxisClutchNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto v = ReadString(aFrame); aFrame->code++;
            config::SetAxisClutch(v);
            if (aOut) *aOut = true;
        }

        // -------- Device picker natives -------------------------------------
        // Used by the in-game settings page to build wheel/pedal selectors.

        // Returns a pipe-separated list of all attached HID game controllers,
        // e.g. "Moza KS Wheel|G Pro Racing Pedals|vJoy Device".
        // Split on '|' in redscript to populate a dropdown.
        void GetConnectedDeviceList(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, RED4ext::CString* aOut, int64_t)
        {
            aFrame->code++;
            if (aOut) *aOut = RED4ext::CString(wheel::GetConnectedDeviceList().c_str());
        }

        void SetWheelDeviceName(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto v = ReadString(aFrame); aFrame->code++;
            config::SetWheelDeviceName(v);
            wheel::ResetDevices(); // take effect immediately without game restart
            if (aOut) *aOut = true;
        }

        void SetPedalDeviceName(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            auto v = ReadString(aFrame); aFrame->code++;
            config::SetPedalDeviceName(v);
            wheel::ResetDevices();
            if (aOut) *aOut = true;
        }

        // Index-based device selection for Mod Settings sliders.
        // 0 = Auto (use heuristic), 1..N = pick device by index from enumerated list.
        void SetWheelDeviceIndex(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t idx = 0;
            RED4ext::GetParameter(aFrame, &idx);
            aFrame->code++;
            if (idx <= 0) {
                config::SetWheelDeviceName("");
            } else {
                auto list = wheel::GetConnectedDeviceList();
                // split by '|'
                std::vector<std::string> names;
                size_t pos = 0;
                while (pos < list.size()) {
                    auto sep = list.find('|', pos);
                    names.push_back(list.substr(pos, sep - pos));
                    if (sep == std::string::npos) break;
                    pos = sep + 1;
                }
                if (idx <= (int32_t)names.size()) {
                    config::SetWheelDeviceName(names[idx - 1]);
                    log::InfoF("[direct_wheel] Wheel device set to index %d: \"%s\"", idx, names[idx - 1].c_str());
                }
            }
            if (aOut) *aOut = true;
        }

        void SetPedalDeviceIndex(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t idx = 0;
            RED4ext::GetParameter(aFrame, &idx);
            aFrame->code++;
            if (idx <= 0) {
                config::SetPedalDeviceName("");
            } else {
                auto list = wheel::GetConnectedDeviceList();
                std::vector<std::string> names;
                size_t pos = 0;
                while (pos < list.size()) {
                    auto sep = list.find('|', pos);
                    names.push_back(list.substr(pos, sep - pos));
                    if (sep == std::string::npos) break;
                    pos = sep + 1;
                }
                if (idx <= (int32_t)names.size()) {
                    config::SetPedalDeviceName(names[idx - 1]);
                    log::InfoF("[direct_wheel] Pedal device set to index %d: \"%s\"", idx, names[idx - 1].c_str());
                }
            }
            if (aOut) *aOut = true;
        }

        void GetDeviceCount(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, int32_t* aOut, int64_t)
        {
            aFrame->code++;
            auto list = wheel::GetConnectedDeviceList();
            if (list.empty()) { if (aOut) *aOut = 0; return; }
            int32_t count = 1;
            for (char c : list) if (c == '|') count++;
            if (aOut) *aOut = count;
        }

        void BeginAxisBindingNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t target = 0;
            RED4ext::GetParameter(aFrame, &target);
            aFrame->code++;
            wheel::BeginAxisBinding(target);
            if (aOut) *aOut = true;
        }

        void ResetDevicesNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            aFrame->code++;
            wheel::ResetDevices();
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

        // -------- TweakDB steering multiplier helpers -------------------------
        //
        // Slider index 0..8 -> geometric multiplier progression.
        // Index 1 = 1.0x (stock), so no TweakDB modification occurs by default.
        static constexpr float kSteeringMultTable[] = {
            0.5f, 1.0f, 1.5f, 3.0f, 10.0f, 50.0f, 100.0f, 500.0f, 1000.0f
        };

        float SteeringMultFromIdx(int32_t idx)
        {
            if (idx < 0 || idx > 8) return 1.0f;
            return kSteeringMultTable[idx];
        }

        // Per-record original values, keyed by TweakDBID.value (uint64_t).
        // Populated once at ApplySteeringMultAllRecords() on game init.
        struct OrigVals { float add; float sub; };
        static std::unordered_map<uint64_t, OrigVals> s_origVals;
        static bool s_tweakApplied = false;

        // Write a float flat value by creating a new FlatValue and pointing
        // the flat ID at it. NEVER modify in-place because TweakDB pools
        // flat values — the same FlatValue can back camera FOV, physics
        // constants, etc. Mutating it corrupts unrelated systems.
        //
        // CRITICAL: AddFlat uses SortedUniqueArray::Insert which SKIPS
        // duplicates. We must RemoveFlat first so the new offset sticks.
        bool TweakDBSetFloat(RED4ext::TweakDB* tdb, RED4ext::TweakDBID flatId, float newVal)
        {
            auto* rtti = RED4ext::CRTTISystem::Get();
            auto* floatType = rtti->GetType("Float");
            RED4ext::CStackType stackVal;
            stackVal.type = floatType;
            stackVal.value = &newVal;
            int32_t newOffset = tdb->CreateFlatValue(stackVal);
            if (newOffset < 0)
            {
                log::Warn("[direct_wheel:tweak] CreateFlatValue failed");
                return false;
            }
            // Remove old flat entry so Insert doesn't skip it as duplicate
            tdb->RemoveFlat(flatId);
            flatId.SetTDBOffset(newOffset);
            tdb->AddFlat(flatId);
            return true;
        }
    } // end anonymous namespace

    // Apply (or restore) steering multipliers to ALL VehicleDriveModelData records.
    // Called at game init AND whenever the user changes the index slider.
    bool ApplySteeringMultAllRecords()
    {
        auto* tdb = RED4ext::TweakDB::Get();
        if (!tdb) return false;

        auto cfg = config::Current();
        float turnMult     = SteeringMultFromIdx(cfg.input.steeringTurnSpeedIdx);
        float recenterMult = SteeringMultFromIdx(cfg.input.steeringRecenterSpeedIdx);

        auto* rtti = RED4ext::CRTTISystem::Get();
        // Covers both VehicleDriveModelData_Record and BikeDriveModelData_Record
        const char* kTypes[] = {
            "gamedataVehicleDriveModelData_Record",
            "gamedataBikeDriveModelData_Record",
        };

        int patchedCount = 0;
        for (const char* typeName : kTypes)
        {
            auto* type = rtti->GetClass(typeName);
            if (!type) continue;

            RED4ext::DynArray<RED4ext::Handle<RED4ext::IScriptable>> records;
            tdb->TryGetRecordsByType(type, records);

            for (uint32_t i = 0; i < records.Size(); ++i)
            {
                auto& handle = records[i];
                if (!handle.instance) continue;

                auto* rec = static_cast<RED4ext::gamedataTweakDBRecord*>(handle.instance);
                RED4ext::TweakDBID recId = rec->recordID;
                if (recId.value == 0) continue;

                RED4ext::TweakDBID addId(recId, ".wheelTurnMaxAddPerSecond");
                RED4ext::TweakDBID subId(recId, ".wheelTurnMaxSubPerSecond");

                // Read & cache originals on first pass
                if (s_origVals.find(recId.value) == s_origVals.end())
                {
                    float origAdd = 0.f, origSub = 0.f;
                    tdb->TryGetValue<float>(addId, origAdd);
                    tdb->TryGetValue<float>(subId, origSub);
                    // Skip records with no steering data (e.g. tanks)
                    if (origAdd == 0.f && origSub == 0.f) continue;
                    s_origVals[recId.value] = { origAdd, origSub };
                }

                const auto& orig = s_origVals[recId.value];
                float newAdd = orig.add * turnMult;
                float newSub = orig.sub * recenterMult;

                TweakDBSetFloat(tdb, addId, newAdd);
                TweakDBSetFloat(tdb, subId, newSub);
                tdb->UpdateRecord(recId);
                ++patchedCount;
            }
        }

        s_tweakApplied = (turnMult != 1.0f || recenterMult != 1.0f);
        log::InfoF("[direct_wheel:tweak] ApplySteeringMultAllRecords: patched %d records "
                   "(turnMult=%.1fx recenterMult=%.1fx)",
                   patchedCount, turnMult, recenterMult);
        
        return patchedCount > 0;
    }

    namespace
    {
        void SetSteeringTurnSpeedIdxNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t v = 0; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            config::SetSteeringTurnSpeedIdx(v);
            // Re-apply to all records with new multiplier.
            // NOTE: This takes effect for vehicles spawned AFTER this call.
            // Already-spawned vehicles need to be despawned and respawned.
            ApplySteeringMultAllRecords();
            if (aOut) *aOut = true;
        }

        void SetSteeringRecenterSpeedIdxNative(RED4ext::IScriptable*, RED4ext::CStackFrame* aFrame, bool* aOut, int64_t)
        {
            int32_t v = 0; RED4ext::GetParameter(aFrame, &v); aFrame->code++;
            config::SetSteeringRecenterSpeedIdx(v);
            ApplySteeringMultAllRecords();
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
            RegisterGlobal(rtti, "DirectWheel_IsDebugLoggingEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&IsDebugLoggingEnabled),
                           "Bool", {});
            RegisterGlobal(rtti, "DirectWheel_GetDebugRawSteer",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetDebugRawSteer),
                           "Float", {});
            RegisterGlobal(rtti, "DirectWheel_GetDebugWheelSteer",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetDebugWheelSteer),
                           "Float", {});
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
            RegisterGlobal(rtti, "DirectWheel_SetInvertSteering",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetInvertSteering>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetInvertThrottle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetInvertThrottle>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetInvertBrake",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetInvertBrake>),
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

            RegisterGlobal(rtti, "DirectWheel_SetSteeringCurve25",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringCurve25>),
                           "Bool", {{ "Int32", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetSteeringCurve50",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringCurve50>),
                           "Bool", {{ "Int32", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetSteeringCurve75",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInt<&config::SetSteeringCurve75>),
                           "Bool", {{ "Int32", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetSteeringTurnSpeedIdx",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetSteeringTurnSpeedIdxNative),
                           "Bool", {{ "Int32", "idx" }});
            RegisterGlobal(rtti, "DirectWheel_SetSteeringRecenterSpeedIdx",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetSteeringRecenterSpeedIdxNative),
                           "Bool", {{ "Int32", "idx" }});
            RegisterGlobal(rtti, "DirectWheel_GetSteeringTurnSpeedIdx",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetSteeringTurnSpeedIdxNative),
                           "Int32", {});
            RegisterGlobal(rtti, "DirectWheel_GetSteeringRecenterSpeedIdx",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetSteeringRecenterSpeedIdxNative),
                           "Int32", {});

            RegisterGlobal(rtti, "DirectWheel_SetLedEnabled",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLedEnabled>),
                           "Bool", {{ "Bool", "v" }});
            RegisterGlobal(rtti, "DirectWheel_SetLedVisualizerWhileMusic",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetBool<&config::SetLedVisualizerWhileMusic>),
                           "Bool", {{ "Bool", "v" }});

            RegisterGlobal(rtti, "DirectWheel_SetInputBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetInputBinding),
                           "Bool", {{ "Int32", "inputId" }, { "Int32", "action" }});

            RegisterGlobal(rtti, "DirectWheel_SetAxisSteer",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetAxisSteerNative),
                           "Bool", {{ "String", "axis" }});
            RegisterGlobal(rtti, "DirectWheel_SetAxisThrottle",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetAxisThrottleNative),
                           "Bool", {{ "String", "axis" }});
            RegisterGlobal(rtti, "DirectWheel_SetAxisBrake",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetAxisBrakeNative),
                           "Bool", {{ "String", "axis" }});
            RegisterGlobal(rtti, "DirectWheel_SetAxisClutch",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetAxisClutchNative),
                           "Bool", {{ "String", "axis" }});

            RegisterGlobal(rtti, "DirectWheel_GetConnectedDeviceList",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetConnectedDeviceList),
                           "String", {});
            RegisterGlobal(rtti, "DirectWheel_SetWheelDeviceName",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetWheelDeviceName),
                           "Bool", {{ "String", "name" }});
            RegisterGlobal(rtti, "DirectWheel_SetPedalDeviceName",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetPedalDeviceName),
                           "Bool", {{ "String", "name" }});
            RegisterGlobal(rtti, "DirectWheel_SetWheelDeviceIndex",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetWheelDeviceIndex),
                           "Bool", {{ "Int32", "idx" }});
            RegisterGlobal(rtti, "DirectWheel_SetPedalDeviceIndex",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetPedalDeviceIndex),
                           "Bool", {{ "Int32", "idx" }});
            RegisterGlobal(rtti, "DirectWheel_GetDeviceCount",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetDeviceCount),
                           "Int32", {});
            RegisterGlobal(rtti, "DirectWheel_BeginAxisBinding",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&BeginAxisBindingNative),
                           "Bool", {{ "Int32", "target" }});
            RegisterGlobal(rtti, "DirectWheel_ResetDevices",
                           reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ResetDevicesNative),
                           "Bool", {});

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
