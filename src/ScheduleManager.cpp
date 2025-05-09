#include <ArduinoJson.h>
#include "ScheduleManager.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

int sunsetHour = 18;
int sunsetMinute = 0;

void setupScheduleManager() {
  configTzTime("CET-1CEST,M3.5.0/02,M10.5.0/03", "pool.ntp.org");

  // Set a callback to fetch sunset time daily
  fetchSunsetTime();
}

bool isOnSchedule() {
  time_t now = time(nullptr);
  struct tm* ptm = localtime(&now);

  int currentHour = ptm->tm_hour;
  int currentMinute = ptm->tm_min;
  int currentDay = ptm->tm_yday;

  static int lastSunsetCheckDay = -1;
  if (currentDay != lastSunsetCheckDay) {
    // Only check sunset once per day
    fetchSunsetTime();
    lastSunsetCheckDay = currentDay;
  }

  // Calculate sunset time with offset
  int sunsetOnHour = (sunsetHour + scheduleConfig.sunsetOffsetHour) % 24;
  int sunsetOnMinute = (sunsetMinute + scheduleConfig.sunsetOffsetMinute) % 60;

  // Morning schedule
  bool morningActive = (currentHour > scheduleConfig.morningOnHour ||
                       (currentHour == scheduleConfig.morningOnHour && currentMinute >= scheduleConfig.morningOnMinute)) &&
                      (currentHour < scheduleConfig.morningOffHour ||
                       (currentHour == scheduleConfig.morningOffHour && currentMinute < scheduleConfig.morningOffMinute));

  // Evening schedule
  bool eveningActive = (currentHour > sunsetOnHour ||
                       (currentHour == sunsetOnHour && currentMinute >= sunsetOnMinute)) &&
                      (currentHour < scheduleConfig.eveningOffHour ||
                       (currentHour == scheduleConfig.eveningOffHour && currentMinute < scheduleConfig.eveningOffMinute));

  return morningActive || eveningActive;
}

void fetchSunsetTime() {
  if (WiFi.status() != WL_CONNECTED) {
    logEvent("Cannot fetch sunset time - WiFi not connected");
    return;
  }

  HTTPClient http;
  WiFiClientSecure client;

  client.setInsecure(); // Skip SSL verification for simplicity

  // Using Stockholm coordinates (59.3293° N, 18.0686° E)
  String url = "https://api.sunrise-sunset.org/json?lat=59.3293&lng=18.0686&formatted=0";
  http.begin(client, url);

  logEvent("Fetching sunset time from API...");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, http.getString());

    if (!error) {
      String sunsetTime = doc["results"]["sunset"];
      // Example format: "2023-05-10T18:45:12+00:00"

      sunsetHour = sunsetTime.substring(11, 13).toInt();
      sunsetMinute = sunsetTime.substring(14, 16).toInt();

      // Convert from UTC to local time (UTC+2 for Stockholm in summer)
      sunsetHour = (sunsetHour + 2) % 24;

      logEvent("Today's sunset time: " + String(sunsetHour) + ":" +
              (sunsetMinute < 10 ? "0" : "") + String(sunsetMinute));
    } else {
      logEvent("Failed to parse sunset API response: " + String(error.c_str()));
    }
  } else {
    logEvent("Sunset API request failed: " + String(httpCode));
  }

  http.end();
}