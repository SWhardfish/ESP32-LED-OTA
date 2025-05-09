#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>

// Hardware configuration
#define FIRMWARE_VERSION "1.0.2"
#define LED_PIN 6
#define NUM_LEDS 61
#define PIR_PIN 7
#define LOG_BUFFER_SIZE 50
#define DNS_PORT 53

// WiFi configuration structure
struct WiFiConfig {
  String ssid;
  String password;
};

// Light mode configuration
struct LightModeConfig {
  uint8_t brightness = 20;    // 0-255
  uint32_t color = 0xFFFFFF;  // RGB hex value
  uint8_t white = 0;          // White channel 0-255
  bool colorEnabled = false;
  bool whiteEnabled = true;
};

// Schedule configuration
struct ScheduleConfig {
  // Time settings
  int morningOnHour = 6;
  int morningOnMinute = 0;
  int morningOffHour = 9;
  int morningOffMinute = 0;
  int sunsetOffsetHour = 0;
  int sunsetOffsetMinute = 0;
  int eveningOffHour = 23;
  int eveningOffMinute = 30;

  // Light modes
  LightModeConfig highMode;
  LightModeConfig lowMode;

  // Timing
  unsigned long fadeDurationOn = 2000;   // ms
  unsigned long fadeDurationOff = 30000; // ms
  unsigned long holdTime = 300000;       // 5 min
};

// Global variables
extern Adafruit_NeoPixel strip;
extern ScheduleConfig scheduleConfig;
extern WiFiConfig wifiConfig;
extern WebServer server;
extern DNSServer dnsServer;
extern LightModeConfig currentMode;
extern String logBuffer[];
extern int logIndex;

// Function declarations
void logEvent(String event);
bool saveConfig();
void setLEDs(const LightModeConfig& mode);
void fadeToTarget(const LightModeConfig& target, unsigned long duration);
bool loadWiFiConfig();
bool connectWiFi();
void startConfigPortal();