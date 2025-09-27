#include "LedStatus.h"
#include "WifiManager.h"

LedStatus led;
WifiManager wifi;

void setup() {
    Serial.begin(115200);
    led.begin();

    wifi.onStatusChange =  {
        if (connected) {
            led.setColor(0, 255, 0);  // steady green
        } else {
            led.flashRed();          // flashing red
        }
    };

    wifi.begin();
}

void loop() {
    wifi.loop();
    led.loop();
}
