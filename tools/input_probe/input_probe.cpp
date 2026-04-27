#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <thread>
#include <chrono>

IDirectInput8W* g_pDI = nullptr;
IDirectInputDevice8W* g_pWheel = nullptr;

BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCEW* pdidInstance, VOID* pContext) {
    if (g_pWheel) return DIENUM_STOP;
    HRESULT hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &g_pWheel, nullptr);
    if (FAILED(hr)) return DIENUM_CONTINUE;
    
    std::wcout << L"Found device: " << pdidInstance->tszProductName << L"\n";
    return DIENUM_STOP;
}

int main() {
    std::cout << "Initializing DirectInput...\n";
    if (FAILED(DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (VOID**)&g_pDI, nullptr))) {
        std::cerr << "DirectInput8Create failed\n";
        return 1;
    }

    std::cout << "Searching for game controllers...\n";
    g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, nullptr, DIEDFL_ATTACHEDONLY);
    
    if (!g_pWheel) {
        std::cerr << "No game controller found! Ensure your Moza base is connected and powered on.\n";
        return 1;
    }

    g_pWheel->SetDataFormat(&c_dfDIJoystick2);
    // Use BACKGROUND | NONEXCLUSIVE so it works even when the console isn't focused
    g_pWheel->SetCooperativeLevel(GetConsoleWindow(), DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    g_pWheel->Acquire();

    std::cout << "Wheel connected and acquired. Reading axes (Press Ctrl+C to exit)...\n";
    std::cout << "--------------------------------------------------------\n";

    while (true) {
        if (FAILED(g_pWheel->Poll())) {
            g_pWheel->Acquire();
        }
        DIJOYSTATE2 js;
        if (SUCCEEDED(g_pWheel->GetDeviceState(sizeof(DIJOYSTATE2), &js))) {
            // Carriage return \r to overwrite the same line in the console
            std::cout << "\rX:" << js.lX 
                      << " Y:" << js.lY 
                      << " Z:" << js.lZ 
                      << " Rx:" << js.lRx
                      << " Ry:" << js.lRy
                      << " Rz:" << js.lRz 
                      << " S0:" << js.rglSlider[0]
                      << " S1:" << js.rglSlider[1]
                      << "          " << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    return 0;
}
