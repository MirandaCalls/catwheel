#pragma once

#include <Arduino.h>

#include "settings.h"

// Web pages served by the Pico, reachable from any browser (including an iPad):
// - Setup mode: an open "CatWheel-Setup" Wi-Fi network with a captive page for
//   entering Wi-Fi, MQTT and admin details.
// - Normal mode: http://catwheel.local/ shows status, /settings edits the
//   settings and /update installs new firmware (both need the admin login).
namespace WebUi {

struct Status {
  double lifetimeMeters;
  uint32_t sessionCounts;
  bool mouseConnected;
  bool mqttConnected;
};

using StatusFn = Status (*)();
// Called just before the Pico restarts after settings are saved.
using RestartFn = void (*)();

void beginSetup(Settings& settings, RestartFn beforeRestart);
void beginNormal(Settings& settings, StatusFn status, RestartFn beforeRestart);
void loop();
// millis() of the last page request, for the setup-mode timeout.
uint32_t lastRequestMs();

}  // namespace WebUi
