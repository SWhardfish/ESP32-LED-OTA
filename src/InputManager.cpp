#include "InputManager.h"

InputManager::InputManager(uint8_t pin) : pin(pin), lastState(false), pressed(false), lastDebounce(0) {}

void InputManager::begin() {
  pinMode(pin, INPUT_PULLDOWN);
}

void InputManager::update() {
  bool reading = digitalRead(pin);
  unsigned long now = millis();

  if (reading != lastState) {
    lastDebounce = now;
  }

  if ((now - lastDebounce) > debounceMs) {
    if (reading && !lastState) {
      pressed = true;
    }
  }

  lastState = reading;
}

bool InputManager::wasPressed() {
  if (pressed) {
    pressed = false;
    return true;
  }
  return false;
}