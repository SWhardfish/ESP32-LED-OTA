//WifiManager.h
#pragma once
#include <WiFi.h>
#include <functional>

class WifiManager {
public:
    void begin();
    void loop();
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

    // Callback to notify connection status
    std::function<void(bool connected)> onStatusChange;
};

