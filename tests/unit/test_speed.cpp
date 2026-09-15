/**
 * @file test_speed.cpp
 * @brief Unit tests for Speed class with all 7 unit conversions
 *
 * Tests verify:
 * - Conversion accuracy from all 7 units to RPM
 * - Microstepping compensation (rpm_for_hardware)
 * - Edge cases (zero, negative, max values)
 * - Steps/s conversion for ESPHome base class
 */

#include "servoxxd_speed.h"
#include "servoxxd.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace esphome::servoxxd;

// Tolerance for float comparisons
constexpr float EPSILON = 0.01f;

bool float_eq(float a, float b) { return std::abs(a - b) < EPSILON; }

// Mock ServoXxd for testing
class MockServoXxd : public ServoXxd {
 public:
  MockServoXxd(uint16_t microsteps = 16) : microsteps_(static_cast<uint8_t>(microsteps)) {}

  float get_steps_per_revolution() const override { return 200.0f * microsteps_; }
  uint8_t get_microstepping() const override { return microsteps_; }

 private:
  uint8_t microsteps_;
};

void test_speed_steps_per_sec() {
  std::cout << "Testing STEPS_PER_SEC conversion..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: 1000 steps/s should be (1000 * 60) / 3200 = 18.75 RPM → rounds to 19
  Speed speed(1000.0f, SpeedUnit::STEPS_PER_SEC, &mock);
  assert(speed.rpm() == 19.0f);  // int16_t storage rounds to 19

  std::cout << "  ✓ 1000 steps/s = " << speed.rpm() << " RPM (18.75 → 19 after rounding)" << std::endl;
}

void test_speed_rpm() {
  std::cout << "Testing RPM conversion (direct)..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: 100 RPM should remain 100 RPM (motor native unit)
  Speed speed(100.0f, SpeedUnit::RPM, &mock);
  assert(float_eq(speed.rpm(), 100.0f));

  std::cout << "  ✓ 100 RPM = " << speed.rpm() << " RPM (expected 100)" << std::endl;
}

void test_speed_rev_per_sec() {
  std::cout << "Testing REV_PER_SEC conversion..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: 1.5 rev/s should be 1.5 * 60 = 90 RPM
  Speed speed(1.5f, SpeedUnit::REV_PER_SEC, &mock);
  assert(float_eq(speed.rpm(), 90.0f));

  std::cout << "  ✓ 1.5 rev/s = " << speed.rpm() << " RPM (expected 90)" << std::endl;
}

void test_speed_degrees_per_sec() {
  std::cout << "Testing DEGREES_PER_SEC conversion..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: 360 deg/s should be (360 * 60) / 360 = 60 RPM
  Speed speed(360.0f, SpeedUnit::DEGREES_PER_SEC, &mock);
  assert(float_eq(speed.rpm(), 60.0f));

  std::cout << "  ✓ 360 deg/s = " << speed.rpm() << " RPM (expected 60)" << std::endl;
}

void test_speed_radians_per_sec() {
  std::cout << "Testing RADIANS_PER_SEC conversion..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev
  const float pi_value = static_cast<float>(PI);
  const float two_pi = 2.0f * pi_value;

  // Test: 2π rad/s (1 rev/s) should be 60 RPM
  Speed speed(two_pi, SpeedUnit::RADIANS_PER_SEC, &mock);
  assert(float_eq(speed.rpm(), 60.0f));

  std::cout << "  ✓ 2π rad/s = " << speed.rpm() << " RPM (expected 60)" << std::endl;
}

void test_speed_degrees_per_min() {
  std::cout << "Testing DEGREES_PER_MIN conversion..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: 360 deg/min should be 360 / 6 = 60 RPM
  Speed speed(360.0f, SpeedUnit::DEGREES_PER_MIN, &mock);
  assert(float_eq(speed.rpm(), 60.0f));

  std::cout << "  ✓ 360 deg/min = " << speed.rpm() << " RPM (expected 60)" << std::endl;
}

void test_speed_degrees_per_hour() {
  std::cout << "Testing DEGREES_PER_HOUR conversion..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: 21600 deg/h should be 21600 / 360 = 60 RPM
  Speed speed(21600.0f, SpeedUnit::DEGREES_PER_HOUR, &mock);
  assert(float_eq(speed.rpm(), 60.0f));

  std::cout << "  ✓ 21600 deg/h = " << speed.rpm() << " RPM (expected 60)" << std::endl;
}

void test_speed_microstepping_compensation() {
  std::cout << "Testing microstepping compensation..." << std::endl;

  // Test different microstepping values - need separate Speed objects for each
  MockServoXxd mock_8(8);
  MockServoXxd mock_16(16);
  MockServoXxd mock_64(64);
  MockServoXxd mock_128(128);
  MockServoXxd mock_255(255);  // Maximum valid for uint8_t encoding

  // Reference: 16/32/64 should return unchanged
  Speed speed_16(100.0f, SpeedUnit::RPM, &mock_16);
  int16_t rpm_16 = speed_16.rpm_for_hardware();
  assert(rpm_16 == 100);
  std::cout << "  ✓ microsteps=16: " << rpm_16 << " RPM (no compensation)" << std::endl;

  Speed speed_64(100.0f, SpeedUnit::RPM, &mock_64);
  int16_t rpm_64 = speed_64.rpm_for_hardware();
  assert(rpm_64 == 100);
  std::cout << "  ✓ microsteps=64: " << rpm_64 << " RPM (no compensation)" << std::endl;

  // microsteps=8: multiply by 2 (hardware divides by 2, so we compensate)
  Speed speed_8(100.0f, SpeedUnit::RPM, &mock_8);
  int16_t rpm_8 = speed_8.rpm_for_hardware();
  assert(rpm_8 == 200);
  std::cout << "  ✓ microsteps=8: " << rpm_8 << " RPM (×2)" << std::endl;

  // microsteps=128: divide by 8 (hardware multiplies by 8, so we compensate)
  Speed speed_128(100.0f, SpeedUnit::RPM, &mock_128);
  int16_t rpm_128 = speed_128.rpm_for_hardware();
  assert(rpm_128 == 12);  // 100 / 8 = 12 (integer division)
  std::cout << "  ✓ microsteps=128: " << rpm_128 << " RPM (÷8)" << std::endl;

  // microsteps=255: Not a valid menu value, but tests uint8_t encoding boundary
  // Expects no compensation (only menu values 1,2,4,8,16,32,64,128,256 are scaled)
  MockServoXxd mock_255_edge(255);
  Speed speed_255_edge(100.0f, SpeedUnit::RPM, &mock_255_edge);
  int16_t rpm_255_edge = speed_255_edge.rpm_for_hardware();
  assert(rpm_255_edge == 100);  // No compensation for non-menu value
  std::cout << "  ✓ microsteps=255: " << rpm_255_edge << " RPM (no compensation - non-menu value)" << std::endl;
}

void test_speed_steps_per_sec_conversion() {
  std::cout << "Testing steps_per_sec() for ESPHome..." << std::endl;

  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev
  Speed speed(60.0f, SpeedUnit::RPM, &mock);

  // 60 RPM = 1 rev/s = 3200 steps/s
  float steps_per_s = speed.steps_per_sec();
  assert(float_eq(steps_per_s, 3200.0f));

  std::cout << "  ✓ 60 RPM = " << steps_per_s << " steps/s (expected 3200)" << std::endl;
}

void test_speed_negative_values() {
  std::cout << "Testing negative speed values..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: -100 RPM should work (reverse direction)
  Speed speed(-100.0f, SpeedUnit::RPM, &mock);
  assert(float_eq(speed.rpm(), -100.0f));
  assert(speed.rpm_as_i16() == -100);

  std::cout << "  ✓ -100 RPM = " << speed.rpm() << " RPM (reverse direction)" << std::endl;
}

void test_speed_range_clamping() {
  std::cout << "Testing range clamping..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test: Very high speed should clamp to hardware max (3000 RPM)
  Speed speed_high(40000.0f, SpeedUnit::RPM, &mock);
  assert(speed_high.rpm_as_i16() == 3000);
  std::cout << "  ✓ 40000 RPM clamped to " << speed_high.rpm_as_i16() << " (hardware max)" << std::endl;

  // Test: Very low speed should clamp to hardware min (-3000 RPM)
  Speed speed_low(-40000.0f, SpeedUnit::RPM, &mock);
  assert(speed_low.rpm_as_i16() == -3000);
  std::cout << "  ✓ -40000 RPM clamped to " << speed_low.rpm_as_i16() << " (hardware min)" << std::endl;
}

void test_speed_null_parent() {
  std::cout << "Testing null parent pointer handling..." << std::endl;

  // Test: STEPS_PER_SEC with null parent should default to 0
  Speed speed_null(1000.0f, SpeedUnit::STEPS_PER_SEC, nullptr);
  assert(speed_null.rpm() == 0.0f);
  std::cout << "  ✓ STEPS_PER_SEC with null parent → 0 RPM (error handling)" << std::endl;

  // Test: Other units with null parent should work fine
  Speed speed_rpm_null(100.0f, SpeedUnit::RPM, nullptr);
  assert(speed_rpm_null.rpm() == 100.0f);
  std::cout << "  ✓ RPM with null parent → 100 RPM (parent not needed)" << std::endl;
}

void test_speed_zero_value() {
  std::cout << "Testing zero speed value..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  Speed speed_zero(0.0f, SpeedUnit::RPM, &mock);
  assert(speed_zero.rpm() == 0.0f);
  assert(speed_zero.rpm_as_i16() == 0);
  std::cout << "  ✓ 0 RPM = 0 RPM" << std::endl;
}

void test_speed_invalid_microsteps() {
  std::cout << "Testing edge case microstepping values..." << std::endl;

  // Test microsteps=1 (minimum valid)
  MockServoXxd mock_1(1);
  Speed speed_1(1000.0f, SpeedUnit::STEPS_PER_SEC, &mock_1);
  assert(speed_1.rpm() > 0.0f);  // Should work with microsteps=1
  std::cout << "  ✓ microsteps=1 works correctly" << std::endl;

  // Test microsteps=255 (maximum valid for uint8_t encoding)
  MockServoXxd mock_255(255);
  Speed speed_255(1000.0f, SpeedUnit::STEPS_PER_SEC, &mock_255);
  assert(speed_255.rpm() > 0.0f);  // Should work with microsteps=255
  std::cout << "  ✓ microsteps=255 works correctly" << std::endl;
}

void test_speed_factory_methods() {
  std::cout << "Testing factory methods..." << std::endl;

  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test from_rpm
  Speed speed1 = Speed::from_rpm(100.0f, &mock);
  assert(speed1.get_rpm() == 100);
  std::cout << "  ✓ from_rpm(100) → " << speed1.get_rpm() << " RPM" << std::endl;

  // Test from_steps_per_sec
  Speed speed2 = Speed::from_steps_per_sec(3200.0f, &mock);
  assert(speed2.get_rpm() == 60);
  std::cout << "  ✓ from_steps_per_sec(3200) → " << speed2.get_rpm() << " RPM" << std::endl;

  // Test from_rev_per_sec
  Speed speed3 = Speed::from_rev_per_sec(1.5f, &mock);
  assert(speed3.get_rpm() == 90);
  std::cout << "  ✓ from_rev_per_sec(1.5) → " << speed3.get_rpm() << " RPM" << std::endl;

  // Test from_degrees_per_sec
  Speed speed4 = Speed::from_degrees_per_sec(360.0f, &mock);
  assert(speed4.get_rpm() == 60);
  std::cout << "  ✓ from_degrees_per_sec(360) → " << speed4.get_rpm() << " RPM" << std::endl;

  // Test from_radians_per_sec
  const float pi_value = static_cast<float>(PI);
  const float two_pi = 2.0f * pi_value;
  Speed speed5 = Speed::from_radians_per_sec(two_pi, &mock);
  assert(speed5.get_rpm() == 60);
  std::cout << "  ✓ from_radians_per_sec(2π) → " << speed5.get_rpm() << " RPM" << std::endl;

  // Test from_degrees_per_min
  Speed speed6 = Speed::from_degrees_per_min(360.0f, &mock);
  assert(speed6.get_rpm() == 60);
  std::cout << "  ✓ from_degrees_per_min(360) → " << speed6.get_rpm() << " RPM" << std::endl;

  // Test from_degrees_per_hour
  Speed speed7 = Speed::from_degrees_per_hour(21600.0f, &mock);
  assert(speed7.get_rpm() == 60);
  std::cout << "  ✓ from_degrees_per_hour(21600) → " << speed7.get_rpm() << " RPM" << std::endl;
}

void test_speed_unit_conversions() {
  std::cout << "Testing all unit accessor methods..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Create 60 RPM speed (1 rev/s)
  Speed speed(60.0f, SpeedUnit::RPM, &mock);

  // Test int return types
  assert(speed.get_rpm() == 60);
  assert(speed.get_steps_per_sec() == 3200);
  assert(speed.get_degrees_per_sec() == 360);
  assert(speed.get_degrees_per_min() == 360);
  assert(speed.get_degrees_per_hour() == 21600);

  // Test float return types
  assert(float_eq(speed.get_rev_per_sec(), 1.0f));
  assert(float_eq(speed.get_radians_per_sec(), 2.0f * static_cast<float>(PI)));

  std::cout << "  ✓ 60 RPM = " << speed.get_rpm() << " RPM (int16)" << std::endl;
  std::cout << "  ✓ 60 RPM = " << speed.get_steps_per_sec() << " steps/s (int32)" << std::endl;
  std::cout << "  ✓ 60 RPM = " << speed.get_rev_per_sec() << " rev/s (float)" << std::endl;
  std::cout << "  ✓ 60 RPM = " << speed.get_degrees_per_sec() << " deg/s (int32)" << std::endl;
  std::cout << "  ✓ 60 RPM = " << speed.get_radians_per_sec() << " rad/s (float)" << std::endl;
  std::cout << "  ✓ 60 RPM = " << speed.get_degrees_per_min() << " deg/min (int32)" << std::endl;
  std::cout << "  ✓ 60 RPM = " << speed.get_degrees_per_hour() << " deg/h (int32)" << std::endl;
}

void test_speed_setters() {
  std::cout << "Testing setter methods..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  Speed speed(&mock);
  assert(speed.rpm_internal() == 0);

  // Test set_rpm
  speed.set_rpm(100.0f);
  assert(speed.get_rpm() == 100);
  std::cout << "  ✓ set_rpm(100) → " << speed.get_rpm() << " RPM" << std::endl;

  // Test set_steps_per_sec
  speed.set_steps_per_sec(3200.0f);
  assert(speed.get_rpm() == 60);
  std::cout << "  ✓ set_steps_per_sec(3200) → " << speed.get_rpm() << " RPM" << std::endl;

  // Test set_rev_per_sec
  speed.set_rev_per_sec(2.0f);
  assert(speed.get_rpm() == 120);
  std::cout << "  ✓ set_rev_per_sec(2) → " << speed.get_rpm() << " RPM" << std::endl;

  // Test set_degrees_per_sec
  speed.set_degrees_per_sec(360.0f);
  assert(speed.get_rpm() == 60);
  std::cout << "  ✓ set_degrees_per_sec(360) → " << speed.get_rpm() << " RPM" << std::endl;

  // Test set_radians_per_sec
  const float two_pi = 2.0f * static_cast<float>(PI);
  speed.set_radians_per_sec(two_pi);
  assert(speed.get_rpm() == 60);
  std::cout << "  ✓ set_radians_per_sec(2π) → " << speed.get_rpm() << " RPM" << std::endl;

  // Test set_degrees_per_min
  speed.set_degrees_per_min(360.0f);
  assert(speed.get_rpm() == 60);
  std::cout << "  ✓ set_degrees_per_min(360) → " << speed.get_rpm() << " RPM" << std::endl;

  // Test set_degrees_per_hour
  speed.set_degrees_per_hour(21600.0f);
  assert(speed.get_rpm() == 60);
  std::cout << "  ✓ set_degrees_per_hour(21600) → " << speed.get_rpm() << " RPM" << std::endl;
}

void test_speed_int_overloads() {
  std::cout << "Testing int constructor/setter overloads..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  // Test int64_t constructor
  Speed speed1(static_cast<int64_t>(100), SpeedUnit::RPM, &mock);
  assert(speed1.get_rpm() == 100);
  std::cout << "  ✓ Speed(int64_t 100, RPM) → " << speed1.get_rpm() << " RPM" << std::endl;

  // Test int32_t constructor
  Speed speed2(static_cast<int32_t>(50), SpeedUnit::RPM, &mock);
  assert(speed2.get_rpm() == 50);
  std::cout << "  ✓ Speed(int32_t 50, RPM) → " << speed2.get_rpm() << " RPM" << std::endl;

  // Test int64_t setter
  Speed speed3(&mock);
  speed3.set_rpm(static_cast<int64_t>(100));
  assert(speed3.get_rpm() == 100);
  std::cout << "  ✓ set_rpm(int64_t 100) → " << speed3.get_rpm() << " RPM" << std::endl;

  // Test int32_t setter
  speed3.set_rpm(static_cast<int32_t>(50));
  assert(speed3.get_rpm() == 50);
  std::cout << "  ✓ set_rpm(int32_t 50) → " << speed3.get_rpm() << " RPM" << std::endl;
}

void test_speed_comparison_operators() {
  std::cout << "Testing comparison operators..." << std::endl;

  // 200 * 16 = 3200 effective steps/rev
  MockServoXxd mock(16);  // 200 * 16 = 3200 effective steps/rev

  Speed speed1(100.0f, SpeedUnit::RPM, &mock);
  Speed speed2(100.0f, SpeedUnit::RPM, &mock);
  Speed speed3(50.0f, SpeedUnit::RPM, &mock);

  // Test equality
  assert(speed1 == speed2);
  assert(!(speed1 == speed3));
  std::cout << "  ✓ Equality: speed1 == speed2, speed1 != speed3" << std::endl;

  // Test inequality
  assert(speed1 != speed3);
  assert(!(speed1 != speed2));
  std::cout << "  ✓ Inequality: speed1 != speed3, !(speed1 != speed2)" << std::endl;
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "Speed Class Unit Tests" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;

  // Basic unit conversions
  test_speed_steps_per_sec();
  test_speed_rpm();
  test_speed_rev_per_sec();
  test_speed_degrees_per_sec();
  test_speed_radians_per_sec();
  test_speed_degrees_per_min();
  test_speed_degrees_per_hour();

  // Hardware-specific
  test_speed_microstepping_compensation();
  test_speed_steps_per_sec_conversion();

  // Edge cases
  test_speed_negative_values();
  test_speed_range_clamping();
  test_speed_zero_value();

  // API pattern tests (matching Position/Acceleration)
  test_speed_factory_methods();
  test_speed_unit_conversions();
  test_speed_setters();
  test_speed_int_overloads();
  test_speed_comparison_operators();

  // Error handling
  test_speed_null_parent();
  test_speed_invalid_microsteps();

  std::cout << "\n========================================" << std::endl;
  std::cout << "✅ All Speed Tests Passed!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
