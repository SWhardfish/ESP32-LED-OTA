#pragma once
#include <Adafruit_NeoPixel.h>

class LedStatus {
public:
    // default brightness ~8% (20 was ~8% of 255)
    LedStatus(uint8_t pin = 21, uint8_t brightness = 20);
    void begin();
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void flashRed();   // slow blink
    void loop();       // call in main loop

private:
    Adafruit_NeoPixel strip;
    bool flashing = false;
    unsigned long lastToggle = 0;
    bool ledOn = false;
};
