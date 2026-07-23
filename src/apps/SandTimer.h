#pragma once

#include <Arduino.h>

namespace SandTimer {

int durationCount();
void drawSelect(int selectedIndex);
void start(int selectedIndex);
bool drawRun(bool force = false);
void drawPrompt(int promptIndex);
void stop();

}  // namespace SandTimer
