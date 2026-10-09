#include "web_ui.h"

#include <DNSServer.h>
#include <HTTPUpdateServer.h>
#include <LEAmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

namespace {

constexpr const char* ADMIN_USER = "admin";

WebServer server(80);
HTTPUpdateServer updateServer;
DNSServer dnsServer;

Settings* settings = nullptr;
WebUi::StatusFn statusFn = nullptr;
WebUi::RestartFn restartFn = nullptr;
bool setupMode = false;
uint32_t lastRequest = 0;
uint32_t restartAtMs = 0;

const char PAGE_HEAD[] PROGMEM = R"(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Cat Wheel</title><style>
body{font-family:-apple-system,system-ui,sans-serif;max-width:32rem;margin:0 auto;padding:1rem;line-height:1.4}
label{display:block;margin-top:.8rem;font-weight:600}
input{width:100%;box-sizing:border-box;padding:.5rem;font-size:1rem}
button{margin-top:1.2rem;padding:.6rem 1.2rem;font-size:1rem}
small{color:#666}td{padding:.2rem .8rem .2rem 0}
</style></head><body><h1>Cat Wheel</h1>)";

String escape(const String& s) {
  String out;
  out.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

// Form value with newlines stripped (settings are stored one per line).
String formArg(const char* name) {
  String v = server.arg(name);
  v.replace("\r", "");
  v.replace("\n", "");
  return v;
}

void sendPage(int code, const String& body) {
  server.send(code, "text/html", String(FPSTR(PAGE_HEAD)) + body + "</body></html>");
}

bool requireAdmin() {
  if (setupMode) return true;  // Only reachable on the Pico's own setup network.
  if (server.authenticate(ADMIN_USER, settings->adminPassword.c_str())) return true;
  server.requestAuthentication();
  return false;
}

String textField(const char* name, const char* label, const String& value,
                 const char* type = "text") {
  return String("<label for=") + name + ">" + label + "</label><input id=" + name +
         " name=" + name + " type=\"" + type + "\" value=\"" + escape(value) + "\">";
}

// Password inputs never echo the stored value; leaving one blank keeps it.
String passwordField(const char* name, const char* label, bool hasValue) {
  return String("<label for=") + name + ">" + label + "</label><input id=" + name +
         " name=" + name + " type=password autocomplete=new-password placeholder=\"" +
         (hasValue ? "unchanged" : "") + "\">";
}

void handleSettingsForm() {
  if (!requireAdmin()) return;
  lastRequest = millis();
  const Settings& s = *settings;
  String body;
  if (setupMode) body += "<p>Connect the Cat Wheel to your home network.</p>";
  body += "<form method=post action=/settings><h2>Wi-Fi</h2>";
  body += textField("wifi_ssid", "Network name", s.wifiSsid);
  body += passwordField("wifi_password", "Password", s.wifiPassword.length());
  body += "<h2>Home Assistant (MQTT)</h2>";
  body += textField("mqtt_host", "Broker address", s.mqttHost);
  body += "<small>Usually your Home Assistant address, e.g. homeassistant.local</small>";
  body += textField("mqtt_port", "Port", String(s.mqttPort), "number");
  body += textField("mqtt_user", "Username", s.mqttUser);
  body += passwordField("mqtt_password", "Password", s.mqttPassword.length());
  body += "<h2>Calibration</h2>";
  body += textField("counts_per_meter", "Mouse counts per meter", String(s.countsPerMeter, 1),
                    "number\" step=\"any");
  body += "<small>See the README for how to measure this.</small>";
  body += "<h2>Admin password</h2>";
  body += passwordField("admin_password", "Password for settings and updates",
                        s.adminPassword.length());
  body += "<small>Username is <b>admin</b>.</small>";
  body += "<br><button type=submit>Save and restart</button></form>";
  sendPage(200, body);
}

void handleSettingsSave() {
  if (!requireAdmin()) return;
  lastRequest = millis();
  Settings updated = *settings;
  updated.wifiSsid = formArg("wifi_ssid");
  updated.mqttHost = formArg("mqtt_host");
  updated.mqttPort = formArg("mqtt_port").toInt() > 0 ? formArg("mqtt_port").toInt() : 1883;
  updated.mqttUser = formArg("mqtt_user");
  if (formArg("counts_per_meter").toFloat() > 0) {
    updated.countsPerMeter = formArg("counts_per_meter").toFloat();
  }
  if (formArg("wifi_password").length()) updated.wifiPassword = formArg("wifi_password");
  if (formArg("mqtt_password").length()) updated.mqttPassword = formArg("mqtt_password");
  if (formArg("admin_password").length()) updated.adminPassword = formArg("admin_password");

  if (!updated.configured()) {
    sendPage(400, "<p>A Wi-Fi network name and an admin password are required.</p>"
                  "<p><a href=/settings>Back</a></p>");
    return;
  }
  if (!updated.save()) {
    sendPage(500, "<p>Couldn't save settings to flash.</p><p><a href=/settings>Back</a></p>");
    return;
  }
  // The running settings stay untouched (MQTT still points at them); the
  // restart picks up the saved ones.
  sendPage(200, "<p>Saved. The Cat Wheel is restarting and joining <b>" +
                    escape(updated.wifiSsid) +
                    "</b>.</p><p>Reconnect this device to your home Wi-Fi, then open "
                    "<a href=http://catwheel.local/>http://catwheel.local/</a>.</p>");
  restartAtMs = millis() + 1500;  // Let the response reach the browser first.
}

void handleStatus() {
  lastRequest = millis();
  WebUi::Status st = statusFn();
  String body = "<table>";
  body += "<tr><td>Lifetime distance</td><td>" + String(st.lifetimeMeters, 2) + " m</td></tr>";
  body += "<tr><td>Mouse</td><td>" + String(st.mouseConnected ? "connected" : "not connected") +
          "</td></tr>";
  body += "<tr><td>Raw counts since boot</td><td>" + String(st.sessionCounts) + "</td></tr>";
  body += "<tr><td>Home Assistant</td><td>" +
          String(st.mqttConnected ? "connected" : "not connected") + "</td></tr>";
  body += "</table><p><a href=/settings>Settings</a> &middot; <a href=/update>Update firmware</a></p>";
  sendPage(200, body);
}

void redirectToSetup() {
  lastRequest = millis();
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/settings");
  server.send(302, "text/plain", "");
}

}  // namespace

namespace WebUi {

void beginSetup(Settings& s, RestartFn beforeRestart) {
  settings = &s;
  restartFn = beforeRestart;
  setupMode = true;

  // Answer every DNS lookup with our address so phones and tablets open the
  // setup page automatically (captive portal).
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.on("/settings", HTTP_GET, handleSettingsForm);
  server.on("/settings", HTTP_POST, handleSettingsSave);
  server.onNotFound(redirectToSetup);
  server.begin();
  lastRequest = millis();
}

void beginNormal(Settings& s, StatusFn status, RestartFn beforeRestart) {
  settings = &s;
  statusFn = status;
  restartFn = beforeRestart;
  setupMode = false;

  server.on("/", HTTP_GET, handleStatus);
  server.on("/settings", HTTP_GET, handleSettingsForm);
  server.on("/settings", HTTP_POST, handleSettingsSave);
  updateServer.setup(&server, "/update", ADMIN_USER, s.adminPassword);
  server.begin();

  MDNS.begin("catwheel");
  MDNS.addService("http", "tcp", 80);
}

void loop() {
  if (setupMode) dnsServer.processNextRequest();
  else MDNS.update();
  server.handleClient();

  if (restartAtMs && int32_t(millis() - restartAtMs) >= 0) {
    if (restartFn) restartFn();
    rp2040.reboot();
  }
}

uint32_t lastRequestMs() { return lastRequest; }

}  // namespace WebUi
