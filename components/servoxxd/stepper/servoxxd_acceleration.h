#pragma once

#include "esphome/core/log.h"

namespace esphome {
namespace servoxxd {

// Forward declaration
class ServoXxd;

/**
 * @brief Unit options for acceleration values
 *
 * Defines the units that can be used to specify motor acceleration.
 * All units are converted to a hardware-native value (0-255) during construction.
 */
enum class AccelerationUnit : uint8_t {
  STEPS_PER_SEC_SQ = 0,    ///< Steps per second squared (ESPHome default)
  RPM_PER_SEC = 1,         ///< Revolutions per minute per second (motor native)
  REV_PER_SEC_SQ = 2,      ///< Revolutions per second squared
  DEGREES_PER_SEC_SQ = 3,  ///< Degrees per second squared
  RADIANS_PER_SEC_SQ = 4   ///< Radians per second squared
};

/**
 * @brief Acceleration with hardware encoding (non-linear inverse time mapping)
 *
 * Hardware format: acc = 0-255 (inverse time control)
 * - acc = 0: Instant (no ramp)
 * - acc = 1-255: Δt = (256 - acc) × 50μs per ±1 RPM change
 * - Conversion: acc = 256 - (20000 / rpm_per_sec), clamped [1, 255]
 */
class Acceleration {
  friend class ServoXxd;

 public:
  // Constructors - float primary, double/int overloads
  Acceleration(float value, AccelerationUnit unit, const ServoXxd *parent);
  Acceleration(double value, AccelerationUnit unit, const ServoXxd *parent);
  Acceleration(int64_t value, AccelerationUnit unit, const ServoXxd *parent);
  Acceleration(int32_t value, AccelerationUnit unit, const ServoXxd *parent);
  explicit Acceleration(const ServoXxd *parent) : acc_(0), parent_(parent) {}

  // Factory methods for direct unit conversion
  static Acceleration from_steps_per_sec2(float value, const ServoXxd *parent);
  static Acceleration from_rpm_per_sec(float value, const ServoXxd *parent);
  static Acceleration from_rev_per_sec2(float value, const ServoXxd *parent);
  static Acceleration from_degrees_per_sec2(float value, const ServoXxd *parent);
  static Acceleration from_radians_per_sec2(float value, const ServoXxd *parent);

  /**
   * @brief Direct hardware value factory (for testing/low-level control)
   *
   * Creates an Acceleration object with a direct hardware value (0-255).
   * - acc = 0: Instant acceleration (no ramp)
   * - acc = 1-255: Higher values = faster acceleration
   *   Time per ±1 RPM change: Δt = (256 - acc) × 50μs
   *
   * Example: acc=236 → Δt = 20×50μs = 1ms per RPM (≈1000 RPM/s)
   *
   * @param acc_value Hardware acceleration value (0-255)
   * @return Acceleration object with direct hardware value
   */
  static Acceleration from_internal(uint8_t acc_value) {
    Acceleration accel(nullptr);
    accel.acc_ = acc_value;
    return accel;
  }

  // Direct accessor to internal representation
  uint8_t acc_internal() const { return acc_; }

  // Unit conversions (getters) - all return float
  float get(AccelerationUnit unit) const;
  float get_steps_per_sec2() const { return get(AccelerationUnit::STEPS_PER_SEC_SQ); }
  float get_rpm_per_sec() const { return get(AccelerationUnit::RPM_PER_SEC); }
  float get_rev_per_sec2() const { return get(AccelerationUnit::REV_PER_SEC_SQ); }
  float get_degrees_per_sec2() const { return get(AccelerationUnit::DEGREES_PER_SEC_SQ); }
  float get_radians_per_sec2() const { return get(AccelerationUnit::RADIANS_PER_SEC_SQ); }

  // Unit conversions (setters) - float/double/int overloads
  void set(float value, AccelerationUnit unit);
  void set(double value, AccelerationUnit unit);
  void set(int64_t value, AccelerationUnit unit);
  void set(int32_t value, AccelerationUnit unit);

  void set_steps_per_sec2(float value) { set(value, AccelerationUnit::STEPS_PER_SEC_SQ); }
  void set_steps_per_sec2(double value) { set(value, AccelerationUnit::STEPS_PER_SEC_SQ); }
  void set_steps_per_sec2(int64_t value) { set(value, AccelerationUnit::STEPS_PER_SEC_SQ); }
  void set_steps_per_sec2(int32_t value) { set(value, AccelerationUnit::STEPS_PER_SEC_SQ); }

  void set_rpm_per_sec(float value) { set(value, AccelerationUnit::RPM_PER_SEC); }
  void set_rpm_per_sec(double value) { set(value, AccelerationUnit::RPM_PER_SEC); }
  void set_rpm_per_sec(int64_t value) { set(value, AccelerationUnit::RPM_PER_SEC); }
  void set_rpm_per_sec(int32_t value) { set(value, AccelerationUnit::RPM_PER_SEC); }

  void set_rev_per_sec2(float value) { set(value, AccelerationUnit::REV_PER_SEC_SQ); }
  void set_rev_per_sec2(double value) { set(value, AccelerationUnit::REV_PER_SEC_SQ); }
  void set_rev_per_sec2(int64_t value) { set(value, AccelerationUnit::REV_PER_SEC_SQ); }
  void set_rev_per_sec2(int32_t value) { set(value, AccelerationUnit::REV_PER_SEC_SQ); }

  void set_degrees_per_sec2(float value) { set(value, AccelerationUnit::DEGREES_PER_SEC_SQ); }
  void set_degrees_per_sec2(double value) { set(value, AccelerationUnit::DEGREES_PER_SEC_SQ); }
  void set_degrees_per_sec2(int64_t value) { set(value, AccelerationUnit::DEGREES_PER_SEC_SQ); }
  void set_degrees_per_sec2(int32_t value) { set(value, AccelerationUnit::DEGREES_PER_SEC_SQ); }

  void set_radians_per_sec2(float value) { set(value, AccelerationUnit::RADIANS_PER_SEC_SQ); }
  void set_radians_per_sec2(double value) { set(value, AccelerationUnit::RADIANS_PER_SEC_SQ); }
  void set_radians_per_sec2(int64_t value) { set(value, AccelerationUnit::RADIANS_PER_SEC_SQ); }
  void set_radians_per_sec2(int32_t value) { set(value, AccelerationUnit::RADIANS_PER_SEC_SQ); }

  // Operators for comparison (matching Position pattern)
  bool operator==(const Acceleration &rhs) const { return acc_ == rhs.acc_; }
  bool operator!=(const Acceleration &rhs) const { return !(*this == rhs); }

 private:
  uint8_t acc_{0};                   ///< Hardware value 0-255 (inverse time mapping)
  const ServoXxd *parent_{nullptr};  ///< Parent component (for steps_per_revolution)
};

}  // namespace servoxxd
}  // namespace esphome
