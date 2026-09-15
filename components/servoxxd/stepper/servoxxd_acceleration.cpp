#include "servoxxd_acceleration.h"
#include "servoxxd.h"
#include <cmath>
#include <algorithm>

namespace esphome {
namespace servoxxd {

static const char *const TAG = "servoxxd.acceleration";

// Hardware acceleration constants
static constexpr float MAX_RPM_PER_SEC = 20000.0f;  // acc=255 → Δt=50μs → 20000 RPM/s

// Primary constructor: float value
Acceleration::Acceleration(float value, AccelerationUnit unit, const ServoXxd *parent) : parent_(parent) {
  // Step 1: Convert to RPM/s
  float rpm_per_sec = 0.0f;

  switch (unit) {
    case AccelerationUnit::STEPS_PER_SEC_SQ:
      // Need parent for steps_per_revolution
      if (parent_ == nullptr) {
        ESP_LOGE(TAG, "Acceleration: parent required for STEPS_PER_SEC_SQ conversion");
        acc_ = 0;
        return;
      } else {
        float steps_per_rev = parent_->get_steps_per_revolution();
        if (steps_per_rev <= 0) {
          ESP_LOGE(TAG, "Acceleration: Invalid steps_per_revolution: %.2f", steps_per_rev);
          acc_ = 0;
          return;
        }
        rpm_per_sec = (value / steps_per_rev) * 60.0f;
      }
      break;

    case AccelerationUnit::RPM_PER_SEC:
      rpm_per_sec = value;
      break;

    case AccelerationUnit::REV_PER_SEC_SQ:
      rpm_per_sec = value * 60.0f;
      break;

    case AccelerationUnit::DEGREES_PER_SEC_SQ:
      rpm_per_sec = (value / 360.0f) * 60.0f;
      break;

    case AccelerationUnit::RADIANS_PER_SEC_SQ:
      rpm_per_sec = (value / (2.0f * M_PI)) * 60.0f;
      break;

    default:
      ESP_LOGE(TAG, "Unknown acceleration unit: %d", static_cast<int>(unit));
      acc_ = 0;
      return;
  }

  // Step 2: Map RPM/s to hardware value (0-255, non-linear inverse time)
  if (rpm_per_sec <= 0.0f || std::isinf(rpm_per_sec)) {
    // Zero, negative, or infinite acceleration → instant (no ramp)
    // Negative acceleration is physically invalid
    acc_ = 0;
  } else if (rpm_per_sec >= MAX_RPM_PER_SEC) {
    // Very high acceleration (>= 20000 RPM/s) → instant (no ramp)
    // This avoids acc values >= 255 which would clamp incorrectly
    acc_ = 0;
  } else {
    // Formula: acc = 256 - (20000 / rpm_per_sec)
    // We use 20000 instead of 20 because hardware Δt = (256 - acc) × 50 μs
    float acc_float = 256.0f - (MAX_RPM_PER_SEC / rpm_per_sec);
    // Clamp manually (C++11 compatible)
    if (acc_float < 1.0f)
      acc_ = 1;
    else if (acc_float > 255.0f)
      acc_ = 255;
    else
      acc_ = static_cast<uint8_t>(std::round(acc_float));  // Round instead of truncate
  }
}

// Constructor overload: double → float
Acceleration::Acceleration(double value, AccelerationUnit unit, const ServoXxd *parent)
    : Acceleration(static_cast<float>(value), unit, parent) {}

// Constructor overload: int64_t → float
Acceleration::Acceleration(int64_t value, AccelerationUnit unit, const ServoXxd *parent)
    : Acceleration(static_cast<float>(value), unit, parent) {}

// Constructor overload: int32_t → float
Acceleration::Acceleration(int32_t value, AccelerationUnit unit, const ServoXxd *parent)
    : Acceleration(static_cast<float>(value), unit, parent) {}

// Factory methods
Acceleration Acceleration::from_steps_per_sec2(float value, const ServoXxd *parent) {
  return Acceleration(value, AccelerationUnit::STEPS_PER_SEC_SQ, parent);
}

Acceleration Acceleration::from_rpm_per_sec(float value, const ServoXxd *parent) {
  return Acceleration(value, AccelerationUnit::RPM_PER_SEC, parent);
}

Acceleration Acceleration::from_rev_per_sec2(float value, const ServoXxd *parent) {
  return Acceleration(value, AccelerationUnit::REV_PER_SEC_SQ, parent);
}

Acceleration Acceleration::from_degrees_per_sec2(float value, const ServoXxd *parent) {
  return Acceleration(value, AccelerationUnit::DEGREES_PER_SEC_SQ, parent);
}

Acceleration Acceleration::from_radians_per_sec2(float value, const ServoXxd *parent) {
  return Acceleration(value, AccelerationUnit::RADIANS_PER_SEC_SQ, parent);
}

// Generic getter with unit parameter
float Acceleration::get(AccelerationUnit unit) const {
  // Special case: instant acceleration
  if (acc_ == 0) {
    return -1.0f;  // Sentinel for instant
  }

  // Calculate effective RPM/s from hardware value
  // a_eff = 20000 / (256 - acc)
  float rpm_per_s = MAX_RPM_PER_SEC / static_cast<float>(256 - acc_);

  // Convert to requested unit
  switch (unit) {
    case AccelerationUnit::RPM_PER_SEC:
      return rpm_per_s;

    case AccelerationUnit::REV_PER_SEC_SQ:
      return rpm_per_s / 60.0f;

    case AccelerationUnit::DEGREES_PER_SEC_SQ:
      return rpm_per_s * 6.0f;  // rpm/s * 360/60 = rpm/s * 6

    case AccelerationUnit::RADIANS_PER_SEC_SQ:
      return rpm_per_s * (2.0f * M_PI / 60.0f);

    case AccelerationUnit::STEPS_PER_SEC_SQ:
      if (parent_ == nullptr) {
        ESP_LOGE(TAG, "get(STEPS_PER_SEC_SQ): parent is null");
        return -1.0f;
      } else {
        float steps_per_rev = parent_->get_steps_per_revolution();
        if (steps_per_rev <= 0) {
          ESP_LOGE(TAG, "get(STEPS_PER_SEC_SQ): invalid steps_per_rev=%.1f", steps_per_rev);
          return -1.0f;
        }
        // Convert RPM/s to steps/s²
        return (rpm_per_s / 60.0f) * steps_per_rev;
      }

    default:
      ESP_LOGE(TAG, "Unknown acceleration unit in get(): %d", static_cast<int>(unit));
      return -1.0f;
  }
}

// Generic setter with unit parameter
void Acceleration::set(float value, AccelerationUnit unit) { *this = Acceleration(value, unit, parent_); }

void Acceleration::set(double value, AccelerationUnit unit) {
  *this = Acceleration(static_cast<float>(value), unit, parent_);
}

void Acceleration::set(int64_t value, AccelerationUnit unit) {
  *this = Acceleration(static_cast<float>(value), unit, parent_);
}

void Acceleration::set(int32_t value, AccelerationUnit unit) {
  *this = Acceleration(static_cast<float>(value), unit, parent_);
}

}  // namespace servoxxd
}  // namespace esphome
