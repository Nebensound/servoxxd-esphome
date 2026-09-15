#pragma once

#include "esphome/core/component.h"
#include <cstdint>
#include <cmath>

namespace esphome {
namespace servoxxd {

// Forward declaration
class ServoXxd;

/// Speed unit enumeration - maps to YAML SpeedUnit
enum class SpeedUnit : uint8_t {
  STEPS_PER_SEC = 0,    ///< Steps per second (default - ESPHome stepper compatibility)
  RPM = 1,              ///< Revolutions per minute (motor native)
  REV_PER_SEC = 2,      ///< Revolutions per second
  DEGREES_PER_SEC = 3,  ///< Degrees per second
  RADIANS_PER_SEC = 4,  ///< Radians per second
  DEGREES_PER_MIN = 5,  ///< Degrees per minute (moderate rotation)
  DEGREES_PER_HOUR = 6  ///< Degrees per hour (astronomical tracking)
};

/**
 * @brief Speed with unit conversion and hardware encoding
 *
 * Hardware format: int16_t RPM (-32768 to +32767)
 * Supports 7 units with automatic conversion
 */
class Speed {
  friend class ServoXxd;

 public:
  // Constructors - float primary, double/int overloads
  Speed(float value, SpeedUnit unit, const ServoXxd *parent);
  Speed(double value, SpeedUnit unit, const ServoXxd *parent);
  Speed(int64_t value, SpeedUnit unit, const ServoXxd *parent);
  Speed(int32_t value, SpeedUnit unit, const ServoXxd *parent);
  explicit Speed(const ServoXxd *parent) : rpm_(0), parent_(parent) {}

  // Factory methods for direct unit conversion
  static Speed from_steps_per_sec(float value, const ServoXxd *parent);
  static Speed from_rpm(float value, const ServoXxd *parent);
  static Speed from_rev_per_sec(float value, const ServoXxd *parent);
  static Speed from_degrees_per_sec(float value, const ServoXxd *parent);
  static Speed from_radians_per_sec(float value, const ServoXxd *parent);
  static Speed from_degrees_per_min(float value, const ServoXxd *parent);
  static Speed from_degrees_per_hour(float value, const ServoXxd *parent);

  // Direct accessor to internal representation
  int16_t rpm_internal() const { return rpm_; }

  // Unit conversions (getters) - optimized return types (int for linear conversions, float for fractional)
  float get(SpeedUnit unit) const;
  int32_t get_steps_per_sec() const { return static_cast<int32_t>(std::round(get(SpeedUnit::STEPS_PER_SEC))); }
  int16_t get_rpm() const { return rpm_; }
  float get_rev_per_sec() const { return get(SpeedUnit::REV_PER_SEC); }
  int32_t get_degrees_per_sec() const { return static_cast<int32_t>(std::round(get(SpeedUnit::DEGREES_PER_SEC))); }
  float get_radians_per_sec() const { return get(SpeedUnit::RADIANS_PER_SEC); }
  int32_t get_degrees_per_min() const { return static_cast<int32_t>(std::round(get(SpeedUnit::DEGREES_PER_MIN))); }
  int32_t get_degrees_per_hour() const { return static_cast<int32_t>(std::round(get(SpeedUnit::DEGREES_PER_HOUR))); }

  // Unit conversions (setters) - float/double/int overloads
  void set(float value, SpeedUnit unit);
  void set(double value, SpeedUnit unit);
  void set(int64_t value, SpeedUnit unit);
  void set(int32_t value, SpeedUnit unit);

  void set_steps_per_sec(float value) { set(value, SpeedUnit::STEPS_PER_SEC); }
  void set_steps_per_sec(double value) { set(value, SpeedUnit::STEPS_PER_SEC); }
  void set_steps_per_sec(int64_t value) { set(value, SpeedUnit::STEPS_PER_SEC); }
  void set_steps_per_sec(int32_t value) { set(value, SpeedUnit::STEPS_PER_SEC); }

  void set_rpm(float value) { set(value, SpeedUnit::RPM); }
  void set_rpm(double value) { set(value, SpeedUnit::RPM); }
  void set_rpm(int64_t value) { set(value, SpeedUnit::RPM); }
  void set_rpm(int32_t value) { set(value, SpeedUnit::RPM); }

  void set_rev_per_sec(float value) { set(value, SpeedUnit::REV_PER_SEC); }
  void set_rev_per_sec(double value) { set(value, SpeedUnit::REV_PER_SEC); }
  void set_rev_per_sec(int64_t value) { set(value, SpeedUnit::REV_PER_SEC); }
  void set_rev_per_sec(int32_t value) { set(value, SpeedUnit::REV_PER_SEC); }

  void set_degrees_per_sec(float value) { set(value, SpeedUnit::DEGREES_PER_SEC); }
  void set_degrees_per_sec(double value) { set(value, SpeedUnit::DEGREES_PER_SEC); }
  void set_degrees_per_sec(int64_t value) { set(value, SpeedUnit::DEGREES_PER_SEC); }
  void set_degrees_per_sec(int32_t value) { set(value, SpeedUnit::DEGREES_PER_SEC); }

  void set_radians_per_sec(float value) { set(value, SpeedUnit::RADIANS_PER_SEC); }
  void set_radians_per_sec(double value) { set(value, SpeedUnit::RADIANS_PER_SEC); }
  void set_radians_per_sec(int64_t value) { set(value, SpeedUnit::RADIANS_PER_SEC); }
  void set_radians_per_sec(int32_t value) { set(value, SpeedUnit::RADIANS_PER_SEC); }

  void set_degrees_per_min(float value) { set(value, SpeedUnit::DEGREES_PER_MIN); }
  void set_degrees_per_min(double value) { set(value, SpeedUnit::DEGREES_PER_MIN); }
  void set_degrees_per_min(int64_t value) { set(value, SpeedUnit::DEGREES_PER_MIN); }
  void set_degrees_per_min(int32_t value) { set(value, SpeedUnit::DEGREES_PER_MIN); }

  void set_degrees_per_hour(float value) { set(value, SpeedUnit::DEGREES_PER_HOUR); }
  void set_degrees_per_hour(double value) { set(value, SpeedUnit::DEGREES_PER_HOUR); }
  void set_degrees_per_hour(int64_t value) { set(value, SpeedUnit::DEGREES_PER_HOUR); }
  void set_degrees_per_hour(int32_t value) { set(value, SpeedUnit::DEGREES_PER_HOUR); }

  // Legacy compatibility methods
  float rpm() const { return static_cast<float>(rpm_); }
  int16_t rpm_as_i16() const { return rpm_; }
  int16_t rpm_for_hardware() const;
  float steps_per_sec() const { return get_steps_per_sec(); }

  // Operators for comparison
  bool operator==(const Speed &rhs) const { return rpm_ == rhs.rpm_; }
  bool operator!=(const Speed &rhs) const { return !(*this == rhs); }

 private:
  int16_t rpm_{0};                   ///< Internal storage: RPM (signed, -32768 to +32767)
  const ServoXxd *parent_{nullptr};  ///< Parent component (for microstepping and steps_per_revolution)
};

}  // namespace servoxxd
}  // namespace esphome
