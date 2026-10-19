#pragma once
#include <Wire.h>
#include "calendar.h"

class DeviceClock {
 public:
  int zoneMinutes = 180;
  bool storedUtc = false;
  bool formatCertain = true;
  bool read();
  bool setLocal(const calendar::DateTime &local);
  bool trusted(uint32_t now) const { return valid_ && trusted_ && formatCertain && uint32_t(now - readAt_) < 5000; }
  bool available(uint32_t now) const { return valid_ && uint32_t(now - readAt_) < 5000; }
  int64_t utc(uint32_t now) const { return available(now) ? utc_ + uint32_t(now - readAt_) / 1000 : 0; }
  calendar::DateTime local(uint32_t now) const { return calendar::civil(utc(now) + int64_t(zoneMinutes) * 60); }
 private:
  bool valid_ = false, trusted_ = false;
  int64_t utc_ = 0;
  uint32_t readAt_ = 0;
};
