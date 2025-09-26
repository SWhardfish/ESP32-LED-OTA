#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

typedef std::function<void(bool connected)> WifiCallback;

class WifiManager {
public:
    WifiManager();
    void begin();
    bool tryConnectSaved();
    void startConfigPortal();
    void stopConfigPortal();
    void loop();

    void onConnectionChanged(WifiCallback cb);

    void saveCredentials(const String &ssid, const String &password);

private:
    void loadCredentials();
    void saveCredentialsInternal();

    String _ssid;
    String _password;
    bool _connected;
    WifiCallback _cb;
    unsigned long _connectStart;
};

#endif // WIFIMANAGER_H
