#pragma once
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace flightlog {
enum class End : uint8_t { Active, Manual, PowerLoss, StorageFull, IoError };
struct Record {
  uint32_t id = 0, durationMs = 0, samples = 0, gaps = 0;
  int64_t startUtc = 0;
  int zoneMinutes = 0;
  bool clockTrusted = false;
  float qnh = 1013.25f, startAltitude = 0, maxAltitude = 0, maxGain = 0, maxClimb = 0, minClimb = 0;
  End end = End::Active;
};
inline bool validSample(float altitude, float velocity, float pressure) {
  return std::isfinite(altitude) && std::isfinite(velocity) && std::isfinite(pressure) &&
         pressure >= 30000 && pressure <= 110000 && std::fabs(velocity) <= 200 && std::fabs(altitude) <= 20000;
}
struct Session {
  Record record;
  bool active = false;
  uint32_t last = 0;
  void begin(const Record &r, uint32_t now) { record = r; active = true; last = now; }
  void tick(uint32_t now) { if (active) { record.durationMs += uint32_t(now - last); last = now; } }
  void sample(float altitude, float velocity, float pressure) {
    if (!active) return;
    if (!validSample(altitude, velocity, pressure)) { ++record.gaps; return; }
    if (altitude > record.maxAltitude) record.maxAltitude = altitude;
    if (altitude - record.startAltitude > record.maxGain) record.maxGain = altitude - record.startAltitude;
    if (velocity > record.maxClimb) record.maxClimb = velocity;
    if (velocity < record.minClimb) record.minClimb = velocity;
    ++record.samples;
  }
};
inline uint32_t crc32(const char *text, size_t length) {
  uint32_t crc = 0xffffffff;
  for (size_t i = 0; i < length; ++i) {
    crc ^= uint8_t(text[i]);
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return ~crc;
}
}
