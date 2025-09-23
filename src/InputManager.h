#pragma once
#include <Arduino.h>

class InputManager {
public:
  InputManager(uint8_t pin);
  void begin();
  void update();
  bool wasPressed();

private:
  uint8_t pin;
  bool lastState;
  bool pressed;
  unsigned long lastDebounce;
  const unsigned long debounceMs = 50;
};