#include "LEDController.h"
#include "config.h"

LightModeConfig currentMode;
unsigned long lastMotionTime = 0;

void setupLEDController() {
  pinMode(PIR_PIN, INPUT);
  strip.begin();
  strip.show();

  // Initialize with low mode settings
  currentMode = scheduleConfig.lowMode;
  setLEDs(currentMode);
}

void setLEDs(const LightModeConfig& mode) {
  uint8_t r = 0, g = 0, b = 0, w = 0;

  if (mode.colorEnabled) {
    r = (mode.color >> 16) & 0xFF;
    g = (mode.color >> 8) & 0xFF;
    b = mode.color & 0xFF;
  }

  if (mode.whiteEnabled) {
    w = mode.white;
  }

  // For SK6812 (GRBW)
  uint32_t color = strip.Color(g, r, b, w);

  strip.setBrightness(mode.brightness);
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void fadeToTarget(const LightModeConfig& target, unsigned long duration) {
  if (duration == 0) {
    setLEDs(target);
    return;
  }

  LightModeConfig start = currentMode;
  unsigned long startTime = millis();

  while (millis() - startTime < duration) {
    float progress = float(millis() - startTime) / duration;
    if (progress > 1.0) progress = 1.0;

    LightModeConfig intermediate;
    intermediate.brightness = start.brightness + (target.brightness - start.brightness) * progress;
    intermediate.white = start.white + (target.white - start.white) * progress;

    // Only fade color if both modes have color enabled
    if (start.colorEnabled && target.colorEnabled) {
      intermediate.colorEnabled = true;
      uint8_t startR = (start.color >> 16) & 0xFF;
      uint8_t startG = (start.color >> 8) & 0xFF;
      uint8_t startB = start.color & 0xFF;

      uint8_t targetR = (target.color >> 16) & 0xFF;
      uint8_t targetG = (target.color >> 8) & 0xFF;
      uint8_t targetB = target.color & 0xFF;

      uint8_t interR = startR + (targetR - startR) * progress;
      uint8_t interG = startG + (targetG - startG) * progress;
      uint8_t interB = startB + (targetB - startB) * progress;

      intermediate.color = (interR << 16) | (interG << 8) | interB;
    } else {
      intermediate.colorEnabled = target.colorEnabled;
      intermediate.color = target.color;
    }

    intermediate.whiteEnabled = target.whiteEnabled;
    setLEDs(intermediate);
    delay(20);
  }

  setLEDs(target);
  currentMode = target;
}

void handleMotionDetection(bool motionDetected) {
  if (motionDetected) {
    lastMotionTime = millis();
    if (!(currentMode.brightness == scheduleConfig.highMode.brightness &&
          currentMode.color == scheduleConfig.highMode.color &&
          currentMode.white == scheduleConfig.highMode.white)) {
      logEvent("Motion detected - switching to high mode");
      fadeToTarget(scheduleConfig.highMode, scheduleConfig.fadeDurationOn);
    }
  } else if (millis() - lastMotionTime > scheduleConfig.holdTime) {
    // Handle timeout
  }
}

void handleSchedule(bool isOnSchedule) {
  // This would be called from main loop with schedule state
  if (isOnSchedule && currentMode.brightness == 0) {
    fadeToTarget(scheduleConfig.lowMode, scheduleConfig.fadeDurationOn);
  } else if (!isOnSchedule && currentMode.brightness > 0) {
    LightModeConfig offMode;
    offMode.brightness = 0;
    offMode.color = 0;
    offMode.white = 0;
    offMode.colorEnabled = false;
    offMode.whiteEnabled = false;
    fadeToTarget(offMode, scheduleConfig.fadeDurationOff);
  }
}