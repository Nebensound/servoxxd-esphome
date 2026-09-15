#include <iostream>
#include <cassert>
#include "../../components/servoxxd/stepper/servoxxd_commands.h"

using namespace esphome::servoxxd;

// Simple test statistics tracker
struct TestStats {
  int passed = 0;
  int failed = 0;

  void check(bool condition, const char *description) {
    if (condition) {
      std::cout << "  ✓ " << description << std::endl;
      passed++;
    } else {
      std::cout << "  ✗ FAILED: " << description << std::endl;
      failed++;
    }
  }

  void print_summary(const char *test_name) {
    std::cout << "\n" << test_name << ": ";
    if (failed == 0) {
      std::cout << "✅ All " << passed << " assertions passed" << std::endl;
    } else {
      std::cout << "❌ " << failed << " of " << (passed + failed) << " assertions failed" << std::endl;
      exit(1);
    }
  }
};

// ============================================================================
// Test 1: function_code() for READ commands (0x04)
// ============================================================================
void test_01_function_code_read_commands(TestStats &stats) {
  std::cout << "\nTEST 1: function_code() returns 0x04 for READ commands" << std::endl;

  // Test all READ commands (0x30-0x3F range)
  Command cmd1(Commandtype::READ_ENCODER_CARRY);
  stats.check(cmd1.function_code() == 0x04, "READ_ENCODER_CARRY → 0x04");

  Command cmd2(Commandtype::READ_ENCODER_ADDITION);
  stats.check(cmd2.function_code() == 0x04, "READ_ENCODER_ADDITION → 0x04");

  Command cmd3(Commandtype::READ_CURRENT_SPEED);
  stats.check(cmd3.function_code() == 0x04, "READ_CURRENT_SPEED → 0x04");

  Command cmd4(Commandtype::READ_PULSE_COUNT);
  stats.check(cmd4.function_code() == 0x04, "READ_PULSE_COUNT → 0x04");

  Command cmd5(Commandtype::READ_IO_STATUS);
  stats.check(cmd5.function_code() == 0x04, "READ_IO_STATUS → 0x04");

  Command cmd6(Commandtype::READ_ANGLE_ERROR);
  stats.check(cmd6.function_code() == 0x04, "READ_ANGLE_ERROR → 0x04");

  Command cmd7(Commandtype::READ_ENABLE_STATUS);
  stats.check(cmd7.function_code() == 0x04, "READ_ENABLE_STATUS → 0x04");

  Command cmd8(Commandtype::READ_ZERO_RETURN_STATUS);
  stats.check(cmd8.function_code() == 0x04, "READ_ZERO_RETURN_STATUS → 0x04");

  Command cmd9(Commandtype::READ_PROTECTION_STATUS);
  stats.check(cmd9.function_code() == 0x04, "READ_PROTECTION_STATUS → 0x04");

  Command cmd10(Commandtype::READ_MOTOR_STATUS);
  stats.check(cmd10.function_code() == 0x04, "READ_MOTOR_STATUS → 0x04");
}

// ============================================================================
// Test 2: function_code() for simple WRITE commands (0x06)
// ============================================================================
void test_02_function_code_simple_write(TestStats &stats) {
  std::cout << "\nTEST 2: function_code() returns 0x06 for simple WRITE commands" << std::endl;

  Command cmd1(Commandtype::ENABLE_MOTOR);
  stats.check(cmd1.function_code() == 0x06, "ENABLE_MOTOR → 0x06");

  Command cmd2(Commandtype::SET_WORKING_CURRENT);
  stats.check(cmd2.function_code() == 0x06, "SET_WORKING_CURRENT → 0x06");

  Command cmd3(Commandtype::SET_WORK_MODE);
  stats.check(cmd3.function_code() == 0x06, "SET_WORK_MODE → 0x06");

  Command cmd4(Commandtype::SET_SUBDIVISION);
  stats.check(cmd4.function_code() == 0x06, "SET_SUBDIVISION → 0x06");

  Command cmd5(Commandtype::SET_EN_PIN_ACTIVE);
  stats.check(cmd5.function_code() == 0x06, "SET_EN_PIN_ACTIVE → 0x06");

  Command cmd6(Commandtype::GO_HOME);
  stats.check(cmd6.function_code() == 0x06, "GO_HOME → 0x06");

  Command cmd7(Commandtype::SET_ZERO);
  stats.check(cmd7.function_code() == 0x06, "SET_ZERO → 0x06");

  Command cmd8(Commandtype::EMERGENCY_STOP);
  stats.check(cmd8.function_code() == 0x06, "EMERGENCY_STOP → 0x06");

  Command cmd9(Commandtype::RELEASE_PROTECTION);
  stats.check(cmd9.function_code() == 0x06, "RELEASE_PROTECTION → 0x06");

  Command cmd10(Commandtype::STOP_POSITION_MODE_2);
  stats.check(cmd10.function_code() == 0x06, "STOP_POSITION_MODE_2 → 0x06");
}

// ============================================================================
// Test 3: function_code() for complex WRITE commands (0x10)
// ============================================================================
void test_03_function_code_complex_write(TestStats &stats) {
  std::cout << "\nTEST 3: function_code() returns 0x10 for complex WRITE commands" << std::endl;

  Command cmd1(Commandtype::MOVE_POSITION_MODE_1);
  stats.check(cmd1.function_code() == 0x10, "MOVE_POSITION_MODE_1 → 0x10");

  Command cmd2(Commandtype::MOVE_POSITION_MODE_2);
  stats.check(cmd2.function_code() == 0x10, "MOVE_POSITION_MODE_2 (0xFE/32) → 0x10");

  Command cmd3(Commandtype::MOVE_POSITION_MODE_3);
  stats.check(cmd3.function_code() == 0x10, "MOVE_POSITION_MODE_3 → 0x10");

  Command cmd4(Commandtype::MOVE_POSITION_MODE_4);
  stats.check(cmd4.function_code() == 0x10, "MOVE_POSITION_MODE_4 → 0x10");

  Command cmd5(Commandtype::MOVE_SPEED_MODE);
  stats.check(cmd5.function_code() == 0x10, "MOVE_SPEED_MODE → 0x10");

  Command cmd6(Commandtype::SET_HOMING_PARAMETERS);
  stats.check(cmd6.function_code() == 0x10, "SET_HOMING_PARAMETERS → 0x10");

  Command cmd7(Commandtype::SET_NOLIMIT_HOMING_PARAMS);
  stats.check(cmd7.function_code() == 0x10, "SET_NOLIMIT_HOMING_PARAMS → 0x10");

  Command cmd8(Commandtype::SET_ZERO_MODE);
  stats.check(cmd8.function_code() == 0x10, "SET_ZERO_MODE → 0x10");
}

// ============================================================================
// Test 4: register_address() returns command_type value
// ============================================================================
void test_04_register_address(TestStats &stats) {
  std::cout << "\nTEST 4: register_address() returns command_type as uint16_t" << std::endl;

  Command cmd1(Commandtype::READ_ENCODER_CARRY);
  stats.check(cmd1.register_address() == 0x30, "READ_ENCODER_CARRY → 0x30");

  Command cmd2(Commandtype::MOVE_POSITION_MODE_2);
  stats.check(cmd2.register_address() == 0xFE, "MOVE_POSITION_MODE_2 → 0xFE");

  Command cmd3(Commandtype::ENABLE_MOTOR);
  stats.check(cmd3.register_address() == 0xF3, "ENABLE_MOTOR → 0xF3");

  Command cmd4(Commandtype::READ_MOTOR_STATUS);
  stats.check(cmd4.register_address() == 0xF1, "READ_MOTOR_STATUS → 0xF1");

  Command cmd5(Commandtype::SET_HOMING_PARAMETERS);
  stats.check(cmd5.register_address() == 0x90, "SET_HOMING_PARAMETERS → 0x90");

  Command cmd6(Commandtype::RELEASE_PROTECTION);
  stats.check(cmd6.register_address() == 0x3D, "RELEASE_PROTECTION → 0x3D");
}

// ============================================================================
// Test 5: expected_response_length() for READ commands
// ============================================================================
void test_05_expected_response_length_read(TestStats &stats) {
  std::cout << "\nTEST 5: expected_response_length() for READ commands" << std::endl;

  // 6-byte responses
  Command cmd1(Commandtype::READ_ENCODER_CARRY);
  stats.check(cmd1.expected_response_length() == 6, "READ_ENCODER_CARRY → 6 bytes");

  Command cmd2(Commandtype::READ_ENCODER_ADDITION);
  stats.check(cmd2.expected_response_length() == 6, "READ_ENCODER_ADDITION → 6 bytes");

  // 4-byte responses
  Command cmd3(Commandtype::READ_PULSE_COUNT);
  stats.check(cmd3.expected_response_length() == 4, "READ_PULSE_COUNT → 4 bytes");

  Command cmd4(Commandtype::READ_ANGLE_ERROR);
  stats.check(cmd4.expected_response_length() == 4, "READ_ANGLE_ERROR → 4 bytes");

  // 2-byte responses
  Command cmd5(Commandtype::READ_CURRENT_SPEED);
  stats.check(cmd5.expected_response_length() == 2, "READ_CURRENT_SPEED → 2 bytes");

  Command cmd6(Commandtype::READ_MOTOR_STATUS);
  stats.check(cmd6.expected_response_length() == 2, "READ_MOTOR_STATUS → 2 bytes");

  Command cmd7(Commandtype::READ_PROTECTION_STATUS);
  stats.check(cmd7.expected_response_length() == 2, "READ_PROTECTION_STATUS → 2 bytes");

  Command cmd8(Commandtype::READ_IO_STATUS);
  stats.check(cmd8.expected_response_length() == 2, "READ_IO_STATUS → 2 bytes");

  Command cmd9(Commandtype::READ_ZERO_RETURN_STATUS);
  stats.check(cmd9.expected_response_length() == 2, "READ_ZERO_RETURN_STATUS → 2 bytes");
}

// ============================================================================
// Test 6: expected_response_length() for WRITE commands (always 0)
// ============================================================================
void test_06_expected_response_length_write(TestStats &stats) {
  std::cout << "\nTEST 6: expected_response_length() returns 0 for WRITE commands" << std::endl;

  Command cmd1(Commandtype::ENABLE_MOTOR);
  stats.check(cmd1.expected_response_length() == 0, "ENABLE_MOTOR → 0 bytes");

  Command cmd2(Commandtype::MOVE_POSITION_MODE_2);
  stats.check(cmd2.expected_response_length() == 0, "MOVE_POSITION_MODE_2 → 0 bytes");

  Command cmd3(Commandtype::MOVE_SPEED_MODE);
  stats.check(cmd3.expected_response_length() == 0, "MOVE_SPEED_MODE → 0 bytes");

  Command cmd4(Commandtype::EMERGENCY_STOP);
  stats.check(cmd4.expected_response_length() == 0, "EMERGENCY_STOP → 0 bytes");

  Command cmd5(Commandtype::SET_WORKING_CURRENT);
  stats.check(cmd5.expected_response_length() == 0, "SET_WORKING_CURRENT → 0 bytes");

  Command cmd6(Commandtype::SET_HOMING_PARAMETERS);
  stats.check(cmd6.expected_response_length() == 0, "SET_HOMING_PARAMETERS → 0 bytes");
}

// ============================================================================
// Test 7: Command constructor and payload handling
// ============================================================================
void test_07_command_constructor(TestStats &stats) {
  std::cout << "\nTEST 7: Command constructor and payload handling" << std::endl;

  // Constructor without payload
  Command cmd1(Commandtype::READ_ENCODER_CARRY);
  stats.check(cmd1.command_type == Commandtype::READ_ENCODER_CARRY, "Constructor sets command_type");
  stats.check(cmd1.payload.empty(), "Default payload is empty");
  stats.check(cmd1.response.empty(), "Default response is empty");

  // Constructor with payload
  std::vector<uint8_t> test_payload = {0x01, 0x02, 0x03};
  Command cmd2(Commandtype::ENABLE_MOTOR, test_payload);
  stats.check(cmd2.command_type == Commandtype::ENABLE_MOTOR, "Constructor with payload sets command_type");
  stats.check(cmd2.payload.size() == 3, "Payload size is 3");
  stats.check(cmd2.payload[0] == 0x01 && cmd2.payload[1] == 0x02 && cmd2.payload[2] == 0x03, "Payload data is correct");

  // Verify command_type is const (compile-time check, but we can test immutability conceptually)
  // cmd2.command_type = Commandtype::EMERGENCY_STOP; // Would not compile - this is good!
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "Command Structure Unit Tests" << std::endl;
  std::cout << "========================================" << std::endl;

  TestStats stats;

  test_01_function_code_read_commands(stats);
  stats.print_summary("TEST 1");

  test_02_function_code_simple_write(stats);
  stats.print_summary("TEST 2");

  test_03_function_code_complex_write(stats);
  stats.print_summary("TEST 3");

  test_04_register_address(stats);
  stats.print_summary("TEST 4");

  test_05_expected_response_length_read(stats);
  stats.print_summary("TEST 5");

  test_06_expected_response_length_write(stats);
  stats.print_summary("TEST 6");

  test_07_command_constructor(stats);
  stats.print_summary("TEST 7");

  std::cout << "\n========================================" << std::endl;
  std::cout << "✅ All Command Tests Passed!" << std::endl;
  std::cout << "========================================" << std::endl;

  return 0;
}
