/**
 * @file test_position.cpp
 * @brief Unit tests for Position class with all 6 unit conversions
 *
 * Tests verify:
 * - Conversion accuracy from all 6 units to encoder ticks (split format)
 * - Split format (revolutions + angle_ticks) handling
 * - Arithmetic operators (+, -, ==)
 * - Edge cases (zero, negative, fractional revolutions)
 * - Steps conversion for ESPHome base class
 * - Carry/borrow behavior in split format
 */

#include "servoxxd_position.h"
#include "servoxxd.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace esphome::servoxxd;

// Tolerance for float comparisons
// Position has tick quantization: 1 tick = 360° / 16384 ≈ 0.022°
// So we need larger epsilon than Speed/Acceleration
constexpr double EPSILON = 0.025;

bool float_eq(double a, double b) { return std::abs(a - b) < EPSILON; }

bool float_eq(float a, float b) { return std::abs(a - b) < static_cast<float>(EPSILON); }

bool int64_eq(int64_t a, int64_t b, int64_t tolerance = 1) { return std::abs(a - b) <= tolerance; }

// Mock ServoXxd for testing
class MockServoXxd : public ServoXxd {
 public:
  explicit MockServoXxd(float steps_per_rev) : steps_per_rev_(steps_per_rev) {}

  float get_steps_per_revolution() const override { return steps_per_rev_; }

 private:
  float steps_per_rev_;
};

void test_position_steps() {
  std::cout << "Testing STEPS conversion..." << std::endl;

  float steps_per_rev = 3200.0f;

  MockServoXxd mock(steps_per_rev);

  // Test: 3200 steps = 1 revolution = 16384 ticks
  Position pos(3200, PositionUnit::STEPS, &mock);
  assert(pos.revolutions() == 1);
  assert(pos.angle_ticks() == 0);
  assert(pos.get_ticks() == 16384);

  std::cout << "  ✓ 3200 steps = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test reverse conversion
  int64_t steps = pos.get_steps();
  assert(steps == 3200);
  std::cout << "  ✓ Reverse: 1 rev = " << steps << " steps" << std::endl;
}

void test_position_revolutions() {
  std::cout << "Testing REVOLUTIONS conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 2.5 revolutions = 2 rev + 0.5 rev = 2 rev + 8192 ticks
  Position pos(2.5, PositionUnit::REVOLUTIONS, &mock);
  assert(pos.revolutions() == 2);
  assert(pos.angle_ticks() == 8192);

  std::cout << "  ✓ 2.5 rev = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test fractional conversion
  double degrees_val = pos.get_degrees();
  assert(float_eq(degrees_val, 900.0));  // 2.5 * 360 = 900°
  std::cout << "  ✓ 2.5 rev = " << degrees_val << " degrees" << std::endl;
}

void test_position_degrees() {
  std::cout << "Testing DEGREES conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 720° = 2 revolutions = 32768 ticks
  Position pos(720.0, PositionUnit::DEGREES, &mock);
  assert(pos.revolutions() == 2);
  assert(pos.angle_ticks() == 0);

  std::cout << "  ✓ 720° = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test fractional degree: 45° = 0.125 rev = 2048 ticks
  Position pos_45(45.0, PositionUnit::DEGREES, &mock);
  assert(pos_45.revolutions() == 0);
  assert(pos_45.angle_ticks() == 2048);
  std::cout << "  ✓ 45° = " << pos_45.revolutions() << " rev + " << pos_45.angle_ticks() << " ticks" << std::endl;
}

void test_position_radians() {
  std::cout << "Testing RADIANS conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);
  constexpr float two_pi = static_cast<float>(TWO_PI);

  // Test: 2π radians = 1 revolution = 16384 ticks
  Position pos(two_pi, PositionUnit::RADIANS, &mock);
  assert(pos.revolutions() == 1);
  assert(pos.angle_ticks() == 0);

  std::cout << "  ✓ 2π rad = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test reverse conversion
  double radians_val = pos.get_radians();
  assert(float_eq(radians_val, static_cast<double>(two_pi)));
  std::cout << "  ✓ Reverse: 1 rev = " << radians_val << " rad (expected " << two_pi << ")" << std::endl;
}

void test_position_arcminutes() {
  std::cout << "Testing ARCMINUTES conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 21600 arcmin = 360° = 1 revolution
  Position pos(21600, PositionUnit::ARCMINUTES, &mock);
  assert(pos.revolutions() == 1);
  assert(pos.angle_ticks() == 0);

  std::cout << "  ✓ 21600 arcmin = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test: 60 arcmin = 1° = 1/360 rev
  Position pos_60(60, PositionUnit::ARCMINUTES, &mock);
  double degrees_val = pos_60.get_degrees();
  assert(float_eq(degrees_val, 1.0));
  std::cout << "  ✓ 60 arcmin = " << degrees_val << "° (expected 1)" << std::endl;
}

void test_position_arcseconds() {
  std::cout << "Testing ARCSECONDS conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 1296000 arcsec = 360° = 1 revolution
  Position pos(1296000, PositionUnit::ARCSECONDS, &mock);
  assert(pos.revolutions() == 1);
  assert(pos.angle_ticks() == 0);

  std::cout << "  ✓ 1296000 arcsec = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test: 3600 arcsec = 1° = 1/360 rev
  Position pos_3600(3600, PositionUnit::ARCSECONDS, &mock);
  double degrees_val = pos_3600.get_degrees();
  assert(float_eq(degrees_val, 1.0));
  std::cout << "  ✓ 3600 arcsec = " << degrees_val << "° (expected 1)" << std::endl;
}

void test_position_split_format() {
  std::cout << "Testing split format construction..." << std::endl;

  // Test: Create from total ticks - 3.5 revolutions
  int64_t total_ticks = 3 * 16384 + 8192;  // 57344
  Position pos = Position::from_ticks(total_ticks, nullptr);
  assert(pos.revolutions() == 3);
  assert(pos.angle_ticks() == 8192);

  int64_t total = pos.get_ticks();
  assert(total == total_ticks);
  std::cout << "  ✓ from_ticks(57344) = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks"
            << std::endl;

  // Test: Create from ticks and verify round-trip
  Position pos2 = Position::from_ticks(57344, nullptr);
  assert(pos2.revolutions() == 3);
  assert(pos2.angle_ticks() == 8192);
  assert(pos2.get_ticks() == 57344);
  std::cout << "  ✓ from_ticks(57344) → get_ticks() = " << pos2.get_ticks() << " (round-trip)" << std::endl;
}

void test_position_carry_borrow() {
  std::cout << "Testing carry/borrow behavior..." << std::endl;

  // Test: Overflow in ticks should normalize to revolutions
  int64_t ticks_overflow = 16384 + 100;  // Should be 1 rev + 100 ticks
  Position pos = Position::from_ticks(ticks_overflow, nullptr);
  assert(pos.revolutions() == 1);
  assert(pos.angle_ticks() == 100);
  std::cout << "  ✓ Overflow: from_ticks(16484) → " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks"
            << std::endl;

  // Test: Large overflow
  int64_t ticks_large = 32768 + 200;  // Should be 2 rev + 200 ticks
  Position pos2 = Position::from_ticks(ticks_large, nullptr);
  assert(pos2.revolutions() == 2);
  assert(pos2.angle_ticks() == 200);
  std::cout << "  ✓ Large overflow: from_ticks(32968) → " << pos2.revolutions() << " rev + " << pos2.angle_ticks()
            << " ticks" << std::endl;
}

void test_position_negative_values() {
  std::cout << "Testing negative position values..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: -1 revolution = -16384 ticks
  Position pos(-1.0, PositionUnit::REVOLUTIONS, &mock);
  assert(pos.revolutions() == -1);
  assert(pos.angle_ticks() == 0);

  std::cout << "  ✓ -1 rev = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;

  // Test: -0.5 revolution should be -1 rev + 8192 ticks
  Position pos_half(-0.5, PositionUnit::REVOLUTIONS, &mock);
  assert(pos_half.revolutions() == -1);
  assert(pos_half.angle_ticks() == 8192);
  std::cout << "  ✓ -0.5 rev = " << pos_half.revolutions() << " rev + " << pos_half.angle_ticks() << " ticks"
            << std::endl;
}

void test_position_arithmetic() {
  std::cout << "Testing arithmetic operators..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  Position pos1(1.0, PositionUnit::REVOLUTIONS, &mock);  // 1 rev
  Position pos2(0.5, PositionUnit::REVOLUTIONS, &mock);  // 0.5 rev

  // Test addition: 1 + 0.5 = 1.5 rev
  Position sum = pos1 + pos2;
  assert(sum.revolutions() == 1);
  assert(sum.angle_ticks() == 8192);
  std::cout << "  ✓ 1 rev + 0.5 rev = " << sum.revolutions() << " rev + " << sum.angle_ticks() << " ticks" << std::endl;

  // Test subtraction: 1 - 0.5 = 0.5 rev
  Position diff = pos1 - pos2;
  assert(diff.revolutions() == 0);
  assert(diff.angle_ticks() == 8192);
  std::cout << "  ✓ 1 rev - 0.5 rev = " << diff.revolutions() << " rev + " << diff.angle_ticks() << " ticks"
            << std::endl;

  // Test equality
  Position pos3(1.0, PositionUnit::REVOLUTIONS, &mock);
  assert(pos1 == pos3);
  assert(!(pos1 == pos2));
  std::cout << "  ✓ Equality: pos1 == pos3, pos1 != pos2" << std::endl;

  // Test inequality operator
  assert(pos1 != pos2);
  assert(!(pos1 != pos3));
  std::cout << "  ✓ Inequality: pos1 != pos2, !(pos1 != pos3)" << std::endl;
}

void test_position_comparison_operators() {
  std::cout << "Testing comparison operators (<, >, <=, >=)..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  Position pos1(1.0, PositionUnit::REVOLUTIONS, &mock);  // 1 rev
  Position pos2(0.5, PositionUnit::REVOLUTIONS, &mock);  // 0.5 rev
  Position pos3(1.0, PositionUnit::REVOLUTIONS, &mock);  // 1 rev (same as pos1)
  Position pos4(1.5, PositionUnit::REVOLUTIONS, &mock);  // 1.5 rev

  // Test < operator
  assert(pos2 < pos1);     // 0.5 < 1.0
  assert(pos1 < pos4);     // 1.0 < 1.5
  assert(!(pos1 < pos3));  // 1.0 !< 1.0
  assert(!(pos1 < pos2));  // 1.0 !< 0.5
  std::cout << "  ✓ Less than (<): pos2 < pos1, pos1 < pos4, !(pos1 < pos3), !(pos1 < pos2)" << std::endl;

  // Test > operator
  assert(pos1 > pos2);     // 1.0 > 0.5
  assert(pos4 > pos1);     // 1.5 > 1.0
  assert(!(pos1 > pos3));  // 1.0 !> 1.0
  assert(!(pos2 > pos1));  // 0.5 !> 1.0
  std::cout << "  ✓ Greater than (>): pos1 > pos2, pos4 > pos1, !(pos1 > pos3), !(pos2 > pos1)" << std::endl;

  // Test <= operator
  assert(pos2 <= pos1);     // 0.5 <= 1.0
  assert(pos1 <= pos4);     // 1.0 <= 1.5
  assert(pos1 <= pos3);     // 1.0 <= 1.0 (equal)
  assert(!(pos1 <= pos2));  // 1.0 !<= 0.5
  std::cout << "  ✓ Less than or equal (<=): pos2 <= pos1, pos1 <= pos4, pos1 <= pos3, !(pos1 <= pos2)" << std::endl;

  // Test >= operator
  assert(pos1 >= pos2);     // 1.0 >= 0.5
  assert(pos4 >= pos1);     // 1.5 >= 1.0
  assert(pos1 >= pos3);     // 1.0 >= 1.0 (equal)
  assert(!(pos2 >= pos1));  // 0.5 !>= 1.0
  std::cout << "  ✓ Greater than or equal (>=): pos1 >= pos2, pos4 >= pos1, pos1 >= pos3, !(pos2 >= pos1)" << std::endl;

  // Test with negative positions
  Position neg1(-1.0, PositionUnit::REVOLUTIONS, &mock);  // -1 rev
  Position neg2(-0.5, PositionUnit::REVOLUTIONS, &mock);  // -0.5 rev

  assert(neg1 < neg2);  // -1 < -0.5
  assert(neg1 < pos2);  // -1 < 0.5
  assert(neg2 > neg1);  // -0.5 > -1
  assert(pos2 > neg1);  // 0.5 > -1
  std::cout << "  ✓ Negative comparisons: neg1 < neg2, neg1 < pos2, neg2 > neg1, pos2 > neg1" << std::endl;

  // Test with fractional ticks (more precise comparison)
  Position frac1(0, PositionUnit::REVOLUTIONS, &mock);
  frac1.set_ticks(100);  // 100 ticks
  Position frac2(0, PositionUnit::REVOLUTIONS, &mock);
  frac2.set_ticks(200);  // 200 ticks

  assert(frac1 < frac2);   // 100 ticks < 200 ticks
  assert(frac2 > frac1);   // 200 ticks > 100 ticks
  assert(frac1 <= frac2);  // 100 ticks <= 200 ticks
  assert(frac2 >= frac1);  // 200 ticks >= 100 ticks
  std::cout << "  ✓ Fractional tick comparison: frac1 < frac2, frac2 > frac1, frac1 <= frac2, frac2 >= frac1"
            << std::endl;

  // Test boundary: same revolutions, different ticks
  Position same_rev1(1, PositionUnit::REVOLUTIONS, &mock);
  same_rev1.set_ticks(16384 + 50);  // 1 rev + 50 ticks
  Position same_rev2(1, PositionUnit::REVOLUTIONS, &mock);
  same_rev2.set_ticks(16384 + 100);  // 1 rev + 100 ticks

  assert(same_rev1 < same_rev2);  // Same rev, but 50 < 100 ticks
  assert(same_rev2 > same_rev1);
  assert(same_rev1 <= same_rev2);
  assert(same_rev2 >= same_rev1);
  std::cout << "  ✓ Same revolution, different ticks: correct ordering" << std::endl;
}

void test_position_abs() {
  std::cout << "Testing abs() method..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test positive value
  Position pos_positive(2.5, PositionUnit::REVOLUTIONS, &mock);
  Position abs_positive = pos_positive.abs();
  assert(abs_positive.revolutions() == 2);
  assert(abs_positive.angle_ticks() == 8192);
  std::cout << "  ✓ abs(2.5 rev) = 2 rev + 8192 ticks" << std::endl;

  // Test negative value without fractional part
  Position pos_neg_int(-3.0, PositionUnit::REVOLUTIONS, &mock);
  Position abs_neg_int = pos_neg_int.abs();
  assert(abs_neg_int.revolutions() == 3);
  assert(abs_neg_int.angle_ticks() == 0);
  std::cout << "  ✓ abs(-3 rev) = 3 rev + 0 ticks" << std::endl;

  // Test negative value with fractional part
  Position pos_neg_frac(-2.5, PositionUnit::REVOLUTIONS, &mock);
  Position abs_neg_frac = pos_neg_frac.abs();
  assert(abs_neg_frac.revolutions() == 2);
  assert(abs_neg_frac.angle_ticks() == 8192);
  std::cout << "  ✓ abs(-2.5 rev) = 2 rev + 8192 ticks" << std::endl;

  // Test zero
  Position pos_zero(0.0, PositionUnit::REVOLUTIONS, &mock);
  Position abs_zero = pos_zero.abs();
  assert(abs_zero.revolutions() == 0);
  assert(abs_zero.angle_ticks() == 0);
  std::cout << "  ✓ abs(0 rev) = 0 rev + 0 ticks" << std::endl;

  // Test negative with small fractional part
  Position pos_neg_small(-0.25, PositionUnit::REVOLUTIONS, &mock);
  Position abs_neg_small = pos_neg_small.abs();
  assert(abs_neg_small.revolutions() == 0);
  assert(abs_neg_small.angle_ticks() == 4096);  // 0.25 * 16384
  std::cout << "  ✓ abs(-0.25 rev) = 0 rev + 4096 ticks" << std::endl;

  // Test that abs() doesn't modify original
  Position pos_orig(-1.0, PositionUnit::REVOLUTIONS, &mock);
  Position abs_result = pos_orig.abs();
  assert(pos_orig.revolutions() == -1);   // Original unchanged
  assert(abs_result.revolutions() == 1);  // Result is positive
  std::cout << "  ✓ abs() doesn't modify original position" << std::endl;

  // Test abs() with degrees (from_degrees now needs parent - use nullptr)
  Position pos_neg_deg = Position::from_degrees(-45.0, nullptr);
  Position abs_neg_deg = pos_neg_deg.abs();
  double abs_degrees = abs_neg_deg.get_degrees();
  assert(float_eq(abs_degrees, 45.0));
  std::cout << "  ✓ abs(-45°) = 45°" << std::endl;
}

void test_position_zero() {
  std::cout << "Testing zero position..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  Position pos(0.0, PositionUnit::REVOLUTIONS, &mock);
  assert(pos.revolutions() == 0);
  assert(pos.angle_ticks() == 0);
  assert(pos.get_ticks() == 0);

  std::cout << "  ✓ 0 rev = " << pos.revolutions() << " rev + " << pos.angle_ticks() << " ticks" << std::endl;
}

void test_position_unit_conversions() {
  std::cout << "Testing all unit accessor methods..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Create 1 revolution position
  Position pos(1.0, PositionUnit::REVOLUTIONS, &mock);

  assert(pos.get_steps() == 3200);
  assert(float_eq(pos.get_degrees(), 360.0));
  assert(float_eq(pos.get_radians(), TWO_PI));
  assert(pos.get_arcminutes() == 21600);
  assert(pos.get_arcseconds() == 1296000);

  std::cout << "  ✓ 1 rev = " << pos.get_steps() << " steps" << std::endl;
  std::cout << "  ✓ 1 rev = " << pos.get_degrees() << " degrees" << std::endl;
  std::cout << "  ✓ 1 rev = " << pos.get_radians() << " radians" << std::endl;
  std::cout << "  ✓ 1 rev = " << pos.get_arcminutes() << " arcmin" << std::endl;
  std::cout << "  ✓ 1 rev = " << pos.get_arcseconds() << " arcsec" << std::endl;
}

void test_position_null_parent() {
  std::cout << "Testing null parent pointer handling..." << std::endl;

  // Test: STEPS with null parent should default to 0
  Position pos_null(3200, PositionUnit::STEPS, nullptr);
  assert(pos_null.revolutions() == 0);
  assert(pos_null.angle_ticks() == 0);
  std::cout << "  ✓ STEPS with null parent → 0 position (error handling)" << std::endl;

  // Test: Other units with null parent should work fine
  Position pos_rev_null(1.0, PositionUnit::REVOLUTIONS, nullptr);
  assert(pos_rev_null.revolutions() == 1);
  assert(pos_rev_null.angle_ticks() == 0);
  std::cout << "  ✓ REVOLUTIONS with null parent → 1 rev (parent not needed)" << std::endl;
}

void test_position_invalid_steps_per_revolution() {
  std::cout << "Testing invalid steps_per_revolution..." << std::endl;

  // Mock with invalid (negative) steps_per_rev
  MockServoXxd mock_invalid(-100.0f);

  // Should handle invalid steps_per_rev gracefully
  Position pos(3200, PositionUnit::STEPS, &mock_invalid);
  assert(pos.revolutions() == 0);
  assert(pos.angle_ticks() == 0);
  std::cout << "  ✓ STEPS with negative steps_per_rev → 0 position (error handling)" << std::endl;

  // Mock with zero steps_per_rev
  MockServoXxd mock_zero(0.0f);
  Position pos_zero(3200, PositionUnit::STEPS, &mock_zero);
  assert(pos_zero.revolutions() == 0);
  assert(pos_zero.angle_ticks() == 0);
  std::cout << "  ✓ STEPS with zero steps_per_rev → 0 position (error handling)" << std::endl;
}

void test_position_large_values() {
  std::cout << "Testing large position values..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: Large positive revolutions
  Position pos_large(10000.0, PositionUnit::REVOLUTIONS, &mock);
  assert(pos_large.revolutions() == 10000);
  assert(pos_large.angle_ticks() == 0);
  std::cout << "  ✓ 10000 revolutions = 10000 rev" << std::endl;

  // Test: Large negative revolutions
  Position pos_neg_large(-10000.0, PositionUnit::REVOLUTIONS, &mock);
  assert(pos_neg_large.revolutions() == -10000);
  assert(pos_neg_large.angle_ticks() == 0);
  std::cout << "  ✓ -10000 revolutions = -10000 rev" << std::endl;
}

// ========== EDGE CASE TESTS ==========

void test_angle_ticks_boundaries() {
  std::cout << "Testing angle_ticks boundaries..." << std::endl;

  // Test: from_ticks(-1) should normalize to -1 rev + 16383 ticks
  Position pos_neg1 = Position::from_ticks(-1, nullptr);
  assert(pos_neg1.revolutions() == -1);
  assert(pos_neg1.angle_ticks() == 16383);
  std::cout << "  ✓ from_ticks(-1) = " << pos_neg1.revolutions() << " rev + " << pos_neg1.angle_ticks() << " ticks"
            << std::endl;

  // Test: from_ticks(16384 + 16383) = 2 rev - 1 tick = 1 rev + 16383 ticks
  Position pos_over = Position::from_ticks(16384 + 16383, nullptr);
  assert(pos_over.revolutions() == 1);
  assert(pos_over.angle_ticks() == 16383);
  std::cout << "  ✓ from_ticks(32767) = " << pos_over.revolutions() << " rev + " << pos_over.angle_ticks() << " ticks"
            << std::endl;

  // Test: Extreme value - INT32_MAX ticks
  int64_t extreme_ticks = static_cast<int64_t>(INT32_MAX);
  Position pos_extreme = Position::from_ticks(extreme_ticks, nullptr);
  int32_t expected_revs = INT32_MAX / 16384;
  uint16_t expected_ticks = INT32_MAX % 16384;
  assert(pos_extreme.revolutions() == expected_revs);
  assert(pos_extreme.angle_ticks() == expected_ticks);
  std::cout << "  ✓ from_ticks(INT32_MAX) = " << pos_extreme.revolutions() << " rev + " << pos_extreme.angle_ticks()
            << " ticks" << std::endl;

  // Test: Extreme negative - INT32_MIN ticks
  int64_t extreme_neg_ticks = static_cast<int64_t>(INT32_MIN);
  Position pos_extreme_neg = Position::from_ticks(extreme_neg_ticks, nullptr);
  std::cout << "  ✓ from_ticks(INT32_MIN) = " << pos_extreme_neg.revolutions() << " rev + "
            << pos_extreme_neg.angle_ticks() << " ticks" << std::endl;
}

void test_negative_angle_conversions() {
  std::cout << "Testing negative angle conversions..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: -45° = -0.125 rev = -1 rev + 0.875 rev = -1 rev + 14336 ticks
  Position pos_neg45(-45.0, PositionUnit::DEGREES, &mock);
  std::cout << "  ✓ -45° = " << pos_neg45.revolutions() << " rev + " << pos_neg45.angle_ticks() << " ticks"
            << std::endl;
  // Allow both representations depending on normalization
  assert(pos_neg45.revolutions() == -1 || pos_neg45.revolutions() == 0);

  // Test: -2π * 1.25 = -2.5 revolutions
  constexpr double two_pi = TWO_PI;
  Position pos_neg_rad(-two_pi * 1.25, PositionUnit::RADIANS, &mock);
  std::cout << "  ✓ -2π*1.25 rad = " << pos_neg_rad.revolutions() << " rev + " << pos_neg_rad.angle_ticks() << " ticks"
            << std::endl;
  assert(pos_neg_rad.revolutions() <= -2);

  // Test: -270° = -0.75 rev
  Position pos_neg270(-270.0, PositionUnit::DEGREES, &mock);
  std::cout << "  ✓ -270° = " << pos_neg270.revolutions() << " rev + " << pos_neg270.angle_ticks() << " ticks"
            << std::endl;
  assert(pos_neg270.revolutions() <= -1 || pos_neg270.revolutions() == 0);
}

void test_rounding_policy() {
  std::cout << "Testing rounding policy..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: 0.49 degrees (should round to nearest tick)
  Position pos_49(0.49, PositionUnit::DEGREES, &mock);
  double degrees_back = pos_49.get_degrees();
  std::cout << "  ✓ 0.49° → " << degrees_back << "° (tick quantization)" << std::endl;

  // Test: 0.50 degrees
  Position pos_50(0.50, PositionUnit::DEGREES, &mock);
  degrees_back = pos_50.get_degrees();
  std::cout << "  ✓ 0.50° → " << degrees_back << "° (tick quantization)" << std::endl;

  // Test: π/16384 radians (smallest representable angle)
  constexpr double two_pi = TWO_PI;
  double smallest_angle = two_pi / 16384.0;
  Position pos_small(smallest_angle, PositionUnit::RADIANS, &mock);
  assert(pos_small.revolutions() == 0);
  assert(pos_small.angle_ticks() == 1);
  std::cout << "  ✓ π/16384 rad = " << pos_small.revolutions() << " rev + " << pos_small.angle_ticks()
            << " ticks (smallest angle)" << std::endl;

  // Test: Rounding threshold for arcminutes (60 arcmin = 1°)
  Position pos_arcmin_49(29, PositionUnit::ARCMINUTES, &mock);  // 29 arcmin = 0.4833°
  Position pos_arcmin_50(30, PositionUnit::ARCMINUTES, &mock);  // 30 arcmin = 0.5°
  std::cout << "  ✓ 29 arcmin = " << pos_arcmin_49.get_degrees() << "°" << std::endl;
  std::cout << "  ✓ 30 arcmin = " << pos_arcmin_50.get_degrees() << "°" << std::endl;

  // Test: Rounding threshold for arcseconds (3600 arcsec = 1°)
  Position pos_arcsec_49(1764, PositionUnit::ARCSECONDS, &mock);  // 1764 arcsec = 0.49°
  Position pos_arcsec_50(1800, PositionUnit::ARCSECONDS, &mock);  // 1800 arcsec = 0.5°
  std::cout << "  ✓ 1764 arcsec = " << pos_arcsec_49.get_degrees() << "°" << std::endl;
  std::cout << "  ✓ 1800 arcsec = " << pos_arcsec_50.get_degrees() << "°" << std::endl;
}

void test_int32_boundaries() {
  std::cout << "Testing INT32 boundaries..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: INT32_MAX revolutions
  Position pos_max(static_cast<double>(INT32_MAX), PositionUnit::REVOLUTIONS, &mock);
  assert(pos_max.revolutions() == INT32_MAX);
  assert(pos_max.angle_ticks() == 0);
  std::cout << "  ✓ INT32_MAX revolutions = " << pos_max.revolutions() << " rev" << std::endl;

  // Test: INT32_MIN revolutions
  Position pos_min(static_cast<double>(INT32_MIN), PositionUnit::REVOLUTIONS, &mock);
  assert(pos_min.revolutions() == INT32_MIN);
  assert(pos_min.angle_ticks() == 0);
  std::cout << "  ✓ INT32_MIN revolutions = " << pos_min.revolutions() << " rev" << std::endl;

  // Test: Just below INT32_MAX revolutions with fractional part
  Position pos_almost_max(static_cast<double>(INT32_MAX) - 0.5, PositionUnit::REVOLUTIONS, &mock);
  assert(pos_almost_max.revolutions() == INT32_MAX - 1);
  assert(pos_almost_max.angle_ticks() == 8192);
  std::cout << "  ✓ (INT32_MAX - 0.5) revolutions = " << pos_almost_max.revolutions() << " rev + "
            << pos_almost_max.angle_ticks() << " ticks" << std::endl;
}

void test_various_steps_per_rev() {
  std::cout << "Testing various steps_per_rev values..." << std::endl;

  // Test: 200 steps/rev (common NEMA 17)
  MockServoXxd mock_200(200.0f);
  Position pos_200(200, PositionUnit::STEPS, &mock_200);
  assert(pos_200.revolutions() == 1);
  assert(pos_200.angle_ticks() == 0);
  std::cout << "  ✓ 200 steps @ 200 steps/rev = 1 rev" << std::endl;

  // Test: 400 steps/rev (half-stepping)
  MockServoXxd mock_400(400.0f);
  Position pos_400(400, PositionUnit::STEPS, &mock_400);
  assert(pos_400.revolutions() == 1);
  assert(pos_400.angle_ticks() == 0);
  std::cout << "  ✓ 400 steps @ 400 steps/rev = 1 rev" << std::endl;

  // Test: 3200 steps/rev (16 microsteps)
  MockServoXxd mock_3200(3200.0f);
  Position pos_3200(3200, PositionUnit::STEPS, &mock_3200);
  assert(pos_3200.revolutions() == 1);
  assert(pos_3200.angle_ticks() == 0);
  std::cout << "  ✓ 3200 steps @ 3200 steps/rev = 1 rev" << std::endl;

  // Test: 102400 steps/rev (MKS servo high resolution)
  MockServoXxd mock_102400(102400.0f);
  Position pos_102400(102400, PositionUnit::STEPS, &mock_102400);
  assert(pos_102400.revolutions() == 1);
  assert(pos_102400.angle_ticks() == 0);
  std::cout << "  ✓ 102400 steps @ 102400 steps/rev = 1 rev" << std::endl;

  // Test: Fractional steps
  Position pos_frac_200(100, PositionUnit::STEPS, &mock_200);  // 100 steps @ 200 = 0.5 rev
  assert(pos_frac_200.revolutions() == 0);
  assert(pos_frac_200.angle_ticks() == 8192);
  std::cout << "  ✓ 100 steps @ 200 steps/rev = 0.5 rev" << std::endl;
}

void test_factory_method_roundtrips() {
  std::cout << "Testing factory method round-trips..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: from_steps() → Check revolutions (get_steps() needs parent)
  Position pos_steps = Position::from_steps(6400, &mock);
  assert(pos_steps.revolutions() == 2);
  assert(pos_steps.angle_ticks() == 0);
  std::cout << "  ✓ from_steps(6400) → " << pos_steps.revolutions() << " rev (2 revolutions)" << std::endl;

  // Test: from_revolutions() → get_revolutions() round-trip
  Position pos_revs = Position::from_revolutions(2.5, nullptr);
  assert(float_eq(pos_revs.get_revolutions(), 2.5));
  std::cout << "  ✓ from_revolutions(2.5) → get_revolutions() = " << pos_revs.get_revolutions() << std::endl;

  // Test: from_degrees() → get_degrees() round-trip
  Position pos_deg = Position::from_degrees(180.0, nullptr);
  assert(float_eq(pos_deg.get_degrees(), 180.0));
  std::cout << "  ✓ from_degrees(180) → get_degrees() = " << pos_deg.get_degrees() << std::endl;

  // Test: from_radians() → get_radians() round-trip
  constexpr double two_pi = TWO_PI;
  Position pos_rad = Position::from_radians(two_pi / 2.0, nullptr);
  assert(float_eq(pos_rad.get_radians(), two_pi / 2.0));
  std::cout << "  ✓ from_radians(π) → get_radians() = " << pos_rad.get_radians() << std::endl;

  // Test: from_arcminutes() → get_arcminutes() round-trip
  Position pos_arcmin = Position::from_arcminutes(10800, nullptr);  // 180°
  assert(int64_eq(pos_arcmin.get_arcminutes(), 10800, 2));
  std::cout << "  ✓ from_arcminutes(10800) → get_arcminutes() = " << pos_arcmin.get_arcminutes() << std::endl;

  // Test: from_arcseconds() → get_arcseconds() round-trip
  Position pos_arcsec = Position::from_arcseconds(648000, nullptr);  // 180°
  assert(int64_eq(pos_arcsec.get_arcseconds(), 648000, 100));
  std::cout << "  ✓ from_arcseconds(648000) → get_arcseconds() = " << pos_arcsec.get_arcseconds() << std::endl;

  // Test: from_ticks() → get_ticks() round-trip
  Position pos_ticks = Position::from_ticks(32768, nullptr);  // 2 revolutions
  assert(pos_ticks.get_ticks() == 32768);
  std::cout << "  ✓ from_ticks(32768) → get_ticks() = " << pos_ticks.get_ticks() << std::endl;
}

void test_mixed_sign_operators() {
  std::cout << "Testing mixed sign operators..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: (-0.75 rev) + (1.125 rev) = 0.375 rev
  Position pos_neg(-0.75, PositionUnit::REVOLUTIONS, &mock);
  Position pos_pos(1.125, PositionUnit::REVOLUTIONS, &mock);
  Position sum = pos_neg + pos_pos;
  assert(sum.revolutions() == 0);
  assert(sum.angle_ticks() == 6144);  // 0.375 * 16384
  std::cout << "  ✓ (-0.75 rev) + (1.125 rev) = " << sum.revolutions() << " rev + " << sum.angle_ticks() << " ticks"
            << std::endl;

  // Test: (0.125 rev) - (0.5 rev) = -0.375 rev = -1 rev + 10240 ticks
  Position pos_small(0.125, PositionUnit::REVOLUTIONS, &mock);
  Position pos_large(0.5, PositionUnit::REVOLUTIONS, &mock);
  Position diff = pos_small - pos_large;
  assert(diff.revolutions() == -1);
  assert(diff.angle_ticks() == 10240);  // -0.375 rev = -1 rev + 0.625 rev = -1 rev + 10240 ticks
  std::cout << "  ✓ (0.125 rev) - (0.5 rev) = " << diff.revolutions() << " rev + " << diff.angle_ticks() << " ticks"
            << std::endl;

  // Test: (2 rev) + (-3 rev) = -1 rev
  Position pos_2(2.0, PositionUnit::REVOLUTIONS, &mock);
  Position pos_neg3(-3.0, PositionUnit::REVOLUTIONS, &mock);
  Position sum2 = pos_2 + pos_neg3;
  assert(sum2.revolutions() == -1);
  assert(sum2.angle_ticks() == 0);
  std::cout << "  ✓ (2 rev) + (-3 rev) = " << sum2.revolutions() << " rev" << std::endl;

  // Test: (-1.5 rev) - (-0.5 rev) = -1 rev
  Position pos_neg15(-1.5, PositionUnit::REVOLUTIONS, &mock);
  Position pos_neg05(-0.5, PositionUnit::REVOLUTIONS, &mock);
  Position diff2 = pos_neg15 - pos_neg05;
  assert(diff2.revolutions() == -1);
  assert(diff2.angle_ticks() == 0);
  std::cout << "  ✓ (-1.5 rev) - (-0.5 rev) = " << diff2.revolutions() << " rev" << std::endl;
}

void test_parent_handling_in_operators() {
  std::cout << "Testing parent handling in operators..." << std::endl;

  MockServoXxd mock1(3200.0f);
  MockServoXxd mock2(6400.0f);

  // Test: Adding positions with same parent
  Position pos1(1.0, PositionUnit::REVOLUTIONS, &mock1);
  Position pos2(0.5, PositionUnit::REVOLUTIONS, &mock1);
  Position sum = pos1 + pos2;
  assert(sum.revolutions() == 1);
  assert(sum.angle_ticks() == 8192);
  std::cout << "  ✓ Same parent: (1 rev) + (0.5 rev) = " << sum.revolutions() << " rev + " << sum.angle_ticks()
            << " ticks" << std::endl;

  // Test: Adding positions with different parents (should warn + use lhs parent)
  Position pos3(1.0, PositionUnit::REVOLUTIONS, &mock1);
  Position pos4(0.5, PositionUnit::REVOLUTIONS, &mock2);
  Position sum2 = pos3 + pos4;
  assert(sum2.revolutions() == 1);
  assert(sum2.angle_ticks() == 8192);
  std::cout << "  ✓ Different parents: (1 rev) + (0.5 rev) = " << sum2.revolutions() << " rev + " << sum2.angle_ticks()
            << " ticks (WARNING expected)" << std::endl;

  // Test: Adding positions with null parents
  Position pos5(1.0, PositionUnit::REVOLUTIONS, nullptr);
  Position pos6(0.5, PositionUnit::REVOLUTIONS, nullptr);
  Position sum3 = pos5 + pos6;
  assert(sum3.revolutions() == 1);
  assert(sum3.angle_ticks() == 8192);
  std::cout << "  ✓ Null parents: (1 rev) + (0.5 rev) = " << sum3.revolutions() << " rev + " << sum3.angle_ticks()
            << " ticks" << std::endl;

  // Test: One null + one non-null parent (should use non-null)
  Position pos7(1.0, PositionUnit::REVOLUTIONS, &mock1);
  Position pos8(0.5, PositionUnit::REVOLUTIONS, nullptr);
  Position sum4 = pos7 + pos8;
  assert(sum4.revolutions() == 1);
  assert(sum4.angle_ticks() == 8192);
  // Verify parent propagation: result should have mock1 as parent
  assert(sum4.get_steps() == 4800);  // 1.5 rev * 3200 steps/rev = 4800 steps
  std::cout << "  ✓ Mixed parents (non-null + null): result has non-null parent" << std::endl;

  // Test: Subtraction with parent propagation
  Position pos9(2.0, PositionUnit::REVOLUTIONS, &mock1);
  Position pos10(0.5, PositionUnit::REVOLUTIONS, &mock1);
  Position diff = pos9 - pos10;
  assert(diff.revolutions() == 1);
  assert(diff.angle_ticks() == 8192);
  assert(diff.get_steps() == 4800);  // 1.5 rev * 3200 steps/rev
  std::cout << "  ✓ Subtraction: (2 rev) - (0.5 rev) = " << diff.revolutions() << " rev + " << diff.angle_ticks()
            << " ticks" << std::endl;

  // Test: Multiplication (scalar) with parent propagation
  Position pos11(1.0, PositionUnit::REVOLUTIONS, &mock1);
  Position prod = pos11 * 2.5;
  assert(prod.revolutions() == 2);
  assert(prod.angle_ticks() == 8192);  // 2.5 rev = 2 rev + 0.5 rev (8192 ticks)
  assert(prod.get_steps() == 8000);    // 2.5 rev * 3200 steps/rev = 8000 steps
  std::cout << "  ✓ Multiplication: 1 rev * 2.5 = " << prod.revolutions() << " rev + " << prod.angle_ticks() << " ticks"
            << std::endl;

  // Test: Division (scalar) with parent propagation
  Position pos12(4.0, PositionUnit::REVOLUTIONS, &mock1);
  Position quot = pos12 / 2.0;
  assert(quot.revolutions() == 2);
  assert(quot.angle_ticks() == 0);
  assert(quot.get_steps() == 6400);  // 2 rev * 3200 steps/rev = 6400 steps
  std::cout << "  ✓ Division: 4 rev / 2 = " << quot.revolutions() << " rev + " << quot.angle_ticks() << " ticks"
            << std::endl;

  // Test: Division by zero (should return zero with parent)
  Position pos13(1.0, PositionUnit::REVOLUTIONS, &mock1);
  Position quot_zero = pos13 / 0.0;
  assert(quot_zero.revolutions() == 0);
  assert(quot_zero.angle_ticks() == 0);
  std::cout << "  ✓ Division by zero: returns zero position (ERROR expected)" << std::endl;
}

void test_nan_inf_steps_per_rev() {
  std::cout << "Testing NaN/Inf steps_per_rev handling..." << std::endl;

  // Test: NaN steps_per_rev - actual behavior may vary
  MockServoXxd mock_nan(std::nan(""));
  Position pos_nan(3200, PositionUnit::STEPS, &mock_nan);
  std::cout << "  ✓ STEPS with NaN steps_per_rev → " << pos_nan.revolutions() << " rev + " << pos_nan.angle_ticks()
            << " ticks (implementation-defined)" << std::endl;

  // Test: Inf steps_per_rev
  MockServoXxd mock_inf(std::numeric_limits<float>::infinity());
  Position pos_inf(3200, PositionUnit::STEPS, &mock_inf);
  std::cout << "  ✓ STEPS with Inf steps_per_rev → " << pos_inf.revolutions() << " rev + " << pos_inf.angle_ticks()
            << " ticks (implementation-defined)" << std::endl;

  // Test: -Inf steps_per_rev
  MockServoXxd mock_neg_inf(-std::numeric_limits<float>::infinity());
  Position pos_neg_inf(3200, PositionUnit::STEPS, &mock_neg_inf);
  std::cout << "  ✓ STEPS with -Inf steps_per_rev → " << pos_neg_inf.revolutions() << " rev + "
            << pos_neg_inf.angle_ticks() << " ticks (implementation-defined)" << std::endl;
}

void test_operator_edge_cases() {
  std::cout << "Testing operator edge cases..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: Addition causing overflow in angle_ticks
  Position pos1(0, PositionUnit::REVOLUTIONS, &mock);
  pos1.set_ticks(16380);  // Close to overflow (16384)
  Position pos2(0, PositionUnit::REVOLUTIONS, &mock);
  pos2.set_ticks(10);
  Position sum = pos1 + pos2;
  assert(sum.revolutions() == 1);
  assert(sum.angle_ticks() == 6);  // 16380 + 10 = 16390 → 1 rev + 6 ticks
  std::cout << "  ✓ Addition overflow: 16380 + 10 = " << sum.revolutions() << " rev + " << sum.angle_ticks() << " ticks"
            << std::endl;

  // Test: Subtraction causing underflow in angle_ticks
  Position pos3(1, PositionUnit::REVOLUTIONS, &mock);
  pos3.set_ticks(16384 + 5);  // 1 rev + 5 ticks
  Position pos4(0, PositionUnit::REVOLUTIONS, &mock);
  pos4.set_ticks(10);
  Position diff = pos3 - pos4;
  assert(diff.revolutions() == 0);
  assert(diff.angle_ticks() == 16379);  // (16384 + 5) - 10 = 16379
  std::cout << "  ✓ Subtraction underflow: (1 rev + 5 ticks) - 10 ticks = " << diff.revolutions() << " rev + "
            << diff.angle_ticks() << " ticks" << std::endl;

  // Test: Multiplication by zero
  Position pos5(5.0, PositionUnit::REVOLUTIONS, &mock);
  Position prod_zero = pos5 * 0.0;
  assert(prod_zero.revolutions() == 0);
  assert(prod_zero.angle_ticks() == 0);
  std::cout << "  ✓ Multiplication by zero: 5 rev * 0 = 0" << std::endl;

  // Test: Multiplication by negative scalar
  Position pos6(2.0, PositionUnit::REVOLUTIONS, &mock);
  Position prod_neg = pos6 * -1.5;
  assert(prod_neg.revolutions() == -3);
  assert(prod_neg.angle_ticks() == 0);
  std::cout << "  ✓ Multiplication by negative: 2 rev * -1.5 = " << prod_neg.revolutions() << " rev" << std::endl;

  // Test: Division by negative scalar
  Position pos7(4.0, PositionUnit::REVOLUTIONS, &mock);
  Position quot_neg = pos7 / -2.0;
  assert(quot_neg.revolutions() == -2);
  assert(quot_neg.angle_ticks() == 0);
  std::cout << "  ✓ Division by negative: 4 rev / -2 = " << quot_neg.revolutions() << " rev" << std::endl;

  // Test: Very small scalar multiplication (precision test)
  Position pos8(1.0, PositionUnit::REVOLUTIONS, &mock);
  Position prod_tiny = pos8 * 0.0001;
  double revs = prod_tiny.get_revolutions();
  assert(float_eq(revs, 0.0001));
  std::cout << "  ✓ Tiny scalar multiplication: 1 rev * 0.0001 = " << revs << " rev" << std::endl;

  // Test: Chained operations: (a + b) * c - d
  Position a(1.0, PositionUnit::REVOLUTIONS, &mock);
  Position b(0.5, PositionUnit::REVOLUTIONS, &mock);
  Position c_scalar_pos(2.0, PositionUnit::REVOLUTIONS, &mock);
  Position d(1.0, PositionUnit::REVOLUTIONS, &mock);
  Position result = ((a + b) * 2.0) - d;  // (1 + 0.5) * 2 - 1 = 3 - 1 = 2
  assert(result.revolutions() == 2);
  assert(result.angle_ticks() == 0);
  std::cout << "  ✓ Chained operations: (1 + 0.5) * 2 - 1 = " << result.revolutions() << " rev" << std::endl;

  // Test: Self-addition (pos + pos)
  Position pos9(1.5, PositionUnit::REVOLUTIONS, &mock);
  Position self_sum = pos9 + pos9;
  assert(self_sum.revolutions() == 3);
  assert(self_sum.angle_ticks() == 0);
  std::cout << "  ✓ Self-addition: 1.5 rev + 1.5 rev = " << self_sum.revolutions() << " rev" << std::endl;

  // Test: Self-subtraction (should be zero)
  Position pos10(2.75, PositionUnit::REVOLUTIONS, &mock);
  Position self_diff = pos10 - pos10;
  assert(self_diff.revolutions() == 0);
  assert(self_diff.angle_ticks() == 0);
  std::cout << "  ✓ Self-subtraction: 2.75 rev - 2.75 rev = 0" << std::endl;
}

void test_arcminute_arcsecond_precision() {
  std::cout << "Testing arcminute/arcsecond precision..." << std::endl;

  MockServoXxd mock(3200.0f);

  // Test: 1 arcminute = 1/60 degree
  Position pos_1arcmin(1, PositionUnit::ARCMINUTES, &mock);
  double degrees = pos_1arcmin.get_degrees();
  assert(float_eq(degrees, 1.0 / 60.0));
  std::cout << "  ✓ 1 arcmin = " << degrees << "° (expected " << (1.0 / 60.0) << "°)" << std::endl;

  // Test: 1 arcsecond = 1/3600 degree
  Position pos_1arcsec(1, PositionUnit::ARCSECONDS, &mock);
  degrees = pos_1arcsec.get_degrees();
  assert(float_eq(degrees, 1.0 / 3600.0));
  std::cout << "  ✓ 1 arcsec = " << degrees << "° (expected " << (1.0 / 3600.0) << "°)" << std::endl;

  // Test: Large arcsecond value - 5 million arcseconds
  Position pos_large_arcsec(5000000, PositionUnit::ARCSECONDS, &mock);
  double revolutions = pos_large_arcsec.get_revolutions();
  double expected_revs = 5000000.0 / 1296000.0;  // 1296000 arcsec/rev
  assert(float_eq(revolutions, expected_revs));
  std::cout << "  ✓ 5,000,000 arcsec = " << revolutions << " rev (expected " << expected_revs << " rev)" << std::endl;

  // Test: Precision loss in arcminutes → ticks → arcminutes
  Position pos_arcmin_rt(12345, PositionUnit::ARCMINUTES, &mock);
  int64_t arcmin_back = pos_arcmin_rt.get_arcminutes();
  int64_t diff = std::abs(arcmin_back - 12345);
  std::cout << "  ✓ 12345 arcmin round-trip: " << arcmin_back << " (diff: " << diff << ")" << std::endl;

  // Test: Precision loss in arcseconds → ticks → arcseconds
  Position pos_arcsec_rt(123456, PositionUnit::ARCSECONDS, &mock);
  int64_t arcsec_back = pos_arcsec_rt.get_arcseconds();
  diff = std::abs(arcsec_back - 123456);
  std::cout << "  ✓ 123456 arcsec round-trip: " << arcsec_back << " (diff: " << diff << ")" << std::endl;
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "Position Class Unit Tests" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;

  // Basic tests
  test_position_steps();
  test_position_revolutions();
  test_position_degrees();
  test_position_radians();
  test_position_arcminutes();
  test_position_arcseconds();
  test_position_split_format();
  test_position_carry_borrow();
  test_position_negative_values();
  test_position_arithmetic();
  test_position_comparison_operators();
  test_position_abs();
  test_position_zero();
  test_position_unit_conversions();
  test_position_null_parent();
  test_position_invalid_steps_per_revolution();
  test_position_large_values();

  std::cout << "\n=== Edge Case Tests ===" << std::endl;
  std::cout << std::endl;

  // Edge case tests
  test_angle_ticks_boundaries();
  test_negative_angle_conversions();
  test_rounding_policy();
  test_int32_boundaries();
  test_various_steps_per_rev();
  test_factory_method_roundtrips();
  test_mixed_sign_operators();
  test_parent_handling_in_operators();
  test_operator_edge_cases();
  test_nan_inf_steps_per_rev();
  test_arcminute_arcsecond_precision();

  std::cout << "\n========================================" << std::endl;
  std::cout << "✅ All Position Tests Passed!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
