#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>

IDirectInput8W* g_pDI = nullptr;

struct DeviceEntry {
    IDirectInputDevice8W* pDev = nullptr;
    std::string name;
    DIJOYSTATE2 state{};
    bool hasFfb = false;
};

static std::vector<DeviceEntry> g_devices;

BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCEW* pdidInstance, VOID*) {
    IDirectInputDevice8W* pDev = nullptr;
    if (FAILED(g_pDI->CreateDevice(pdidInstance->guidInstance, &pDev, nullptr)))
        return DIENUM_CONTINUE;

    pDev->SetDataFormat(&c_dfDIJoystick2);
    pDev->SetCooperativeLevel(GetConsoleWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);

    // Check FFB capability
    bool hasFfb = false;
    DIDEVCAPS caps = {};
    caps.dwSize = sizeof(DIDEVCAPS);
    if (SUCCEEDED(pDev->GetCapabilities(&caps)))
        hasFfb = (caps.dwFlags & DIDC_FORCEFEEDBACK) != 0;

    if (FAILED(pDev->Acquire())) {
        pDev->Release();
        return DIENUM_CONTINUE;
    }

    char utf8[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, pdidInstance->tszProductName, -1,
                        utf8, sizeof(utf8), nullptr, nullptr);

    DeviceEntry e;
    e.pDev = pDev;
    e.name = utf8;
    e.hasFfb = hasFfb;
    g_devices.push_back(e);

    return DIENUM_CONTINUE;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "Initializing DirectInput...\n";
    if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION,
               IID_IDirectInput8W, (VOID**)&g_pDI, nullptr))) {
        std::cerr << "DirectInput8Create failed\n";
        return 1;
    }

    std::cout << "Enumerating all game controllers...\n\n";
    g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, nullptr, DIEDFL_ATTACHEDONLY);

    if (g_devices.empty()) {
        std::cerr << "No game controllers found!\n";
        return 1;
    }

    for (int i = 0; i < (int)g_devices.size(); ++i) {
        std::cout << "  Device #" << (i + 1) << ": \"" << g_devices[i].name
                  << "\" (FFB: " << (g_devices[i].hasFfb ? "yes" : "no") << ")\n";
    }
    std::cout << "\nReading all axes. Press Ctrl+C to exit.\n";
    std::cout << std::string(80, '-') << "\n\n";

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD startPos = csbi.dwCursorPosition;  // remember where output starts

    while (true) {
        // Reset cursor to start position each frame
        SetConsoleCursorPosition(hConsole, startPos);

        for (int i = 0; i < (int)g_devices.size(); ++i) {
            auto& d = g_devices[i];
            if (FAILED(d.pDev->Poll())) d.pDev->Acquire();
            if (SUCCEEDED(d.pDev->GetDeviceState(sizeof(DIJOYSTATE2), &d.state))) {
                auto& js = d.state;
                char line1[256], line2[256];
                sprintf_s(line1, "#%d %s", i + 1, d.name.c_str());
                sprintf_s(line2, "  lX:%6ld  lY:%6ld  lZ:%6ld  lRx:%6ld  lRy:%6ld  lRz:%6ld  S0:%6ld  S1:%6ld",
                    js.lX, js.lY, js.lZ, js.lRx, js.lRy, js.lRz, js.rglSlider[0], js.rglSlider[1]);
                printf("%-79s\n%-79s\n", line1, line2);
            }
        }
        std::cout << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}
