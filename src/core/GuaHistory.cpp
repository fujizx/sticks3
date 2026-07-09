#include "GuaHistory.h"

namespace {
constexpr const char *kNamespace = "gua-log";
constexpr const char *kCountKey = "count";
constexpr const char *kRecordsKey = "records";
}

bool GuaHistory::begin() {
  if (!prefs_.begin(kNamespace, false)) return false;

  count_ = prefs_.getUChar(kCountKey, 0);
  if (count_ > kMaxRecords) count_ = 0;

  const size_t expected = sizeof(GuaRecord) * kMaxRecords;
  if (prefs_.getBytesLength(kRecordsKey) == expected) {
    prefs_.getBytes(kRecordsKey, records_, expected);
  }
  return true;
}

void GuaHistory::add(const GuaRecord &record) {
  for (int i = kMaxRecords - 1; i > 0; --i) {
    records_[i] = records_[i - 1];
  }
  records_[0] = record;
  if (count_ < kMaxRecords) ++count_;
  persist();
}

uint8_t GuaHistory::count() const {
  return count_;
}

bool GuaHistory::get(uint8_t index, GuaRecord &record) const {
  if (index >= count_) return false;
  record = records_[index];
  return true;
}

void GuaHistory::clear() {
  count_ = 0;
  memset(records_, 0, sizeof(records_));
  persist();
}

void GuaHistory::persist() {
  prefs_.putUChar(kCountKey, count_);
  prefs_.putBytes(kRecordsKey, records_, sizeof(records_));
}
