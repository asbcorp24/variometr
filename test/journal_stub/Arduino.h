#pragma once
#include <cstdint>
inline void yield(){}
struct JournalSerial { void printf(const char *,...){} };
extern JournalSerial Serial;
