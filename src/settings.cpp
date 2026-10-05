#include "settings.h"

#include <LittleFS.h>

namespace {

constexpr const char* SETTINGS_FILE = "/settings.txt";

// One "key=value" per line. Values never contain newlines: the web form
// strips them before saving.
void writeLine(File& f, const char* key, const String& value) {
  f.print(key);
  f.print('=');
  f.print(value);
  f.print('\n');
}

}  // namespace

bool Settings::load() {
  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);
    if (key == "wifi_ssid") wifiSsid = value;
    else if (key == "wifi_password") wifiPassword = value;
    else if (key == "mqtt_host") mqttHost = value;
    else if (key == "mqtt_port") mqttPort = value.toInt() ? value.toInt() : 1883;
    else if (key == "mqtt_user") mqttUser = value;
    else if (key == "mqtt_password") mqttPassword = value;
    else if (key == "admin_password") adminPassword = value;
    else if (key == "counts_per_meter" && value.toFloat() > 0) countsPerMeter = value.toFloat();
  }
  f.close();
  return true;
}

bool Settings::save() const {
  File f = LittleFS.open(SETTINGS_FILE, "w");
  if (!f) return false;
  writeLine(f, "wifi_ssid", wifiSsid);
  writeLine(f, "wifi_password", wifiPassword);
  writeLine(f, "mqtt_host", mqttHost);
  writeLine(f, "mqtt_port", String(mqttPort));
  writeLine(f, "mqtt_user", mqttUser);
  writeLine(f, "mqtt_password", mqttPassword);
  writeLine(f, "admin_password", adminPassword);
  writeLine(f, "counts_per_meter", String(countsPerMeter, 1));
  f.close();
  return true;
}
