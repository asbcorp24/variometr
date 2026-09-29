#pragma once
#include <cstdint>
constexpr int LOW = 0;
constexpr int HIGH = 1;
constexpr int INPUT = 1;
constexpr int INPUT_PULLUP = 2;
unsigned long millis();
int digitalRead(int pin);
void pinMode(int pin, int mode);
