// wheel_reset — one-shot console utility that uses the Logitech Steering
// Wheel SDK to sync the wheel's physical state with G HUB and clear any
// sticky FFB. Useful when the wheel firmware is stuck in an SDK-managed
// state and G HUB's GUI changes no longer reach the hardware.
//
// Usage:
//   wheel_reset.exe             -> match whatever G HUB currently shows
//                                  (reads LogiGetCurrentControllerProperties
//                                  and pushes that config to the wheel)
//   wheel_reset.exe 540         -> force 540 deg instead
//
// Requires G HUB (or LGS) to be running, same as the main plugin.

#include <windows.h>
#include <LogitechSteerindirectWheelLib.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main(int argc, char** argv)
{
    // No arg -> match G HUB's current preferred settings.
    // With arg -> force that explicit range.
    bool matchGhub = true;
    int range = 0;
    if (argc > 1)
    {
        matchGhub = false;
        range = std::atoi(argv[1]);
        if (range < 40 || range > 900)
        {
            std::printf("range must be between 40 and 900 degrees (got %d)\n", range);
            return 2;
        }
    }

    // The SDK's DirectInput layer requires a real top-level window it owns.
    // GetConsoleWindow() returns 0 when launched detached (e.g. from certain
    // shells), so we always create our own hidden window.
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hinst;
    wc.lpszClassName = L"direct_wheel_reset_window";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"wheel_reset",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        nullptr, nullptr, hinst, nullptr);

    std::printf("wheel_reset: hwnd=%p target range=%d deg\n", static_cast<void*>(hwnd), range);

    bool ok = hwnd ? LogiSteeringInitializeWithWindow(false, hwnd)
                   : LogiSteeringInitialize(false);
    if (!ok)
    {
        std::printf("LogiSteeringInitialize failed. Is G HUB (or LGS) running?\n");
        return 3;
    }

    int major = 0, minor = 0, build = 0;
    if (LogiSteeringGetSdkVersion(&major, &minor, &build))
        std::printf("SDK v%d.%d build %d\n", major, minor, build);

    // SDK discovers connected wheels lazily; pump LogiUpdate for up to 3s.
    int idx = -1;
    for (int i = 0; i < 60 && idx < 0; ++i)
    {
        LogiUpdate();
        for (int j = 0; j < LOGI_MAX_CONTROLLERS; ++j)
        {
            if (!LogiIsConnected(j)) continue;
            if (LogiIsDeviceConnected(j, LOGI_DEVICE_TYPE_WHEEL)) { idx = j; break; }
        }
        if (idx < 0) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Fallback: any Logitech-manufactured controller.
    if (idx < 0)
    {
        for (int j = 0; j < LOGI_MAX_CONTROLLERS; ++j)
        {
            if (!LogiIsConnected(j)) continue;
            if (LogiIsManufacturerConnected(j, LOGI_MANUFACTURER_LOGITECH)) { idx = j; break; }
        }
    }

    if (idx < 0)
    {
        std::printf("No wheel detected. Plug it in and try again.\n");
        LogiSteeringShutdown();
        return 4;
    }

    wchar_t name[256] = {};
    if (LogiGetFriendlyProductName(idx, name, 256))
        std::wprintf(L"wheel at slot %d: %s\n", idx, name);
    else
        std::printf("wheel at slot %d (no friendly name)\n", idx);

    int currentRange = 0;
    if (LogiGetOperatingRange(idx, currentRange))
        std::printf("current hardware range: %d deg\n", currentRange);

    LogiControllerPropertiesData ghubProps{};
    const bool haveProps = LogiGetCurrentControllerProperties(idx, ghubProps);
    if (haveProps)
    {
        std::printf("G HUB preferred: range=%d springGain=%d defaultSpringEnabled=%d "
                    "defaultSpringGain=%d overallGain=%d\n",
                    ghubProps.wheelRange, ghubProps.springGain,
                    ghubProps.defaultSpringEnabled ? 1 : 0, ghubProps.defaultSpringGain,
                    ghubProps.overallGain);
    }
    else
    {
        std::printf("LogiGetCurrentControllerProperties failed\n");
    }

    LogiStopSpringForce(idx);
    LogiStopDamperForce(idx);
    LogiStopConstantForce(idx);
    std::printf("stopped all FFB effects\n");

    bool propsOk = true;
    bool rangeOk = true;

    if (matchGhub)
    {
        // Match the pattern in Logitech's own SDK sample: zero the struct,
        // set only wheelRange. All the *Gain / *Enabled fields default to 0,
        // which in this API means "use G HUB's values."
        LogiControllerPropertiesData release{};
        release.wheelRange = 900;
        propsOk = LogiSetPreferredControllerProperties(release);
        std::printf("LogiSetPreferredControllerProperties(wheelRange=900 only) -> %s\n",
                    propsOk ? "ok" : "FAILED");

        // Also push an explicit range set as a belt-and-suspenders.
        rangeOk = LogiSetOperatingRange(idx, 900);
        std::printf("LogiSetOperatingRange(900) -> %s\n", rangeOk ? "ok" : "FAILED");
    }
    else
    {
        rangeOk = LogiSetOperatingRange(idx, range);
        std::printf("LogiSetOperatingRange(%d) -> %s\n", range, rangeOk ? "ok" : "FAILED");
    }

    // Give the wheel a moment to process the change before we shut down.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    int confirmed = 0;
    if (LogiGetOperatingRange(idx, confirmed))
        std::printf("range after reset: %d deg\n", confirmed);

    LogiSteeringShutdown();
    std::printf("done.\n");
    return (rangeOk && propsOk) ? 0 : 5;
}
