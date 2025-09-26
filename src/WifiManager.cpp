#include "WifiManager.h"
#include <WebServer.h>
#include "WebFiles.h"
#include "LedIndicator.h"
#include "esp_wifi.h"   // <-- Needed for esp_wifi_set_ps

extern LedIndicator led; // Main instantiates; or inject dependency

static WebServer server(80);

#define WIFI_CONNECT_TIMEOUT 15000UL // 15 seconds

WifiManager::WifiManager()
  : _ssid(""), _password(""), _connected(false), _cb(nullptr), _connectStart(0) {}

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
    if (_ssid.length() == 0) {
        return false; // No saved credentials
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    // Disable WiFi sleep to improve stability
    esp_wifi_set_ps(WIFI_PS_NONE);

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - startAttemptTime < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        Serial.print(".");
    }

    bool connected = WiFi.status() == WL_CONNECTED;
    if (_cb) _cb(connected);
    return connected;
}

void WifiManager::startConfigPortal() {
    WiFi.mode(WIFI_AP_STA);
    String apName = "ESP32S3-Zero-Setup";
    WiFi.softAP(apName.c_str());

    // Setup minimal webserver endpoints
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "text/html", WebFiles::index_html());
    });

    // Endpoint to post wifi credentials
    server.on("/save", HTTP_POST, [this]() {
        if (server.hasArg("ssid") && server.hasArg("pass")) {
            _ssid = server.arg("ssid");
            _password = server.arg("pass");
            saveCredentialsInternal();
            server.send(200, "text/plain", "OK");
            delay(500);
            if (_cb) _cb(false); // trigger callback so main can reconnect
        } else {
            server.send(400, "text/plain", "missing");
        }
    });

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
