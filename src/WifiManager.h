// WifiManager.h
#pragma once
#include <WiFi.h>
#include "LittleFS.h"


class WifiManager {
public:
    void begin();
    void loop();
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
};
