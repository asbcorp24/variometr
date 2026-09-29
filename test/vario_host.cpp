#include "vario.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>
#include <deque>
#include <utility>

double pressure(double meters, double qnh = 1013.25) {
  return qnh * 100 * std::pow(1.0 - meters / 44330.0, 1.0 / 0.1903);
}
int main() {
  vario::Filter f;
  assert(std::abs(vario::altitude(101325, 1013.25)) < 1e-8);
  for (uint32_t t = 0; t <= 20000; t += 100) f.update(pressure(125), t);
  assert(f.ready(20000) && std::abs(f.velocity) < 1e-6);
  assert(f.zero(20000) && std::abs(f.relative()) < 1e-6);
  assert(!f.ready(20600));
  f.update(pressure(500), 21000);
  assert(!f.ready(21000) && f.velocity == 0);
  assert(f.setQnh(1025) && !f.ready(21000));
  assert(!f.setQnh(0) && !f.setQnh(NAN) && !f.setAverage(0));
  assert(!f.update(NAN, 21100) && !f.ready(21100));
  assert(!f.update(20000, 21200));
  for (double climb : {2.0, -3.0, -12.0}) {
    vario::Filter ramp;
    for (uint32_t t = 0; t <= 60000; t += 100) ramp.update(pressure(1000 + climb * t / 1000), t);
    assert(std::abs(ramp.velocity - climb) < 0.02);
    assert(std::abs(ramp.average - climb) < 0.05);
    assert(ramp.setQnh(1040));
    ramp.update(pressure(1000 + climb * 60), 60100);
    assert(!ramp.ready(60100) && ramp.velocity == 0);
  }
  vario::Filter noise;
  for (uint32_t t = 0; t <= 30000; t += 100) {
    noise.update(101325 + ((t / 100) % 2 ? 3 : -3), t);
    if (t > 5000) assert(std::abs(noise.velocity) < 0.1);
  }
  vario::Filter wrap;
  const uint32_t start = UINT32_MAX - 1000;
  for (uint32_t elapsed = 0; elapsed <= 10000; elapsed += 100)
    wrap.update(pressure(100 + elapsed / 1000.0), start + elapsed);
  assert(wrap.ready(start + 10000) && std::abs(wrap.velocity - 1) < 0.02);
  vario::Filter irregular;
  uint32_t elapsed = 0;
  for (unsigned i = 0; i < 400; ++i) {
    elapsed += i % 2 ? 90 : 130;
    irregular.update(pressure(300 + elapsed * 0.002), elapsed);
  }
  assert(std::abs(irregular.velocity - 2) < 0.02);
  const double vBeforeDuplicate = irregular.velocity;
  assert(!irregular.update(pressure(500), elapsed));
  assert(irregular.velocity == vBeforeDuplicate);
  assert(irregular.zero(elapsed));
  assert(irregular.setQnh(1030));
  irregular.update(pressure(300 + elapsed * 0.002), elapsed + 100);
  assert(irregular.velocity == 0 && !irregular.ready(elapsed + 100));

  vario::Filter movingAverage;
  assert(movingAverage.setAverage(3));
  std::deque<std::pair<uint32_t, double>> reference;
  for (uint32_t t = 0; t <= 15000; t += 100) {
    movingAverage.update(pressure(100 + 3 * std::sin(t / 2000.0)), t);
    if (!movingAverage.ready(t)) continue;
    reference.emplace_back(t, movingAverage.velocity);
    while (t - reference.front().first >= 3000) reference.pop_front();
    double sum = 0;
    for (const auto &sample : reference) sum += sample.second;
    assert(std::abs(movingAverage.average - sum / reference.size()) < 1e-9);
  }
  using vario::Sound;
  assert(vario::soundMode(0.05, Sound::Silent) == Sound::Silent);
  assert(vario::soundMode(0.2, Sound::Silent) == Sound::Climb);
  assert(vario::soundMode(0.15, Sound::Climb) == Sound::Climb);
  assert(vario::soundMode(0.09, Sound::Climb) == Sound::Silent);
  assert(vario::soundMode(-2, Sound::Silent) == Sound::Sink);
  assert(vario::soundMode(-10, Sound::Sink) == Sound::Alarm);
  assert(vario::soundMode(-9.6, Sound::Alarm) == Sound::Alarm);
  assert(vario::soundMode(-9.4, Sound::Alarm) == Sound::Sink);
  assert(vario::soundMode(NAN, Sound::Climb) == Sound::Silent);
  assert(vario::tone(2, Sound::Climb, 0) == 1000);
  assert(vario::tone(2, Sound::Climb, 400) == 0);
  assert(vario::tone(-12, Sound::Alarm, 0) != vario::tone(-12, Sound::Alarm, 180));
  std::puts("PASS: stationary, noise, climb/sink, QNH, invalid input, gaps, timer wrap, jitter, moving average, audio thresholds");
}
