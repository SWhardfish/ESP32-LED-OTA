//main.
#include <Arduino.h>
#include "LedStatus.h"
#include "WifiManager.h"
#include "LittleFS.h"

LedStatus led;
WifiManager wifi;

void setup() {
    Serial.begin(115200);
    led.begin();
    wifi.begin();
}

void loop() {
    wifi.loop();

    if (wifi.isConnected()) {
        led.setColor(0, 255, 0);      // steady green
    } else {
        led.flashRed();               // blinking red
    }
    led.loop();
}
