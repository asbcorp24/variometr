#pragma once
#include <cmath>
#include <cstdint>

namespace compass {
inline float normalize(float angle) {
  angle = std::fmod(angle, 360.0f);
  return angle < 0 ? angle + 360.0f : angle;
}
struct Heading {
  float degrees = 0;
  uint32_t timestamp = 0;
  bool valid = false;
  void invalidate() { valid = false; }
  bool fresh(uint32_t now) const { return valid && uint32_t(now - timestamp) < 500; }
  void update(float x, float y, uint32_t now) {
    if (!std::isfinite(x) || !std::isfinite(y) || x * x + y * y < 1) {
      invalidate(); return;
    }
    const float next = normalize(std::atan2(y, x) * 180.0f / 3.141592654f);
    if (!fresh(now)) degrees = next;
    else {
      // Interpolate along the short arc, including the 359 -> 0 transition.
      const float delta = normalize(next - degrees + 180) - 180;
      const float dt = uint32_t(now - timestamp) / 1000.0f;
      degrees = normalize(degrees + delta * (dt / (0.18f + dt)));
    }
    timestamp = now; valid = true;
  }
};
}
