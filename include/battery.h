#pragma once
#include <cstdint>
#include <cmath>

namespace battery {
constexpr uint8_t Address = 0x36;
enum class Mode { Demo, Measured, Error };
struct State {
  Mode mode = Mode::Demo;
  float percent = 100;
  float voltage = NAN;
  bool seen = false;
  uint32_t timestamp = 0;
  void missing() {
    mode = seen ? Mode::Error : Mode::Demo;
    percent = seen ? NAN : 100;
    voltage = NAN;
  }
  void error() { mode = Mode::Error; percent = voltage = NAN; }
  bool update(uint16_t version, uint16_t vcell, uint16_t soc, uint32_t now) {
    if ((version & 0xfff0) != 0x0010) { error(); return false; }
    seen = true;
    // MAX17048: VCELL 78.125 uV/LSB, SOC 1/256 percent/LSB.
    const float v = vcell * 0.000078125f;
    if (vcell == 0 || vcell == 0xffff || soc == 0xffff || v > 5.0f) {
      error(); return false;
    }
    voltage = v;
    percent = soc / 256.0f;
    if (percent > 100) percent = 100;
    timestamp = now; mode = Mode::Measured;
    return true;
  }
  void expire(uint32_t now) {
    if (mode == Mode::Measured && uint32_t(now - timestamp) > 3000) error();
  }
};
}
