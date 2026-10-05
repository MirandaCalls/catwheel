// Cat wheel tracker: counts how far the cat runs on her wheel using a USB
// mouse, and reports it to Home Assistant over MQTT.
//
// Core 1 runs the USB host that reads the mouse (see wheel_mouse.cpp).
// Core 0 (this file) converts counts to distance, saves the lifetime total
// to flash, and talks to Home Assistant.
//
// The micro-USB port is busy as the mouse's host port, so logs go to UART0
// (GP0 TX, GP1 RX). Settings and firmware updates go through web pages (see
// web_ui.cpp), so everything can be done from a phone or tablet.

#include <Arduino.h>
#include <ArduinoHA.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "config.h"
#include "settings.h"
#include "web_ui.h"
#include "wheel_mouse.h"

// Debug log on UART0; read it with a USB-serial adapter or a Debug Probe.
#define Log Serial1

namespace {

constexpr const char* LIFETIME_FILE = "/lifetime.bin";
// Present when the next boot should start the setup network.
constexpr const char* SETUP_REQUEST_FILE = "/setup_requested";

Settings settings;
bool setupMode = false;
bool webStarted = false;
bool everConnected = false;
uint32_t lastWifiAttemptMs = 0;
uint32_t bootselPressedMs = 0;
uint32_t lastBootselPollMs = 0;

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

void saveIfNeeded() {
  if (unsavedDistance) saveLifetime();
}

// Restarts into setup mode. Going through a restart keeps the web server's
// two configurations (setup network vs. home network) from ever mixing.
void restartIntoSetup() {
  Log.println("Restarting into setup mode");
  File f = LittleFS.open(SETUP_REQUEST_FILE, "w");
  if (f) f.close();
  saveIfNeeded();
  rp2040.reboot();
}

void startSetupMode() {
  setupMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(SETUP_NETWORK_NAME);
  WebUi::beginSetup(settings, saveIfNeeded);
  Log.printf("Setup mode: join Wi-Fi \"%s\" and open http://%s/\n", SETUP_NETWORK_NAME,
             WiFi.softAPIP().toString().c_str());
}

WebUi::Status currentStatus() {
  return {lifetimeMeters, sessionCounts, WheelMouse::connected(), mqtt.isConnected()};
}

// Normal mode: keeps Wi-Fi up, starts the web pages once connected, and falls
// back to the setup network if the home network never comes up.
void serviceWifi(uint32_t now) {
  if (WiFi.status() == WL_CONNECTED) {
    if (!everConnected) {
      everConnected = true;
      Log.printf("Wi-Fi connected: http://catwheel.local/ (%s)\n",
                 WiFi.localIP().toString().c_str());
    }
    if (!webStarted) {
      WebUi::beginNormal(settings, currentStatus, saveIfNeeded);
      webStarted = true;
    }
    if (settings.mqttHost.length()) mqtt.loop();
    return;
  }

  if (!everConnected && now >= WIFI_SETUP_FALLBACK_MS) {
    Log.println("Couldn't join Wi-Fi");
    restartIntoSetup();
  }
  if (now - lastWifiAttemptMs >= WIFI_RETRY_MS) {
    lastWifiAttemptMs = now;
    WiFi.beginNoBlock(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  }
}

// Holding BOOTSEL for a few seconds opens the setup network, e.g. after
// changing Wi-Fi networks. Polled slowly: reading it briefly pauses core 1.
void checkBootsel(uint32_t now) {
  if (now - lastBootselPollMs < 250) return;
  lastBootselPollMs = now;
  if (!BOOTSEL) {
    bootselPressedMs = 0;
    return;
  }
  if (!bootselPressedMs) bootselPressedMs = now;
  if (now - bootselPressedMs >= BOOTSEL_SETUP_HOLD_MS) restartIntoSetup();
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

  if (settings.mqttHost.length()) {
    mqtt.begin(settings.mqttHost.c_str(), settings.mqttPort,
               settings.mqttUser.length() ? settings.mqttUser.c_str() : nullptr,
               settings.mqttPassword.length() ? settings.mqttPassword.c_str() : nullptr);
  }
}

void updateSpeed(uint32_t now, uint32_t counts) {
  speedWindowCounts += counts;
  uint32_t elapsed = now - speedWindowStartMs;
  if (elapsed < SPEED_WINDOW_MS) return;

  float metersPerSecond = (speedWindowCounts / settings.countsPerMeter) / (elapsed / 1000.0f);
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

  settings.load();

  bool setupRequested = LittleFS.exists(SETUP_REQUEST_FILE);
  if (setupRequested) LittleFS.remove(SETUP_REQUEST_FILE);
  if (setupRequested || !settings.configured()) {
    startSetupMode();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("catwheel");
    WiFi.beginNoBlock(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    lastWifiAttemptMs = millis();
    setupHomeAssistant();
  }

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
    lifetimeMeters += counts / settings.countsPerMeter;
    lastMotionMs = now;
    unsavedDistance = true;
    unpublishedDistance = true;
  }

  bool running = lastMotionMs && (now - lastMotionMs < RUNNING_TIMEOUT_MS);
  runningSensor.setState(running);
  mouseSensor.setState(WheelMouse::connected());
  updateSpeed(now, counts);

  if (unpublishedDistance && now - lastPublishMs >= PUBLISH_INTERVAL_MS) {
    // Only counts as published once MQTT accepted it; otherwise retry later.
    unpublishedDistance = !lifetimeSensor.setValue(float(lifetimeMeters));
    rawCountsSensor.setValue(sessionCounts);
    lastPublishMs = now;
    Log.printf("counts=%lu lifetime=%.2f m\n", (unsigned long)sessionCounts,
                  lifetimeMeters);
  }

  if (unsavedDistance && (!running || now - lastSaveMs >= SAVE_INTERVAL_MS)) {
    saveLifetime();
  }

  if (setupMode) {
    // Give up on setup after a while if there are settings to go back to,
    // e.g. the home network was only down temporarily.
    if (settings.configured() && now - WebUi::lastRequestMs() >= SETUP_IDLE_TIMEOUT_MS) {
      saveIfNeeded();
      rp2040.reboot();
    }
  } else {
    serviceWifi(now);
    checkBootsel(now);
  }
  if (setupMode || webStarted) WebUi::loop();
}

// Core 1: USB host for the mouse.
void setup1() { WheelMouse::begin(); }

void loop1() { WheelMouse::task(); }
