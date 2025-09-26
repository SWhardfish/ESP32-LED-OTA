#pragma once
#include <Arduino.h>
#include <WebServer.h>

class OtaManager {
public:
  OtaManager(WebServer* srv);
  void begin();
  void handle(); // call in main loop to handle server uploads if needed

private:
  WebServer* _server;
  void setupArduinoOta();
  void setupWebUpdate();
};
