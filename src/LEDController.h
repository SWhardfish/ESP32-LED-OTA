#pragma once
#include <Adafruit_NeoPixel.h>

class LEDController {
public:
  LEDController(uint8_t pin, uint16_t count);
  void begin();
  void setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
  void setBrightness(uint8_t level);

  // New: run a blocking RGBW test cycle
  void testCycle(unsigned long onMs = 2000, unsigned long offMs = 1000, unsigned long pauseMs = 5000);

private:
  Adafruit_NeoPixel strip;
  uint8_t savedR, savedG, savedB, savedW;
};
