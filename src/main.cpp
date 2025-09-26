#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "LedIndicator.h"
#include "WifiManager.h"
#include "OtaManager.h"
#include "WebFiles.h"

// choose GPIO21 for onboard LED per your request
#define LED_GPIO 21

LedIndicator led(LED_GPIO);
WifiManager wifiManager;
WebServer server(80);
OtaManager ota(&server);

void setup() {
  Serial.begin(115200);
  delay(100);

  // LittleFS
  if(!LittleFS.begin()){
    Serial.println("LittleFS mount failed");
  }

  led.begin();
  led.setState(LED_CONNECTING);

  wifiManager.begin();

  // connection callback to update LED
  wifiManager.onConnectionChanged([](bool connected) {
    if (connected) {
      led.setState(LED_CONNECTED);
    } else {
      led.setState(LED_CONNECTING);
    }
  });

  bool ok = wifiManager.tryConnectSaved();
  if (!ok) {
    // start AP mode for configuration
    wifiManager.startConfigPortal();
    led.setState(LED_CONNECTING);
  } else {
    led.setState(LED_CONNECTED);
  }

  // setup server endpoints (serves index.html)
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    // prefer serving LittleFS file if available:
    if (LittleFS.exists("/index.html")) {
      File f = LittleFS.open("/index.html", "r");
      server.streamFile(f, "text/html");
      f.close();
    } else {
      server.send(200, "text/html", WebFiles::index_html());
    }
  });

  // let WifiManager also handle /save saving earlier (it uses the same global server in WifiManager.cpp)
  // but our WifiManager used its own server instance in that code; for a single server approach, adapt as needed.
  // We'll also attach ota endpoints to this server
  ota.begin();
  server.begin();
}

void loop() {
  led.loop();
  ota.handle();
  server.handleClient();
  delay(1);
}
