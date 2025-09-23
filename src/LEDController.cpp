#include "LEDController.h"

LEDController::LEDController(uint8_t pin, uint16_t count)
: strip(count, pin, NEO_RGBW + NEO_KHZ800) {
  savedR = 0; savedG = 0; savedB = 0; savedW = 255; // default white only
}

void LEDController::begin() {
  strip.begin();
  strip.show();
}

void LEDController::setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  savedR = r; savedG = g; savedB = b; savedW = w;
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b, w));
  }
  strip.show();
}

void LEDController::setBrightness(uint8_t level) {
  uint8_t r = (uint8_t)((level / 255.0) * savedR);
  uint8_t g = (uint8_t)((level / 255.0) * savedG);
  uint8_t b = (uint8_t)((level / 255.0) * savedB);
  uint8_t w = (uint8_t)((level / 255.0) * savedW);

  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(r, g, b, w));
  }
  strip.show();
}

void LEDController::testCycle(unsigned long onMs, unsigned long offMs, unsigned long pauseMs) {
  struct Color { uint8_t r, g, b, w; };
  Color colors[] = {
    {255, 0, 0, 0},   // Red
    {0, 255, 0, 0},   // Green
    {0, 0, 255, 0},   // Blue
    {0, 0, 0, 255}    // White
  };

  for (auto &c : colors) {
    setAll(c.r, c.g, c.b, c.w);
    delay(onMs);
    setAll(0, 0, 0, 0);
    delay(offMs);
  }
  delay(pauseMs);
}
