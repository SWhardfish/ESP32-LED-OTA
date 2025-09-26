#include "WifiManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include "WebFiles.h"
#include "LedIndicator.h"
extern LedIndicator led; // weak link - main instantiates; or pass as dependency if desired

static WebServer server(80);

WifiManager::WifiManager() : _ssid(""), _password(""), _connected(false), _cb(nullptr), _connectStart(0) {}

void WifiManager::begin() {
  loadCredentials();
}

void WifiManager::loadCredentials() {
  Preferences prefs;
  prefs.begin("wifi", true);
  _ssid = prefs.getString("ssid", "");
  _password = prefs.getString("pass", "");
  prefs.end();
}

void WifiManager::saveCredentialsInternal() {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", _ssid);
  prefs.putString("pass", _password);
  prefs.end();
}

bool WifiManager::tryConnectSaved() {
  if (_ssid.length() == 0) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid.c_str(), _password.c_str());
  _connectStart = millis();

  // disable sleep
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);

  unsigned long start = millis();
  while (millis() - start < 10000) { // 10s attempt
    if (WiFi.status() == WL_CONNECTED) {
      _connected = true;
      if (_cb) _cb(true);
      return true;
    }
    delay(200);
  }
  _connected = false;
  if (_cb) _cb(false);
  return false;
}

void WifiManager::startConfigPortal() {
  WiFi.mode(WIFI_AP_STA);
  String apName = "ESP32S3-Zero-Setup";
  WiFi.softAP(apName.c_str());
  // Setup minimal webserver endpoints:
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/html", WebFiles::index_html());
  });
  // endpoint to post wifi credentials
  server.on("/save", HTTP_POST, [this]() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
      _ssid = server.arg("ssid");
      _password = server.arg("pass");
      saveCredentialsInternal();
      server.send(200, "text/plain", "OK");
      // small delay and try connect after posting
      delay(500);
      if (_cb) _cb(false);
    } else {
      server.send(400, "text/plain", "missing");
    }
  });
  // OTA upload page is served by index.html and handled by /update (handled in OtaManager)
  server.begin();
}

void WifiManager::stopConfigPortal() {
  server.stop();
}

void WifiManager::loop() {
  server.handleClient();
}

void WifiManager::onConnectionChanged(WifiCallback cb) {
  _cb = cb;
}

void WifiManager::saveCredentials(const String &ssid, const String &password) {
  _ssid = ssid;
  _password = password;
  saveCredentialsInternal();
}
