#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct GuaRecord {
  uint32_t createdAt = 0;
  uint8_t hexagramId = 0;
  uint8_t transformedId = 0;
};

class GuaHistory {
 public:
  static constexpr uint8_t kMaxRecords = 3;

  bool begin();
  void add(const GuaRecord &record);
  uint8_t count() const;
  bool get(uint8_t index, GuaRecord &record) const;
  void clear();

 private:
  Preferences prefs_;
  GuaRecord records_[kMaxRecords] = {};
  uint8_t count_ = 0;
  void persist();
};
