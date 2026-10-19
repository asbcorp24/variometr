#pragma once
#include <cstdint>
#include <cmath>

namespace battery {
constexpr uint8_t Address = 0x36;
enum class Mode { Demo, Measured, Error, VoltageOnly };
struct State {
  Mode mode = Mode::Demo;
  float percent = 100;
  float voltage = NAN;
  bool seen = false;
  uint32_t timestamp = 0;
  bool estimated = false;
  float currentAmps = NAN;
  void missing() {
    mode = seen ? Mode::Error : Mode::Demo;
    percent = seen ? NAN : 100;
    voltage = NAN;
  }
  void error() { mode = Mode::Error; percent = voltage = currentAmps = NAN; }
  static float voltagePercent(float cell) {
    // Approximate resting-voltage curve for ordinary 4.2 V Li-ion, not LiFePO4.
    static const float volts[] = {3.0f,3.3f,3.5f,3.6f,3.7f,3.8f,3.9f,4.0f,4.1f,4.2f};
    static const float soc[] = {0,5,10,20,40,60,75,85,95,100};
    if (cell <= volts[0]) return 0;
    for (unsigned i = 1; i < 10; ++i)
      if (cell <= volts[i]) return soc[i-1] + (cell-volts[i-1])/(volts[i]-volts[i-1])*(soc[i]-soc[i-1]);
    return 100;
  }
  bool updateIna(float volts, float amps, int cells, uint32_t now) {
    seen = true; estimated = true;
    if (!std::isfinite(volts) || volts <= 0 || volts > 85) { error(); return false; }
    voltage = volts; currentAmps = amps; timestamp = now; percent = NAN;
    mode = Mode::VoltageOnly;
    if (cells >= 1 && cells <= 6) {
      const float cell = volts / cells;
      if (cell > 4.35f || cell < 2.0f) { error(); return false; }
      percent = voltagePercent(cell); mode = Mode::Measured;
    }
    return true;
  }
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
    estimated = false; currentAmps = NAN;
    return true;
  }
  void expire(uint32_t now) {
    if ((mode == Mode::Measured || mode == Mode::VoltageOnly) && uint32_t(now - timestamp) > 3000) error();
  }
};
}
