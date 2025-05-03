#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>
#include <DNSServer.h>
#include <time.h>

#define LED_PIN 6
#define NUM_LEDS 61
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);
#define PIR_PIN 7
#define BRIGHTNESS_LOW 20
#define BRIGHTNESS_HIGH 180
unsigned long fadeDurationOn = 2000;   // 2 seconds
unsigned long fadeDurationOff = 30000; // 30 seconds
unsigned long lastMotionTime = 0;
const unsigned long holdTime = 300000;

#ifndef VERSION_TAG
#define VERSION_TAG "unknown"
#endif
const char* current_version = VERSION_TAG;

// Web and NTP
WebServer server(80);

// Sunset time
int sunsetHour = 18;

// Logging
#define LOG_BUFFER_SIZE 50
String logBuffer[LOG_BUFFER_SIZE];
int logIndex = 0;

// State tracking
int previousBrightness = -1;
enum LightState { OFF_STATE, LOW_STATE, HIGH_STATE };
LightState currentState = OFF_STATE;

// Wi-Fi config
struct WiFiConfig {
  String ssid;
  String password;
};

WiFiConfig wifiConfig;

// Captive portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

void logEvent(String event) {
  time_t now = time(nullptr);
  struct tm* ptm = localtime(&now);
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  String entry = String(timeStr) + " - " + event;
  logBuffer[logIndex] = entry;
  logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
  Serial.println(entry);
}

void fadeToWhiteBrightness(uint8_t targetBrightness, unsigned long duration) {
  if (targetBrightness == 0) {
    strip.clear();              // Turn off all pixels
    strip.show();               // Apply the change
    previousBrightness = 0;
    return;
  }

  int start = previousBrightness;
  int delta = targetBrightness - start;
  unsigned long startTime = millis();

  while (millis() - startTime < duration) {
    float progress = float(millis() - startTime) / duration;
    int current = start + delta * progress;
    strip.setBrightness(current);
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0, 255));
    }
    strip.show();
    delay(20);
  }

  strip.setBrightness(targetBrightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 255));
  }
  strip.show();
  previousBrightness = targetBrightness;
}

void turnOffStrip() {
  strip.clear();
  strip.show();
  previousBrightness = 0;
}

void fetchSunsetHour() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    http.begin(client, "https://api.sunrise-sunset.org/json?lat=59.3293&lng=18.0686&formatted=0");
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, http.getString());
      String sunset = doc["results"]["sunset"];
      int hourUTC = sunset.substring(11, 13).toInt();
      int minuteUTC = sunset.substring(14, 16).toInt();
      sunsetHour = (hourUTC + 2) % 24; // UTC+2 Stockholm
      logEvent("Sunset time: " + String(sunsetHour) + ":" + String(minuteUTC));
    }
    http.end();
  }
}

void handleRoot() {
  String html = "<html><body><h1>Event Log</h1><ul>";
  for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
    int index = (logIndex - 1 - i + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    if (logBuffer[index].length()) html += "<li>" + logBuffer[index] + "</li>";
  }
  html += "</ul></body></html>";
  server.send(200, "text/html", html);
}

void handleOTAUpdate() {
  if (WiFi.status() != WL_CONNECTED) {
    server.send(500, "text/plain", "WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://api.github.com/repos/SWhardfish/ESP32-LED-OTA/releases/latest");
  http.addHeader("User-Agent", "ESP32-OTA");  // GitHub API requires a User-Agent

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    logEvent("Failed to fetch latest version. HTTP code: " + String(httpCode));
    server.send(500, "text/plain", "Failed to fetch version");
    http.end();
    return;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();

  if (err) {
    logEvent("JSON parse error");
    server.send(500, "text/plain", "Failed to parse JSON");
    return;
  }

  String latestVersion = doc["tag_name"].as<String>();
  logEvent("Current: " + String(current_version) + ", Latest: " + latestVersion);

  if (String(current_version) == latestVersion) {
    logEvent("Already up to date.");
    server.send(200, "text/plain", "Already up to date.");
    return;
  }

  server.send(200, "text/plain", "Updating to " + latestVersion);
  delay(1000);
  String binURL = "https://github.com/SWhardfish/ESP32-LED-OTA/releases/download/" + latestVersion + "/ESP32-LED-OTA.bin";
  performOTA(binURL.c_str());
}


void checkForOTAUpdate() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, "https://api.github.com/repos/SWhardfish/ESP32-LED-OTA/releases/latest");
  http.setUserAgent("ESP32-OTA-Updater"); // Required by GitHub API

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    logEvent("Failed to fetch release info. HTTP code: " + String(httpCode));
    http.end();
    return;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();

  if (error) {
    logEvent("Failed to parse release info.");
    return;
  }

  String latest_version = doc["tag_name"].as<String>();
  String firmware_url = doc["assets"][0]["browser_download_url"].as<String>();

  logEvent("Latest version: " + latest_version);
  logEvent("Current version: " + String(current_version));
  logEvent("Firmware URL: " + firmware_url);

  if (latest_version == current_version) {
    logEvent("Already up to date.");
    return;
  }

  logEvent("Downloading firmware...");
  applyFirmwareUpdate(firmware_url.c_str());
}

void applyFirmwareUpdate(const char* firmwareURL) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, firmwareURL);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    logEvent("Firmware download failed. HTTP code: " + String(httpCode));
    http.end();
    return;
  }

  int contentLength = http.getSize();
  if (!Update.begin(contentLength)) {
    logEvent("Not enough space for update.");
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);

  if (written == contentLength && Update.end(true)) {
    logEvent("OTA update complete. Restarting...");
    ESP.restart();
  } else {
    logEvent("OTA update failed.");
    Update.end();
  }

  http.end();
}

bool loadWiFiConfig() {
  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, f);
  f.close();
  if (error) return false;
  wifiConfig.ssid = doc["ssid"].as<String>();
  wifiConfig.password = doc["password"].as<String>();
  return true;
}

void saveWiFiConfig(String ssid, String pass) {
  DynamicJsonDocument doc(256);
  doc["ssid"] = ssid;
  doc["password"] = pass;
  File f = LittleFS.open("/config.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Setup");
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", []() {
    String html = "<form action='/save' method='POST'>"
                  "SSID: <input name='ssid'><br>"
                  "Password: <input name='pass'><br>"
                  "<input type='submit'></form>";
    server.send(200, "text/html", html);
  });

  server.on("/save", []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length()) {
      saveWiFiConfig(ssid, pass);
      server.send(200, "text/html", "<p>Saved. Rebooting...</p>");
      delay(1000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "SSID is required");
    }
  });

  server.begin();
}

bool connectWiFi() {
  WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }
  return WiFi.status() == WL_CONNECTED;
}

void setup() {
  Serial.begin(115200);
  LittleFS.begin();

  pinMode(PIR_PIN, INPUT);
  strip.begin();
  fadeToWhiteBrightness(BRIGHTNESS_LOW, fadeDurationOn);

  if (!loadWiFiConfig() || !connectWiFi()) {
    Serial.println("WiFi failed. Starting AP mode.");
    startAPMode();
    return;
  }

  Serial.println("WiFi connected.");
  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org");
  fetchSunsetHour();

  server.on("/", handleRoot);
  server.on("/update", handleOTAUpdate);
  server.begin();

  logEvent("System initialized.");
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();

  static bool motionActive = false;
  bool motionDetected = digitalRead(PIR_PIN);
  unsigned long now = millis();

  time_t rawTime =  time(nullptr);
  struct tm *ptm = localtime(&rawTime);
  int hour = ptm->tm_hour;
  int minute = ptm->tm_min;
  bool isScheduledOn = (hour >= 6 && hour < 9) || (hour >= sunsetHour && (hour < 23 || (hour == 23 && minute < 30)));

  if (motionDetected) {
    motionActive = true;
    lastMotionTime = now;
    if (currentState != HIGH_STATE) {
      fadeToWhiteBrightness(BRIGHTNESS_HIGH, fadeDurationOn);
      currentState = HIGH_STATE;
      logEvent("Motion detected. Light turned ON to 75% brightness.");
    }
  }

  if (!motionDetected && motionActive && now - lastMotionTime > holdTime) {
    motionActive = false;
    if (isScheduledOn && currentState != LOW_STATE) {
      fadeToWhiteBrightness(BRIGHTNESS_LOW, fadeDurationOn);
      currentState = LOW_STATE;
      logEvent("No motion detected. Light turned ON to 10% brightness.");
    } else if (!isScheduledOn && currentState != OFF_STATE) {
      fadeToWhiteBrightness(0, fadeDurationOff);
      currentState = OFF_STATE;
      logEvent("No motion detected. Light turned OFF.");
    }
  }
  static bool wasScheduledOn = false;
  if (isScheduledOn != wasScheduledOn) {
    wasScheduledOn = isScheduledOn;
    logEvent(isScheduledOn ? "Schedule activated." : "Schedule deactivated.");
  }


  delay(100);
}
