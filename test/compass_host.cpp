#include "compass.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
  compass::Heading h;
  assert(!h.fresh(0));
  const float x[] = {100, 0, -100, 0};
  const float y[] = {0, 100, 0, -100};
  for (int i = 0; i < 4; ++i) {
    h.invalidate(); h.update(x[i], y[i], 0);
    assert(std::fabs(h.degrees - i * 90) < 0.01f);
  }
  h.invalidate(); h.update(100, -1, 1000); h.update(100, 1, 1100);
  assert(h.degrees > 359 || h.degrees < 1); // Never rotates through 180 at north.
  assert(h.fresh(1599)); assert(!h.fresh(1600));
  h.update(0, 100, 1700); assert(std::fabs(h.degrees - 90) < 0.01f);
  h.update(0, 0, 1800); assert(!h.fresh(1800));
  h.update(std::numeric_limits<float>::quiet_NaN(), 1, 1900); assert(!h.valid);
  h.update(100, 0, UINT32_MAX - 50);
  assert(h.fresh(30)); assert(!h.fresh(500));
  std::puts("PASS: cardinal headings, north wrap, stale data, invalid data, timer wrap");
}
