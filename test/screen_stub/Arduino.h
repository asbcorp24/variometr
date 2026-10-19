#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdarg>
#define PROGMEM
#define PI 3.14159265358979323846
inline float radians(float degrees){return degrees*PI/180;}
using String = std::string;
class __FlashStringHelper;
inline unsigned long millis(){return 10000;}
struct SerialStub { void printf(const char *,...){} };
extern SerialStub Serial;
