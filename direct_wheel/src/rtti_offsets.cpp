#include "rtti_offsets.h"
#include "logging.h"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/RTTISystem.hpp>
#include <RED4ext/RTTITypes.hpp>
#include <RED4ext/CName.hpp>
#include <RED4ext/Scripting/CProperty.hpp>

#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace direct_wheel::rtti_offsets
{
    namespace
    {
        struct State
        {
            std::atomic<bool>     ready{false};
            std::atomic<uint32_t> vehicleBaseObject_physics{0};
            std::atomic<uint32_t> vehicleBaseObject_physicsData{0};
            std::atomic<uint32_t> physicsData_velocity{0};
            std::atomic<uint32_t> physicsData_angularVelocity{0};
            std::atomic<uint32_t> wheeledPhysics_turnRate{0};
            std::atomic<uint32_t> wheeledPhysics_maxWheelTurnDeg{0};
            std::atomic<uint32_t> wheeledPhysics_slipAngleCurveScale{0};
            std::atomic<uint32_t> wheeledPhysics_numDriveWheels{0};
            std::atomic<uint32_t> wheeledPhysics_frontBackWheelDistance{0};

            // Runtime-probe results: where physicsData / physics live on
            // vehicleBaseObject, discovered by back-pointer correlation.
            // Zero = not yet resolved (use fallback).
            std::atomic<uint32_t> probed_physicsDataOffset{0};
            std::atomic<uint32_t> probed_physicsOffset{0};
            std::atomic<bool>     probeDone{false};
        };

        State& S() { static State s; return s; }

        // Resolve a single (className, fieldName) pair and log the result.
        // Returns the field's valueOffset or 0 if unresolvable.
        uint32_t Resolve(RED4ext::CRTTISystem* rtti,
                         const char* className,
                         const char* fieldName)
        {
            auto* cls = rtti->GetClass(RED4ext::CName(className));
            if (!cls)
            {
                log::WarnF("[direct_wheel:rtti-off] class '%s' not found — cannot resolve '%s.%s'",
                           className, className, fieldName);
                return 0;
            }
            auto* prop = cls->GetProperty(RED4ext::CName(fieldName));
            if (!prop)
            {
                log::WarnF("[direct_wheel:rtti-off] field '%s.%s' not found on this build — check DumpClassProperties output",
                           className, fieldName);
                return 0;
            }
            const uint32_t off = prop->valueOffset;
            const char* typeName = prop->type ? prop->type->GetName().ToString() : "?";
            log::InfoF("[direct_wheel:rtti-off] %s.%s -> offset=0x%X type=%s",
                       className, fieldName, off, typeName ? typeName : "?");
            return off;
        }

        // Pick the first resolvable (className, fieldName) pair from a list.
        // Useful when the game has used multiple names for the same field
        // across versions, or when a field lives on a parent/subclass with a
        // differently-named RTTI class in different builds.
        uint32_t ResolveFirst(RED4ext::CRTTISystem* rtti,
                              std::initializer_list<std::pair<const char*, const char*>> candidates)
        {
            for (const auto& [cls, field] : candidates)
            {
                const uint32_t off = Resolve(rtti, cls, field);
                if (off != 0) return off;
            }
            return 0;
        }
    }

    void Init()
    {
        auto& st = S();
        if (st.ready.load(std::memory_order_acquire)) return;

        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Warn("[direct_wheel:rtti-off] RTTI system unavailable — all offsets will fall back to hardcoded values");
            return;
        }

        log::Info("[direct_wheel:rtti-off] ======== resolving struct offsets from game RTTI ========");

        // vehicle::BaseObject — has `physics` and `physicsData` handles.
        st.vehicleBaseObject_physics.store(
            ResolveFirst(rtti, {{"vehicleBaseObject", "physics"},
                                {"vehicleBaseObject", "Physics"}}),
            std::memory_order_release);
        st.vehicleBaseObject_physicsData.store(
            ResolveFirst(rtti, {{"vehicleBaseObject", "physicsData"},
                                {"vehicleBaseObject", "PhysicsData"}}),
            std::memory_order_release);

        // vehicle::PhysicsData — velocity + angularVelocity.
        st.physicsData_velocity.store(
            ResolveFirst(rtti, {{"vehiclePhysicsData", "velocity"}}),
            std::memory_order_release);
        st.physicsData_angularVelocity.store(
            ResolveFirst(rtti, {{"vehiclePhysicsData", "angularVelocity"}}),
            std::memory_order_release);

        // vehicle::WheeledPhysics — per-car handling tuning.
        st.wheeledPhysics_turnRate.store(
            ResolveFirst(rtti, {{"vehicleWheeledPhysics", "turnRate"}}),
            std::memory_order_release);
        st.wheeledPhysics_maxWheelTurnDeg.store(
            ResolveFirst(rtti, {{"vehicleWheeledPhysics", "maxWheelTurnDeg"}}),
            std::memory_order_release);
        st.wheeledPhysics_slipAngleCurveScale.store(
            ResolveFirst(rtti, {{"vehicleWheeledPhysics", "slipAngleCurveScale"}}),
            std::memory_order_release);
        st.wheeledPhysics_numDriveWheels.store(
            ResolveFirst(rtti, {{"vehicleWheeledPhysics", "numDriveWheels"}}),
            std::memory_order_release);
        st.wheeledPhysics_frontBackWheelDistance.store(
            ResolveFirst(rtti, {{"vehicleWheeledPhysics", "frontBackWheelDistance"}}),
            std::memory_order_release);

        log::Info("[direct_wheel:rtti-off] ======== dumping full property lists (for discovery) ========");
        DumpClassProperties("vehicleBaseObject");
        DumpClassProperties("vehiclePhysicsData");
        DumpClassProperties("vehicleWheeledPhysics");
        DumpClassProperties("vehiclePersistentDataPS");
        DumpClassProperties("vehicleWheelRuntimePSData");
        // Camera / action discovery sweeps were here on 2026-04-24 to find
        // direct-method callables for RearViewCamera / RadioMenu / Siren.
        // Results captured in the build log; the only lasting use was
        // wiring up `RadioNext` -> NextRadioReceiverStation. Sweep removed
        // from the startup path because it produces ~5 KB of one-time
        // output on every launch. The helpers (DumpClassesContaining /
        // DumpFunctionsContaining / DumpClassFunctions) remain available
        // in the header for ad-hoc reuse if we go hunting for more
        // direct-action paths later.
        log::Info("[direct_wheel:rtti-off] ======== offset resolution complete ========");

        st.ready.store(true, std::memory_order_release);
    }

    bool IsReady() { return S().ready.load(std::memory_order_acquire); }

    uint32_t VehicleBaseObject_physicsOffset()          { return S().vehicleBaseObject_physics.load(std::memory_order_acquire); }
    uint32_t VehicleBaseObject_physicsDataOffset()      { return S().vehicleBaseObject_physicsData.load(std::memory_order_acquire); }
    uint32_t PhysicsData_velocityOffset()               { return S().physicsData_velocity.load(std::memory_order_acquire); }
    uint32_t PhysicsData_angularVelocityOffset()        { return S().physicsData_angularVelocity.load(std::memory_order_acquire); }
    uint32_t WheeledPhysics_turnRateOffset()            { return S().wheeledPhysics_turnRate.load(std::memory_order_acquire); }
    uint32_t WheeledPhysics_maxWheelTurnDegOffset()     { return S().wheeledPhysics_maxWheelTurnDeg.load(std::memory_order_acquire); }
    uint32_t WheeledPhysics_slipAngleCurveScaleOffset() { return S().wheeledPhysics_slipAngleCurveScale.load(std::memory_order_acquire); }
    uint32_t WheeledPhysics_numDriveWheelsOffset()      { return S().wheeledPhysics_numDriveWheels.load(std::memory_order_acquire); }
    uint32_t WheeledPhysics_frontBackWheelDistanceOffset() { return S().wheeledPhysics_frontBackWheelDistance.load(std::memory_order_acquire); }

    namespace
    {
        // Cheap "is this pointer plausibly a valid heap allocation" check.
        // We VirtualQuery it and make sure it's committed, readable, and
        // not code/shared memory. Returns false for null / misaligned / bad.
        bool LooksLikeValidHeapPtr(void* p)
        {
            if (!p) return false;
            if (reinterpret_cast<uintptr_t>(p) < 0x10000) return false; // low memory / small ints
            if (reinterpret_cast<uintptr_t>(p) & 0x7) return false; // must be 8-byte aligned
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return false;
            if (mbi.State != MEM_COMMIT) return false;
            const DWORD rw = PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE;
            if (!(mbi.Protect & rw)) return false;
            return true;
        }

        // Look for the pointer `needle` anywhere in the 8-byte-aligned slots
        // of `[base, base+range)`. Returns the offset of the first match, or
        // -1 if not found. Cheap linear scan; range is always small.
        std::ptrdiff_t FindPointerIn(void* base, std::ptrdiff_t range, void* needle)
        {
            auto* p = static_cast<const uintptr_t*>(base);
            const auto target = reinterpret_cast<uintptr_t>(needle);
            const std::ptrdiff_t slots = range / sizeof(uintptr_t);
            for (std::ptrdiff_t i = 0; i < slots; ++i)
            {
                if (p[i] == target)
                    return i * static_cast<std::ptrdiff_t>(sizeof(uintptr_t));
            }
            return -1;
        }
    }

    bool ProbeStructOffsets(void* vehicle)
    {
        auto& st = S();
        if (st.probeDone.load(std::memory_order_acquire)) return true;
        if (!vehicle) return false;

        // Candidate outer offsets to probe on vehicleBaseObject. LTBF's
        // fork had physics@0x2C8 and physicsData@0x2D0 — we scan a generous
        // window around that, covering +/- patch drift.
        constexpr std::ptrdiff_t kScanStart = 0x200;
        constexpr std::ptrdiff_t kScanEnd   = 0x400;
        // Inner range: back-pointer on PhysicsData was @0x180 in LTBF fork,
        // on Physics @0x60. Scan a superset range inside each candidate.
        constexpr std::ptrdiff_t kInnerStart = 0x00;
        constexpr std::ptrdiff_t kInnerEnd   = 0x200;

        log::InfoF("[direct_wheel:rtti-off] probing struct offsets from live vehicle=%p", vehicle);

        struct Hit { std::ptrdiff_t outerOff; std::ptrdiff_t backOff; void* inner; };
        Hit hits[8] = {};
        int hitCount = 0;

        auto* bytes = static_cast<char*>(vehicle);
        for (std::ptrdiff_t o = kScanStart; o + static_cast<std::ptrdiff_t>(sizeof(void*)) <= kScanEnd;
             o += sizeof(void*))
        {
            void* cand = *reinterpret_cast<void**>(bytes + o);
            if (!LooksLikeValidHeapPtr(cand)) continue;

            const std::ptrdiff_t backOff =
                FindPointerIn(cand, kInnerEnd - kInnerStart, vehicle);
            if (backOff < 0) continue;

            if (hitCount < static_cast<int>(std::size(hits)))
            {
                hits[hitCount++] = { o, kInnerStart + backOff, cand };
                log::InfoF("[direct_wheel:rtti-off]   candidate: outer=0x%03X -> inner=%p with back-ref @0x%03X",
                           static_cast<unsigned>(o), cand, static_cast<unsigned>(kInnerStart + backOff));
            }
        }

        if (hitCount == 0)
        {
            log::Warn("[direct_wheel:rtti-off] probe: no correlated pointers found — falling back to hardcoded offsets");
            st.probeDone.store(true, std::memory_order_release); // don't re-probe every tick
            return false;
        }

        // Classify hits by back-pointer-offset signature. From LTBF's SDK
        // fork the signatures were stable:
        //   PhysicsData.vehicle (back-ref) @ 0x180
        //   Physics.parent (back-ref)      @ 0x060
        // We match each candidate to the nearest signature within a
        // tolerance. Pointers with back-refs far from any signature
        // (e.g. chassis @0x050, movement @0x020) are rejected.
        constexpr std::ptrdiff_t kSigPhysicsData = 0x180;
        constexpr std::ptrdiff_t kSigPhysics     = 0x060;
        constexpr std::ptrdiff_t kSigTolerance   = 0x18;

        int physicsDataIdx = -1;
        int physicsIdx     = -1;
        std::ptrdiff_t bestPDDelta = kSigTolerance + 1;
        std::ptrdiff_t bestPhDelta = kSigTolerance + 1;

        for (int i = 0; i < hitCount; ++i)
        {
            const auto bo = hits[i].backOff;
            const auto pdDelta = std::abs(bo - kSigPhysicsData);
            const auto phDelta = std::abs(bo - kSigPhysics);
            if (pdDelta <= kSigTolerance && pdDelta < bestPDDelta)
            {
                physicsDataIdx = i;
                bestPDDelta = pdDelta;
            }
            if (phDelta <= kSigTolerance && phDelta < bestPhDelta)
            {
                physicsIdx = i;
                bestPhDelta = phDelta;
            }
        }

        if (physicsDataIdx >= 0)
        {
            const auto& h = hits[physicsDataIdx];
            st.probed_physicsDataOffset.store(static_cast<uint32_t>(h.outerOff),
                                              std::memory_order_release);
            log::InfoF("[direct_wheel:rtti-off] probe: physicsData -> outer=0x%03X (back-ref @0x%03X, inner=%p)",
                       static_cast<unsigned>(h.outerOff),
                       static_cast<unsigned>(h.backOff), h.inner);
        }
        if (physicsIdx >= 0 && physicsIdx != physicsDataIdx)
        {
            const auto& h = hits[physicsIdx];
            st.probed_physicsOffset.store(static_cast<uint32_t>(h.outerOff),
                                          std::memory_order_release);
            log::InfoF("[direct_wheel:rtti-off] probe: physics     -> outer=0x%03X (back-ref @0x%03X, inner=%p)",
                       static_cast<unsigned>(h.outerOff),
                       static_cast<unsigned>(h.backOff), h.inner);
        }

        // Populate the existing "vehicleBaseObject_*" accessors so the rest
        // of the plugin sees the probed values transparently.
        if (auto o = st.probed_physicsDataOffset.load(std::memory_order_acquire); o != 0)
            st.vehicleBaseObject_physicsData.store(o, std::memory_order_release);
        if (auto o = st.probed_physicsOffset.load(std::memory_order_acquire); o != 0)
            st.vehicleBaseObject_physics.store(o, std::memory_order_release);

        st.probeDone.store(true, std::memory_order_release);
        return (physicsDataIdx >= 0 || physicsIdx >= 0);
    }

    bool StructProbeDone() { return S().probeDone.load(std::memory_order_acquire); }

    void ProbeInnerWheeledFields(void* wp)
    {
        if (!wp) return;
        auto* bytes = static_cast<const char*>(wp);
        constexpr std::ptrdiff_t kScanEnd = 0x200;

        log::InfoF("[direct_wheel:rtti-off] ---- WheeledPhysics inner-field scan at %p ----", wp);

        // numDriveWheels: uint32 == 2 or 4. Highly distinctive — random
        // memory rarely hits either exact value aligned to uint32.
        {
            bool any = false;
            log::Info("[direct_wheel:rtti-off]   numDriveWheels candidates (uint32 == 2 or 4):");
            for (std::ptrdiff_t off = 0; off + 4 <= kScanEnd; off += 4)
            {
                const uint32_t v = *reinterpret_cast<const uint32_t*>(bytes + off);
                if (v == 2 || v == 4)
                {
                    log::InfoF("[direct_wheel:rtti-off]     +0x%03X = %u", static_cast<unsigned>(off), v);
                    any = true;
                }
            }
            if (!any) log::Info("[direct_wheel:rtti-off]     (none)");
        }

        // wheelbase: float in [1.0, 5.0] m. Multiple hits typical; cross-
        // vehicle consistency picks the winner.
        {
            bool any = false;
            log::Info("[direct_wheel:rtti-off]   wheelbase candidates (float in [1.0, 5.0]):");
            for (std::ptrdiff_t off = 0; off + 4 <= kScanEnd; off += 4)
            {
                const float f = *reinterpret_cast<const float*>(bytes + off);
                if (std::isfinite(f) && f >= 1.0f && f <= 5.0f)
                {
                    log::InfoF("[direct_wheel:rtti-off]     +0x%03X = %.3f", static_cast<unsigned>(off), f);
                    any = true;
                }
            }
            if (!any) log::Info("[direct_wheel:rtti-off]     (none)");
        }

        // turnRate: float in [0.5, 100.0]. Broader — includes rad/s and
        // deg/s interpretations. Cross-reference against numDriveWheels
        // and wheelbase hits to narrow.
        {
            bool any = false;
            log::Info("[direct_wheel:rtti-off]   turnRate candidates (float in [0.5, 100.0]):");
            for (std::ptrdiff_t off = 0; off + 4 <= kScanEnd; off += 4)
            {
                const float f = *reinterpret_cast<const float*>(bytes + off);
                if (std::isfinite(f) && f >= 0.5f && f <= 100.0f)
                {
                    log::InfoF("[direct_wheel:rtti-off]     +0x%03X = %.3f", static_cast<unsigned>(off), f);
                    any = true;
                }
            }
            if (!any) log::Info("[direct_wheel:rtti-off]     (none)");
        }

        log::Info("[direct_wheel:rtti-off] ---- end inner-field scan ----");
    }

    void DumpClassProperties(const char* className)
    {
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti || !className)
        {
            log::Warn("[direct_wheel:rtti-off] DumpClassProperties: RTTI or class name unavailable");
            return;
        }

        auto* cls = rtti->GetClass(RED4ext::CName(className));
        if (!cls)
        {
            log::WarnF("[direct_wheel:rtti-off] DumpClassProperties: class '%s' not found", className);
            return;
        }

        RED4ext::DynArray<RED4ext::CProperty*> props;
        cls->GetProperties(props);
        const uint32_t n = props.Size();
        log::InfoF("[direct_wheel:rtti-off] --- %s (size=0x%X, %u properties) ---", className, cls->size, n);
        for (uint32_t i = 0; i < n; ++i)
        {
            auto* p = props[i];
            if (!p) continue;
            const char* name = p->name.ToString();
            const char* typeName = p->type ? p->type->GetName().ToString() : "?";
            log::InfoF("[direct_wheel:rtti-off]   0x%04X  %s : %s",
                       p->valueOffset, name ? name : "?", typeName ? typeName : "?");
        }
    }

    namespace
    {
        // Lowercase ASCII compare-substring. Cheap, allocation-free.
        bool ContainsCI(const char* haystack, const char* needle)
        {
            if (!haystack || !needle) return false;
            for (const char* p = haystack; *p; ++p)
            {
                size_t i = 0;
                while (needle[i] && p[i])
                {
                    char a = p[i], b = needle[i];
                    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
                    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
                    if (a != b) break;
                    ++i;
                }
                if (!needle[i]) return true;
            }
            return false;
        }

        // Format one function signature: name(arg0:T0, arg1:T1, ...) -> ret.
        // Writes into `out` (size cap). Truncation OK — diagnostic only.
        void FormatFunctionSig(const RED4ext::CBaseFunction* fn, char* out, size_t cap)
        {
            if (!fn || !out || cap == 0) return;
            out[0] = '\0';
            const char* nm = fn->fullName.ToString();
            int written = std::snprintf(out, cap, "%s(", nm ? nm : "?");
            const uint32_t nParams = fn->params.Size();
            for (uint32_t i = 0; i < nParams; ++i)
            {
                auto* p = fn->params[i];
                if (!p) continue;
                const char* pn = p->name.ToString();
                const char* pt = p->type ? p->type->GetName().ToString() : "?";
                written += std::snprintf(out + written,
                                         (static_cast<size_t>(written) < cap) ? cap - static_cast<size_t>(written) : 0,
                                         "%s%s:%s",
                                         (i ? ", " : ""), pn ? pn : "?", pt ? pt : "?");
                if (static_cast<size_t>(written) + 1 >= cap) break;
            }
            const char* rt = (fn->returnType && fn->returnType->type)
                                ? fn->returnType->type->GetName().ToString()
                                : "void";
            std::snprintf(out + written,
                          (static_cast<size_t>(written) < cap) ? cap - static_cast<size_t>(written) : 0,
                          ") -> %s", rt ? rt : "void");
        }
    }

    void DumpClassFunctions(const char* className)
    {
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti || !className)
        {
            log::Warn("[direct_wheel:rtti-off] DumpClassFunctions: RTTI or class name unavailable");
            return;
        }

        auto* cls = rtti->GetClass(RED4ext::CName(className));
        if (!cls)
        {
            log::WarnF("[direct_wheel:rtti-off] DumpClassFunctions: class '%s' not found", className);
            return;
        }

        const uint32_t fns = cls->funcs.Size();
        const uint32_t sfs = cls->staticFuncs.Size();
        log::InfoF("[direct_wheel:rtti-off] --- %s methods (%u instance, %u static) ---",
                   className, fns, sfs);
        char buf[1024];
        for (uint32_t i = 0; i < fns; ++i)
        {
            auto* f = cls->funcs[i];
            if (!f) continue;
            FormatFunctionSig(f, buf, sizeof(buf));
            log::InfoF("[direct_wheel:rtti-off]   inst %s", buf);
        }
        for (uint32_t i = 0; i < sfs; ++i)
        {
            auto* f = cls->staticFuncs[i];
            if (!f) continue;
            FormatFunctionSig(f, buf, sizeof(buf));
            log::InfoF("[direct_wheel:rtti-off]   stat %s", buf);
        }
    }

    void DumpClassesContaining(std::initializer_list<const char*> subs)
    {
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Warn("[direct_wheel:rtti-off] DumpClassesContaining: RTTI unavailable");
            return;
        }

        log::Info("[direct_wheel:rtti-off] === sweeping RTTI classes for substring matches ===");
        RED4ext::DynArray<RED4ext::CClass*> classes;
        rtti->GetClasses(nullptr, classes, nullptr, true);
        const uint32_t n = classes.Size();
        size_t hits = 0;
        for (uint32_t i = 0; i < n; ++i)
        {
            auto* cls = classes[i];
            if (!cls) continue;
            const char* nm = cls->GetName().ToString();
            if (!nm) continue;
            for (const char* s : subs)
            {
                if (ContainsCI(nm, s))
                {
                    log::InfoF("[direct_wheel:rtti-off]   class %s (size=0x%X, props=%u, funcs=%u)",
                               nm, cls->size, cls->props.Size(), cls->funcs.Size());
                    ++hits;
                    break;
                }
            }
        }
        log::InfoF("[direct_wheel:rtti-off] === sweep complete (%zu matches across %u classes) ===",
                   hits, n);
    }

    void DumpFunctionsContaining(std::initializer_list<const char*> subs)
    {
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti)
        {
            log::Warn("[direct_wheel:rtti-off] DumpFunctionsContaining: RTTI unavailable");
            return;
        }

        log::Info("[direct_wheel:rtti-off] === sweeping RTTI methods for substring matches ===");
        RED4ext::DynArray<RED4ext::CBaseFunction*> fns;
        rtti->GetClassFunctions(fns);
        const uint32_t n = fns.Size();
        size_t hits = 0;
        char buf[1024];
        for (uint32_t i = 0; i < n; ++i)
        {
            auto* f = fns[i];
            if (!f) continue;
            const char* fn = f->fullName.ToString();
            if (!fn) continue;
            for (const char* s : subs)
            {
                if (ContainsCI(fn, s))
                {
                    FormatFunctionSig(f, buf, sizeof(buf));
                    log::InfoF("[direct_wheel:rtti-off]   %s", buf);
                    ++hits;
                    break;
                }
            }
        }
        log::InfoF("[direct_wheel:rtti-off] === function sweep complete (%zu matches across %u methods) ===",
                   hits, n);
    }
}
