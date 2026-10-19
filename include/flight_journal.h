#pragma once
#include <FS.h>
#include "flight_record.h"

class FlightJournal {
 public:
  static constexpr size_t Capacity = 64;
  bool begin();
  bool start(uint32_t now, int64_t utc, bool trusted, int zone, float qnh, float altitude);
  bool stop(uint32_t now, flightlog::End reason = flightlog::End::Manual);
  void sample(uint32_t now, float altitude, float velocity, float pressure, float batteryVoltage);
  void tick(uint32_t now);
  bool active() const { return session_.active; }
  bool ready() const { return ready_; }
  const char *error() const { return error_; }
  size_t count() const { return count_; }
  const flightlog::Record &record(size_t newestIndex) const;
  const flightlog::Record &current() const { return session_.record; }
  uint32_t revision() const { return revision_; }
  size_t freeBytes() const;
 private:
  flightlog::Record records_[Capacity];
  flightlog::Session session_;
  File file_;
  size_t count_ = 0;
  uint32_t nextId_ = 1, sampleAt_ = 0, flushAt_ = 0, revision_ = 0;
  bool ready_ = false;
  char error_[80] = {};
  bool append(const char *format, ...);
  bool checkpoint(char kind, flightlog::End reason);
  bool load(File &file, flightlog::Record &record);
  void fault(const char *message);
};
