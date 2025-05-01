#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200); // GMT+2 for Stockholm (summer time)

// LED strip settings
#define LED_PIN 6
#define NUM_LEDS 61
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);

// PIR sensor
#define PIR_PIN 7

// Brightness levels
#define BRIGHTNESS_LOW 25    // 10%
#define BRIGHTNESS_HIGH 191  // 75%

// Motion hold
unsigned long lastMotionTime = 0;
const unsigned long holdTime = 60000; // 60 seconds

// Sunset hour (dynamic)
int sunsetHour = 18; // default fallback

// Web server
WebServer server(80);

// Logging
#define LOG_BUFFER_SIZE 50
String logBuffer[LOG_BUFFER_SIZE];
int logIndex = 0;

// State tracking
int previousBrightness = -1;

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
    client.setInsecure(); // Skip TLS validation (not secure for production)

    String url = "https://api.sunrise-sunset.org/json?lat=59.3293&lng=18.0686&formatted=0";
    http.begin(client, url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      DeserializationError err = deserializeJson(doc, payload);
      if (!err) {
        String sunsetTime = doc["results"]["sunset"]; // Format: "2024-05-01T18:37:00+00:00"
        int hourUTC = sunsetTime.substring(11, 13).toInt();
        sunsetHour = (hourUTC + 2) % 24; // Stockholm summer time = UTC+2
        logEvent("Sunset time updated from API: " + String(sunsetHour) + ":00");
      } else {
        logEvent("JSON parse error on sunset time.");
      }
    } else {
      logEvent("Failed to fetch sunset time. HTTP code: " + String(httpCode));
    }
    http.end();
  }
}

void handleRoot() {
  String html = "<html><body><h1>Event Log</h1><ul>";
  for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
    int index = (logIndex - 1 - i + LOG_BUFFER_SIZE) % LOG_BUFFER_SIZE;
    if (logBuffer[index].length() > 0) {
      html += "<li>" + logBuffer[index] + "</li>";
    }
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
    bool canBegin = Update.begin(contentLength);
    if (canBegin) {
      WiFiClient* stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);
      if (written == contentLength) {
        logEvent("OTA update written. Rebooting.");
        if (Update.end(true)) {
          ESP.restart();
        } else {
          logEvent("OTA failed: " + String(Update.getError()));
        }
      } else {
        logEvent("OTA incomplete. " + String(written) + "/" + String(contentLength));
        Update.end();
      }
    } else {
      logEvent("Not enough space for OTA.");
    }
  } else {
    logEvent("OTA download failed. HTTP code: " + String(httpCode));
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  timeClient.begin();

  fetchSunsetHour(); // Get dynamic sunset time

  strip.begin();
  setWhiteBrightness(BRIGHTNESS_LOW); // Default to 10%

  pinMode(PIR_PIN, INPUT);

  server.on("/", handleRoot);
  server.on("/update", handleOTAUpdate);
  server.begin();

  logEvent("System initialized.");
}

// State tracking
enum LightState { OFF_STATE, LOW_STATE, HIGH_STATE };
LightState currentState = OFF_STATE;

void loop() {
  timeClient.update();
  server.handleClient();

  unsigned long now = millis();
  static unsigned long lastUpdate = 0;
  static bool motionActive = false;

  bool motionDetected = digitalRead(PIR_PIN);
  unsigned long currentTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&currentTime);
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

  // Handle no motion
  if (!motionDetected && motionActive && now - lastMotionTime > holdTime) {
    motionActive = false;
    if (isScheduledOn) {
      if (currentState != LOW_STATE) {
        setWhiteBrightness(BRIGHTNESS_LOW);
        currentState = LOW_STATE;
        logEvent("No motion detected. Light turned ON to 10% brightness.");
      }
    } else {
      if (currentState != OFF_STATE) {
        turnOffStrip();
        currentState = OFF_STATE;
        logEvent("No motion detected. Light turned OFF.");
      }
    }
  }

  delay(100);
}

