#pragma once

#include <Arduino.h>
#include <time.h>

#include "AppConfig.h"

class TimeSync {
 public:
  void begin(const AppSettings &settings);
  bool loop(bool wifiConnected);
  bool ready() const;
  String timeText() const;
  String dateText() const;

 private:
  bool configured_ = false;
  bool ready_ = false;
  uint32_t lastCheckMs_ = 0;
};
