#include <Arduino.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_wifi.h>
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

#define FIRMWARE_VERSION "1.0.2 - Fixing schedule logic"
#define LED_PIN 6
#define NUM_LEDS 61
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);
#define PIR_PIN 7

// Configurable parameters
uint8_t BRIGHTNESS_LOW = 20;
uint8_t BRIGHTNESS_HIGH = 180;
unsigned long fadeDurationOn = 2000;   // ms
unsigned long fadeDurationOff = 30000; // ms
unsigned long lastMotionTime = 0;
unsigned long holdTime = 300000; // 5 min

int morningOnHour = 6;
int morningOnMinute = 0;
int morningOffHour = 9;
int morningOffMinute = 0;
int sunsetOffsetHour = 0;
int sunsetOffsetMinute = 0;
int eveningOffHour = 23;
int eveningOffMinute = 30;

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

int sunsetHour = 18;
int sunsetMinute = 0;

#define LOG_BUFFER_SIZE 50
String logBuffer[LOG_BUFFER_SIZE];
int logIndex = 0;

int previousBrightness = -1;
enum LightState { OFF_STATE, LOW_STATE, HIGH_STATE };
LightState currentState = OFF_STATE;

struct WiFiConfig {
  String ssid;
  String password;
};
WiFiConfig wifiConfig;

struct RGBWConfig {
  bool useRGB;
  bool useWhite;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
  uint8_t brightness;
};

struct StateConfig {
  RGBWConfig lowState;
  RGBWConfig highState;
  unsigned long fadeDurationOn;
  unsigned long fadeDurationOff;
  unsigned long holdTime;
};

struct ScheduleConfig {
  int morningOnHour;
  int morningOnMinute;
  int morningOffHour;
  int morningOffMinute;
  int sunsetOffsetHour;
  int sunsetOffsetMinute;
  int eveningOffHour;
  int eveningOffMinute;
  uint8_t brightnessLow;
  uint8_t brightnessHigh;
  unsigned long fadeDurationOn;
  unsigned long fadeDurationOff;
  unsigned long holdTime;
} scheduleConfig;

StateConfig stateConfig;

// Function declarations
void handleRoot();
void handleLog();
void handleConfig();
void handleUploadForm();
void handleUpload();
void handleBrightness();
void handleColor();
void handleWhite();
void handleStatus();
void startConfigPortal();
void logEvent(String event);
void fadeToRGBWBrightness(const RGBWConfig& targetConfig, unsigned long duration);
bool loadWiFiConfig();
bool loadScheduleConfig();
bool connectWiFi();
void fetchSunsetHour();
void saveConfig();

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

void fadeToRGBWBrightness(const RGBWConfig& targetConfig, unsigned long duration) {
  RGBWConfig startConfig;
  // Determine starting state
  if (previousBrightness < 0) {
    // Initial state - all off
    startConfig = {false, false, 0, 0, 0, 0, 0};
  } else {
    // Get current state based on currentState
    startConfig = (currentState == HIGH_STATE) ? stateConfig.highState : stateConfig.lowState;
  }

  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    float progress = float(millis() - startTime) / duration;
    if (progress > 1.0) progress = 1.0;

    uint8_t r = startConfig.r + (targetConfig.r - startConfig.r) * progress;
    uint8_t g = startConfig.g + (targetConfig.g - startConfig.g) * progress;
    uint8_t b = startConfig.b + (targetConfig.b - startConfig.b) * progress;
    uint8_t w = startConfig.w + (targetConfig.w - startConfig.w) * progress;
    uint8_t brightness = startConfig.brightness + (targetConfig.brightness - startConfig.brightness) * progress;

    strip.setBrightness(brightness);
    for (int i = 0; i < NUM_LEDS; i++) {
      if (targetConfig.useRGB && targetConfig.useWhite) {
        strip.setPixelColor(i, strip.Color(g, r, b, w));
      } else if (targetConfig.useRGB) {
        strip.setPixelColor(i, strip.Color(g, r, b, 0));
      } else if (targetConfig.useWhite) {
        strip.setPixelColor(i, strip.Color(0, 0, 0, w));
      }
    }
    strip.show();
    delay(20);
  }

  // Set final state
  strip.setBrightness(targetConfig.brightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    if (targetConfig.useRGB && targetConfig.useWhite) {
      strip.setPixelColor(i, strip.Color(targetConfig.g, targetConfig.r, targetConfig.b, targetConfig.w));
    } else if (targetConfig.useRGB) {
      strip.setPixelColor(i, strip.Color(targetConfig.g, targetConfig.r, targetConfig.b, 0));
    } else if (targetConfig.useWhite) {
      strip.setPixelColor(i, strip.Color(0, 0, 0, targetConfig.w));
    }
  }
  strip.show();
  previousBrightness = targetConfig.brightness;
}

bool loadWiFiConfig() {
  if (!LittleFS.exists("/wifi_config.json")) {
    logEvent("No WiFi config file found");
    return false;
  }

  File configFile = LittleFS.open("/wifi_config.json", "r");
  if (!configFile) {
    logEvent("Failed to open WiFi config file");
    return false;
  }

  size_t size = configFile.size();
  if (size == 0) {
    logEvent("WiFi config file is empty");
    configFile.close();
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    logEvent("Failed to parse WiFi config file: " + String(error.c_str()));
    return false;
  }

  if (!doc.containsKey("ssid") || !doc.containsKey("password")) {
    logEvent("WiFi config missing required fields");
    return false;
  }

  wifiConfig.ssid = doc["ssid"].as<String>();
  wifiConfig.password = doc["password"].as<String>();

  if (wifiConfig.ssid.length() == 0) {
    logEvent("Empty SSID in config");
    return false;
  }

  logEvent("Loaded WiFi config for: " + wifiConfig.ssid);
  return true;
}

bool loadScheduleConfig() {
  if (!LittleFS.exists("/schedule_config.json")) {
    logEvent("No schedule config file found");
    return false;
  }

  File configFile = LittleFS.open("/schedule_config.json", "r");
  if (!configFile) {
    logEvent("Failed to open schedule config file");
    return false;
  }

  size_t size = configFile.size();
  if (size == 0) {
    logEvent("Schedule config file is empty");
    configFile.close();
    return false;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    logEvent("Failed to parse schedule config file: " + String(error.c_str()));
    return false;
  }

  // Load time schedule settings
  scheduleConfig.morningOnHour = doc["morningOnHour"];
  scheduleConfig.morningOnMinute = doc["morningOnMinute"];
  scheduleConfig.morningOffHour = doc["morningOffHour"];
  scheduleConfig.morningOffMinute = doc["morningOffMinute"];
  scheduleConfig.sunsetOffsetHour = doc["sunsetOffsetHour"];
  scheduleConfig.sunsetOffsetMinute = doc["sunsetOffsetMinute"];
  scheduleConfig.eveningOffHour = doc["eveningOffHour"];
  scheduleConfig.eveningOffMinute = doc["eveningOffMinute"];

  // Load state configurations
  JsonObject highState = doc["highState"];
  stateConfig.highState.useRGB = highState["useRGB"] | true;  // Default to true if not set
  stateConfig.highState.useWhite = highState["useWhite"] | true;
  stateConfig.highState.r = highState["r"] | 255;
  stateConfig.highState.g = highState["g"] | 255;
  stateConfig.highState.b = highState["b"] | 255;
  stateConfig.highState.w = highState["w"] | 255;
  stateConfig.highState.brightness = highState["brightness"] | 180;

  JsonObject lowState = doc["lowState"];
  stateConfig.lowState.useRGB = lowState["useRGB"] | false;
  stateConfig.lowState.useWhite = lowState["useWhite"] | true;
  stateConfig.lowState.r = lowState["r"] | 0;
  stateConfig.lowState.g = lowState["g"] | 0;
  stateConfig.lowState.b = lowState["b"] | 0;
  stateConfig.lowState.w = lowState["w"] | 255;
  stateConfig.lowState.brightness = lowState["brightness"] | 20;

  // Load timing parameters
  stateConfig.fadeDurationOn = doc["fadeDurationOn"] | 2000;
  stateConfig.fadeDurationOff = doc["fadeDurationOff"] | 30000;
  stateConfig.holdTime = doc["holdTime"] | 300000;

  // Update global variables
  fadeDurationOn = stateConfig.fadeDurationOn;
  fadeDurationOff = stateConfig.fadeDurationOff;
  holdTime = stateConfig.holdTime;

  logEvent("Loaded schedule and state configuration");
  return true;
}

bool connectWiFi() {
  logEvent("Connecting to WiFi: " + wifiConfig.ssid);
  WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
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
  WiFi.softAP("ESP32-Setup");
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", []() {
    server.send(200, "text/html", "<h1>WiFi Configuration</h1><p>Please visit <a href='/config'>/config</a> to set up WiFi.</p>");
  });
  server.on("/config", handleConfig);
  server.on("/upload", HTTP_GET, handleUploadForm);
  server.on("/upload", HTTP_POST, [](){}, handleUpload);

  server.begin();
  logEvent("Config portal started at 192.168.4.1");
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
      sunsetHour = sunset.substring(11, 13).toInt();
      sunsetMinute = sunset.substring(14, 16).toInt();
      sunsetHour = (sunsetHour + 2) % 24; // UTC+2
      logEvent("Sunset time: " + String(sunsetHour) + ":" + String(sunsetMinute));
    }
    http.end();
  }
}

void saveConfig() {
  DynamicJsonDocument doc(1024);
  doc["ssid"] = wifiConfig.ssid;
  doc["password"] = wifiConfig.password;
  File wifiConfigFile = LittleFS.open("/wifi_config.json", "w");
  serializeJson(doc, wifiConfigFile);
  wifiConfigFile.close();

  doc.clear();
  doc["morningOnHour"] = scheduleConfig.morningOnHour;
  doc["morningOnMinute"] = scheduleConfig.morningOnMinute;
  doc["morningOffHour"] = scheduleConfig.morningOffHour;
  doc["morningOffMinute"] = scheduleConfig.morningOffMinute;
  doc["sunsetOffsetHour"] = scheduleConfig.sunsetOffsetHour;
  doc["sunsetOffsetMinute"] = scheduleConfig.sunsetOffsetMinute;
  doc["eveningOffHour"] = scheduleConfig.eveningOffHour;
  doc["eveningOffMinute"] = scheduleConfig.eveningOffMinute;

  // Save high state configuration
  JsonObject highState = doc.createNestedObject("highState");
  highState["useRGB"] = stateConfig.highState.useRGB;
  highState["useWhite"] = stateConfig.highState.useWhite;
  highState["r"] = stateConfig.highState.r;
  highState["g"] = stateConfig.highState.g;
  highState["b"] = stateConfig.highState.b;
  highState["w"] = stateConfig.highState.w;
  highState["brightness"] = stateConfig.highState.brightness;

  // Save low state configuration
  JsonObject lowState = doc.createNestedObject("lowState");
  lowState["useRGB"] = stateConfig.lowState.useRGB;
  lowState["useWhite"] = stateConfig.lowState.useWhite;
  lowState["r"] = stateConfig.lowState.r;
  lowState["g"] = stateConfig.lowState.g;
  lowState["b"] = stateConfig.lowState.b;
  lowState["w"] = stateConfig.lowState.w;
  lowState["brightness"] = stateConfig.lowState.brightness;

  // Save timing parameters
  doc["fadeDurationOn"] = stateConfig.fadeDurationOn;
  doc["fadeDurationOff"] = stateConfig.fadeDurationOff;
  doc["holdTime"] = stateConfig.holdTime;

  File scheduleConfigFile = LittleFS.open("/schedule_config.json", "w");
  serializeJson(doc, scheduleConfigFile);
  scheduleConfigFile.close();

  logEvent("Configuration saved");
}

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Entrance Light</title>
      <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
      <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/themes/classic.min.css">
      <style>
          .slider {
              -webkit-appearance: none;
              width: calc(100% - 20px);
              height: 4px;
              background: #d3d3d3;
              outline: none;
              opacity: 0.7;
              -webkit-transition: .2s;
              transition: opacity .2s;
              margin: 10px 0;
              padding: 0 10px;
          }
          .slider:hover {
              opacity: 1;
          }
          .slider::-webkit-slider-thumb {
              -webkit-appearance: none;
              appearance: none;
              width: 20px;
              height: 20px;
              background: #4CAF50;
              cursor: pointer;
              border-radius: 50%;
          }
          .slider::-moz-range-thumb {
              width: 20px;
              height: 20px;
              background: #4CAF50;
              cursor: pointer;
              border-radius: 50%;
          }
      </style>
  </head>
  <body class="flex flex-col items-center justify-start min-h-screen bg-gray-100 text-gray-800">
      <!-- Title -->
      <h1 class="text-2xl font-bold mt-4">Entrance Light</h1>
      <!-- Status Indicators -->
      <div class="flex space-x-4 mt-4">
          <div class="flex items-center">
              <span class="text-lg">Lights:</span>
              <span id="lightStatus" class="ml-2 text-lg font-semibold">OFF</span>
          </div>
          <div class="flex items-center">
              <span class="text-lg">PIR:</span>
              <span id="pirStatus" class="ml-2 text-lg font-semibold">No Motion</span>
          </div>
      </div>
      <!-- Brightness Slider -->
      <div class="flex flex-col items-center mt-4 w-full px-[10%]">
          <label for="brightnessSlider" class="text-lg mb-2">Brightness</label>
          <input type="range" id="brightnessSlider" class="w-full slider" min="0" max="100" value="50">

          <button id="brightnessButton" class="mt-4 w-12 h-12 bg-gray-100 border border-gray-300 rounded-lg shadow text-sm">
              💡
          </button>
      </div>

      <!-- RGB Color Picker -->
      <div class="flex flex-col items-center mt-4 w-full">
          <label class="text-lg mb-2">RGB Color</label>
          <div id="colorPicker" class="mb-4"></div>

          <div class="flex justify-center space-x-4 w-full max-w-xs">
              <button id="rgbLedButton" class="flex-1 px-4 py-2 bg-blue-500 text-white rounded-lg shadow">RGB</button>
              <button id="whiteLedButton" class="flex-1 px-4 py-2 bg-gray-100 border border-gray-300 rounded-lg shadow">White</button>
        </div>
      </div>

      <!-- Navigation Buttons -->
      <div class="flex space-x-4 mt-4 w-full justify-center">
          <a href="/log" class="px-4 py-2 bg-blue-500 text-white rounded-lg shadow">LOG</a>
          <a href="/upload" class="px-4 py-2 bg-green-500 text-white rounded-lg shadow">UPLOAD</a>
          <a href="/config" class="px-4 py-2 bg-yellow-500 text-white rounded-lg shadow">CONFIG</a>
      </div>
      <script src="https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/pickr.min.js"></script>
      <script>
          document.addEventListener('DOMContentLoaded', function() {
              const lightStatus = document.getElementById('lightStatus');
              const pirStatus = document.getElementById('pirStatus');
              const brightnessSlider = document.getElementById('brightnessSlider');
              const whiteLedButton = document.getElementById('whiteLedButton');

              const pickr = Pickr.create({
                  el: '#colorPicker',
                  theme: 'classic',
                  default: '#ffffff',
                  components: {
                      preview: true,
                      opacity: true,
                      hue: true,
                      interaction: {
                          hex: true,
                          rgba: true,
                          input: true,
                          save: true
                      }
                  }
              });

              pickr.on('save', (color) => {
                  const hexColor = color.toHEXA().toString();
                  fetch('/color', {
                      method: 'POST',
                      headers: {
                          'Content-Type': 'application/json',
                      },
                      body: JSON.stringify({ color: hexColor }),
                  });
              });

              function updateLightStatus(status) {
                  lightStatus.textContent = status ? 'ON' : 'OFF';
              }

              function updatePirStatus(status) {
                  pirStatus.textContent = status ? 'Motion Detected' : 'No Motion';
              }

              brightnessSlider.addEventListener('input', function() {
                  const brightness = brightnessSlider.value;
                  fetch('/brightness', {
                      method: 'POST',
                      headers: {
                          'Content-Type': 'application/json',
                      },
                      body: JSON.stringify({ brightness: brightness }),
                  });
              });

              whiteLedButton.addEventListener('click', function() {
                  fetch('/white', {
                      method: 'POST',
                  });
              });

              setInterval(() => {
                  fetch('/status')
                      .then(response => response.json())
                      .then(data => {
                          updateLightStatus(data.lightStatus);
                          updatePirStatus(data.pirStatus);
                      });
              }, 2000);
          });
      </script>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleLog() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Event Log</title>
      <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
  </head>
  <body class="flex flex-col items-center justify-start min-h-screen bg-gray-100 text-gray-800">
      <h1 class="text-2xl font-bold mt-4">Event Log</h1>
      <ul id="logList" class="w-full px-4 mt-4">
  )rawliteral";

  for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
    int index = (logIndex - 1 - i + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    if (logBuffer[index].length()) {
      html += "<li>" + logBuffer[index] + "</li>";
    }
  }

  html += R"rawliteral(
      </ul>
      <a href="/" class="mt-4 px-4 py-2 bg-blue-500 text-white rounded-lg shadow">Back</a>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleUploadForm() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Upload Firmware</title>
      <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
  </head>
  <body class="flex flex-col items-center justify-start min-h-screen bg-gray-100 text-gray-800">
      <h1 class="text-2xl font-bold mt-4">Upload Firmware</h1>
      <form method="POST" action="/upload" enctype="multipart/form-data" class="mt-4">
          <input type="file" name="update" class="mb-2">
          <input type="submit" value="Upload" class="px-4 py-2 bg-green-500 text-white rounded-lg shadow">
      </form>
      <a href="/" class="mt-4 px-4 py-2 bg-blue-500 text-white rounded-lg shadow">Back</a>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleConfig() {
  if (server.method() == HTTP_POST) {
    // Save WiFi config
    wifiConfig.ssid = server.arg("ssid");
    wifiConfig.password = server.arg("password");

    // Save state configurations
    stateConfig.highState.useRGB = server.arg("highUseRGB") == "on";
    stateConfig.highState.useWhite = server.arg("highUseWhite") == "on";
    String highColor = server.arg("highColor");
    if (highColor.length() == 7) {
      stateConfig.highState.r = strtol(highColor.substring(1, 3).c_str(), NULL, 16);
      stateConfig.highState.g = strtol(highColor.substring(3, 5).c_str(), NULL, 16);
      stateConfig.highState.b = strtol(highColor.substring(5, 7).c_str(), NULL, 16);
    }
    stateConfig.highState.brightness = server.arg("highBrightness").toInt();

    stateConfig.lowState.useRGB = server.arg("lowUseRGB") == "on";
    stateConfig.lowState.useWhite = server.arg("lowUseWhite") == "on";
    String lowColor = server.arg("lowColor");
    if (lowColor.length() == 7) {
      stateConfig.lowState.r = strtol(lowColor.substring(1, 3).c_str(), NULL, 16);
      stateConfig.lowState.g = strtol(lowColor.substring(3, 5).c_str(), NULL, 16);
      stateConfig.lowState.b = strtol(lowColor.substring(5, 7).c_str(), NULL, 16);
    }
    stateConfig.lowState.brightness = server.arg("lowBrightness").toInt();

    // Save timing parameters
    stateConfig.fadeDurationOn = server.arg("fadeDurationOn").toInt();
    stateConfig.fadeDurationOff = server.arg("fadeDurationOff").toInt();
    stateConfig.holdTime = server.arg("holdTime").toInt();

    // Save time schedule settings
    String morningOnStr = server.arg("morningOnTime");
    scheduleConfig.morningOnHour = morningOnStr.substring(0, 2).toInt();
    scheduleConfig.morningOnMinute = morningOnStr.substring(3, 5).toInt();

    String morningOffStr = server.arg("morningOffTime");
    scheduleConfig.morningOffHour = morningOffStr.substring(0, 2).toInt();
    scheduleConfig.morningOffMinute = morningOffStr.substring(3, 5).toInt();

    String eveningOffStr = server.arg("eveningOffTime");
    scheduleConfig.eveningOffHour = eveningOffStr.substring(0, 2).toInt();
    scheduleConfig.eveningOffMinute = eveningOffStr.substring(3, 5).toInt();

    // Parse sunset offset
    int sunsetOffsetMinutes = server.arg("sunsetOffset").toInt();
    scheduleConfig.sunsetOffsetHour = sunsetOffsetMinutes / 60;
    scheduleConfig.sunsetOffsetMinute = sunsetOffsetMinutes % 60;

    // Save configuration
    saveConfig();

    server.send(200, "text/plain", "Configuration saved. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Configuration</title>
        <link href="https://cdn.jsdelivr.net/npm/tailwindcss@2.2.19/dist/tailwind.min.css" rel="stylesheet">
    </head>
    <body class="flex flex-col items-center justify-start min-h-screen bg-gray-100 text-gray-800">
        <h1 class="text-2xl font-bold mt-4">Configuration</h1>
        <form method="POST" action="/config" class="mt-4 w-full max-w-md">
            <div class="mb-4">
                <label for="ssid" class="block text-lg mb-2">WiFi SSID</label>
                <input type="text" id="ssid" name="ssid" class="w-full px-4 py-2 border rounded-lg shadow" value=")rawliteral";
    html += wifiConfig.ssid;
    html += R"rawliteral(">
            </div>
            <div class="mb-4">
                <label for="password" class="block text-lg mb-2">WiFi Password</label>
                <input type="password" id="password" name="password" class="w-full px-4 py-2 border rounded-lg shadow" value=")rawliteral";
    html += wifiConfig.password;
    html += R"rawliteral(">
            </div>

            <!-- High State Configuration -->
            <div class="mb-4 p-4 border rounded-lg">
                <h2 class="text-xl font-bold mb-2">High State (Motion)</h2>

                <div class="mb-2">
                    <label class="inline-flex items-center">
                        <input type="checkbox" name="highUseRGB" class="form-checkbox" )rawliteral";
    html += stateConfig.highState.useRGB ? "checked" : "";
    html += R"rawliteral(>
                        <span class="ml-2">Use RGB</span>
                    </label>
                </div>

                <div class="mb-2">
                    <label class="inline-flex items-center">
                        <input type="checkbox" name="highUseWhite" class="form-checkbox" )rawliteral";
    html += stateConfig.highState.useWhite ? "checked" : "";
    html += R"rawliteral(>
                        <span class="ml-2">Use White</span>
                    </label>
                </div>

                <div class="mb-2" id="highColorContainer" style="display: )rawliteral";
    html += stateConfig.highState.useRGB ? "block" : "none";
    html += R"rawliteral(;">
                    <label class="block text-lg mb-2">RGB Color</label>
                    <input type="color" name="highColor" value="#)rawliteral";
    char highColor[8];
    sprintf(highColor, "%02X%02X%02X", stateConfig.highState.r, stateConfig.highState.g, stateConfig.highState.b);
    html += highColor;
    html += R"rawliteral(" class="w-full">
                </div>

                <div class="mb-2">
                    <label class="block text-lg mb-2">Brightness (0-255)</label>
                    <input type="range" name="highBrightness" min="0" max="255" value=")rawliteral";
    html += String(stateConfig.highState.brightness);
    html += R"rawliteral(" class="w-full slider">
                    <span class="block text-center">)rawliteral";
    html += String(stateConfig.highState.brightness);
    html += R"rawliteral(</span>
                </div>
            </div>

            <!-- Low State Configuration -->
            <div class="mb-4 p-4 border rounded-lg">
                <h2 class="text-xl font-bold mb-2">Low State (Schedule)</h2>

                <div class="mb-2">
                    <label class="inline-flex items-center">
                        <input type="checkbox" name="lowUseRGB" class="form-checkbox" )rawliteral";
    html += stateConfig.lowState.useRGB ? "checked" : "";
    html += R"rawliteral(>
                        <span class="ml-2">Use RGB</span>
                    </label>
                </div>

                <div class="mb-2">
                    <label class="inline-flex items-center">
                        <input type="checkbox" name="lowUseWhite" class="form-checkbox" )rawliteral";
    html += stateConfig.lowState.useWhite ? "checked" : "";
    html += R"rawliteral(>
                        <span class="ml-2">Use White</span>
                    </label>
                </div>

                <div class="mb-2" id="lowColorContainer" style="display: )rawliteral";
    html += stateConfig.lowState.useRGB ? "block" : "none";
    html += R"rawliteral(;">
                    <label class="block text-lg mb-2">RGB Color</label>
                    <input type="color" name="lowColor" value="#)rawliteral";
    char lowColor[8];
    sprintf(lowColor, "%02X%02X%02X", stateConfig.lowState.r, stateConfig.lowState.g, stateConfig.lowState.b);
    html += lowColor;
    html += R"rawliteral(" class="w-full">
                </div>

                <div class="mb-2">
                    <label class="block text-lg mb-2">Brightness (0-255)</label>
                    <input type="range" name="lowBrightness" min="0" max="255" value=")rawliteral";
    html += String(stateConfig.lowState.brightness);
    html += R"rawliteral(" class="w-full slider">
                    <span class="block text-center">)rawliteral";
    html += String(stateConfig.lowState.brightness);
    html += R"rawliteral(</span>
                </div>
            </div>

            <!-- Time Schedule Configuration -->
            <div class="mb-4 p-4 border rounded-lg">
                <h2 class="text-xl font-bold mb-2">Time Schedule</h2>

                <div class="mb-2">
                    <label for="morningOnTime" class="block text-lg mb-2">Morning On Time</label>
                    <input type="time" id="morningOnTime" name="morningOnTime" class="w-full px-4 py-2 border rounded-lg shadow"
                            value=")rawliteral";
    html += (String(scheduleConfig.morningOnHour).length() < 2 ? "0" + String(scheduleConfig.morningOnHour) : String(scheduleConfig.morningOnHour)) + ":" +
            (String(scheduleConfig.morningOnMinute).length() < 2 ? "0" + String(scheduleConfig.morningOnMinute) : String(scheduleConfig.morningOnMinute));
    html += R"rawliteral(" step="60">
                </div>

                <div class="mb-2">
                    <label for="morningOffTime" class="block text-lg mb-2">Morning Off Time</label>
                    <input type="time" id="morningOffTime" name="morningOffTime" class="w-full px-4 py-2 border rounded-lg shadow"
                           value=")rawliteral";
    html += (String(scheduleConfig.morningOffHour).length() < 2 ? "0" + String(scheduleConfig.morningOffHour) : String(scheduleConfig.morningOffHour)) + ":" +
            (String(scheduleConfig.morningOffMinute).length() < 2 ? "0" + String(scheduleConfig.morningOffMinute) : String(scheduleConfig.morningOffMinute));
    html += R"rawliteral(">
                </div>

                <div class="mb-2">
                    <label for="sunsetOffset" class="block text-lg mb-2">Sunset Offset (minutes)</label>
                    <input type="number" id="sunsetOffset" name="sunsetOffset" min="-120" max="120" step="15"
                           class="w-full px-4 py-2 border rounded-lg shadow"
                           value=")rawliteral";
    html += String(scheduleConfig.sunsetOffsetHour * 60 + scheduleConfig.sunsetOffsetMinute);
    html += R"rawliteral(">
                </div>

                <div class="mb-2">
                    <label for="eveningOffTime" class="block text-lg mb-2">Evening Off Time</label>
                    <input type="time" id="eveningOffTime" name="eveningOffTime" class="w-full px-4 py-2 border rounded-lg shadow"
                           value=")rawliteral";
    html += (String(scheduleConfig.eveningOffHour).length() < 2 ? "0" + String(scheduleConfig.eveningOffHour) : String(scheduleConfig.eveningOffHour)) + ":" +
            (String(scheduleConfig.eveningOffMinute).length() < 2 ? "0" + String(scheduleConfig.eveningOffMinute) : String(scheduleConfig.eveningOffMinute));
    html += R"rawliteral(">
                </div>
            </div>

            <!-- Timing Configuration -->
            <div class="mb-4">
                <label for="fadeDurationOn" class="block text-lg mb-2">Fade Duration ON (ms)</label>
                <input type="range" id="fadeDurationOn" name="fadeDurationOn" min="0" max="60000" value=")rawliteral";
    html += String(stateConfig.fadeDurationOn);
    html += R"rawliteral(" class="w-full slider">
                <span id="fadeDurationOnValue" class="block text-lg mt-2">)rawliteral";
    html += String(stateConfig.fadeDurationOn);
    html += R"rawliteral(</span>
            </div>

            <div class="mb-4">
                <label for="fadeDurationOff" class="block text-lg mb-2">Fade Duration OFF (ms)</label>
                <input type="range" id="fadeDurationOff" name="fadeDurationOff" min="0" max="60000" value=")rawliteral";
    html += String(stateConfig.fadeDurationOff);
    html += R"rawliteral(" class="w-full slider">
                <span id="fadeDurationOffValue" class="block text-lg mt-2">)rawliteral";
    html += String(stateConfig.fadeDurationOff);
    html += R"rawliteral(</span>
            </div>

            <div class="mb-4">
                <label for="holdTime" class="block text-lg mb-2">Hold Time (ms)</label>
                <input type="range" id="holdTime" name="holdTime" min="0" max="600000" value=")rawliteral";
    html += String(stateConfig.holdTime);
    html += R"rawliteral(" class="w-full slider">
                <span id="holdTimeValue" class="block text-lg mt-2">)rawliteral";
    html += String(stateConfig.holdTime);
    html += R"rawliteral(</span>
            </div>

            <input type="submit" value="Save" class="px-4 py-2 bg-yellow-500 text-white rounded-lg shadow">
        </form>
        <a href="/" class="mt-4 px-4 py-2 bg-blue-500 text-white rounded-lg shadow">Back</a>

        <script>
            document.addEventListener('DOMContentLoaded', function() {
                // Show/hide color pickers based on RGB checkboxes
                document.querySelector('[name="highUseRGB"]').addEventListener('change', function() {
                    document.getElementById('highColorContainer').style.display = this.checked ? 'block' : 'none';
                });

                document.querySelector('[name="lowUseRGB"]').addEventListener('change', function() {
                    document.getElementById('lowColorContainer').style.display = this.checked ? 'block' : 'none';
                });

                // Update displayed value for sliders
                document.querySelectorAll('input[type="range"]').forEach(slider => {
                    slider.addEventListener('input', function() {
                        this.nextElementSibling.textContent = this.value;
                    });
                });
            });
        </script>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", html);
  }
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  static File updateFile;

  if (upload.status == UPLOAD_FILE_START) {
    logEvent("Upload started: " + upload.filename);
    updateFile = LittleFS.open("/update.bin", "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (updateFile) updateFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (updateFile) {
      updateFile.close();
      logEvent("Upload complete. Saved to /update.bin");
      server.send(200, "text/html", "<p>Update uploaded. Rebooting...</p>");
      delay(1000);
      ESP.restart();
    }
  }
}

void handleBrightness() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "text/plain", "Failed to parse request");
    return;
  }

  int brightness = doc["brightness"];
  strip.setBrightness(map(brightness, 0, 100, 0, 255));
  strip.show();
  server.send(200, "text/plain", "Brightness set to " + String(brightness));
}

void handleColor() {
  if (server.hasArg("plain") == false) {
    server.send(400, "text/plain", "Body not received");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error) {
    server.send(400, "text/plain", "Failed to parse request");
    return;
  }

  String color = doc["color"].as<String>();
  long number = strtol(color.substring(1).c_str(), NULL, 16);
  int r = number >> 16;
  int g = number >> 8 & 0xFF;
  int b = number & 0xFF;

  // Convert RGB to RGBW for SK6812
  int w = min(r, min(g, b)); // Calculate white component
  r -= w;
  g -= w;
  b -= w;

  // Set color with proper GRBW order
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(g, r, b, w));
  }
  strip.show();
  server.send(200, "text/plain", "Color set to " + color);
}

void handleWhite() {
  RGBWConfig whiteConfig;
  whiteConfig.useRGB = false;
  whiteConfig.useWhite = true;
  whiteConfig.r = 0;
  whiteConfig.g = 0;
  whiteConfig.b = 0;
  whiteConfig.w = 255;
  whiteConfig.brightness = 255;

  fadeToRGBWBrightness(whiteConfig, 500); // 500ms fade time
  server.send(200, "text/plain", "White LEDs activated");
}

void handleStatus() {
  DynamicJsonDocument doc(1024);
  doc["lightStatus"] = (currentState != OFF_STATE);
  doc["pirStatus"] = digitalRead(PIR_PIN);

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void setup() {
  Serial.begin(115200);
  logEvent("Firmware version: " + String(FIRMWARE_VERSION));
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  logEvent("Firmware version: " + String(FIRMWARE_VERSION));
  logEvent("ESP32 Chip Info:");
  logEvent("  Cores: " + String(chip_info.cores));
  logEvent("  Features: " +
           String((chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "") +
           String((chip_info.features & CHIP_FEATURE_BT) ? "BT " : "") +
           String((chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : ""));
  logEvent("  Revision: " + String(chip_info.revision));
  logEvent("Free heap: " + String(esp_get_free_heap_size()) + " bytes");
  logEvent("Minimum free heap since boot: " + String(esp_get_minimum_free_heap_size()) + " bytes");
  logEvent("CPU Frequency: " + String(getCpuFrequencyMhz()) + " MHz");
  logEvent("WiFi MAC: " + WiFi.macAddress());


  // Initialize LittleFS
  if (!LittleFS.begin(true)) { // true = format if mount fails
    logEvent("Failed to mount LittleFS");
    delay(1000);
    ESP.restart();
  }

  pinMode(PIR_PIN, INPUT);
  strip.begin();
  strip.show();

  // Try to load config and connect
  if (!loadWiFiConfig() || !loadScheduleConfig()) {
    logEvent("No valid config - starting config portal");

    // Set default values if config loading failed
    scheduleConfig.brightnessLow = 20;
    scheduleConfig.brightnessHigh = 225;
    scheduleConfig.fadeDurationOn = 2000;
    scheduleConfig.fadeDurationOff = 30000;
    scheduleConfig.holdTime = 300000;
    // Other schedule defaults...
    scheduleConfig.morningOnHour = 6;
    scheduleConfig.morningOnMinute = 0;
    scheduleConfig.morningOffHour = 9;
    scheduleConfig.morningOffMinute = 0;
    scheduleConfig.sunsetOffsetHour = 0;
    scheduleConfig.sunsetOffsetMinute = 0;
    scheduleConfig.eveningOffHour = 23;
    scheduleConfig.eveningOffMinute = 30;

    startConfigPortal();
    return;
  }

  // Update global variables from loaded config
  BRIGHTNESS_LOW = scheduleConfig.brightnessLow;
  BRIGHTNESS_HIGH = scheduleConfig.brightnessHigh;
  fadeDurationOn = scheduleConfig.fadeDurationOn;
  fadeDurationOff = scheduleConfig.fadeDurationOff;
  holdTime = scheduleConfig.holdTime;

  if (!connectWiFi()) {
    logEvent("Failed to connect - starting config portal");
    startConfigPortal();
    return;
  }

  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org");
  fetchSunsetHour();
  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/config", handleConfig);
  server.on("/upload", HTTP_POST, [](){}, handleUpload);
  server.on("/upload", HTTP_GET, handleUploadForm);
  server.on("/brightness", HTTP_POST, handleBrightness);
  server.on("/color", HTTP_POST, handleColor);
  server.on("/white", HTTP_POST, handleWhite);
  server.on("/status", handleStatus);
  server.begin();
  logEvent("System initialised.");

  if (LittleFS.exists("/update.bin")) {
    File updateBin = LittleFS.open("/update.bin");
    if (updateBin) {
      size_t updateSize = updateBin.size();
      logEvent("Found update.bin, size: " + String(updateSize));
      if (Update.begin(updateSize)) {
        size_t written = Update.writeStream(updateBin);
        if (written == updateSize && Update.end()) {
          logEvent("OTA update successful. Rebooting...");
          updateBin.close();
          LittleFS.remove("/update.bin");
          delay(1000);
          ESP.restart();
        } else {
          logEvent("OTA update failed.");
        }
      } else {
        logEvent("OTA begin failed.");
      }
      updateBin.close();
    }
  }
}

void loop() {
    server.handleClient();
    dnsServer.processNextRequest();

    // Get the current time
    time_t now = time(nullptr);
    struct tm* ptm = localtime(&now);
    int currentHour = ptm->tm_hour;
    int currentMinute = ptm->tm_min;

    // Calculate sunset time with offset
    int sunsetOnHour = (sunsetHour + scheduleConfig.sunsetOffsetHour) % 24;
    int sunsetOnMinute = (sunsetMinute + scheduleConfig.sunsetOffsetMinute) % 60;

    // Check for motion detection
    bool motionDetected = digitalRead(PIR_PIN);

    // Handle motion-triggered state
    if (motionDetected) {
        lastMotionTime = millis();
        if (currentState != HIGH_STATE) {
            logEvent("HIGH_STATE - Motion detected");
            fadeToRGBWBrightness(stateConfig.highState, stateConfig.fadeDurationOn);
            currentState = HIGH_STATE;
        }
        return;
    }

    // Handle schedule-based transitions
    if (currentHour == scheduleConfig.morningOnHour && currentMinute == scheduleConfig.morningOnMinute) {
        if (currentState != LOW_STATE) {
            logEvent("LOW_STATE - Morning schedule active");
            fadeToRGBWBrightness(stateConfig.lowState, stateConfig.fadeDurationOn);
            currentState = LOW_STATE;
        }
    } else if (currentHour == scheduleConfig.morningOffHour && currentMinute == scheduleConfig.morningOffMinute) {
        if (currentState != OFF_STATE) {
            logEvent("OFF_STATE - Morning schedule ended");
            RGBWConfig offConfig = {false, false, 0, 0, 0, 0, 0};
            fadeToRGBWBrightness(offConfig, stateConfig.fadeDurationOff);
            currentState = OFF_STATE;
        }
    } else if (currentHour == sunsetOnHour && currentMinute == sunsetOnMinute) {
        if (currentState != LOW_STATE) {
            logEvent("LOW_STATE - Sunset schedule active");
            fadeToRGBWBrightness(stateConfig.lowState, stateConfig.fadeDurationOn);
            currentState = LOW_STATE;
        }
    } else if (currentHour == scheduleConfig.eveningOffHour && currentMinute == scheduleConfig.eveningOffMinute) {
        if (currentState != OFF_STATE) {
            logEvent("OFF_STATE - Evening schedule ended");
            RGBWConfig offConfig = {false, false, 0, 0, 0, 0, 0};
            fadeToRGBWBrightness(offConfig, stateConfig.fadeDurationOff);
            currentState = OFF_STATE;
        }
    }

    // Handle motion hold expiration
    if (!motionDetected && millis() - lastMotionTime > stateConfig.holdTime) {
        if (currentState == HIGH_STATE) {
            if ((currentHour >= scheduleConfig.morningOnHour && currentHour < scheduleConfig.morningOffHour) ||
                (currentHour >= sunsetOnHour && currentHour < scheduleConfig.eveningOffHour)) {
                logEvent("LOW_STATE - Motion hold expired, returning to schedule");
                fadeToRGBWBrightness(stateConfig.lowState, stateConfig.fadeDurationOff);
                currentState = LOW_STATE;
            } else {
                logEvent("OFF_STATE - Motion hold expired");
                RGBWConfig offConfig = {false, false, 0, 0, 0, 0, 0};
                fadeToRGBWBrightness(offConfig, stateConfig.fadeDurationOff);
                currentState = OFF_STATE;
            }
        }
    }
}