#pragma once

// ---------------------------------------------------------------------------
// Calibration
// ---------------------------------------------------------------------------

// Mouse counts per meter of wheel travel until it's calibrated on the
// settings page (see README). 39370 is what a 1000 CPI mouse reads on a
// perfectly flat surface.
constexpr float DEFAULT_COUNTS_PER_METER = 39370.0f;

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

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

// Open Wi-Fi network the Pico creates for entering settings.
constexpr const char* SETUP_NETWORK_NAME = "CatWheel-Setup";
// Start the setup network if the home Wi-Fi isn't joined this long after boot.
constexpr uint32_t WIFI_SETUP_FALLBACK_MS = 3UL * 60UL * 1000UL;
// Leave the setup network (and retry the home Wi-Fi) after this long unused.
constexpr uint32_t SETUP_IDLE_TIMEOUT_MS = 10UL * 60UL * 1000UL;
constexpr uint32_t WIFI_RETRY_MS = 30000;
// Hold BOOTSEL this long while running to open the setup network.
constexpr uint32_t BOOTSEL_SETUP_HOLD_MS = 5000;
