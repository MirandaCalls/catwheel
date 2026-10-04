// Cat wheel tracker: counts how far the cat runs on her wheel using a USB
// mouse, and reports it to Home Assistant over MQTT.
//
// Core 1 runs the USB host that reads the mouse (see wheel_mouse.cpp).
// Core 0 (this file) converts counts to distance, saves the lifetime total
// to flash, and talks to Home Assistant.
//
// The micro-USB port is busy as the mouse's host port, so logs go to UART0
// (GP0 TX, GP1 RX) and firmware updates go over Wi-Fi.

#include <Arduino.h>
#include <ArduinoHA.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "config.h"
#include "wheel_mouse.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.example.h to include/secrets.h and fill it in."
#endif

// Debug log on UART0; read it with a USB-serial adapter or a Debug Probe.
#define Log Serial1

namespace {

constexpr const char* LIFETIME_FILE = "/lifetime.bin";

WiFiClient wifiClient;
HADevice device;
HAMqtt mqtt(wifiClient, device);

HASensorNumber lifetimeSensor("lifetime_distance", HASensorNumber::PrecisionP2);
HASensorNumber speedSensor("speed", HASensorNumber::PrecisionP1);
HASensorNumber rawCountsSensor("raw_counts");
HABinarySensor runningSensor("running");
HABinarySensor mouseSensor("mouse_connected");

double lifetimeMeters = 0.0;
// Counts since boot, used for calibration.
uint32_t sessionCounts = 0;

uint32_t lastMotionMs = 0;
uint32_t lastPublishMs = 0;
uint32_t lastSaveMs = 0;
bool unsavedDistance = false;
bool unpublishedDistance = true;

uint32_t speedWindowStartMs = 0;
uint32_t speedWindowCounts = 0;

void loadLifetime() {
  File f = LittleFS.open(LIFETIME_FILE, "r");
  if (!f) return;
  double saved = 0.0;
  if (f.read(reinterpret_cast<uint8_t*>(&saved), sizeof(saved)) == sizeof(saved) &&
      saved >= 0.0) {
    lifetimeMeters = saved;
  }
  f.close();
}

void saveLifetime() {
  File f = LittleFS.open(LIFETIME_FILE, "w");
  if (!f) {
    Log.println("Failed to save lifetime distance");
    return;
  }
  f.write(reinterpret_cast<const uint8_t*>(&lifetimeMeters), sizeof(lifetimeMeters));
  f.close();
  unsavedDistance = false;
  lastSaveMs = millis();
}

void setupHomeAssistant() {
  byte mac[6];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.setName("Cat Wheel");
  device.setManufacturer("DIY");
  device.setModel("Pico 2 W wheel tracker");
  device.setSoftwareVersion("0.1.0");
  device.enableSharedAvailability();
  device.enableLastWill();

  lifetimeSensor.setName("Lifetime distance");
  lifetimeSensor.setDeviceClass("distance");
  lifetimeSensor.setStateClass("total_increasing");
  lifetimeSensor.setUnitOfMeasurement("m");
  lifetimeSensor.setIcon("mdi:cat");

  speedSensor.setName("Speed");
  speedSensor.setDeviceClass("speed");
  speedSensor.setStateClass("measurement");
  speedSensor.setUnitOfMeasurement("km/h");

  rawCountsSensor.setName("Raw mouse counts");
  rawCountsSensor.setIcon("mdi:counter");

  runningSensor.setName("Running");
  runningSensor.setDeviceClass("moving");

  mouseSensor.setName("Mouse connected");
  mouseSensor.setDeviceClass("connectivity");

  mqtt.begin(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);
}

void updateSpeed(uint32_t now, uint32_t counts) {
  speedWindowCounts += counts;
  uint32_t elapsed = now - speedWindowStartMs;
  if (elapsed < SPEED_WINDOW_MS) return;

  float metersPerSecond = (speedWindowCounts / COUNTS_PER_METER) / (elapsed / 1000.0f);
  speedSensor.setValue(metersPerSecond * 3.6f);
  speedWindowStartMs = now;
  speedWindowCounts = 0;
}

}  // namespace

void setup() {
  Log.begin(115200);

  if (!LittleFS.begin()) {
    Log.println("LittleFS mount failed; lifetime distance won't persist");
  }
  loadLifetime();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  setupHomeAssistant();

  ArduinoOTA.setHostname("catwheel");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  // Save the distance so far before the update reboots the Pico. LittleFS
  // stays mounted: the update image is staged there.
  ArduinoOTA.onStart([] {
    if (unsavedDistance) saveLifetime();
  });
  ArduinoOTA.begin();

  // Values HA shows before the wheel first moves.
  lifetimeSensor.setCurrentValue(float(lifetimeMeters));
  speedSensor.setCurrentValue(0.0f);
  runningSensor.setCurrentState(false);
}

void loop() {
  uint32_t now = millis();
  uint32_t counts = WheelMouse::takeCounts();

  if (counts) {
    sessionCounts += counts;
    lifetimeMeters += counts / COUNTS_PER_METER;
    lastMotionMs = now;
    unsavedDistance = true;
    unpublishedDistance = true;
  }

  bool running = lastMotionMs && (now - lastMotionMs < RUNNING_TIMEOUT_MS);
  runningSensor.setState(running);
  mouseSensor.setState(WheelMouse::connected());
  updateSpeed(now, counts);

  if (unpublishedDistance && now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    lifetimeSensor.setValue(float(lifetimeMeters));
    rawCountsSensor.setValue(sessionCounts);
    unpublishedDistance = false;
    lastPublishMs = now;
    Log.printf("counts=%lu lifetime=%.2f m\n", (unsigned long)sessionCounts,
                  lifetimeMeters);
  }

  if (unsavedDistance && (!running || now - lastSaveMs >= SAVE_INTERVAL_MS)) {
    saveLifetime();
  }

  mqtt.loop();
  ArduinoOTA.handle();
}

// Core 1: USB host for the mouse.
void setup1() { WheelMouse::begin(); }

void loop1() { WheelMouse::task(); }
