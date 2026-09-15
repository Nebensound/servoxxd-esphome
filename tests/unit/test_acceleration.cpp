/**
 * @file test_acceleration.cpp
 * @brief Unit tests for Acceleration class with all 5 unit conversions
 *
 * Tests verify:
 * - Conversion accuracy from all 5 units to hardware value (0-255)
 * - Inverse time mapping (non-linear encoding)
 * - Edge cases (zero, instant, max values)
 * - RPM/s approximation accuracy
 * - Steps/s² conversion for ESPHome base class
 */

#include "servoxxd_acceleration.h"
#include "servoxxd.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace esphome::servoxxd;

// Tolerance for float comparisons
constexpr float EPSILON = 1.0f;  // Larger tolerance for non-linear mapping

bool float_eq(float a, float b, float epsilon = EPSILON) { return std::abs(a - b) < epsilon; }

// Mock ServoXxd for testing
class MockServoXxd : public ServoXxd {
 public:
  explicit MockServoXxd(float steps_per_rev) : steps_per_rev_(steps_per_rev) {}

  float get_steps_per_revolution() const override { return steps_per_rev_; }

 private:
  float steps_per_rev_;
};

void test_acceleration_steps_per_sec_sq() {
  std::cout << "Testing STEPS_PER_SEC_SQ conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 1000 steps/s² should be (1000 * 60) / 3200 = 18.75 RPM/s
  // acc = 256 - (20000 / 18.75) = 256 - 1066.67 = -810.67 → clamped to 1
  Acceleration acc(1000.0f, AccelerationUnit::STEPS_PER_SEC_SQ, &mock);
  assert(acc.acc_internal() == 1);  // Slowest acceleration

  std::cout << "  ✓ 1000 steps/s² → acc=" << (int) acc.acc_internal() << " (slowest)" << std::endl;
}

void test_acceleration_rpm_per_sec() {
  std::cout << "Testing RPM_PER_SEC conversion (motor native)..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 1000 RPM/s should be acc = 256 - (20000 / 1000) = 256 - 20 = 236
  Acceleration acc(1000.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc.acc_internal() == 236);

  std::cout << "  ✓ 1000 RPM/s → acc=" << (int) acc.acc_internal() << std::endl;

  // Verify reverse conversion is approximate
  float rpm_per_s = acc.get_rpm_per_sec();
  assert(float_eq(rpm_per_s, 1000.0f));
  std::cout << "  ✓ Reverse: acc=236 → " << rpm_per_s << " RPM/s (expected ~1000)" << std::endl;
}

void test_acceleration_rev_per_sec_sq() {
  std::cout << "Testing REV_PER_SEC_SQ conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 10 rev/s² should be 10 * 60 = 600 RPM/s
  // acc = 256 - (20000 / 600) = 256 - 33.33 = 222.67 → 223
  Acceleration acc(10.0f, AccelerationUnit::REV_PER_SEC_SQ, &mock);
  assert(acc.acc_internal() >= 222 && acc.acc_internal() <= 223);

  std::cout << "  ✓ 10 rev/s² → acc=" << (int) acc.acc_internal() << " (expected ~223)" << std::endl;
}

void test_acceleration_degrees_per_sec_sq() {
  std::cout << "Testing DEGREES_PER_SEC_SQ conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 360 deg/s² should be (360 * 60) / 360 = 60 RPM/s
  // acc = 256 - (20000 / 60) = 256 - 333.33 = -77.33 → clamped to 1
  Acceleration acc(360.0f, AccelerationUnit::DEGREES_PER_SEC_SQ, &mock);
  assert(acc.acc_internal() == 1);

  std::cout << "  ✓ 360 deg/s² → acc=" << (int) acc.acc_internal() << " (slowest)" << std::endl;
}

void test_acceleration_radians_per_sec_sq() {
  std::cout << "Testing RADIANS_PER_SEC_SQ conversion..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);
  constexpr float two_pi = 2.0f * static_cast<float>(PI);

  // Test: 2π rad/s² (1 rev/s²) should be 60 RPM/s
  Acceleration acc(two_pi, AccelerationUnit::RADIANS_PER_SEC_SQ, &mock);
  assert(acc.acc_internal() == 1);  // Low acceleration → slowest

  std::cout << "  ✓ 2π rad/s² → acc=" << (int) acc.acc_internal() << std::endl;
}

void test_acceleration_instant() {
  std::cout << "Testing instant acceleration (acc=0)..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 0 RPM/s should result in acc=0 (instant, no ramping)
  Acceleration acc_zero(0.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc_zero.acc_internal() == 0);
  std::cout << "  ✓ 0 RPM/s → acc=" << (int) acc_zero.acc_internal() << " (instant)" << std::endl;

  // Test: Very high acceleration should also result in acc=0
  Acceleration acc_high(25000.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc_high.acc_internal() == 0);
  std::cout << "  ✓ 25000 RPM/s → acc=" << (int) acc_high.acc_internal() << " (instant, > max)" << std::endl;

  // Verify reverse conversion returns -1.0f for instant
  float rpm_per_s = acc_zero.get_rpm_per_sec();
  assert(rpm_per_s == -1.0f);  // Sentinel value for instant
  std::cout << "  ✓ acc=0 → get_rpm_per_sec=-1.0 (sentinel for instant)" << std::endl;
}

void test_acceleration_max_value() {
  std::cout << "Testing maximum acceleration (acc=255)..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: acc=255 is the FASTEST ramping (not instant)
  // To get acc=255: 256 - (20000 / rpm_per_s) = 255 → rpm_per_s = 20000
  // But 20000 RPM/s is the hardware MAX, which maps to acc=0 (instant)!
  // So we need slightly less: rpm_per_s = 20000 / (256-255) = 20000 RPM/s
  // Actually, acc=255 means: 256 - 255 = 1 → Δt = 1×50μs → a = 20000/1 = 20000 RPM/s
  // But that's ≥ MAX, so it becomes instant (acc=0)

  // To get acc=255, we need: rpm_per_s = 20000 / (256-255) = 20000 / 1 = 20000
  // But 20000 ≥ MAX → instant. So acc=255 is actually unreachable via constructor!
  // The closest we can get is slightly below MAX: e.g. 19999 RPM/s

  // Test with 19999 RPM/s: acc = 256 - (20000/19999) ≈ 256 - 1.00005 ≈ 255
  Acceleration acc(19999.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc.acc_internal() == 255);

  std::cout << "  ✓ 19999 RPM/s → acc=" << (int) acc.acc_internal() << " (fastest non-instant)" << std::endl;

  // Test that exactly MAX_RPM_PER_SEC → instant (acc=0)
  Acceleration acc_max(20000.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc_max.acc_internal() == 0);
  std::cout << "  ✓ 20000 RPM/s (MAX) → acc=0 (instant)" << std::endl;
}

void test_acceleration_non_linear_mapping() {
  std::cout << "Testing non-linear inverse time mapping..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test several points to verify non-linear relationship
  struct TestCase {
    float rpm_per_s;
    uint8_t expected_acc_min;
    uint8_t expected_acc_max;
  };

  TestCase cases[] = {
      {100.0f, 56, 56},     // 256 - (20000/100) = 56
      {500.0f, 216, 216},   // 256 - (20000/500) = 216
      {1000.0f, 236, 236},  // 256 - (20000/1000) = 236
      {5000.0f, 252, 252},  // 256 - (20000/5000) = 252
      {10000.0f, 254, 254}  // 256 - (20000/10000) = 254
  };

  for (const auto &test : cases) {
    Acceleration acc(test.rpm_per_s, AccelerationUnit::RPM_PER_SEC, &mock);
    uint8_t acc_value = acc.acc_internal();
    assert(acc_value >= test.expected_acc_min && acc_value <= test.expected_acc_max);
    std::cout << "  ✓ " << test.rpm_per_s << " RPM/s → acc=" << (int) acc_value << " (expected "
              << (int) test.expected_acc_min << ")" << std::endl;
  }
}

void test_acceleration_steps_per_sec2_conversion() {
  std::cout << "Testing get_steps_per_sec2() for ESPHome..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: 1000 RPM/s = (1000 / 60) * 3200 = 53333.33 steps/s²
  Acceleration acc(1000.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  float steps_per_s2 = acc.get_steps_per_sec2();
  assert(float_eq(steps_per_s2, 53333.33f, 10.0f));

  std::cout << "  ✓ 1000 RPM/s = " << steps_per_s2 << " steps/s² (expected ~53333)" << std::endl;

  // Test instant (acc=0) returns -1.0f
  Acceleration acc_instant(0.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  float steps_instant = acc_instant.get_steps_per_sec2();
  assert(steps_instant == -1.0f);
  std::cout << "  ✓ Instant (acc=0) → -1.0 steps/s² (sentinel)" << std::endl;
}

void test_acceleration_boundary_values() {
  std::cout << "Testing boundary clamping..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test: Very low acceleration should clamp to 1 (slowest)
  Acceleration acc_low(10.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc_low.acc_internal() == 1);
  std::cout << "  ✓ 10 RPM/s → acc=1 (minimum non-instant)" << std::endl;

  // Test: Negative acceleration should result in acc=0 (instant)
  Acceleration acc_neg(-100.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc_neg.acc_internal() == 0);
  std::cout << "  ✓ -100 RPM/s → acc=0 (instant, invalid input)" << std::endl;
}

void test_acceleration_null_parent() {
  std::cout << "Testing null parent pointer handling..." << std::endl;

  // Test: STEPS_PER_SEC_SQ with null parent should default to 0 (instant)
  Acceleration acc_null(1000.0f, AccelerationUnit::STEPS_PER_SEC_SQ, nullptr);
  assert(acc_null.acc_internal() == 0);
  std::cout << "  ✓ STEPS_PER_SEC_SQ with null parent → acc=0 (error handling)" << std::endl;

  // Test: Other units with null parent should work fine
  Acceleration acc_rpm_null(1000.0f, AccelerationUnit::RPM_PER_SEC, nullptr);
  assert(acc_rpm_null.acc_internal() == 236);  // Normal calculation
  std::cout << "  ✓ RPM_PER_SEC with null parent → acc=236 (parent not needed)" << std::endl;
}

void test_acceleration_invalid_steps_per_revolution() {
  std::cout << "Testing invalid steps_per_revolution..." << std::endl;

  // Mock with invalid (negative) steps_per_rev
  MockServoXxd mock_invalid(-100.0f);

  // Should handle invalid steps_per_rev gracefully
  Acceleration acc(1000.0f, AccelerationUnit::STEPS_PER_SEC_SQ, &mock_invalid);
  assert(acc.acc_internal() == 0);  // Should default to 0 (instant)
  std::cout << "  ✓ STEPS_PER_SEC_SQ with negative steps_per_rev → acc=0 (error handling)" << std::endl;

  // Mock with zero steps_per_rev
  MockServoXxd mock_zero(0.0f);
  Acceleration acc_zero(1000.0f, AccelerationUnit::STEPS_PER_SEC_SQ, &mock_zero);
  assert(acc_zero.acc_internal() == 0);
  std::cout << "  ✓ STEPS_PER_SEC_SQ with zero steps_per_rev → acc=0 (error handling)" << std::endl;
}

void test_acceleration_factory_methods() {
  std::cout << "Testing factory methods..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test from_rpm_per_sec
  Acceleration acc1 = Acceleration::from_rpm_per_sec(1000.0f, &mock);
  assert(acc1.acc_internal() == 236);
  std::cout << "  ✓ from_rpm_per_sec(1000) → acc=236" << std::endl;

  // Test from_steps_per_sec2
  Acceleration acc2 = Acceleration::from_steps_per_sec2(53333.0f, &mock);
  assert(acc2.acc_internal() >= 235 && acc2.acc_internal() <= 237);
  std::cout << "  ✓ from_steps_per_sec2(53333) → acc=" << (int) acc2.acc_internal() << std::endl;

  // Test from_rev_per_sec2
  Acceleration acc3 = Acceleration::from_rev_per_sec2(10.0f, &mock);
  assert(acc3.acc_internal() >= 222 && acc3.acc_internal() <= 223);
  std::cout << "  ✓ from_rev_per_sec2(10) → acc=" << (int) acc3.acc_internal() << std::endl;

  // Test from_degrees_per_sec2
  // 3600 deg/s² = 10 rev/s² = 600 RPM/s → acc=223
  Acceleration acc4 = Acceleration::from_degrees_per_sec2(3600.0f, &mock);
  assert(acc4.acc_internal() >= 222 && acc4.acc_internal() <= 224);
  std::cout << "  ✓ from_degrees_per_sec2(3600) → acc=" << (int) acc4.acc_internal() << std::endl;

  // Test from_radians_per_sec2
  // 2π*10 rad/s² = 10 rev/s² = 600 RPM/s → acc=223
  constexpr float two_pi = 2.0f * static_cast<float>(PI);
  Acceleration acc5 = Acceleration::from_radians_per_sec2(two_pi * 10.0f, &mock);
  assert(acc5.acc_internal() >= 222 && acc5.acc_internal() <= 224);
  std::cout << "  ✓ from_radians_per_sec2(2π*10) → acc=" << (int) acc5.acc_internal() << std::endl;
}

void test_acceleration_unit_conversions() {
  std::cout << "Testing all unit accessor methods..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Create 1000 RPM/s acceleration
  Acceleration acc(1000.0f, AccelerationUnit::RPM_PER_SEC, &mock);

  float rpm_per_s = acc.get_rpm_per_sec();
  assert(float_eq(rpm_per_s, 1000.0f));

  float steps_per_s2 = acc.get_steps_per_sec2();
  assert(float_eq(steps_per_s2, 53333.33f, 10.0f));

  float rev_per_s2 = acc.get_rev_per_sec2();
  assert(float_eq(rev_per_s2, 16.67f, 1.0f));

  float deg_per_s2 = acc.get_degrees_per_sec2();
  assert(float_eq(deg_per_s2, 6000.0f, 10.0f));

  float rad_per_s2 = acc.get_radians_per_sec2();
  assert(float_eq(rad_per_s2, 104.72f, 2.0f));

  std::cout << "  ✓ 1000 RPM/s = " << rpm_per_s << " RPM/s" << std::endl;
  std::cout << "  ✓ 1000 RPM/s = " << steps_per_s2 << " steps/s²" << std::endl;
  std::cout << "  ✓ 1000 RPM/s = " << rev_per_s2 << " rev/s²" << std::endl;
  std::cout << "  ✓ 1000 RPM/s = " << deg_per_s2 << " deg/s²" << std::endl;
  std::cout << "  ✓ 1000 RPM/s = " << rad_per_s2 << " rad/s²" << std::endl;
}

void test_acceleration_setters() {
  std::cout << "Testing setter methods..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  Acceleration acc(&mock);
  assert(acc.acc_internal() == 0);  // Default is instant

  // Test set_rpm_per_sec
  acc.set_rpm_per_sec(1000.0f);
  assert(acc.acc_internal() == 236);
  std::cout << "  ✓ set_rpm_per_sec(1000) → acc=236" << std::endl;

  // Test set_steps_per_sec2
  // 10000 steps/s² @ 3200 steps/rev = 187.5 RPM/s → acc≈149
  acc.set_steps_per_sec2(10000.0f);
  assert(acc.acc_internal() >= 148 && acc.acc_internal() <= 150);
  std::cout << "  ✓ set_steps_per_sec2(10000) → acc=" << (int) acc.acc_internal() << std::endl;

  // Test set_rev_per_sec2
  // 20 rev/s² = 1200 RPM/s → acc≈239
  acc.set_rev_per_sec2(20.0f);
  assert(acc.acc_internal() >= 239 && acc.acc_internal() <= 240);
  std::cout << "  ✓ set_rev_per_sec2(20) → acc=" << (int) acc.acc_internal() << std::endl;

  // Test set_degrees_per_sec2
  // 7200 deg/s² = 20 rev/s² = 1200 RPM/s → acc≈239
  acc.set_degrees_per_sec2(7200.0f);
  assert(acc.acc_internal() >= 239 && acc.acc_internal() <= 240);
  std::cout << "  ✓ set_degrees_per_sec2(7200) → acc=" << (int) acc.acc_internal() << std::endl;

  // Test set_radians_per_sec2
  // 2π*20 rad/s² = 20 rev/s² = 1200 RPM/s → acc≈239
  constexpr float two_pi = 2.0f * static_cast<float>(PI);
  acc.set_radians_per_sec2(two_pi * 20.0f);
  assert(acc.acc_internal() >= 239 && acc.acc_internal() <= 240);
  std::cout << "  ✓ set_radians_per_sec2(2π*20) → acc=" << (int) acc.acc_internal() << std::endl;
}

void test_acceleration_int_overloads() {
  std::cout << "Testing int constructor/setter overloads..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  // Test int64_t constructor
  Acceleration acc1(static_cast<int64_t>(1000), AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc1.acc_internal() == 236);
  std::cout << "  ✓ Acceleration(int64_t 1000, RPM_PER_SEC) → acc=236" << std::endl;

  // Test int32_t constructor
  Acceleration acc2(static_cast<int32_t>(500), AccelerationUnit::RPM_PER_SEC, &mock);
  assert(acc2.acc_internal() == 216);
  std::cout << "  ✓ Acceleration(int32_t 500, RPM_PER_SEC) → acc=216" << std::endl;

  // Test int64_t setter
  Acceleration acc3(&mock);
  acc3.set_rpm_per_sec(static_cast<int64_t>(1000));
  assert(acc3.acc_internal() == 236);
  std::cout << "  ✓ set_rpm_per_sec(int64_t 1000) → acc=236" << std::endl;

  // Test int32_t setter
  acc3.set_rpm_per_sec(static_cast<int32_t>(500));
  assert(acc3.acc_internal() == 216);
  std::cout << "  ✓ set_rpm_per_sec(int32_t 500) → acc=216" << std::endl;
}

void test_acceleration_comparison_operators() {
  std::cout << "Testing comparison operators..." << std::endl;

  float steps_per_rev = 3200.0f;
  MockServoXxd mock(steps_per_rev);

  Acceleration acc1(1000.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  Acceleration acc2(1000.0f, AccelerationUnit::RPM_PER_SEC, &mock);
  Acceleration acc3(500.0f, AccelerationUnit::RPM_PER_SEC, &mock);

  // Test equality
  assert(acc1 == acc2);
  assert(!(acc1 == acc3));
  std::cout << "  ✓ Equality: acc1 == acc2, acc1 != acc3" << std::endl;

  // Test inequality
  assert(acc1 != acc3);
  assert(!(acc1 != acc2));
  std::cout << "  ✓ Inequality: acc1 != acc3, !(acc1 != acc2)" << std::endl;
}

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "Acceleration Class Unit Tests" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;

  // Basic unit conversions
  test_acceleration_steps_per_sec_sq();
  test_acceleration_rpm_per_sec();
  test_acceleration_rev_per_sec_sq();
  test_acceleration_degrees_per_sec_sq();
  test_acceleration_radians_per_sec_sq();

  // Edge cases
  test_acceleration_instant();
  test_acceleration_max_value();
  test_acceleration_non_linear_mapping();
  test_acceleration_boundary_values();

  // API pattern tests (matching Position)
  test_acceleration_factory_methods();
  test_acceleration_unit_conversions();
  test_acceleration_setters();
  test_acceleration_int_overloads();
  test_acceleration_comparison_operators();

  // Legacy compatibility
  test_acceleration_steps_per_sec2_conversion();

  // Error handling
  test_acceleration_null_parent();
  test_acceleration_invalid_steps_per_revolution();

  std::cout << "\n========================================" << std::endl;
  std::cout << "✅ All Acceleration Tests Passed!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
