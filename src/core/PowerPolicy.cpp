#include "PowerPolicy.h"

#include <M5Unified.h>

namespace {
constexpr uint8_t kDimBrightness = 15;
constexpr uint8_t kOffBrightness = 0;
constexpr uint32_t kDimIdleMs = 2UL * 60UL * 1000UL;
constexpr uint32_t kScreenOffIdleMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kAnimatedLoopDelayMs = 20;
constexpr uint32_t kStaticLoopDelayMs = 150;
constexpr uint32_t kScreenOffLoopDelayMs = 1000;
constexpr uint32_t kAnimatedImuIntervalMs = 20;
constexpr uint32_t kStaticImuIntervalMs = 150;
}

void PowerPolicy::begin(uint8_t activeBrightness) {
  activeBrightness_ = activeBrightness;
  lastActivityMs_ = millis();
  applyBrightness(activeBrightness_);
}

void PowerPolicy::noteActivity() {
  lastActivityMs_ = millis();
  dimmed_ = false;
  screenOff_ = false;
  applyBrightness(activeBrightness_);
}

void PowerPolicy::update(bool animatedScreen) {
  (void)animatedScreen;
  const uint32_t idle = idleMs();
  const bool nextScreenOff = idle >= kScreenOffIdleMs;
  const bool nextDimmed = idle >= kDimIdleMs;
  screenOff_ = nextScreenOff;
  dimmed_ = nextDimmed;

  if (screenOff_) {
    applyBrightness(kOffBrightness);
  } else if (dimmed_) {
    applyBrightness(kDimBrightness);
  } else {
    applyBrightness(activeBrightness_);
  }
}

uint32_t PowerPolicy::loopDelayMs(bool animatedScreen) const {
  if (screenOff_) return kScreenOffLoopDelayMs;
  return animatedScreen ? kAnimatedLoopDelayMs : kStaticLoopDelayMs;
}

bool PowerPolicy::shouldSampleImu(bool motionEnabled, bool animatedScreen) {
  if (!motionEnabled) return false;
  const uint32_t now = millis();
  const uint32_t interval = animatedScreen ? kAnimatedImuIntervalMs : kStaticImuIntervalMs;
  if (now - lastImuSampleMs_ < interval) return false;
  lastImuSampleMs_ = now;
  return true;
}

bool PowerPolicy::dimmed() const {
  return dimmed_;
}

bool PowerPolicy::screenOff() const {
  return screenOff_;
}

uint8_t PowerPolicy::brightness() const {
  return brightness_;
}

uint32_t PowerPolicy::idleMs() const {
  return millis() - lastActivityMs_;
}

void PowerPolicy::applyBrightness(uint8_t brightness) {
  if (brightness_ == brightness) return;
  M5.Display.setBrightness(brightness);
  brightness_ = brightness;
}
