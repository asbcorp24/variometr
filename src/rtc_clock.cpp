#include "rtc_clock.h"
#include <Arduino.h>
namespace {
bool readReg(uint8_t reg, uint8_t *data, size_t size) {
  Wire.beginTransmission(0x68); Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(0x68), uint8_t(size)) != size) return false;
  for (size_t i = 0; i < size; ++i) data[i] = Wire.read();
  return true;
}
bool writeReg(uint8_t reg, const uint8_t *data, size_t size) {
  Wire.beginTransmission(0x68); Wire.write(reg); Wire.write(data, size);
  return Wire.endTransmission() == 0;
}
}
bool DeviceClock::read() {
  uint8_t raw[7], status;
  calendar::DateTime d;
  valid_ = readReg(0, raw, 7) && readReg(0x0f, &status, 1) && calendar::decodeRtc(raw, d);
  if (!valid_) { trusted_ = false; return false; }
  utc_ = calendar::epoch(d) - (storedUtc ? 0 : int64_t(zoneMinutes) * 60);
  trusted_ = storedUtc && !(status & 0x80);
  readAt_ = millis();
  return true;
}
bool DeviceClock::setLocal(const calendar::DateTime &local) {
  if (!calendar::valid(local) || !calendar::validZone(zoneMinutes)) return false;
  const int64_t target = calendar::epoch(local) - int64_t(zoneMinutes) * 60;
  const auto d = calendar::civil(target);
  if (d.year < 2000 || d.year > 2199) return false;
  uint8_t control, status, raw[7], verify[7];
  if (!readReg(0x0e, &control, 1) || !readReg(0x0f, &status, 1)) return false;
  calendar::encodeRtc(d, raw);
  control &= ~0x80; // Enable oscillator, preserve alarm/output settings.
  if (!writeReg(0x0e, &control, 1) || !writeReg(0, raw, 7) || !readReg(0, verify, 7)) return false;
  calendar::DateTime actual;
  if (!calendar::decodeRtc(verify, actual)) return false;
  const int64_t difference = calendar::epoch(actual) - target;
  if (difference < 0 || difference > 1) return false;
  status &= ~0x80; // Clear OSF only after verified time write; preserve alarm flags.
  if (!writeReg(0x0f, &status, 1)) return false;
  storedUtc = true;
  return read() && trusted_;
}
