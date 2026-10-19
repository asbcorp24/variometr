#pragma once
#include <Wire.h>
#include "battery.h"

class PowerMonitor {
 public:
  void read(battery::State &state, int cells);
 private:
  bool inaSeen_ = false;
  bool readReg(uint8_t address, uint8_t reg, uint8_t *data, size_t count);
};
