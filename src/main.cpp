#include "LEDController.h"
#include "InputManager.h"

// --- Pin definitions ---
#define LED_PIN     6      // SK6812 data pin
#define LED_COUNT   2      // two LEDs in test rig
#define BUTTON_PIN  8      // button to 3.3V (with internal pulldown)

// --- Global objects ---
LEDController leds(LED_PIN, LED_COUNT);
InputManager button(BUTTON_PIN);

// --- State machine variables ---
enum State {OFF, FADING_IN, ON, FADING_OUT};
State state = OFF;

unsigned long stateStart = 0;
const int fadeInDuration  = 2000;   // ms
const int fadeOutDuration = 10000;  // ms
const int holdTimeMs      = 5000;   // ms

// --- Track whether we're in test mode ---
bool testMode = false;

void setup() {
  leds.begin();
  button.begin();

  // Check for test mode at startup: hold button HIGH at boot
  delay(50); // settle input
  if (digitalRead(BUTTON_PIN) == HIGH) {
    testMode = true;
  }
}

void loop() {
  if (testMode) {
    // --- RGBW wiring test cycle ---
    leds.testCycle();
    return; // never reach state machine
  }

  // --- Update button state ---
  button.update();
  unsigned long now = millis();

  // --- Button press detection ---
  if (button.wasPressed()) {
    if (state == OFF) {
      state = FADING_IN;
      stateStart = now;
    } else if (state == ON) {
      stateStart = now; // restart hold timer
    } else if (state == FADING_OUT) {
      state = ON; // cancel fade out
      stateStart = now;
    }
  }

  // --- State machine ---
  switch (state) {
    case OFF:
      leds.setBrightness(0);
      break;

    case FADING_IN: {
      float progress = (float)(now - stateStart) / fadeInDuration;
      if (progress >= 1.0f) {
        progress = 1.0f;
        state = ON;
        stateStart = now;
      }
      leds.setBrightness((int)(progress * 255));
      break;
    }

    case ON:
      leds.setBrightness(255);
      if (now - stateStart >= holdTimeMs) {
        state = FADING_OUT;
        stateStart = now;
      }
      break;

    case FADING_OUT: {
      float progress = (float)(now - stateStart) / fadeOutDuration;
      if (progress >= 1.0f) {
        progress = 1.0f;
        state = OFF;
      }
      leds.setBrightness((int)((1.0f - progress) * 255));
      break;
    }
  }
}
