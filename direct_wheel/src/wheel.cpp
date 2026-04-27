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

        BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCEW* pdidInstance, VOID* pContext)
        {
            auto& st = S();
            if (st.pWheel) return DIENUM_STOP;

            HRESULT hr = st.pDI->CreateDevice(pdidInstance->guidInstance, &st.pWheel, nullptr);
            if (FAILED(hr)) return DIENUM_CONTINUE;

            return DIENUM_STOP;
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
            HWND hwnd = FindOwnGameWindow();
            if (hwnd) {
                st.pWheel->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
            }
            
            // Acquire the device
            st.pWheel->Acquire();

            std::lock_guard lk(st.capsMtx);
            std::strncpy(st.caps.productName, "DirectInput Wheel (Moza)", 255);
            st.caps.hasFFB = false; // TODO: Implement DI FFB enumeration
            st.caps.operatingRangeDeg = 900;
            
            st.ready.store(true, std::memory_order_release);
            log::Info("[direct_wheel] DirectInput wheel connected.");
            input_bindings::SetDeviceLayout("DirectInput Wheel (Moza)");
        }

        if (FAILED(st.pWheel->Poll())) {
            HRESULT hr = st.pWheel->Acquire();
            while (hr == DIERR_INPUTLOST) {
                hr = st.pWheel->Acquire();
            }
            if (hr == DIERR_OTHERAPPHASPRIO || hr == DIERR_NOTACQUIRED) return; 
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

    void PlayConstant(float magnitude) {}
    void StopConstant() {}
    void PlayDamper(float coefficient) {}
    void StopDamper() {}
    void PlaySpring(float coefficient) {}
    void StopSpring() {}
    void PlayRoadSurface(float magnitude, int periodMs) {}
    void StopRoadSurface() {}
    void PlayCarAirborne() {}
    void StopCarAirborne() {}
    void SetGlobalStrength(float mul) { S().globalStrength.store(mul); }
    void StopAll() {}
    void PlayLeds(float level) {}
    void ClearLeds() {}
    bool IsHandshakeActive() { return false; }
    void SetSurfaceBaselineMag(float mag) {}
    void TriggerJolt(float lateralKick, int durationMs) {}
    void UpdateCenteringSpring(float, float, float, float, float, float, float, bool, bool, bool, float, float, float, int, float, int, bool) {}
}
