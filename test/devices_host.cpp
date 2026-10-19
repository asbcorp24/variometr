#include "rtc_clock.h"
#include "power_monitor.h"
#include <cassert>
#include <cstdio>
#include <cstring>
TwoWire Wire;
uint32_t deviceTestMillis=0;
int main(){
  Wire.present[0x68]=true;
  calendar::DateTime local;local.year=2026;local.month=10;local.day=19;local.hour=15;local.minute=30;
  calendar::encodeRtc(local,Wire.regs[0]);Wire.regs[0x0e][0]=0x9b;Wire.regs[0x0f][0]=0x83;
  DeviceClock clock;assert(clock.read()&&!clock.trusted(0));
  assert(clock.setLocal(local));assert(clock.trusted(0));
  calendar::DateTime stored;assert(calendar::decodeRtc(Wire.regs[0],stored));assert(stored.hour==12&&stored.minute==30);
  assert(Wire.regs[0x0e][0]==0x1b&&Wire.regs[0x0f][0]==0x03); // Only oscillator/OSF bits changed.
  const auto utc=clock.utc(0);clock.zoneMinutes=-240;assert(clock.local(0).hour==8&&clock.utc(0)==utc);
  clock.formatCertain=false;assert(!clock.trusted(0));clock.formatCertain=true;
  deviceTestMillis=5000;assert(!clock.trusted(5000));assert(clock.read());
  Wire.failWrite=true;assert(!clock.setLocal(local));Wire.failWrite=false;
  Wire.failRead=true;assert(!clock.read()&&!clock.available(5000));Wire.failRead=false;
  Wire=TwoWire{};Wire.present[0x40]=true;
  Wire.regs[0x3e][0]=0x54;Wire.regs[0x3e][1]=0x49;
  Wire.regs[0x3f][0]=0x22;Wire.regs[0x3f][1]=0x81;
  Wire.regs[1][0]=0xf0;
  // 8.0 V / 195.3125 uV = 40960, shifted left four bits: 0x0A0000.
  Wire.regs[5][0]=0x0a;
  PowerMonitor power;battery::State state;power.read(state,0);
  assert(state.mode==battery::Mode::VoltageOnly&&state.voltage==8.0f&&std::isnan(state.percent));
  assert(Wire.writes==0);power.read(state,2);assert(state.percent==85&&state.estimated);
  // +1.5 mV across R015 = +100 mA, normal +/-163.84 mV ADC range.
  Wire.regs[4][0]=0x01;Wire.regs[4][1]=0x2c;Wire.regs[4][2]=0x00;
  power.read(state,2);assert(std::fabs(state.currentAmps-0.1f)<0.000001f);
  // Same magnitude, negative two's-complement 20-bit shunt voltage.
  Wire.regs[4][0]=0xfe;Wire.regs[4][1]=0xd4;
  power.read(state,2);assert(std::fabs(state.currentAmps+0.1f)<0.000001f);
  // ADC range bit changes shunt LSB by four, but not the bus voltage scale.
  Wire.regs[0][1]=0x10;Wire.regs[4][0]=0x01;Wire.regs[4][1]=0x2c;
  power.read(state,2);assert(std::fabs(state.currentAmps-0.025f)<0.000001f&&state.voltage==8.0f);
  Wire.regs[0][1]=0;Wire.regs[4][0]=Wire.regs[4][1]=Wire.regs[4][2]=0;
  power.read(state,2);assert(state.currentAmps==0&&Wire.writes==0);
  Wire.present[0x40]=false;power.read(state,2);assert(state.mode==battery::Mode::Error);
  Wire.present[0x40]=true;Wire.regs[0x3f][0]=0;power.read(state,2);assert(state.mode==battery::Mode::Error&&Wire.writes==0);
  Wire.regs[0x3f][0]=0x22;Wire.regs[1][0]=0;
  power.read(state,2);assert(state.mode==battery::Mode::Error&&Wire.regs[1][0]==0xf0&&Wire.writes==1);
  power.read(state,2);assert(state.mode==battery::Mode::Measured);
  Wire.failRead=true;power.read(state,2);assert(state.mode==battery::Mode::Error);
  std::puts("PASS: RTC/UTC; INA228 identity, voltage, R015 signed current, both ADC ranges, zero current, disconnect/recovery");
}
