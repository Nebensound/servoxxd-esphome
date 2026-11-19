#pragma once

#include "esphome/core/log.h"
#include <cstdint>

// Define math constants if not already available (Arduino/system compatibility)
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#ifndef TWO_PI
#define TWO_PI 6.283185307179586476925286766559
#endif

namespace esphome {
namespace servoxxd {

// Forward declaration
class ServoXxd;

/**
 * @brief Unit options for position values
 */
enum class PositionUnit : uint8_t {
  STEPS = 0,
  REVOLUTIONS = 1,
  DEGREES = 2,
  RADIANS = 3,
  ARCMINUTES = 4,
  ARCSECONDS = 5,
  TICKS = 6
};

/**
 * @brief Position with encoder split format (revolutions + angle_ticks)
 *
 * Hardware format: Position = revolutions + (angle_ticks / 16384)
 * - revolutions: int32_t, full rotations
 * - angle_ticks: uint16_t, 0-16383 (2^14 ticks per revolution)
 * - Automatic carry/borrow between fields
 */
class Position {
  friend class ServoXxd;

 public:
  // Constructors
  Position(double value, PositionUnit unit, const ServoXxd *parent);
  Position(float value, PositionUnit unit, const ServoXxd *parent);
  Position(int64_t value, PositionUnit unit, const ServoXxd *parent);
  Position(int32_t value, PositionUnit unit, const ServoXxd *parent);
  explicit Position(const ServoXxd *parent) : revs_(0), angle_ticks_(0), parent_(parent) {}

  // Copy constructor (explicit for debugging)
  Position(const Position &other);

  // Copy assignment operator (required because we have user-provided copy constructor)
  Position &operator=(const Position &other);

  // Factory methods - direct unit conversion
  static Position from_ticks(int64_t ticks, const ServoXxd *parent);
  static Position from_steps(int64_t steps, const ServoXxd *parent);
  static Position from_revolutions(double revolutions, const ServoXxd *parent);
  static Position from_degrees(double deg, const ServoXxd *parent);
  static Position from_radians(double rad, const ServoXxd *parent);
  static Position from_arcminutes(int64_t arcminutes, const ServoXxd *parent);
  static Position from_arcseconds(int64_t arcseconds, const ServoXxd *parent);
  // Direct accessors to internal representation
  int32_t revolutions() const { return revs_; }
  uint16_t angle_ticks() const { return angle_ticks_; }

  // Unit conversions (getters)
  double get(PositionUnit unit) const { return get_double_unit(unit); };
  double get_double_unit(PositionUnit unit) const;
  int64_t get_int64_unit(PositionUnit unit) const;
  int64_t get_ticks() const { return get_int64_unit(PositionUnit::TICKS); };
  int64_t get_steps() const { return get_int64_unit(PositionUnit::STEPS); };
  double get_revolutions() const { return get_double_unit(PositionUnit::REVOLUTIONS); };
  double get_degrees() const { return get_double_unit(PositionUnit::DEGREES); };
  double get_radians() const { return get_double_unit(PositionUnit::RADIANS); };
  int64_t get_arcminutes() const { return get_int64_unit(PositionUnit::ARCMINUTES); };
  int64_t get_arcseconds() const { return get_int64_unit(PositionUnit::ARCSECONDS); };

  // Unit conversions (setters) - uses internal parent_
  void set(double value, PositionUnit unit);
  void set(float value, PositionUnit unit);
  void set(int64_t value, PositionUnit unit);
  void set(int32_t value, PositionUnit unit);

  void set_ticks(int64_t ticks) { set(ticks, PositionUnit::TICKS); };
  void set_ticks(int32_t ticks) { set(ticks, PositionUnit::TICKS); };
  void set_ticks(double ticks) { set(ticks, PositionUnit::TICKS); };
  void set_ticks(float ticks) { set(ticks, PositionUnit::TICKS); };

  void set_steps(int64_t steps) { set(steps, PositionUnit::STEPS); };
  void set_steps(int32_t steps) { set(steps, PositionUnit::STEPS); };
  void set_steps(float steps) { set(steps, PositionUnit::STEPS); };
  void set_steps(double steps) { set(steps, PositionUnit::STEPS); };

  void set_revolutions(double revolutions) { set(revolutions, PositionUnit::REVOLUTIONS); };
  void set_revolutions(float revolutions) { set(revolutions, PositionUnit::REVOLUTIONS); };
  void set_revolutions(int64_t revolutions) { set(revolutions, PositionUnit::REVOLUTIONS); };
  void set_revolutions(int32_t revolutions) { set(revolutions, PositionUnit::REVOLUTIONS); };

  void set_degrees(double deg) { set(deg, PositionUnit::DEGREES); };
  void set_degrees(float deg) { set(deg, PositionUnit::DEGREES); };
  void set_degrees(int64_t deg) { set(deg, PositionUnit::DEGREES); };
  void set_degrees(int32_t deg) { set(deg, PositionUnit::DEGREES); };

  void set_radians(double rad) { set(rad, PositionUnit::RADIANS); };
  void set_radians(float rad) { set(rad, PositionUnit::RADIANS); };
  void set_radians(int64_t rad) { set(rad, PositionUnit::RADIANS); };
  void set_radians(int32_t rad) { set(rad, PositionUnit::RADIANS); };

  void set_arcminutes(int64_t arcminutes) { set(arcminutes, PositionUnit::ARCMINUTES); };
  void set_arcminutes(int32_t arcminutes) { set(arcminutes, PositionUnit::ARCMINUTES); };
  void set_arcminutes(float arcminutes) { set(arcminutes, PositionUnit::ARCMINUTES); };
  void set_arcminutes(double arcminutes) { set(arcminutes, PositionUnit::ARCMINUTES); };

  void set_arcseconds(int64_t arcseconds) { set(arcseconds, PositionUnit::ARCSECONDS); };
  void set_arcseconds(int32_t arcseconds) { set(arcseconds, PositionUnit::ARCSECONDS); };
  void set_arcseconds(float arcseconds) { set(arcseconds, PositionUnit::ARCSECONDS); };
  void set_arcseconds(double arcseconds) { set(arcseconds, PositionUnit::ARCSECONDS); };

  // Operators for position arithmetic
  Position operator+(const Position &rhs) const;
  Position operator-(const Position &rhs) const;
  Position operator*(double scalar) const;
  Position operator/(double scalar) const;
  bool operator==(const Position &rhs) const;
  bool operator!=(const Position &rhs) const { return !(*this == rhs); }
  bool operator<(const Position &rhs) const;
  bool operator>(const Position &rhs) const;
  bool operator<=(const Position &rhs) const;
  bool operator>=(const Position &rhs) const;

  // Absolute value
  Position abs() const;

 private:
  static constexpr uint16_t TICKS_PER_REV = 16384u;  // 2^14
  int32_t revs_{0};
  uint16_t angle_ticks_{0};  // 0-16383
  const ServoXxd *parent_{nullptr};

  Position() = default;  // For factory methods
  void normalize();      // Ensure angle_ticks in [0, 16383]
};

}  // namespace servoxxd
}  // namespace esphome
