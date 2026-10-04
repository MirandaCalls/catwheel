#include "wheel_mouse.h"

#include <Arduino.h>
#include <atomic>
#include <math.h>

#include "Adafruit_TinyUSB.h"
#include "pio_usb.h"
#include "config.h"

namespace {

Adafruit_USBH_Host usbHost;

std::atomic<uint32_t> pendingCounts{0};
std::atomic<bool> mouseConnected{false};

// Sub-count remainder of sqrt(dx^2 + dy^2); only touched on core 1.
float fractionalCounts = 0.0f;

void addMotion(int dx, int dy) {
  // Measure total movement rather than one axis, so a mouse mounted at a
  // slight angle to the wheel's direction of travel still reads correctly.
  fractionalCounts += sqrtf(float(dx * dx + dy * dy));
  uint32_t whole = uint32_t(fractionalCounts);
  fractionalCounts -= float(whole);
  if (whole) pendingCounts.fetch_add(whole, std::memory_order_relaxed);
}

}  // namespace

namespace WheelMouse {

void begin() {
  pio_usb_configuration_t cfg = PIO_USB_DEFAULT_CONFIG;
  cfg.pin_dp = PIN_USB_HOST_DP;
  cfg.pio_tx_num = USB_HOST_PIO;
  cfg.pio_rx_num = USB_HOST_PIO;
  cfg.tx_ch = USB_HOST_DMA_CH;
  usbHost.configure_pio_usb(1, &cfg);
  // Boot protocol gives every mouse the same simple report layout.
  tuh_hid_set_default_protocol(HID_PROTOCOL_BOOT);
  usbHost.begin(1);
}

void task() { usbHost.task(); }

uint32_t takeCounts() {
  return pendingCounts.exchange(0, std::memory_order_relaxed);
}

bool connected() { return mouseConnected.load(std::memory_order_relaxed); }

}  // namespace WheelMouse

// TinyUSB host callbacks (run on core 1).
extern "C" {

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const* desc_report, uint16_t desc_len) {
  (void)desc_report;
  (void)desc_len;
  if (tuh_hid_interface_protocol(dev_addr, instance) != HID_ITF_PROTOCOL_MOUSE) {
    return;
  }
  mouseConnected = true;
  tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_MOUSE) {
    mouseConnected = false;
  }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const* report, uint16_t len) {
  if (len >= 3) {
    auto const* mouse = reinterpret_cast<hid_mouse_report_t const*>(report);
    addMotion(mouse->x, mouse->y);
  }
  tuh_hid_receive_report(dev_addr, instance);
}

}  // extern "C"
