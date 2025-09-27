#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ====== SETTINGS ======
#define LED_PIN   21      // On-board WS2812 data pin
#define LED_COUNT 1       // Only one built-in LED

Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  // Initialize serial for debug (optional)
  Serial.begin(115200);
  delay(500);

  // Initialize the NeoPixel
  pixel.begin();
  pixel.clear();          // Ensure LED starts off
  pixel.show();
  Serial.println("NeoPixel test starting...");
}

void loop() {
  // Cycle Red → Green → Blue → Off, 1 s each
  pixel.setPixelColor(0, pixel.Color(255, 0, 0)); // Red
  pixel.show();
  delay(1000);

  pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green
  pixel.show();
  delay(1000);

  pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue
  pixel.show();
  delay(1000);

  pixel.clear();  // Off
  pixel.show();
  delay(1000);
}
