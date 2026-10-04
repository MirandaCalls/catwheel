#pragma once

// ---------------------------------------------------------------------------
// Calibration
// ---------------------------------------------------------------------------

// Mouse counts per meter of wheel travel. Measure it (see README):
//   counts_per_meter = counts_for_N_turns / (N * PI * wheel_diameter_m)
// 39370 is what a 1000 CPI mouse reads on a perfectly flat surface.
constexpr float COUNTS_PER_METER = 39370.0f;

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------

// The wheel counts as "running" while it moved within this window.
constexpr uint32_t RUNNING_TIMEOUT_MS = 3000;
// Speed is averaged over this window.
constexpr uint32_t SPEED_WINDOW_MS = 2000;
// How often distance is pushed to Home Assistant while it is changing.
constexpr uint32_t PUBLISH_INTERVAL_MS = 5000;
// Lifetime distance is saved to flash once the wheel stops, and at most this
// often while it keeps running (flash has limited write cycles).
constexpr uint32_t SAVE_INTERVAL_MS = 10UL * 60UL * 1000UL;
