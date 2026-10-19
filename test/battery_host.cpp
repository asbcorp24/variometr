#include "battery.h"
#include <cassert>
#include <cstdio>
int main() {
  battery::State s;
  s.missing(); assert(s.mode == battery::Mode::Demo && s.percent == 100 && std::isnan(s.voltage));
  assert(!s.update(0xffff, 50000, 12800, 0));
  assert(s.mode == battery::Mode::Error);
  assert(s.update(0x0011, 51200, 12800, 100));
  assert(std::fabs(s.voltage - 4.0f) < 0.001f && s.percent == 50);
  s.missing(); assert(s.mode == battery::Mode::Error && std::isnan(s.percent));
  assert(s.update(0x0012, 51200, 26000, 100)); assert(s.percent == 100);
  assert(s.update(0x0012, 40000, 0, 100)); assert(s.percent == 0);
  assert(!s.update(0x0012, 0xffff, 0xffff, 100));
  assert(s.update(0x0012, 51200, 25600, UINT32_MAX - 100));
  s.expire(200); assert(s.mode == battery::Mode::Measured);
  s.expire(4000); assert(s.mode == battery::Mode::Error);
  assert(s.updateIna(4.0f,NAN,0,0));assert(s.mode==battery::Mode::VoltageOnly&&std::isnan(s.percent));
  assert(s.updateIna(8.0f,0.5f,2,100));assert(s.estimated&&s.percent==85&&s.currentAmps==0.5f);
  assert(!s.updateIna(8.0f,0.5f,1,200));assert(s.mode==battery::Mode::Error);
  assert(s.updateIna(6.0f,-0.5f,2,300));assert(s.percent==0);
  assert(s.updateIna(8.4f,0,2,400));assert(s.percent==100);
  s.expire(3401);assert(s.mode==battery::Mode::Error);
  assert(s.updateIna(3.0f,0.2f,1,4000));assert(s.percent==0);
  assert(s.updateIna(3.7f,0.2f,1,4100));assert(std::fabs(s.percent-40)<0.01f);
  assert(s.updateIna(4.2f,0.0f,1,4200));assert(std::fabs(s.percent-100)<0.01f);
  assert(s.updateIna(3.9f,0.2f,1,4300));assert(std::fabs(s.percent-75)<0.01f);
  std::puts("PASS: battery legacy gauge, INA228 voltage-only/estimated SOC, cells, current, errors and stale data");
}
