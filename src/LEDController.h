#pragma once

#include "config.h"

void setupLEDController();
void fadeToTarget(const LightModeConfig& target, unsigned long duration);
void setLEDs(const LightModeConfig& mode);
void handleMotionDetection(bool motionDetected);
void handleSchedule(bool isOnSchedule);