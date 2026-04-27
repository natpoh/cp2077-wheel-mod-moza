#include "mod_settings_seed.h"
#include "config.h"
#include "logging.h"
#include "device_table.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace direct_wheel::mod_settings_seed
{
    namespace
    {
        struct DetectedWheel
        {
            bool     found = false;
            uint16_t pid   = 0;
            char     name[256] = {};
        };

        BOOL CALLBACK EnumDevicesCallback(LPCDIDEVICEINSTANCEW inst, LPVOID ud)
        {
            auto* result = reinterpret_cast<DetectedWheel*>(ud);

            // DIDEVICEINSTANCE.guidProduct.Data1 packs (PID << 16) | VID.
            const uint16_t vid = static_cast<uint16_t>(inst->guidProduct.Data1 & 0xFFFF);
            const uint16_t pid = static_cast<uint16_t>(inst->guidProduct.Data1 >> 16);
            if (vid != static_cast<uint16_t>(kLogitechVid)) return DIENUM_CONTINUE;

            // Only treat it as a wheel if it's in our PID table. This keeps
            // Logitech mice / keyboards / gamepads from triggering a false
            // positive when the user has those plugged in alongside.
            if (!LookupByPid(pid)) return DIENUM_CONTINUE;

            result->found = true;
            result->pid = pid;
            WideCharToMultiByte(CP_UTF8, 0, inst->tszProductName, -1,
                                result->name, sizeof(result->name),
                                nullptr, nullptr);
            return DIENUM_STOP;
        }

        DetectedWheel ProbeWheel()
        {
            DetectedWheel out;
            IDirectInput8W* dinput = nullptr;
            const HRESULT hr = DirectInput8Create(
                GetModuleHandleW(nullptr),
                DIRECTINPUT_VERSION,
                IID_IDirectInput8W,
                reinterpret_cast<LPVOID*>(&dinput),
                nullptr);
            if (FAILED(hr) || !dinput)
            {
                log::WarnF("[direct_wheel] mod_settings_seed: DirectInput8Create failed (hr=0x%08lX); "
                           "treating as no wheel attached",
                           static_cast<unsigned long>(hr));
                return out;
            }
            dinput->EnumDevices(DI8DEVCLASS_GAMECTRL,
                                EnumDevicesCallback,
                                &out,
                                DIEDFL_ATTACHEDONLY);
            dinput->Release();
            return out;
        }

        std::filesystem::path ResolveUserIniPath()
        {
            // direct_wheel.dll lives at <CP2077>/red4ext/plugins/direct_wheel/direct_wheel.dll;
            // mod_settings/user.ini sits one folder up alongside the
            // mod_settings/ plugin folder.
            HMODULE mod = nullptr;
            GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&ResolveUserIniPath),
                &mod);
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(mod, buf, MAX_PATH);
            std::filesystem::path p(buf);
            p = p.parent_path().parent_path(); // -> <CP2077>/red4ext/plugins/
            p /= L"mod_settings";
            p /= L"user.ini";
            return p;
        }

        void WriteFlag(const std::wstring& path, const wchar_t* key, bool value)
        {
            const wchar_t* str = value ? L"1" : L"0";
            WritePrivateProfileStringW(L"DirectWheelSettings", key, str, path.c_str());
        }
    }

    void Run()
    {
        const DetectedWheel det = ProbeWheel();

        bool hasFfb          = false;
        bool hasLeds         = false;
        bool hasRightCluster = false;

        if (det.found)
        {
            const auto* info = LookupByPid(det.pid);
            if (info)
            {
                hasFfb          = info->ffb_default;
                hasLeds         = info->has_rev_leds;
                hasRightCluster = info->has_right_cluster;
                log::InfoF("[direct_wheel] mod_settings_seed: detected '%s' (pid=0x%04X) "
                           "ffb=%d leds=%d rightCluster=%d",
                           det.name, det.pid,
                           hasFfb ? 1 : 0,
                           hasLeds ? 1 : 0,
                           hasRightCluster ? 1 : 0);
            }
        }
        else
        {
            log::Info("[direct_wheel] mod_settings_seed: no Logitech wheel attached "
                      "(capability flags will all be FALSE; UI sections will hide)");
        }

        const auto path = ResolveUserIniPath();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        const std::wstring wpath = path.wstring();
        WriteFlag(wpath, L"hasFfbHardware",  hasFfb);
        WriteFlag(wpath, L"hasRevLeds",      hasLeds);
        WriteFlag(wpath, L"hasRightCluster", hasRightCluster);

        // Mirror the FOMOD-installed handshake choice into mod_settings/user.ini.
        // The FOMOD writes config.json's handshake.playOnStart at install time,
        // and that value drives the actual handshake firing during wheel-bind.
        // But the in-game Mod Settings menu reads user.ini, so without this
        // seed the UI would show the redscript default (false) on a fresh
        // install while the wheel actually played the handshake. Writing it
        // here keeps both stores in lockstep: redscript Push() always rewrites
        // config.json from the listener, and the next launch's seed reflects
        // the user's last in-game toggle.
        const bool handshake = config::Current().handshake.playOnStart;
        WriteFlag(wpath, L"handshakePlayOnStart", handshake);

        log::InfoF("[direct_wheel] mod_settings_seed: wrote [DirectWheelSettings] hasFfbHardware=%d "
                   "hasRevLeds=%d hasRightCluster=%d handshakePlayOnStart=%d -> %ls",
                   hasFfb ? 1 : 0,
                   hasLeds ? 1 : 0,
                   hasRightCluster ? 1 : 0,
                   handshake ? 1 : 0,
                   wpath.c_str());
    }
}
