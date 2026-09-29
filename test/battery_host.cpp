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
  std::puts("PASS: demo, identification, voltage/SOC conversion, disconnect, bounds, recovery, stale data");
}
