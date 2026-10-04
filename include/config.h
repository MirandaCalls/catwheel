#pragma once

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------

// USB host port for the mouse, bit-banged by PIO-USB. D- must be D+ + 1.
constexpr uint8_t PIN_USB_HOST_DP = 16;  // GP16 -> mouse D+ (usually green)
                                         // GP17 -> mouse D- (usually white)

// The Pico 2 W has three PIO blocks. The Wi-Fi chip claims a free state
// machine on PIO0, so keep USB host on PIO2 to stay out of its way.
constexpr uint8_t USB_HOST_PIO = 2;
// DMA channel used by PIO-USB for transmit. High number avoids the channels
// the Wi-Fi driver grabs first.
constexpr uint8_t USB_HOST_DMA_CH = 11;

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
