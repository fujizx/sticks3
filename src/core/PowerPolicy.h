#pragma once

#include <Arduino.h>

class PowerPolicy {
 public:
  void begin(uint8_t activeBrightness);
  void noteActivity();
  void update(bool animatedScreen);
  uint32_t loopDelayMs(bool animatedScreen) const;
  bool shouldSampleImu(bool motionEnabled, bool animatedScreen);
  bool dimmed() const;
  bool screenOff() const;
  uint8_t brightness() const;
  uint32_t idleMs() const;

 private:
  void applyBrightness(uint8_t brightness);

  uint32_t lastActivityMs_ = 0;
  uint32_t lastImuSampleMs_ = 0;
  uint8_t activeBrightness_ = 50;
  uint8_t brightness_ = 255;
  bool dimmed_ = false;
  bool screenOff_ = false;
};
