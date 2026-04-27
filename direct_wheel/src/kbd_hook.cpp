#include "kbd_hook.h"
#include "sources.h"
#include "logging.h"

#include <windows.h>

#include <atomic>
#include <thread>

namespace direct_wheel::kbd_hook
{
    namespace
    {
        std::atomic<bool> g_running{false};
        std::atomic<bool> g_installed{false};
        std::thread       g_thread;
        DWORD             g_threadId = 0;
        HHOOK             g_hook     = nullptr;

        // VK codes corresponding to every keyboard key the plugin binds for
        // VehicleOnly actions. When V is on foot, we swallow injected events
        // for these keys so G HUB's parallel keyboard emission (paddle ->
        // Space, etc.) can't produce on-foot side-effects like jumping.
        // Physical keypresses from the user's actual keyboard are never
        // touched (LLKHF_INJECTED is not set for them).
        bool IsVehicleOnlyKey(DWORD vk)
        {
            switch (vk)
            {
            case 'Z':       // Horn
            case 'F':       // ExitVehicle
            case 'V':       // Headlights
            case 'G':       // Autodrive
            case 'Q':       // CameraCycleForward
            case 'C':       // CameraReset
            case 'R':       // RadioMenu
            case 'B':       // HolsterWeapon
            case 'X':       // UseConsumable
            case 'E':       // IconicCyberware
            case '1':       // WeaponSlot1
            case '2':       // WeaponSlot2
            case VK_SPACE:  // Handbrake
            case VK_LCONTROL: // ShootTertiary
            case VK_MENU:   // SwitchWeapons (Alt)
                return true;
            default:
                return false;
            }
        }

        LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam)
        {
            if (nCode == HC_ACTION)
            {
                auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
                const bool injected = (info->flags & LLKHF_INJECTED) != 0;
                const bool self     = injected &&
                                      info->dwExtraInfo == kExtraInfoTag;

                if (injected && !self &&
                    !sources::InVehicle() &&
                    IsVehicleOnlyKey(info->vkCode))
                {
                    // Non-zero return stops propagation — event does not
                    // reach the game. Rate-limited log so we don't flood on
                    // held keys.
                    static std::atomic<uint64_t> s_swallowed{0};
                    const uint64_t n = s_swallowed.fetch_add(1, std::memory_order_relaxed) + 1;
                    if (n == 1 || (n % 100) == 0)
                        log::InfoF("[direct_wheel:kbd] swallowed injected vk=0x%02X on-foot (count=%llu)",
                                   static_cast<unsigned>(info->vkCode),
                                   static_cast<unsigned long long>(n));
                    return 1;
                }
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        }

        void HookThread()
        {
            g_threadId = GetCurrentThreadId();
            g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookProc,
                                       GetModuleHandleW(nullptr), 0);
            if (!g_hook)
            {
                log::WarnF("[direct_wheel:kbd] SetWindowsHookExW failed: %lu",
                           GetLastError());
                return;
            }
            g_installed.store(true, std::memory_order_release);
            log::Info("[direct_wheel:kbd] low-level keyboard hook installed "
                      "(G HUB on-foot suppression)");

            // Low-level hook thread must pump messages for the hook to
            // receive callbacks. PostThreadMessage(WM_QUIT) breaks us out.
            MSG msg;
            while (g_running.load(std::memory_order_acquire))
            {
                const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
                if (got == 0 || got == -1) break; // WM_QUIT or error
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            UnhookWindowsHookEx(g_hook);
            g_hook = nullptr;
            g_installed.store(false, std::memory_order_release);
            log::Info("[direct_wheel:kbd] keyboard hook uninstalled");
        }
    }

    bool Install()
    {
        if (g_running.exchange(true)) return g_installed.load();
        g_thread = std::thread(HookThread);
        return true;
    }

    void Uninstall()
    {
        if (!g_running.exchange(false)) return;
        if (g_threadId != 0)
            PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
        if (g_thread.joinable()) g_thread.join();
        g_threadId = 0;
    }

    bool IsInstalled() { return g_installed.load(std::memory_order_acquire); }
}
