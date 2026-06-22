#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct PomodoroRecord {
  uint32_t startedAt = 0;
  uint16_t plannedMinutes = 25;
  uint32_t actualSeconds = 0;
  bool completed = false;
};

class PomodoroHistory {
 public:
  static constexpr uint8_t kMaxRecords = 10;

  bool begin();
  void add(const PomodoroRecord &record);
  uint8_t count() const;
  bool get(uint8_t index, PomodoroRecord &record) const;
  void clear();

 private:
  Preferences prefs_;
  PomodoroRecord records_[kMaxRecords];
  uint8_t count_ = 0;
  void persist();
};
