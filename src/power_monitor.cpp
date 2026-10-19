#include "power_monitor.h"
#include "hardware_config.h"
#include <Arduino.h>
bool PowerMonitor::readReg(uint8_t address, uint8_t reg, uint8_t *data, size_t count) {
  Wire.beginTransmission(address); Wire.write(reg);
  if (Wire.endTransmission(false) || Wire.requestFrom(address, uint8_t(count)) != count) return false;
  for (size_t i = 0; i < count; ++i) data[i] = Wire.read();
  return true;
}
void PowerMonitor::read(battery::State &state, int cells) {
  Wire.beginTransmission(INA228_ADDRESS);
  const uint8_t ack = Wire.endTransmission();
  if (ack == 0) {
    uint8_t manufacturer[2], id[2], config[2], adc[2], bus[3], shunt[3];
    if (!readReg(INA228_ADDRESS, 0x3e, manufacturer, 2) ||
        !readReg(INA228_ADDRESS, 0x3f, id, 2) ||
        manufacturer[0] != 0x54 || manufacturer[1] != 0x49 ||
        ((uint16_t(id[0]) << 4) | (id[1] >> 4)) != 0x228) { state.error(); return; }
    inaSeen_ = true;
    if (!readReg(INA228_ADDRESS, 0x00, config, 2) || !readReg(INA228_ADDRESS, 0x01, adc, 2)) { state.error(); return; }
    if ((adc[0] & 0xf0) != 0xf0) {
      adc[0] |= 0xf0; // Continuous bus, shunt and temperature; preserve conversion times.
      Wire.beginTransmission(INA228_ADDRESS); Wire.write(0x01); Wire.write(adc, 2);
      if (Wire.endTransmission()) { state.error(); return; }
      state.error(); return; // Wait until next poll for conversions; never use pre-start data.
    }
    if (!readReg(INA228_ADDRESS, 0x05, bus, 3) || !readReg(INA228_ADDRESS, 0x04, shunt, 3)) { state.error(); return; }
    const uint32_t rawBus = (uint32_t(bus[0]) << 16) | (uint32_t(bus[1]) << 8) | bus[2];
    if (rawBus == 0xffffff) { state.error(); return; }
    int32_t rawShunt = (int32_t(shunt[0]) << 16) | (int32_t(shunt[1]) << 8) | shunt[2];
    rawShunt >>= 4;
    if (rawShunt & 0x80000) rawShunt -= 0x100000;
    const float shuntLsb = (config[1] & 0x10) ? 0.000000078125f : 0.0000003125f;
    const float amps = INA228_SHUNT_OHMS > 0 ? rawShunt * shuntLsb / INA228_SHUNT_OHMS : NAN;
    state.updateIna((rawBus >> 4) * 0.0001953125f, amps, cells, millis());
    return;
  }
  if (ack != 2 || inaSeen_) { state.error(); return; }
  // Keep compatibility with the earlier MAX17048 board, but never simulate 100%.
  uint8_t version[2], raw[4];
  Wire.beginTransmission(battery::Address);
  if (Wire.endTransmission() != 0 || !readReg(battery::Address, 0x08, version, 2) ||
      !readReg(battery::Address, 0x02, raw, 4)) { state.error(); return; }
  state.update((uint16_t(version[0]) << 8) | version[1], (uint16_t(raw[0]) << 8) | raw[1],
               (uint16_t(raw[2]) << 8) | raw[3], millis());
}
