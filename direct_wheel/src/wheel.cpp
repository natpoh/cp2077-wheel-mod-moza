#include "wheel.h"
#include "logging.h"
#include "device_table.h"
#include "config.h"
#include "input_bindings.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace direct_wheel::wheel
{
    namespace
    {
        struct State
        {
            IDirectInput8W* pDI = nullptr;
            IDirectInputDevice8W* pWheel = nullptr;
            std::atomic<bool> ready{false};
            std::atomic<bool> hasFFB{false};
            std::atomic<float> globalStrength{1.0f};

            std::mutex snapMtx;
            Snapshot snap{};

            std::mutex capsMtx;
            Caps caps{};

            std::atomic<uint32_t> initAttempts{0};
            std::atomic<bool> handshakeActive{false};

            IDirectInputEffect* pConstantEffect = nullptr;
            IDirectInputEffect* pSpringEffect = nullptr;
            IDirectInputEffect* pDamperEffect = nullptr;
            IDirectInputEffect* pFrictionEffect = nullptr;
            IDirectInputEffect* pSineEffect = nullptr;
            HWND hFFBWindow = nullptr;

            // Jolt state: collision triggers a short high-magnitude pulse
            std::atomic<float> joltMagnitude{0.f};
            std::atomic<uint32_t> joltTicksLeft{0};
        };

        State& S() { static State s; return s; }

        HWND FindOwnGameWindow()
        {
            const DWORD myPid = GetCurrentProcessId();
            auto ownedByMe = [&](HWND h) -> bool {
                if (!h || !IsWindow(h)) return false;
                DWORD pid = 0;
                GetWindowThreadProcessId(h, &pid);
                return pid == myPid;
            };
            HWND h = FindWindowW(L"RED4Engine", nullptr);
            if (ownedByMe(h)) return h;
            h = FindWindowW(L"Cyberpunk 2077", nullptr);
            if (ownedByMe(h)) return h;

            struct Ctx { DWORD pid; HWND hit; } ctx{ myPid, nullptr };
            EnumWindows([](HWND w, LPARAM p) -> BOOL {
                auto* c = reinterpret_cast<Ctx*>(p);
                DWORD pid = 0;
                GetWindowThreadProcessId(w, &pid);
                if (pid != c->pid) return TRUE;
                if (!IsWindowVisible(w)) return TRUE;
                c->hit = w;
                return FALSE;
            }, reinterpret_cast<LPARAM>(&ctx));
            return ctx.hit;
        }

        float NormalizePedal(LONG v)
        {
            constexpr float kMax = 65535.f;
            return std::clamp(static_cast<float>(v) / kMax, 0.f, 1.f);
        }

        float NormalizeSteer(LONG v)
        {
            constexpr float kMax = 32767.f;
            return std::clamp(static_cast<float>(v - 32767) / kMax, -1.f, 1.f);
        }

        int ScaleConstant(float v)
        {
            auto cfg = config::Current();
            float mul = cfg.ffb.constantForcePct / 100.0f;
            return static_cast<int>(std::clamp(v, -1.0f, 1.0f) * mul * 10000.0f);
        }

        int ScaleSpring(float v)
        {
            auto cfg = config::Current();
            float mul = cfg.ffb.springForcePct / 100.0f;
            return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * mul * 10000.0f);
        }

        int ScaleDamper(float v)
        {
            auto cfg = config::Current();
            float mul = cfg.ffb.damperForcePct / 100.0f;
            return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * mul * 10000.0f);
        }

        int ScaleFriction(float v)
        {
            auto cfg = config::Current();
            float mul = cfg.ffb.frictionForcePct / 100.0f;
            return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * mul * 10000.0f);
        }

        int ScaleSine(float v)
        {
            auto cfg = config::Current();
            float mul = cfg.ffb.sineForcePct / 100.0f;
            return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * mul * 10000.0f);
        }

        BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCEW* pdidInstance, VOID* pContext)
        {
            auto& st = S();
            if (st.pWheel) return DIENUM_STOP;

            HRESULT hr = st.pDI->CreateDevice(pdidInstance->guidInstance, &st.pWheel, nullptr);
            if (FAILED(hr)) return DIENUM_CONTINUE;

            return DIENUM_STOP;
        }

        void InitFFB(State& st)
        {
            if (!st.pWheel) return;
            log::Info("[direct_wheel] InitFFB: starting FFB initialization...");

            DWORD rgdwAxes[1] = { DIJOFS_X };
            LONG rglDirection[1] = { 0 };

            DIEFFECT diEffect = {0};
            diEffect.dwSize = sizeof(DIEFFECT);
            diEffect.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
            diEffect.cAxes = 1;
            diEffect.rgdwAxes = rgdwAxes;
            diEffect.rglDirection = rglDirection;
            diEffect.dwSamplePeriod = 0;
            diEffect.dwGain = DI_FFNOMINALMAX;
            diEffect.dwTriggerButton = DIEB_NOTRIGGER;
            diEffect.dwTriggerRepeatInterval = 0;
            diEffect.dwDuration = INFINITE;

            // Constant Force
            DICONSTANTFORCE cf = {0};
            diEffect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
            diEffect.lpvTypeSpecificParams = &cf;
            HRESULT hr = st.pWheel->CreateEffect(GUID_ConstantForce, &diEffect, &st.pConstantEffect, nullptr);
            if (FAILED(hr)) {
                log::WarnF("[direct_wheel] InitFFB: GUID_ConstantForce FAILED hr=0x%08lX", (unsigned long)hr);
            } else {
                HRESULT hrd = st.pConstantEffect->Download();
                HRESULT hrs = st.pConstantEffect->Start(1, 0);
                log::InfoF("[direct_wheel] InitFFB: GUID_ConstantForce created OK, Download=0x%08lX Start=0x%08lX",
                           (unsigned long)hrd, (unsigned long)hrs);
            }

            // Spring Force
            DICONDITION spr = {0};
            diEffect.cbTypeSpecificParams = sizeof(DICONDITION);
            diEffect.lpvTypeSpecificParams = &spr;
            hr = st.pWheel->CreateEffect(GUID_Spring, &diEffect, &st.pSpringEffect, nullptr);
            if (FAILED(hr)) {
                log::WarnF("[direct_wheel] InitFFB: GUID_Spring FAILED hr=0x%08lX", (unsigned long)hr);
            } else {
                HRESULT hrd = st.pSpringEffect->Download();
                HRESULT hrs = st.pSpringEffect->Start(1, 0);
                log::InfoF("[direct_wheel] InitFFB: GUID_Spring created OK, Download=0x%08lX Start=0x%08lX",
                           (unsigned long)hrd, (unsigned long)hrs);
            }

            // Damper Force
            DICONDITION dmp = {0};
            diEffect.cbTypeSpecificParams = sizeof(DICONDITION);
            diEffect.lpvTypeSpecificParams = &dmp;
            hr = st.pWheel->CreateEffect(GUID_Damper, &diEffect, &st.pDamperEffect, nullptr);
            if (FAILED(hr)) {
                log::WarnF("[direct_wheel] InitFFB: GUID_Damper FAILED hr=0x%08lX", (unsigned long)hr);
            } else {
                HRESULT hrd = st.pDamperEffect->Download();
                HRESULT hrs = st.pDamperEffect->Start(1, 0);
                log::InfoF("[direct_wheel] InitFFB: GUID_Damper created OK, Download=0x%08lX Start=0x%08lX",
                           (unsigned long)hrd, (unsigned long)hrs);
            }

            // Friction Force
            DICONDITION fric = {0};
            diEffect.cbTypeSpecificParams = sizeof(DICONDITION);
            diEffect.lpvTypeSpecificParams = &fric;
            hr = st.pWheel->CreateEffect(GUID_Friction, &diEffect, &st.pFrictionEffect, nullptr);
            if (FAILED(hr)) {
                log::WarnF("[direct_wheel] InitFFB: GUID_Friction FAILED hr=0x%08lX", (unsigned long)hr);
            } else {
                HRESULT hrd = st.pFrictionEffect->Download();
                HRESULT hrs = st.pFrictionEffect->Start(1, 0);
                log::InfoF("[direct_wheel] InitFFB: GUID_Friction created OK, Download=0x%08lX Start=0x%08lX",
                           (unsigned long)hrd, (unsigned long)hrs);
            }

            // Sine (periodic vibration for road surface)
            DIPERIODIC sine = {0};
            sine.dwMagnitude = 0;
            sine.dwPeriod = 40000; // 40ms = 25Hz base vibration
            diEffect.cbTypeSpecificParams = sizeof(DIPERIODIC);
            diEffect.lpvTypeSpecificParams = &sine;
            diEffect.dwDuration = INFINITE;
            hr = st.pWheel->CreateEffect(GUID_Sine, &diEffect, &st.pSineEffect, nullptr);
            if (FAILED(hr)) {
                log::WarnF("[direct_wheel] InitFFB: GUID_Sine FAILED hr=0x%08lX", (unsigned long)hr);
            } else {
                HRESULT hrd = st.pSineEffect->Download();
                HRESULT hrs = st.pSineEffect->Start(1, 0);
                log::InfoF("[direct_wheel] InitFFB: GUID_Sine created OK, Download=0x%08lX Start=0x%08lX",
                           (unsigned long)hrd, (unsigned long)hrs);
            }

            // Turn off autocenter
            DIPROPDWORD dipdw;
            dipdw.diph.dwSize = sizeof(DIPROPDWORD);
            dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            dipdw.diph.dwObj = 0;
            dipdw.diph.dwHow = DIPH_DEVICE;
            dipdw.dwData = DIPROPAUTOCENTER_OFF;
            st.pWheel->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph);

            st.caps.hasFFB = (st.pConstantEffect || st.pSpringEffect || st.pDamperEffect);
            st.hasFFB.store(st.caps.hasFFB);
        }
    }

    namespace centering_state
    {
        inline std::atomic<int> s_lastCoefPct{-1};
        inline std::atomic<int> s_lastConstPct{INT_MIN};
        inline std::atomic<uint64_t> s_lastCallTickMs{0};
        inline std::atomic<uint32_t> s_joltDurationMs{0};

        inline void Reset()
        {
            s_lastCoefPct.store(-1, std::memory_order_release);
            s_lastConstPct.store(INT_MIN, std::memory_order_release);
            s_joltDurationMs.store(0, std::memory_order_release);
        }
    }

    bool Init()
    {
        log::Info("[direct_wheel] wheel::Init (DirectInput) - Init deferred to Pump()");
        return true;
    }

    void Shutdown()
    {
        auto& st = S();
        st.ready.store(false, std::memory_order_release);
        if (st.pConstantEffect) { st.pConstantEffect->Release(); st.pConstantEffect = nullptr; }
        if (st.pSpringEffect) { st.pSpringEffect->Release(); st.pSpringEffect = nullptr; }
        if (st.pDamperEffect) { st.pDamperEffect->Release(); st.pDamperEffect = nullptr; }
        if (st.pWheel) {
            st.pWheel->Unacquire();
            st.pWheel->Release();
            st.pWheel = nullptr;
        }
        if (st.pDI) {
            st.pDI->Release();
            st.pDI = nullptr;
        }
        log::Info("[direct_wheel] wheel::Shutdown complete");
    }

    bool IsReady() { return S().ready.load(std::memory_order_acquire); }

    const Caps& GetCaps()
    {
        auto& st = S();
        std::lock_guard lk(st.capsMtx);
        static thread_local Caps tl;
        tl = st.caps;
        return tl;
    }

    void Pump()
    {
        auto& st = S();
        if (!st.pDI) {
            if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (VOID**)&st.pDI, nullptr))) {
                return;
            }
        }

        if (!st.pWheel) {
            st.pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, nullptr, DIEDFL_ATTACHEDONLY);
            if (!st.pWheel) return;

            st.pWheel->SetDataFormat(&c_dfDIJoystick2);

            // Initial cooperative level: NONEXCLUSIVE|BACKGROUND so we can
            // read axes immediately even before the game window exists.
            // FFB needs EXCLUSIVE, which we'll set up once HWND is found.
            HWND hwnd = FindOwnGameWindow();
            if (hwnd) {
                HRESULT hrCoop = st.pWheel->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
                log::InfoF("[direct_wheel] SetCooperativeLevel(EXCLUSIVE|BACKGROUND) hr=0x%08lX", (unsigned long)hrCoop);
            } else {
                log::Info("[direct_wheel] Game window not found yet — using NONEXCLUSIVE, FFB deferred");
                st.pWheel->SetCooperativeLevel(GetDesktopWindow(), DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
            }

            // Acquire the device
            HRESULT hrAcq = st.pWheel->Acquire();
            log::InfoF("[direct_wheel] Initial Acquire hr=0x%08lX", (unsigned long)hrAcq);

            std::lock_guard lk(st.capsMtx);
            std::strncpy(st.caps.productName, "DirectInput Wheel (Moza)", 255);
            st.caps.hasFFB = false;
            st.caps.operatingRangeDeg = 900;

            // Only init FFB if we got EXCLUSIVE access
            if (hwnd) {
                InitFFB(st);
            }

            st.ready.store(true, std::memory_order_release);
            log::Info("[direct_wheel] DirectInput wheel connected.");
            input_bindings::SetDeviceLayout("DirectInput Wheel (Moza)");
        }

        // Deferred FFB init: if we started without EXCLUSIVE access,
        // retry every tick until the game window appears.
        if (st.pWheel && !st.hasFFB.load() && !st.pConstantEffect) {
            HWND hwnd = FindOwnGameWindow();
            if (hwnd) {
                st.pWheel->Unacquire();
                HRESULT hrCoop = st.pWheel->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
                log::InfoF("[direct_wheel] Deferred SetCooperativeLevel(EXCLUSIVE|BACKGROUND) hr=0x%08lX", (unsigned long)hrCoop);
                HRESULT hrAcq = st.pWheel->Acquire();
                log::InfoF("[direct_wheel] Deferred Acquire hr=0x%08lX", (unsigned long)hrAcq);
                InitFFB(st);
            }
        }

        if (FAILED(st.pWheel->Poll())) {
            HRESULT hr = st.pWheel->Acquire();
            if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
                // Keep trying to re-acquire (FOREGROUND mode loses access on alt-tab)
                hr = st.pWheel->Acquire();
            }
            if (FAILED(hr)) return;
        }

        DIJOYSTATE2 js;
        if (FAILED(st.pWheel->GetDeviceState(sizeof(DIJOYSTATE2), &js))) {
            return;
        }

        Snapshot s;
        s.connected = true;
        s.steer = NormalizeSteer(js.lX);
        s.throttle = NormalizePedal(js.lZ);
        s.brake = NormalizePedal(js.lRz);
        s.clutch = NormalizePedal(js.rglSlider[0]);
        s.pov = static_cast<uint16_t>(js.rgdwPOV[0] & 0xFFFF);

        uint32_t bits = 0;
        for (int i = 0; i < 32; ++i) {
            if (js.rgbButtons[i] & 0x80) bits |= (1u << i);
        }
        s.buttons = bits;

        {
            std::lock_guard lk(st.snapMtx);
            st.snap = s;
        }
    }

    Snapshot CurrentSnapshot()
    {
        auto& st = S();
        std::lock_guard lk(st.snapMtx);
        return st.snap;
    }

    static uint64_t s_cfCount = 0;
    void PlayConstant(float magnitude) {
        auto& st = S();
        if (!st.pConstantEffect) return;
        DICONSTANTFORCE cf = {0};
        cf.lMagnitude = ScaleConstant(magnitude);
        DIEFFECT eff = {0};
        eff.dwSize = sizeof(DIEFFECT);
        eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        eff.lpvTypeSpecificParams = &cf;
        HRESULT hr = st.pConstantEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr)) {
            st.pWheel->Acquire();
            hr = st.pConstantEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        }
        if (++s_cfCount <= 5 || s_cfCount % 500 == 0) {
            log::InfoF("[direct_wheel:ffb] PlayConstant #%llu mag=%.3f scaled=%d hr=0x%08lX",
                       (unsigned long long)s_cfCount, magnitude, (int)cf.lMagnitude, (unsigned long)hr);
        }
    }
    void StopConstant() {
        if (S().pConstantEffect) S().pConstantEffect->Stop();
    }
    void PlayDamper(float coefficient) {
        auto& st = S();
        if (!st.pDamperEffect) return;
        int val = ScaleDamper(coefficient);
        DICONDITION cond = {0};
        cond.lPositiveCoefficient = val;
        cond.lNegativeCoefficient = val;
        cond.dwPositiveSaturation = 10000;
        cond.dwNegativeSaturation = 10000;
        DIEFFECT eff = {0};
        eff.dwSize = sizeof(DIEFFECT);
        eff.cbTypeSpecificParams = sizeof(DICONDITION);
        eff.lpvTypeSpecificParams = &cond;
        HRESULT hr = st.pDamperEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr)) {
            st.pWheel->Acquire();
            st.pDamperEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        }
    }
    void StopDamper() {
        if (S().pDamperEffect) S().pDamperEffect->Stop();
    }
    void PlaySpring(float coefficient) {
        auto& st = S();
        if (!st.pSpringEffect) return;
        int val = ScaleSpring(coefficient);
        DICONDITION cond = {0};
        cond.lPositiveCoefficient = val;
        cond.lNegativeCoefficient = val;
        cond.dwPositiveSaturation = 10000;
        cond.dwNegativeSaturation = 10000;
        DIEFFECT eff = {0};
        eff.dwSize = sizeof(DIEFFECT);
        eff.cbTypeSpecificParams = sizeof(DICONDITION);
        eff.lpvTypeSpecificParams = &cond;
        HRESULT hr = st.pSpringEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr)) {
            st.pWheel->Acquire();
            st.pSpringEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        }
    }
    void StopSpring() {
        if (S().pSpringEffect) S().pSpringEffect->Stop();
    }
    void PlayFriction(float coefficient) {
        auto& st = S();
        if (!st.pFrictionEffect) return;
        int val = ScaleFriction(coefficient);
        DICONDITION cond = {0};
        cond.lPositiveCoefficient = val;
        cond.lNegativeCoefficient = val;
        cond.dwPositiveSaturation = 10000;
        cond.dwNegativeSaturation = 10000;
        DIEFFECT eff = {0};
        eff.dwSize = sizeof(DIEFFECT);
        eff.cbTypeSpecificParams = sizeof(DICONDITION);
        eff.lpvTypeSpecificParams = &cond;
        HRESULT hr = st.pFrictionEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr)) {
            st.pWheel->Acquire();
            st.pFrictionEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        }
    }
    void StopFriction() {
        if (S().pFrictionEffect) S().pFrictionEffect->Stop();
    }
    void PlaySine(float magnitude) {
        auto& st = S();
        if (!st.pSineEffect) return;
        DIPERIODIC sine = {0};
        sine.dwMagnitude = static_cast<DWORD>(ScaleSine(magnitude));
        sine.dwPeriod = 40000; // 25Hz vibration
        DIEFFECT eff = {0};
        eff.dwSize = sizeof(DIEFFECT);
        eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
        eff.lpvTypeSpecificParams = &sine;
        HRESULT hr = st.pSineEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        if (FAILED(hr)) {
            st.pWheel->Acquire();
            st.pSineEffect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        }
    }
    void StopSine() {
        if (S().pSineEffect) S().pSineEffect->Stop();
    }
    void PlayRoadSurface(float magnitude, int periodMs) {}
    void StopRoadSurface() {}
    void PlayCarAirborne() {}
    void StopCarAirborne() {}
    void SetGlobalStrength(float mul) { S().globalStrength.store(mul); }
    void StopAll() { StopConstant(); StopDamper(); StopSpring(); StopFriction(); StopSine(); }
    void PlayLeds(float level) {}
    void ClearLeds() {}
    bool IsHandshakeActive() { return false; }
    void SetSurfaceBaselineMag(float mag) {}
    void TriggerJolt(float lateralKick, int durationMs) {
        auto& st = S();
        auto cfg = config::Current();
        float mul = cfg.ffb.joltForcePct / 100.0f;
        st.joltMagnitude.store(lateralKick * mul, std::memory_order_release);
        // Convert duration to ticks at 250Hz
        uint32_t ticks = std::max(1u, static_cast<uint32_t>(durationMs * 250 / 1000));
        st.joltTicksLeft.store(ticks, std::memory_order_release);
    }
    void UpdateCenteringSpring(float absSpeedMps, float angVelMagRad,
                               float suspensionActivity, float lateralVelocityMps,
                               float steer, float throttle, float brake,
                               bool isReversing, bool isOnGround, bool enabled,
                               float stationaryMps, float cruiseMps,
                               float centeringBaseline, int yawFeedbackPct,
                               float yawRef, int activeTorqueStrengthPct,
                               bool debugLog)
    {
        if (!enabled || !isOnGround) {
            StopAll();
            return;
        }

        // Below stationary threshold → no forces
        if (absSpeedMps < stationaryMps) {
            StopAll();
            return;
        }

        // Speed factor: ramps 0→1 as speed goes from 0→cruiseMps
        float speedRatio = std::clamp(absSpeedMps / std::max(cruiseMps, 1.0f), 0.f, 1.5f);
        float speedSq = speedRatio * speedRatio;

        // Yaw factor
        float yawRatio = std::abs(angVelMagRad) / std::max(yawRef, 0.01f);
        float yawRamp = std::clamp(yawRatio, 0.f, 1.f);

        // Grip factor — decays past yaw limit
        float gripFactor = yawRatio < 1.0f ? 1.0f : std::exp(-2.0f * (yawRatio - 1.0f));

        // === Spring: centering force scales with speed² ===
        // Base coefficient ~0.25 at cruise (was ~0.87 = way too strong).
        // User slider 50% should feel "medium centering".
        float springCoef = std::clamp(
            centeringBaseline * 0.3f * speedSq * gripFactor
            + yawRamp * 0.2f * (yawFeedbackPct / 100.0f),
            0.f, 0.5f);
        if (isReversing) springCoef *= 0.4f;
        PlaySpring(springCoef);

        // === Damper: viscous resistance scales with speed ===
        // Reduced from 0.4 to 0.15 base so it's felt but not overwhelming.
        float damperCoef = std::clamp(speedRatio * 0.15f, 0.f, 0.3f);
        PlayDamper(damperCoef);

        // === Constant Force: active self-aligning torque ===
        // Two components blended for stronger feel:
        // 1) Speed-proportional centering push (always pushes toward center)
        // 2) Yaw/lateral-load reactive push (dynamic in turns)
        float absSteer = std::abs(steer);

        // Component 1: direct steer-proportional push, scales with speed.
        // At cruise with half steer → ~0.35 magnitude. Strong enough to feel.
        float directPush = -steer * speedRatio * 0.5f;

        // Component 2: lateral load / yaw reactive push
        float steerShape = std::sqrt(std::max(absSteer, 0.001f))
                         * (1.0f - absSteer * absSteer);
        float loadFactor = std::clamp(
            std::abs(angVelMagRad * absSpeedMps) / std::max(yawRef * cruiseMps, 0.01f),
            0.f, 1.5f);
        float weight = 1.0f + 0.3f * brake - 0.05f * throttle;
        float yawPush = -copysignf(1.0f, steer)
                      * steerShape * loadFactor * gripFactor * weight;

        // Blend: 60% direct push + 40% yaw reactive, scaled by config
        float activeForce = (0.6f * directPush + 0.4f * yawPush)
                          * (activeTorqueStrengthPct / 100.0f);
        if (isReversing) activeForce *= 0.4f;

        // === Road-feel: suspension activity → random-direction bumps ===
        static uint32_t s_bumpTick = 0;
        float bumpSign = (++s_bumpTick & 1) ? 1.0f : -1.0f;
        float bumpMag = std::clamp(suspensionActivity * 3.0f * speedRatio, 0.f, 0.5f);
        activeForce += bumpSign * bumpMag;

        // === Jolt: collision pulse override ===
        auto& st = S();
        uint32_t jt = st.joltTicksLeft.load(std::memory_order_acquire);
        if (jt > 0) {
            float jm = st.joltMagnitude.load(std::memory_order_acquire);
            activeForce += jm;  // Add jolt impulse on top
            st.joltTicksLeft.store(jt - 1, std::memory_order_release);
        }

        PlayConstant(std::clamp(activeForce, -1.0f, 1.0f));

        // === Friction: road-texture resistance, scales with speed ===
        float frictionCoef = std::clamp(speedRatio * 0.3f, 0.f, 0.4f);
        PlayFriction(frictionCoef);

        // === Sine: periodic vibration from road roughness ===
        // suspensionActivity drives amplitude; quiet on smooth roads.
        float sineMag = std::clamp(suspensionActivity * 2.0f * speedRatio, 0.f, 0.6f);
        PlaySine(sineMag);
    }
}
