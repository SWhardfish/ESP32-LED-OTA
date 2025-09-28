//WifiManager.cpp
#include <Arduino.h>
#include "WifiManager.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);
static String ssid, pass;

static void handleRoot() {
    String html =
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:sans-serif;max-width:420px;margin:2rem auto;padding:0 1rem}</style>"
        "</head><body>"
        "<h3>Wi‑Fi setup</h3>"
        "/save"
        "SSID:<br><input name='ssid'><br>"
        "Password:<br><input name='pass' type='password'><br><br>"
        "<input type='submit' value='Save'>"
        "</form>"
        "</body></html>";
    server.send(200, "text/html", html);
}

static void handleSave() {
    ssid = server.arg("ssid");
    pass = server.arg("pass");

    // Capacity based on JSON structure + string lengths
    DynamicJsonDocument doc(JSON_OBJECT_SIZE(2) + ssid.length() + pass.length() + 64);
    doc["ssid"] = ssid;
    doc["pass"] = pass;

    File f = LittleFS.open("/wifi.json", FILE_WRITE);
    if (!f) {
        server.send(500, "text/plain", "Failed to open /wifi.json for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();

    server.send(200, "text/plain", "Saved! Rebooting…");
    delay(1000);
    ESP.restart();
}

void WifiManager::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed (even after format)");
    }

    WiFi.mode(WIFI_STA);

    if (LittleFS.exists("/wifi.json")) {
        File f = LittleFS.open("/wifi.json", FILE_READ);
        if (f) {
            DynamicJsonDocument doc(512);
            DeserializationError err = deserializeJson(doc, f);
            f.close();
            if (!err) {
                ssid = doc["ssid"] | "";
                pass = doc["pass"] | "";
            } else {
                Serial.printf("JSON parse error: %s\n", err.c_str());
            }
        }

        if (ssid.length()) {
            WiFi.begin(ssid.c_str(), pass.c_str());

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
                delay(250);
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("Connected to %s, IP: %s\n",
                              ssid.c_str(),
                              WiFi.localIP().toString().c_str());
                if (onStatusChange) onStatusChange(true);
                return;
            }
            Serial.println("WiFi connect failed, starting AP mode");
        }
    }

    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32S3_Setup");
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);
    server.begin();

    Serial.printf("AP mode started. SSID: ESP32S3_Setup, IP: %s\n",
                  WiFi.softAPIP().toString().c_str());
    if (onStatusChange) onStatusChange(false);
}

void WifiManager::loop() {
    if (WiFi.getMode() & WIFI_AP) {
        server.handleClient();
    }
}
