#pragma once
#define OUTPUT 1
#define HIGH 1
#define IRAM_ATTR
void pinMode(int,int);
void digitalWrite(int,int);
struct FlashSerial{template<typename... T>void printf(const char*,T...) {}};
extern FlashSerial Serial;
