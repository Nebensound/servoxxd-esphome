#include "servoxxd_speed.h"
#include "servoxxd.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace servoxxd {

static const char *const TAG = "servoxxd.speed";

// Primary constructor: float value
Speed::Speed(float value, SpeedUnit unit, const ServoXxd *parent) : parent_(parent) {
  float rpm_float = 0.0f;  // Temporary float for conversion

  switch (unit) {
    case SpeedUnit::STEPS_PER_SEC:
      // Need parent for steps_per_revolution
      if (parent_ == nullptr) {
        ESP_LOGE(TAG, "Speed: parent required for STEPS_PER_SEC conversion");
        rpm_ = 0;
        return;
      } else {
        float steps_per_rev = parent_->get_steps_per_revolution();
        if (steps_per_rev <= 0) {
          ESP_LOGE(TAG, "Speed: Invalid steps_per_revolution: %.2f", steps_per_rev);
          rpm_ = 0;
          return;
        }
        rpm_float = (value / steps_per_rev) * 60.0f;
      }
      break;

    case SpeedUnit::RPM:
      rpm_float = value;
      break;

    case SpeedUnit::REV_PER_SEC:
      rpm_float = value * 60.0f;
      break;

    case SpeedUnit::DEGREES_PER_SEC:
      rpm_float = (value / 360.0f) * 60.0f;
      break;

    case SpeedUnit::RADIANS_PER_SEC:
      rpm_float = (value / (2.0f * M_PI)) * 60.0f;
      break;

    case SpeedUnit::DEGREES_PER_MIN:
      rpm_float = value / 6.0f;  // 360° / 60min = 6
      break;

    case SpeedUnit::DEGREES_PER_HOUR:
      rpm_float = value / 360.0f;  // 360° = 1 rev, 60min = 1h → /360
      break;

    default:
      ESP_LOGE(TAG, "Unknown speed unit: %d", static_cast<int>(unit));
      rpm_ = 0;
      return;
  }

  // Clamp to hardware limits (-3000 to +3000 RPM) BEFORE casting to int16_t
  // to avoid undefined behavior on overflow
  if (rpm_float > 3000.0f) {
    ESP_LOGW(TAG, "Speed %.0f RPM exceeds max (3000 RPM), clamping", rpm_float);
    rpm_float = 3000.0f;
  } else if (rpm_float < -3000.0f) {
    ESP_LOGW(TAG, "Speed %.0f RPM below min (-3000 RPM), clamping", rpm_float);
    rpm_float = -3000.0f;
  }

  // Now safe to cast to int16_t
  rpm_ = static_cast<int16_t>(std::round(rpm_float));
}

// Constructor overload: double → float
Speed::Speed(double value, SpeedUnit unit, const ServoXxd *parent) : Speed(static_cast<float>(value), unit, parent) {}

// Constructor overload: int64_t → float
Speed::Speed(int64_t value, SpeedUnit unit, const ServoXxd *parent) : Speed(static_cast<float>(value), unit, parent) {}

// Constructor overload: int32_t → float
Speed::Speed(int32_t value, SpeedUnit unit, const ServoXxd *parent) : Speed(static_cast<float>(value), unit, parent) {}

// Factory methods
Speed Speed::from_steps_per_sec(float value, const ServoXxd *parent) {
  return Speed(value, SpeedUnit::STEPS_PER_SEC, parent);
}

Speed Speed::from_rpm(float value, const ServoXxd *parent) { return Speed(value, SpeedUnit::RPM, parent); }

Speed Speed::from_rev_per_sec(float value, const ServoXxd *parent) {
  return Speed(value, SpeedUnit::REV_PER_SEC, parent);
}

Speed Speed::from_degrees_per_sec(float value, const ServoXxd *parent) {
  return Speed(value, SpeedUnit::DEGREES_PER_SEC, parent);
}

Speed Speed::from_radians_per_sec(float value, const ServoXxd *parent) {
  return Speed(value, SpeedUnit::RADIANS_PER_SEC, parent);
}

Speed Speed::from_degrees_per_min(float value, const ServoXxd *parent) {
  return Speed(value, SpeedUnit::DEGREES_PER_MIN, parent);
}

Speed Speed::from_degrees_per_hour(float value, const ServoXxd *parent) {
  return Speed(value, SpeedUnit::DEGREES_PER_HOUR, parent);
}

// Generic getter with unit parameter
float Speed::get(SpeedUnit unit) const {
  float rpm_float = static_cast<float>(rpm_);

  switch (unit) {
    case SpeedUnit::RPM:
      return rpm_float;

    case SpeedUnit::REV_PER_SEC:
      return rpm_float / 60.0f;

    case SpeedUnit::DEGREES_PER_SEC:
      return rpm_float * 6.0f;  // rpm * 360/60 = rpm * 6

    case SpeedUnit::RADIANS_PER_SEC:
      return rpm_float * (2.0f * M_PI / 60.0f);

    case SpeedUnit::DEGREES_PER_MIN:
      return rpm_float * 6.0f;  // rpm * 360/60 = rpm * 6

    case SpeedUnit::DEGREES_PER_HOUR:
      return rpm_float * 360.0f;  // rpm * 360 deg/hour

    case SpeedUnit::STEPS_PER_SEC:
      if (parent_ == nullptr) {
        ESP_LOGE(TAG, "get(STEPS_PER_SEC): parent is null");
        return 0.0f;
      } else {
        float steps_per_rev = parent_->get_steps_per_revolution();
        if (steps_per_rev <= 0) {
          ESP_LOGE(TAG, "get(STEPS_PER_SEC): invalid steps_per_rev=%.1f", steps_per_rev);
          return 0.0f;
        }
        return (rpm_float / 60.0f) * steps_per_rev;
      }

    default:
      ESP_LOGE(TAG, "Unknown speed unit in get(): %d", static_cast<int>(unit));
      return 0.0f;
  }
}

// Generic setter with unit parameter
void Speed::set(float value, SpeedUnit unit) { *this = Speed(value, unit, parent_); }

void Speed::set(double value, SpeedUnit unit) { *this = Speed(static_cast<float>(value), unit, parent_); }

void Speed::set(int64_t value, SpeedUnit unit) { *this = Speed(static_cast<float>(value), unit, parent_); }

void Speed::set(int32_t value, SpeedUnit unit) { *this = Speed(static_cast<float>(value), unit, parent_); }

// Get speed for hardware with microstepping compensation
int16_t Speed::rpm_for_hardware() const {
  if (parent_ == nullptr) {
    ESP_LOGW(TAG, "rpm_for_hardware: parent is null, returning raw RPM");
    return rpm_;
  }

  uint16_t microsteps = parent_->get_microstepping();
  int16_t scaled_rpm = rpm_;

  // Apply hardware scaling compensation
  if (microsteps == 8) {
    scaled_rpm = rpm_ * 2;  // Compensate for 8 microsteps
  } else if (microsteps == 128) {
    scaled_rpm = rpm_ / 8;  // Compensate for 128 microsteps
  } else if (microsteps == 256) {
    scaled_rpm = rpm_ / 16;  // Compensate for 256 microsteps
  }
  // For 16, 32, 64: no scaling needed (reference values)

  // Re-clamp after scaling
  if (scaled_rpm > 3000)
    scaled_rpm = 3000;
  if (scaled_rpm < -3000)
    scaled_rpm = -3000;

  return scaled_rpm;
}

}  // namespace servoxxd
}  // namespace esphome
