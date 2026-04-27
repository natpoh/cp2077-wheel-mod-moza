#include "led.h"
#include "wheel.h"
#include "audio_monitor.h"
#include "sources.h"
#include "config.h"
#include "logging.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace direct_wheel::led
{
    namespace
    {
        std::atomic<bool> g_running{false};
        std::thread       g_thread;

        // Light smoothing on the RPM signal so the LED bar looks like it's
        // following a tachometer rather than teleporting with every
        // Blackboard callback. alpha=0.35 at 30 Hz ≈ 80 ms time constant —
        // responsive enough that revving the engine still snaps the bar
        // toward redline, slow enough that micro-jitter on the RPM value
        // doesn't strobe segments.
        constexpr float kRpmEnvelopeAlpha = 0.35f;

        void Tick(bool& ledsDarkened, float& rpmState)
        {
            // The startup handshake owns the bar while it runs its own
            // sweep / flashes / breath pulse. Don't fight it from here;
            // hand the bar back once the flag drops.
            if (wheel::IsHandshakeActive())
            {
                ledsDarkened = false;
                return;
            }

            // Outside of a vehicle (main menu, on-foot, dialogue, menus)
            // the bar stays dark regardless of anything else.
            if (!sources::InVehicle())
            {
                if (!ledsDarkened)
                {
                    wheel::ClearLeds();
                    ledsDarkened = true;
                }
                rpmState = 0.f;  // envelope resets so next mount starts clean
                return;
            }

            const auto cfg = config::Current();

            if (!cfg.led.enabled)
            {
                if (!ledsDarkened)
                {
                    wheel::ClearLeds();
                    ledsDarkened = true;
                }
                return;
            }
            ledsDarkened = false;

            // Mode selection is driven by the game's own radio state
            // (VehicleDef.VehRadioState, pushed by the redscript
            // Blackboard listener) — NOT by audio-amplitude heuristics.
            // When the radio is on AND the user opted into the music-
            // visualizer mode, the bar tracks the WASAPI-loopback audio
            // level. When off, the bar shows real engine RPM.
            const bool visualize = cfg.led.visualizerWhileMusic
                                   && sources::RadioActive();

            float level = 0.f;
            if (visualize)
            {
                // Audio monitor's CurrentLevel() is the dynamic-range-
                // normalised WASAPI signal. Even with the radio on this
                // still includes game SFX in the same mix, so the bar
                // reacts to both music AND engine/combat — acceptable
                // fallback until a proper music-bus tap is done.
                level = audio_monitor::CurrentLevel();
            }
            else
            {
                // Real engine RPM, already normalised to [0..1] by the
                // redscript listener dividing by VehEngineData.MaxRPM().
                // Smooth so the bar moves like a needle, not like a jump
                // counter.
                const float target = sources::EngineRpmNormalized();
                rpmState += kRpmEnvelopeAlpha * (target - rpmState);
                level = rpmState;
            }

            wheel::PlayLeds(std::clamp(level, 0.f, 1.f));
        }

        void Loop()
        {
            using namespace std::chrono_literals;
            log::Info("[direct_wheel:led] controller thread started (30 Hz)");
            bool  ledsDarkened = false;
            float rpmState     = 0.f;
            while (g_running.load(std::memory_order_acquire))
            {
                Tick(ledsDarkened, rpmState);
                std::this_thread::sleep_for(33ms);
            }
            // Clean exit — leave the LEDs dark so they don't latch on
            // whatever the last value was when the game closes.
            wheel::ClearLeds();
            log::Info("[direct_wheel:led] controller thread stopped");
        }
    }

    void Init()
    {
        if (g_running.exchange(true, std::memory_order_acq_rel)) return;
        g_thread = std::thread(Loop);
    }

    void Shutdown()
    {
        if (!g_running.exchange(false, std::memory_order_acq_rel)) return;
        if (g_thread.joinable()) g_thread.join();
    }
}
