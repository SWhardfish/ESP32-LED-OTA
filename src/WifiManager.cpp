#include "WifiManager.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);
static String ssid, pass;

static void handleRoot() {
    String html =
        "<form action='/save' method='POST'>"
        "SSID:<input name='ssid'><br>"
        "Password:<input name='pass' type='password'><br>"
        "<input type='submit'></form>";
    server.send(200, "text/html", html);
}

static void handleSave() {
    ssid = server.arg("ssid");
    pass = server.arg("pass");
    DynamicJsonDocument doc(128);
    doc["ssid"] = ssid;
    doc["pass"] = pass;

    File f = LittleFS.open("/wifi.json", "w");
    serializeJson(doc, f);
    f.close();

    server.send(200, "text/plain", "Saved! Rebooting…");
    delay(1000);
    ESP.restart();
}

void WifiManager::begin() {
    LittleFS.begin();

    // Always start in station mode first
    WiFi.mode(WIFI_STA);

    if (LittleFS.exists("/wifi.json")) {
        File f = LittleFS.open("/wifi.json", "r");
        DynamicJsonDocument doc(128);
        if (!deserializeJson(doc, f)) {
            ssid = doc["ssid"].as<String>();
            pass = doc["pass"].as<String>();
        }
        f.close();

        if (ssid.length()) {
            WiFi.begin(ssid.c_str(), pass.c_str());

            // try for up to 10 seconds
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
                delay(250);
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Connected to WiFi");
                return;
            }
            Serial.println("WiFi connect failed, starting AP mode");
        }
    }

    // fallback to AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32S3_Setup");
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.println("AP mode started: connect to SSID 'ESP32S3_Setup'");
}

void WifiManager::loop() {
    if (WiFi.getMode() & WIFI_AP) {
        server.handleClient();
    }
}
