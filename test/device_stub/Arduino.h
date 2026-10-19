#pragma once
#include <cstdint>
extern uint32_t deviceTestMillis;
inline unsigned long millis() { return deviceTestMillis; }
