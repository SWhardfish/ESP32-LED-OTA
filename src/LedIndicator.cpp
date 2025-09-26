#include "LedIndicator.h"
#include <driver/ledc.h>

LedIndicator::LedIndicator(uint8_t gpio) : _gpio(gpio), _ledcChannel(0), _freq(5000),
  _resolutionBits(8), _state(LED_OFF), _lastToggle(0), _blinkOn(false) {}

void LedIndicator::begin() {
  // Configure 3 channels (R,G,B) on separate ledc channels if onboard is RGB common pin mapping is single-pin PWM per color.
  // For a single RGB LED pin that expects full-color control you'd need 3 pins; here we assume the S3 Zero has an RGB LED on one pin that expects single-channel (common) -- adapt if your board has separate pins.
  // We'll implement single-pin RGB pseudo by toggling color using software pattern. Most S3 boards have separate pins; adjust as needed.
  ledcSetup(_ledcChannel, _freq, _resolutionBits);
  ledcAttachPin(_gpio, _ledcChannel);
  setState(LED_OFF);
}

int LedIndicator::_duty20pct() {
  int maxVal = (1 << _resolutionBits) - 1;
  return (maxVal * 20) / 100; // 20% duty
}

void LedIndicator::applyColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightnessPercent) {
  // Simple approach for single-pin RGB hardware: if your board has separate R/G/B pins, refactor to 3 channels.
  // We'll map color to duty on the single channel as a simple indicator:
  int duty = (1 << _resolutionBits) - 1;
  if (brightnessPercent > 100) brightnessPercent = 100;
  duty = (duty * brightnessPercent) / 100;
  // For the simple "on" indicator, just set duty. Color mixing requires separate pins.
  ledcWrite(_ledcChannel, duty);
}

void LedIndicator::setState(LedState state) {
  _state = state;
  if (state == LED_OFF) {
    ledcWrite(_ledcChannel, 0);
  } else if (state == LED_CONNECTED) {
    // steady green at 20% brightness -> use same duty (single-channel)
    ledcWrite(_ledcChannel, _duty20pct());
  } else if (state == LED_CONNECTING) {
    // start blinking pattern
    _lastToggle = millis();
    _blinkOn = true;
    ledcWrite(_ledcChannel, _duty20pct());
  }
}

void LedIndicator::loop() {
  if (_state == LED_CONNECTING) {
    unsigned long now = millis();
    if (now - _lastToggle >= 1000) { // 1s toggle
      _lastToggle = now;
      _blinkOn = !_blinkOn;
      ledcWrite(_ledcChannel, _blinkOn ? _duty20pct() : 0);
    }
  }
}
