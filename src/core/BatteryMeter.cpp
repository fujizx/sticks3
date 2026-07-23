#include "BatteryMeter.h"

#include <M5Unified.h>

namespace {
constexpr uint32_t kBatteryReadIntervalMs = 10000;
}

void BatteryMeter::begin() {
  loop();
}

void BatteryMeter::loop() {
  const uint32_t now = millis();
  if (now - lastReadMs_ < kBatteryReadIntervalMs && level_ >= 0) return;
  lastReadMs_ = now;
  level_ = M5.Power.getBatteryLevel();
  voltageMv_ = M5.Power.getBatteryVoltage();
  currentMa_ = M5.Power.getBatteryCurrent();
  charging_ = M5.Power.isCharging();
}

int BatteryMeter::level() const {
  return level_;
}

int BatteryMeter::voltageMv() const {
  return voltageMv_;
}

int BatteryMeter::currentMa() const {
  return currentMa_;
}

bool BatteryMeter::charging() const {
  return charging_;
}

String BatteryMeter::text() const {
  if (level_ < 0) return "BAT --";
  return "BAT " + String(level_) + "%";
}
