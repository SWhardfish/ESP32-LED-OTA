#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>
#include <DNSServer.h>

// LED strip settingsX
#define LED_PIN 6
#define NUM_LEDS 61
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// PIR sensor
#define PIR_PIN 7

// Brightness levels
#define BRIGHTNESS_LOW 20
#define BRIGHTNESS_HIGH 180

// Motion hold
unsigned long lastMotionTime = 0;
const unsigned long holdTime = 30000;

// Web and NTP
WebServer server(80);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200); // GMT+2

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
  String timestamp = timeClient.getFormattedTime();
  String entry = timestamp + " - " + event;
  logBuffer[logIndex] = entry;
  logIndex = (logIndex + 1) % LOG_BUFFER_SIZE;
  Serial.println(entry);
}

void setWhiteBrightness(uint8_t brightness) {
  strip.setBrightness(brightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0, 255)); // Only white LED
  }
  strip.show();
  previousBrightness = brightness;
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
  server.send(200, "text/plain", "Starting OTA update...");
  delay(1000);
  performOTA("https://raw.githubusercontent.com/SWhardfish/ESP32-LED-OTA/main/ESP32-LED-OTA.bin");
}

void performOTA(const char* binURL) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, binURL);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (Update.begin(contentLength)) {
      WiFiClient* stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);
      if (written == contentLength && Update.end(true)) {
        logEvent("OTA complete. Restarting.");
        ESP.restart();
      } else {
        logEvent("OTA failed.");
        Update.end();
      }
    }
  } else {
    logEvent("OTA HTTP error: " + String(httpCode));
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
  setWhiteBrightness(BRIGHTNESS_LOW);

  if (!loadWiFiConfig() || !connectWiFi()) {
    Serial.println("WiFi failed. Starting AP mode.");
    startAPMode();
    return;
  }

  Serial.println("WiFi connected.");
  timeClient.begin();
  fetchSunsetHour();

  server.on("/", handleRoot);
  server.on("/update", handleOTAUpdate);
  server.begin();

  logEvent("System initialized.");
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();
  timeClient.update();

  static bool motionActive = false;
  bool motionDetected = digitalRead(PIR_PIN);
  unsigned long now = millis();

  time_t rawTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime(&rawTime);
  int hour = ptm->tm_hour;
  int minute = ptm->tm_min;
  bool isScheduledOn = (hour >= 6 && hour < 9) || (hour >= sunsetHour && (hour < 23 || (hour == 23 && minute < 30)));

  if (motionDetected) {
    motionActive = true;
    lastMotionTime = now;
    if (currentState != HIGH_STATE) {
      setWhiteBrightness(BRIGHTNESS_HIGH);
      currentState = HIGH_STATE;
      logEvent("Motion detected. Light turned ON to 75% brightness.");
    }
  }

  if (!motionDetected && motionActive && now - lastMotionTime > holdTime) {
    motionActive = false;
    if (isScheduledOn && currentState != LOW_STATE) {
      setWhiteBrightness(BRIGHTNESS_LOW);
      currentState = LOW_STATE;
      logEvent("No motion detected. Light turned ON to 10% brightness.");
    } else if (!isScheduledOn && currentState != OFF_STATE) {
      turnOffStrip();
      currentState = OFF_STATE;
      logEvent("No motion detected. Light turned OFF.");
    }
  }

  delay(100);
}
