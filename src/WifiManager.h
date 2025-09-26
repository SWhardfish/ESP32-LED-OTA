#pragma once
#include <Arduino.h>
#include <functional>

using WifiCallback = std::function<void(bool connected)>;

class WifiManager {
public:
  WifiManager();
  void begin();
  bool tryConnectSaved(); // tries to connect using saved creds
  void startConfigPortal(); // starts AP and web portal
  void stopConfigPortal();
  void loop();
  void onConnectionChanged(WifiCallback cb);

  // set by web UI when user posts new SSID/password
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
