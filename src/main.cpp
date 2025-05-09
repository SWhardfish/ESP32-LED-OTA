#include <WiFi.h>
#include <LittleFS.h>
#include <Update.h>
#include "config.h"
#include "WiFiManager.h"
#include "LEDController.h"
#include "WebServerHandler.h"
#include "WiFiManager.h"
#include "ScheduleManager.h"

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);
ScheduleConfig scheduleConfig;
String logBuffer[LOG_BUFFER_SIZE];
int logIndex = 0;
WebServer server(80);
DNSServer dnsServer;
WiFiConfig wifiConfig;

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

void setup() {
  Serial.begin(115200);
  logEvent("System starting - Firmware version " + String(FIRMWARE_VERSION));

  // Print chip information
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  logEvent("ESP32 Chip Model: " + String(chip_info.model));
  logEvent("ESP32 Cores: " + String(chip_info.cores));
  logEvent("ESP32 Revision: " + String(chip_info.revision));
  logEvent("Flash Size: " + String(spi_flash_get_chip_size() / (1024 * 1024)) + "MB");
  logEvent("Free Heap: " + String(esp_get_free_heap_size()) + " bytes");

  // Initialize filesystem
  if (!LittleFS.begin(true)) {
    logEvent("Failed to mount LittleFS, formatting...");
    if (!LittleFS.format()) {
      logEvent("LittleFS format failed!");
      delay(1000);
      ESP.restart();
    }
    if (!LittleFS.begin(true)) {
      logEvent("LittleFS still not working after format!");
      delay(1000);
      ESP.restart();
    }
  }
  logEvent("LittleFS mounted successfully");

  // Try to load config and connect to WiFi
  if (!loadWiFiConfig()) {
    logEvent("No WiFi config found, starting config portal");
    startConfigPortal();
  } else if (!connectWiFi()) {
    logEvent("WiFi connection failed, starting config portal");
    startConfigPortal();
  } else {
    // Initialize components
    setupScheduleManager();
    setupLEDController();
    setupWebServer();
    logEvent("System initialization complete");

    // Check for firmware update
    if (LittleFS.exists("/update.bin")) {
      logEvent("Found update.bin, attempting firmware update...");
      File updateFile = LittleFS.open("/update.bin", "r");

      if (updateFile) {
        size_t updateSize = updateFile.size();

        if (Update.begin(updateSize)) {
          size_t written = Update.writeStream(updateFile);

          if (written == updateSize) {
            logEvent("Update successfully written, " + String(written) + " bytes");

            if (Update.end()) {
              logEvent("OTA update complete, rebooting...");
              updateFile.close();
              LittleFS.remove("/update.bin");
              delay(1000);
              ESP.restart();
            } else {
              logEvent("Error ending OTA update: " + String(Update.getError()));
            }
          } else {
            logEvent("OTA write failed, expected " + String(updateSize) +
                    ", written " + String(written));
          }
        } else {
          logEvent("OTA begin failed: " + String(Update.getError()));
        }

        updateFile.close();
      } else {
        logEvent("Failed to open update.bin");
      }
    }
  }
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();

  static unsigned long lastScheduleCheck = 0;
  static bool lastScheduleState = false;

  // Check schedule every minute
  if (millis() - lastScheduleCheck > 60000) {
    bool currentScheduleState = isOnSchedule();

    if (currentScheduleState != lastScheduleState) {
      logEvent(currentScheduleState ? "Entering scheduled on period" : "Leaving scheduled on period");
      lastScheduleState = currentScheduleState;
    }

    lastScheduleCheck = millis();
  }

  // Handle motion detection
  bool motionDetected = digitalRead(PIR_PIN);
  handleMotionDetection(motionDetected);

  // Handle schedule-based lighting
  handleSchedule(lastScheduleState);

  // Other periodic tasks can go here
  static unsigned long lastHeapLog = 0;
  if (millis() - lastHeapLog > 300000) { // Every 5 minutes
    logEvent("Free heap: " + String(esp_get_free_heap_size()) + " bytes");
    lastHeapLog = millis();
  }
}