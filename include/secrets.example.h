#pragma once

// Copy this file to secrets.h and fill it in. secrets.h is git-ignored.

#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-wifi-password"

// Address of the Mosquitto broker (your Home Assistant host).
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883
// A Home Assistant user created for the broker (see README).
#define MQTT_USER "catwheel"
#define MQTT_PASSWORD "change-me"

// Password for over-the-air updates. Export the same value as
// CATWHEEL_OTA_PASSWORD before running: pio run -e ota -t upload
#define OTA_PASSWORD "change-me-too"
