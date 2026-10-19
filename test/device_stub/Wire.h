#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
class TwoWire {
 public:
  uint8_t regs[256][8] = {};
  bool present[128] = {};
  bool failRead = false, failWrite = false;
  unsigned writes = 0;
  void beginTransmission(uint8_t address) { address_=address;length_=0; }
  size_t write(uint8_t value) { buffer_[length_++]=value;return 1; }
  size_t write(const uint8_t *data,size_t size) { for(size_t i=0;i<size;++i)write(data[i]);return size; }
  uint8_t endTransmission(bool = true) {
    if(!present[address_])return 2;
    if(length_==0)return 0;
    selected_=buffer_[0];
    if(length_>1){if(failWrite)return 3;memcpy(regs[selected_],buffer_+1,length_-1);++writes;}
    return 0;
  }
  size_t requestFrom(uint8_t address,uint8_t size){pos_=0;return failRead||!present[address]?0:size;}
  int read(){return regs[selected_][pos_++];}
 private:
  uint8_t address_=0,selected_=0,buffer_[16]={};
  size_t pos_=0,length_=0;
};
extern TwoWire Wire;
