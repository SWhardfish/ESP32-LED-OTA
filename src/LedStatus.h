// LedStatus.h
#pragma once
#include <Adafruit_NeoPixel.h>

class LedStatus {
public:
    LedStatus(uint8_t pin = 21, uint8_t brightness = 51); // 20% of 255 ≈ 51
    void begin();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void flashRed();      // slow blink for connecting
    void loop();          // call in main loop
private:
    Adafruit_NeoPixel strip;
    bool flashing = false;
    unsigned long lastToggle = 0;
    bool ledOn = false;
};
