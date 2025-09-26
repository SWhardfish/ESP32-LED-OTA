// WifiManager.cpp
#include "WifiManager.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);
static String ssid, pass;

static void handleRoot() {
    String html = "<form action='/save' method='POST'>"
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
    LittleFS.begin(true);
    if (LittleFS.exists("/wifi.json")) {
        File f = LittleFS.open("/wifi.json");
        DynamicJsonDocument doc(128);
        DeserializationError e = deserializeJson(doc, f);
        f.close();
        if (!e) {
            ssid = doc["ssid"].as<String>();
            pass = doc["pass"].as<String>();
            WiFi.begin(ssid.c_str(), pass.c_str());
            return;
        }
    }
    // start AP mode if no creds
    WiFi.softAP("ESP32S3_Setup");
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
}

void WifiManager::loop() {
    if (WiFi.getMode() == WIFI_AP) {
        server.handleClient();
    }
}
