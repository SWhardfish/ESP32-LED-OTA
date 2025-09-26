#pragma once
#include <Arduino.h>

enum LedState {
  LED_OFF,
  LED_CONNECTING,
  LED_CONNECTED
};

class LedIndicator {
public:
  LedIndicator(uint8_t gpio);
  void begin();
  void setState(LedState state);
  void loop(); // call regularly from main loop

private:
  uint8_t _gpio;
  uint8_t _ledcChannel;
  uint32_t _freq;
  uint8_t _resolutionBits;
  LedState _state;
  unsigned long _lastToggle;
  bool _blinkOn;
  int _duty20pct();

  void applyColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightnessPercent);
};
