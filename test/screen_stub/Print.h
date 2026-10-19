#pragma once
#include "Arduino.h"
class Print {
 public:
  virtual size_t write(uint8_t)=0;
  size_t write(const uint8_t *s,size_t n){for(size_t i=0;i<n;++i)write(s[i]);return n;}
  size_t write(const char *s){return write(reinterpret_cast<const uint8_t *>(s),strlen(s));}
  size_t print(const char *s){return write(s);}
};
