#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "WiFiManager.h"

// Declare extern variables
extern WebServer server;
extern DNSServer dnsServer;
extern WiFiConfig wifiConfig;

#define DNS_PORT 53

bool loadWiFiConfig() {
  if (!LittleFS.exists("/config.json")) {
    logEvent("No config file found");
    return false;
  }

  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    logEvent("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size == 0) {
    logEvent("Config file is empty");
    configFile.close();
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    logEvent("Failed to parse config file: " + String(error.c_str()));
    return false;
  }

  // Load WiFi config
  if (!doc.containsKey("ssid") || !doc.containsKey("password")) {
    logEvent("Config missing WiFi credentials");
    return false;
  }

  wifiConfig.ssid = doc["ssid"].as<String>();
  wifiConfig.password = doc["password"].as<String>();

  // Load schedule config
  if (doc.containsKey("schedule")) {
    JsonObject schedule = doc["schedule"];

    scheduleConfig.morningOnHour = schedule["morningOnHour"] | 6;
    scheduleConfig.morningOnMinute = schedule["morningOnMinute"] | 0;
    scheduleConfig.morningOffHour = schedule["morningOffHour"] | 9;
    scheduleConfig.morningOffMinute = schedule["morningOffMinute"] | 0;
    scheduleConfig.sunsetOffsetHour = schedule["sunsetOffsetHour"] | 0;
    scheduleConfig.sunsetOffsetMinute = schedule["sunsetOffsetMinute"] | 0;
    scheduleConfig.eveningOffHour = schedule["eveningOffHour"] | 23;
    scheduleConfig.eveningOffMinute = schedule["eveningOffMinute"] | 30;

    scheduleConfig.fadeDurationOn = schedule["fadeDurationOn"] | 2000;
    scheduleConfig.fadeDurationOff = schedule["fadeDurationOff"] | 30000;
    scheduleConfig.holdTime = schedule["holdTime"] | 300000;

    // Load high mode settings
    if (schedule.containsKey("highMode")) {
      JsonObject highMode = schedule["highMode"];
      scheduleConfig.highMode.brightness = highMode["brightness"] | 180;
      scheduleConfig.highMode.color = highMode["color"] | 0xFFFFFF;
      scheduleConfig.highMode.white = highMode["white"] | 255;
      scheduleConfig.highMode.colorEnabled = highMode["colorEnabled"] | false;
      scheduleConfig.highMode.whiteEnabled = highMode["whiteEnabled"] | true;
    }

    // Load low mode settings
    if (schedule.containsKey("lowMode")) {
      JsonObject lowMode = schedule["lowMode"];
      scheduleConfig.lowMode.brightness = lowMode["brightness"] | 20;
      scheduleConfig.lowMode.color = lowMode["color"] | 0xFFFFFF;
      scheduleConfig.lowMode.white = lowMode["white"] | 50;
      scheduleConfig.lowMode.colorEnabled = lowMode["colorEnabled"] | false;
      scheduleConfig.lowMode.whiteEnabled = lowMode["whiteEnabled"] | true;
    }
  }

  logEvent("Configuration loaded successfully");
  return true;
}

bool connectWiFi() {
  if (wifiConfig.ssid.length() == 0) {
    logEvent("No WiFi SSID configured");
    return false;
  }

  logEvent("Connecting to WiFi: " + wifiConfig.ssid);
  WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    logEvent("WiFi connected. IP: " + WiFi.localIP().toString());
    return true;
  } else {
    logEvent("WiFi connection failed");
    return false;
  }
}

void startConfigPortal() {
  WiFi.softAP("LEDController-Setup");
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", []() {
    server.send(200, "text/html",
      "<h1>WiFi Configuration</h1>"
      "<p>Please connect to WiFi network 'LEDController-Setup' and visit "
      "<a href='http://192.168.4.1/config'>http://192.168.4.1/config</a> "
      "to configure your WiFi settings.</p>");
  });

  logEvent("Configuration portal started at 192.168.4.1");
}

bool saveConfig() {
  DynamicJsonDocument doc(2048);

  // WiFi config
  doc["ssid"] = wifiConfig.ssid;
  doc["password"] = wifiConfig.password;

  // Schedule config
  JsonObject schedule = doc.createNestedObject("schedule");
  schedule["morningOnHour"] = scheduleConfig.morningOnHour;
  schedule["morningOnMinute"] = scheduleConfig.morningOnMinute;
  schedule["morningOffHour"] = scheduleConfig.morningOffHour;
  schedule["morningOffMinute"] = scheduleConfig.morningOffMinute;
  schedule["sunsetOffsetHour"] = scheduleConfig.sunsetOffsetHour;
  schedule["sunsetOffsetMinute"] = scheduleConfig.sunsetOffsetMinute;
  schedule["eveningOffHour"] = scheduleConfig.eveningOffHour;
  schedule["eveningOffMinute"] = scheduleConfig.eveningOffMinute;

  schedule["fadeDurationOn"] = scheduleConfig.fadeDurationOn;
  schedule["fadeDurationOff"] = scheduleConfig.fadeDurationOff;
  schedule["holdTime"] = scheduleConfig.holdTime;

  // High mode settings
  JsonObject highMode = schedule.createNestedObject("highMode");
  highMode["brightness"] = scheduleConfig.highMode.brightness;
  highMode["color"] = scheduleConfig.highMode.color;
  highMode["white"] = scheduleConfig.highMode.white;
  highMode["colorEnabled"] = scheduleConfig.highMode.colorEnabled;
  highMode["whiteEnabled"] = scheduleConfig.highMode.whiteEnabled;

  // Low mode settings
  JsonObject lowMode = schedule.createNestedObject("lowMode");
  lowMode["brightness"] = scheduleConfig.lowMode.brightness;
  lowMode["color"] = scheduleConfig.lowMode.color;
  lowMode["white"] = scheduleConfig.lowMode.white;
  lowMode["colorEnabled"] = scheduleConfig.lowMode.colorEnabled;
  lowMode["whiteEnabled"] = scheduleConfig.lowMode.whiteEnabled;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    logEvent("Failed to open config file for writing");
    return false;
  }

  serializeJson(doc, configFile);
  configFile.close();

  logEvent("Configuration saved to LittleFS");
  return true;
}