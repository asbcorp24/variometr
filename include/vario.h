#pragma once
#include <cmath>
#include <cstdint>

namespace vario {
inline double altitude(double pressurePa, double qnhHpa) {
  return 44330.0 * (1.0 - std::pow(pressurePa / (qnhHpa * 100.0), 0.1903));
}

class Filter {
 public:
  double qnh = 1013.25;
  double averageSeconds = 10.0;
  double height = 0, velocity = 0, average = 0;

  void reset() {
    count = 0; initialized = false; velocity = average = 0;
    avgCount = avgHead = 0; avgSum = 0;
  }
  bool setQnh(double value) {
    if (!std::isfinite(value) || value < 800 || value > 1100) return false;
    qnh = value;
    reset(); // Never differentiate a change of reference pressure.
    return true;
  }
  bool setAverage(double seconds) {
    if (!std::isfinite(seconds) || seconds < 1 || seconds > 30) return false;
    averageSeconds = seconds;
    avgCount = avgHead = 0; avgSum = average = 0;
    return true;
  }
  bool fresh(uint32_t now) const { return initialized && uint32_t(now - last) <= 500; }
  bool ready(uint32_t now) const { return fresh(now) && count == window; }
  double relative() const { return height - altitude(zeroPressure, qnh); }
  bool zero(uint32_t now) {
    if (!fresh(now)) return false;
    zeroPressure = qnh * 100.0 * std::pow(1.0 - height / 44330.0, 1.0 / 0.1903);
    return true;
  }
  bool update(double pressurePa, uint32_t now) {
    if (!std::isfinite(pressurePa) || pressurePa < 30000 || pressurePa > 110000) {
      reset();
      return false;
    }
    if (initialized && uint32_t(now - last) == 0) return false;
    const double dt = initialized ? uint32_t(now - last) / 1000.0 : 0;
    if (initialized && dt > 0.5) reset();
    const double measured = altitude(pressurePa, qnh);
    if (!initialized) {
      height = measured;
      if (zeroPressure == 0) zeroPressure = pressurePa;
      initialized = true;
    } else {
      height += (1.0 - std::exp(-dt / 0.35)) * (measured - height);
    }
    last = now;
    if (count == window) {
      for (unsigned i = 1; i < window; ++i) { heights[i - 1] = heights[i]; times[i - 1] = times[i]; }
      --count;
    }
    heights[count] = height;
    times[count++] = now;
    if (count == window) {
      double sx = 0, sy = 0, sxx = 0, sxy = 0;
      for (unsigned i = 0; i < count; ++i) {
        const double x = uint32_t(times[i] - times[0]) / 1000.0;
        const double y = heights[i] - heights[0];
        sx += x; sy += y; sxx += x * x; sxy += x * y;
      }
      const double denominator = count * sxx - sx * sx;
      if (denominator <= 0) { reset(); return false; }
      const double slope = (count * sxy - sx * sy) / denominator;
      velocity += (1.0 - std::exp(-dt / 0.3)) * (slope - velocity);
      while (avgCount && (uint32_t(now - avgTimes[avgHead]) >= averageSeconds * 1000.0 || avgCount == avgCapacity)) {
        avgSum -= avgValues[avgHead];
        avgHead = (avgHead + 1) % avgCapacity;
        --avgCount;
      }
      const unsigned tail = (avgHead + avgCount) % avgCapacity;
      avgValues[tail] = velocity; avgTimes[tail] = now;
      avgSum += velocity; ++avgCount;
      average = avgSum / avgCount;
    }
    return true;
  }
 private:
  static constexpr unsigned window = 16;
  double heights[window] = {}, zeroPressure = 0;
  uint32_t times[window] = {}, last = 0;
  unsigned count = 0;
  bool initialized = false;
  static constexpr unsigned avgCapacity = 301;
  double avgValues[avgCapacity] = {}, avgSum = 0;
  uint32_t avgTimes[avgCapacity] = {};
  unsigned avgHead = 0, avgCount = 0;
};

enum class Sound { Silent, Climb, Sink, Alarm };
inline Sound soundMode(double speed, Sound previous) {
  if (!std::isfinite(speed)) return Sound::Silent;
  if (speed <= -10 || (previous == Sound::Alarm && speed < -9.5)) return Sound::Alarm;
  if (speed <= -2 || (previous == Sound::Sink && speed < -1.8)) return Sound::Sink;
  if (speed >= 0.2 || (previous == Sound::Climb && speed > 0.1)) return Sound::Climb;
  return Sound::Silent;
}
inline uint32_t tone(double speed, Sound mode, uint32_t phaseMs) {
  if (mode == Sound::Alarm) return (phaseMs / 180) % 2 ? 1800 : 700;
  if (mode == Sound::Sink) return 350;
  if (mode != Sound::Climb) return 0;
  const double v = speed < 0 ? 0 : speed > 10 ? 10 : speed;
  const uint32_t period = static_cast<uint32_t>(750 - v * 60);
  return phaseMs % period < period / 2 ? static_cast<uint32_t>(800 + v * 100) : 0;
}
}
