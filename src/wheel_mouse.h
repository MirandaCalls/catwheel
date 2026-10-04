#pragma once

#include <stdint.h>

// Reads relative motion from a USB mouse attached to the PIO-USB host port.
// The USB host stack runs on core 1; everything here is safe to call from
// core 0.
namespace WheelMouse {

// Starts the USB host. Call from setup1() on core 1.
void begin();
// Services the USB host. Call from loop1() on core 1.
void task();

// Mouse counts moved since the last call (core 0).
uint32_t takeCounts();
// Whether a mouse is currently plugged in.
bool connected();

}  // namespace WheelMouse
