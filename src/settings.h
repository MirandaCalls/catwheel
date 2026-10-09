#pragma once

#include <Arduino.h>

#include "config.h"

// Network settings entered on the setup page and stored in flash, so the
// firmware itself contains no passwords.
struct Settings {
  String wifiSsid;
  String wifiPassword;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;
  // Calibration: mouse counts per meter of wheel travel.
  float countsPerMeter = DEFAULT_COUNTS_PER_METER;
  // Protects the settings and firmware update pages (username "admin").
  String adminPassword;

  bool configured() const { return wifiSsid.length() && adminPassword.length(); }

  // Load from / save to LittleFS. LittleFS must already be mounted.
  bool load();
  bool save() const;
};
