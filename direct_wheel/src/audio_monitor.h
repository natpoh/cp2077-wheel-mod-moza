#pragma once

// direct_wheel::audio_monitor — WASAPI loopback capture of the default render
// endpoint, band-passed to 40-160 Hz (kick/bass region) and reduced to
// a single dynamic-range-normalised amplitude in [0..1] suitable for
// driving the LED bar.
//
// Runs on its own thread. Captures whatever the system mixes to the
// default output device — CP2077's full audio post-mix (music + engine
// + dialogue + SFX) because we can't isolate the Wwise music bus from
// the outside. The bass-band bandpass emphasises music kick/bass
// transients over the continuous engine rumble, which is the best
// approximation available without an internal Wwise tap.
//
// Mode gating (rev strip vs. visualizer) lives in the LED controller,
// driven by the game's radio-state Blackboard field — NOT by audio
// classification in here. This module just publishes a level; the LED
// side decides when to use it.

namespace direct_wheel::audio_monitor
{
    // Start the capture thread. Idempotent.
    void Init();

    // Stop the capture thread and release the WASAPI endpoint.
    void Shutdown();

    // Most recent normalised bass-band amplitude, 0..1. Stretched
    // across the rolling-window dynamic range so loud and quiet songs
    // both swing the bar visibly. Zero when the input is silent.
    float CurrentLevel();
}
