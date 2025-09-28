//LedStatus.cpp
#include <Arduino.h>
#include "LedStatus.h"

LedStatus::LedStatus(uint8_t pin, uint8_t brightness)
    : strip(1, pin, NEO_GRB + NEO_KHZ800)
{
    strip.setBrightness(brightness);
}

void LedStatus::begin() {
    strip.begin();
    strip.show(); // turn off initially
}

void LedStatus::setColor(uint8_t r, uint8_t g, uint8_t b) {
    flashing = false;
    strip.setPixelColor(0, strip.Color(r, g, b));
    strip.show();
}

void LedStatus::flashRed() {
    flashing = true;
}

void LedStatus::loop() {
    if (!flashing) return;
    unsigned long now = millis();
    if (now - lastToggle >= 1000) { // 1 s blink
        lastToggle = now;
        ledOn = !ledOn;
        strip.setPixelColor(0, ledOn ? strip.Color(255, 0, 0) : 0);
        strip.show();
    }
}
