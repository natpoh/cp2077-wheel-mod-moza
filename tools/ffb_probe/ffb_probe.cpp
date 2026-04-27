#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <iostream>
#include <thread>
#include <chrono>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

IDirectInput8W* g_pDI = nullptr;
IDirectInputDevice8W* g_pWheel = nullptr;
IDirectInputEffect* g_pConstant = nullptr;
IDirectInputEffect* g_pSpring = nullptr;

BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCEW* pdidInstance, VOID*) {
    if (FAILED(g_pDI->CreateDevice(pdidInstance->guidInstance, &g_pWheel, nullptr)))
        return DIENUM_CONTINUE;
    return DIENUM_STOP;
}

// We need a real Win32 window for DISCL_EXCLUSIVE | DISCL_FOREGROUND (required for FFB).
HWND CreateFFBWindow(HINSTANCE hInst) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.lpszClassName = L"FFBProbeWnd";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(L"FFBProbeWnd", L"FFB Probe",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    // Process pending messages so Windows registers us as foreground
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return hwnd;
}

int main() {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    std::cout << "=== FFB Probe ===" << std::endl;
    std::cout << "Initializing DirectInput..." << std::endl;

    if (FAILED(DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8W,
                                   (VOID**)&g_pDI, nullptr))) {
        std::cout << "DirectInput8Create FAILED" << std::endl;
        return 1;
    }

    g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, nullptr, DIEDFL_ATTACHEDONLY);
    if (!g_pWheel) {
        std::cout << "No wheel found!" << std::endl;
        return 1;
    }
    std::cout << "Wheel found!" << std::endl;

    g_pWheel->SetDataFormat(&c_dfDIJoystick2);

    // Create a real window and bring it to front
    HWND hwnd = CreateFFBWindow(hInst);
    std::cout << "Window HWND = " << hwnd << std::endl;

    HRESULT hr = g_pWheel->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    std::cout << "SetCooperativeLevel hr = " << std::hex << hr << std::dec << std::endl;
    if (FAILED(hr)) {
        std::cout << "SetCooperativeLevel FAILED" << std::endl;
        return 1;
    }

    // Acquire FIRST (some drivers need device acquired before creating effects)
    hr = g_pWheel->Acquire();
    std::cout << "Acquire hr = " << std::hex << hr << std::dec << std::endl;
    if (FAILED(hr)) {
        std::cout << "Acquire FAILED — make sure this window is in focus!" << std::endl;
        // Try once more after pumping messages
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        SetForegroundWindow(hwnd);
        Sleep(200);
        hr = g_pWheel->Acquire();
        std::cout << "Acquire retry hr = " << std::hex << hr << std::dec << std::endl;
        if (FAILED(hr)) {
            std::cout << "Still failed. Exiting." << std::endl;
            return 1;
        }
    }

    // Disable autocenter
    DIPROPDWORD dipdw = {};
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = DIPROPAUTOCENTER_OFF;
    hr = g_pWheel->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph);
    std::cout << "AutoCenter OFF hr = " << std::hex << hr << std::dec << std::endl;

    // Setup common effect structure
    DWORD rgdwAxes[1] = { DIJOFS_X };
    LONG rglDirection[1] = { 0 };

    DIEFFECT diEffect = {};
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

    // === TEST 1: Constant Force ===
    DICONSTANTFORCE cf = {};
    cf.lMagnitude = 0;
    diEffect.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    diEffect.lpvTypeSpecificParams = &cf;
    hr = g_pWheel->CreateEffect(GUID_ConstantForce, &diEffect, &g_pConstant, nullptr);
    std::cout << "CreateEffect ConstantForce hr = " << std::hex << hr << std::dec << std::endl;

    // === TEST 2: Spring ===
    DICONDITION spr = {};
    diEffect.cbTypeSpecificParams = sizeof(DICONDITION);
    diEffect.lpvTypeSpecificParams = &spr;
    hr = g_pWheel->CreateEffect(GUID_Spring, &diEffect, &g_pSpring, nullptr);
    std::cout << "CreateEffect Spring hr = " << std::hex << hr << std::dec << std::endl;

    // ---------- Run the effects ----------

    if (g_pConstant) {
        std::cout << "\n>>> [1/3] Constant Force RIGHT (50%) for 3 sec..." << std::endl;
        cf.lMagnitude = 5000;
        DIEFFECT upd = {};
        upd.dwSize = sizeof(DIEFFECT);
        upd.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        upd.lpvTypeSpecificParams = &cf;
        hr = g_pConstant->SetParameters(&upd, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        std::cout << "   SetParameters hr = " << std::hex << hr << std::dec << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::cout << ">>> [2/3] Constant Force LEFT (50%) for 3 sec..." << std::endl;
        cf.lMagnitude = -5000;
        upd.lpvTypeSpecificParams = &cf;
        hr = g_pConstant->SetParameters(&upd, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        std::cout << "   SetParameters hr = " << std::hex << hr << std::dec << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        g_pConstant->Stop();
        std::cout << "   Constant stopped." << std::endl;
    } else {
        std::cout << "ConstantForce effect not created, skipping." << std::endl;
    }

    if (g_pSpring) {
        std::cout << "\n>>> [3/3] Strong Centering Spring for 5 sec (try turning!)..." << std::endl;
        spr.lPositiveCoefficient = 10000;
        spr.lNegativeCoefficient = 10000;
        spr.dwPositiveSaturation = 10000;
        spr.dwNegativeSaturation = 10000;
        spr.lOffset = 0;
        spr.lDeadBand = 0;
        DIEFFECT upd = {};
        upd.dwSize = sizeof(DIEFFECT);
        upd.cbTypeSpecificParams = sizeof(DICONDITION);
        upd.lpvTypeSpecificParams = &spr;
        hr = g_pSpring->SetParameters(&upd, DIEP_TYPESPECIFICPARAMS | DIEP_START);
        std::cout << "   SetParameters hr = " << std::hex << hr << std::dec << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        g_pSpring->Stop();
        std::cout << "   Spring stopped." << std::endl;
    } else {
        std::cout << "Spring effect not created, skipping." << std::endl;
    }

    std::cout << "\nDone! Releasing resources..." << std::endl;
    if (g_pConstant) g_pConstant->Release();
    if (g_pSpring) g_pSpring->Release();
    g_pWheel->Unacquire();
    g_pWheel->Release();
    g_pDI->Release();
    DestroyWindow(hwnd);

    return 0;
}
