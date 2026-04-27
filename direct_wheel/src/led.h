#pragma once

// direct_wheel::led — rev-strip LED controller for Logitech steering wheels.
//
// Runs a low-rate (~30 Hz) thread that decides what the 10-segment LED
// bar on top of the wheel should show each frame, then ships the value
// through LogiPlayLeds via wheel::PlayLeds.
//
// Decision tree per tick:
//   1. If the master LED toggle is off   → dark (ClearLeds once).
//   2. Else if visualizer-while-music is on AND audio is detected above
//      the noise gate → audio amplitude drives the bar.
//   3. Else → vehicle forward speed drives the bar (rev-strip mode).
//
// Rev-strip mapping is speed-as-RPM-proxy — CP2077 doesn't expose engine
// RPM on a stable offset, and the arcade handling model has no real
// gearing, so mapping speed to the bar is both more honest and more
// usable than faking a tachometer.

namespace direct_wheel::led
{
    // Start the controller thread. Safe to call multiple times (idempotent).
    void Init();

    // Stop the controller thread and dark the LED bar on the way out.
    void Shutdown();
}
