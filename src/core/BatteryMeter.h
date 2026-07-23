#pragma once

#include <Arduino.h>

class BatteryMeter {
 public:
  void begin();
  void loop();
  int level() const;
  int voltageMv() const;
  int currentMa() const;
  bool charging() const;
  String text() const;

 private:
  uint32_t lastReadMs_ = 0;
  int level_ = -1;
  int voltageMv_ = 0;
  int currentMa_ = 0;
  bool charging_ = false;
};
