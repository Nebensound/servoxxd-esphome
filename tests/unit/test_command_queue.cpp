/**
 * @file test_command_queue.cpp
 * @brief Unit tests for CommandQueue class
 *
 * Tests verify:
 * - Basic enqueue and execution
 * - FIFO order preservation
 * - Error handling
 * - Clear operation (emergency stop)
 */

#include <cstdint>
#include <cassert>
#include <iostream>
#include <vector>

// Define millis() mock BEFORE including any ESPHome headers
static uint32_t test_millis = 0;
uint32_t millis() { return test_millis; }

#include "servoxxd_command_queue.h"
#include "servoxxd_transport.h"
#include "servoxxd_commands.h"

using namespace esphome::servoxxd;

// Mock Transport for testing
class MockTransport : public ITransport {
 public:
  MockTransport() : execute_count_(0), read_count_(0), busy_(false) {}

  // Track calls
  int execute_count_;
  int read_count_;
  std::vector<Commandtype> executed_commands_;
  std::vector<Commandtype> read_commands_;
  bool busy_;
  Commandtype last_executed_commandtype_;

  // Callbacks
  std::function<void(const Command &)> response_callback_;
  std::function<void(const Command &, ErrorCode)> error_callback_;

  // Override ITransport interface
  Result execute_command(const Command &cmd) override {
    execute_count_++;
    executed_commands_.push_back(cmd.command_type);
    last_executed_commandtype_ = cmd.command_type;
    if (cmd.expected_response_length() > 0) {
      read_count_++;
      read_commands_.push_back(cmd.command_type);
    }
    busy_ = true;
    Result r;
    r.success = true;
    r.error_code = ErrorCode::OK;
    return r;
  }

  Commandtype get_last_executed_commandtype() const { return last_executed_commandtype_; }

  bool is_busy() const override { return busy_; }

  void update() override {}

  void set_response_callback(std::function<void(const Command &)> cb) override { response_callback_ = cb; }

  void set_error_callback(std::function<void(const Command &, ErrorCode)> cb) override { error_callback_ = cb; }

  // Helper methods for tests
  void simulate_response(const Command &cmd) {
    busy_ = false;
    if (response_callback_) {
      response_callback_(cmd);
    }
  }

  void simulate_error(const Command &cmd, ErrorCode error) {
    busy_ = false;
    if (error_callback_) {
      error_callback_(cmd, error);
    }
  }
};

// Test helpers
struct TestResult {
  bool callback_called = false;
  bool success = false;
  std::vector<uint8_t> data;
};

// ============================================================================
// TEST 1: Basic Enqueue and Execution
// ============================================================================
void test_basic_enqueue() {
  std::cout << "TEST 1: Basic enqueue and execution..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  // Initially empty
  assert(queue.is_empty());
  assert(queue.size() == 0);

  TestResult result;
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), [&](bool success, const Command &cmd) {
    result.callback_called = true;
    result.success = success;
    result.data = cmd.response;
  });

  // After enqueue: size should be 1
  assert(queue.size() == 1);
  assert(!queue.is_empty());

  // Trigger execution
  queue.update();

  // Should have called read_command
  assert(transport.read_count_ == 1);
  assert(transport.read_commands_[0] == Commandtype::READ_ENCODER_CARRY);

  // Simulate response via CommandQueue (not transport callback, since that's not set up yet)
  std::vector<uint8_t> response_data = {0xAA, 0xBB, 0xCC};
  {
    Command cmd(Commandtype::READ_ENCODER_CARRY);
    cmd.response = response_data;
    queue.on_response(cmd);
  }

  // Callback should be invoked
  assert(result.callback_called);
  assert(result.success);
  assert(result.data == response_data);
  assert(queue.is_empty());

  std::cout << "  ✓ Basic enqueue works" << std::endl;
  std::cout << "  ✓ Command execution triggered" << std::endl;
  std::cout << "  ✓ Response handled correctly" << std::endl;
}

// ============================================================================
// TEST 2: FIFO Order
// ============================================================================
void test_fifo_order() {
  std::cout << "\nTEST 2: FIFO order preservation..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  std::vector<int> callback_order;

  // Enqueue 3 commands
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY),
                [&](bool, const Command &cmd) { callback_order.push_back(1); });
  queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED),
                [&](bool, const Command &cmd) { callback_order.push_back(2); });
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS),
                [&](bool, const Command &cmd) { callback_order.push_back(3); });

  assert(queue.size() == 3);

  // Execute and respond to first command
  queue.update();
  assert(transport.read_commands_[0] == Commandtype::READ_ENCODER_CARRY);
  queue.on_response(Command(Commandtype::READ_ENCODER_CARRY));
  assert(callback_order.size() == 1 && callback_order[0] == 1);
  assert(queue.size() == 2);

  // Execute and respond to second command
  queue.update();
  assert(transport.read_commands_[1] == Commandtype::READ_CURRENT_SPEED);
  queue.on_response(Command(Commandtype::READ_CURRENT_SPEED));
  assert(callback_order.size() == 2 && callback_order[1] == 2);
  assert(queue.size() == 1);

  // Execute and respond to third command
  queue.update();
  assert(transport.read_commands_[2] == Commandtype::READ_MOTOR_STATUS);
  queue.on_response(Command(Commandtype::READ_MOTOR_STATUS));
  assert(callback_order.size() == 3 && callback_order[2] == 3);
  assert(queue.is_empty());

  std::cout << "  ✓ FIFO order preserved" << std::endl;
  std::cout << "  ✓ Queue size updates correctly" << std::endl;
}

// ============================================================================
// TEST 3: Error Handling
// ============================================================================
void test_error_handling() {
  std::cout << "\nTEST 3: Error handling..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  TestResult result;
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), [&](bool success, const Command &cmd) {
    result.callback_called = true;
    result.success = success;
    result.data = cmd.response;
  });

  queue.update();

  // Simulate error via CommandQueue
  queue.on_error(Command(Commandtype::READ_ENCODER_CARRY), ErrorCode::TIMEOUT);

  // Callback should be invoked with success=false
  assert(result.callback_called);
  assert(!result.success);
  assert(result.data.empty());
  assert(queue.is_empty());

  std::cout << "  ✓ Error handled correctly" << std::endl;
  std::cout << "  ✓ Callback invoked with success=false" << std::endl;
}

// ============================================================================
// TEST 4: Queue Clear (Emergency Stop)
// ============================================================================
void test_clear() {
  std::cout << "\nTEST 4: Queue clear (emergency stop)..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  int callbacks_invoked = 0;

  // Enqueue 3 commands
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), [&](bool success, const Command &cmd) {
    callbacks_invoked++;
    assert(!success);  // Should be called with failure
  });
  queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED), [&](bool success, const Command &cmd) {
    callbacks_invoked++;
    assert(!success);
  });
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), [&](bool success, const Command &cmd) {
    callbacks_invoked++;
    assert(!success);
  });

  // First command is already being executed (enqueue_read calls execute_next)
  // So we have: 1 executing + 2 pending = 3 total
  assert(queue.size() == 3);

  // Clear queue - should clear only the 2 pending commands
  queue.clear();

  std::cout << "  callbacks_invoked = " << callbacks_invoked << std::endl;

  // Only 2 callbacks should be invoked (pending commands)
  // The executing command is NOT cleared
  assert(callbacks_invoked == 2);
  assert(queue.size() == 1);  // 1 executing command remains

  std::cout << "  ✓ Queue cleared (pending commands only)" << std::endl;
  std::cout << "  ✓ Executing command preserved" << std::endl;
  std::cout << "  ✓ Pending callbacks invoked with failure" << std::endl;
}

// ============================================================================
// TEST 5: Deduplication of Read Commands
// ============================================================================
void test_deduplication() {
  std::cout << "\nTEST 5: Deduplication of read commands..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  int callback1_count = 0;
  int callback2_count = 0;
  int callback3_count = 0;
  std::vector<uint8_t> received_data1;
  std::vector<uint8_t> received_data2;
  std::vector<uint8_t> received_data3;

  // Enqueue a different command first (will become EXECUTING)
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), [](bool, const Command &cmd) {});

  assert(queue.size() == 1);

  // Now enqueue two identical commands (both will be PENDING) with deduplication enabled
  queue.enqueue(
      Command(Commandtype::READ_ENCODER_CARRY),
      [&](bool, const Command &cmd) {
        callback1_count++;
        received_data1 = cmd.response;
      },
      Priority::NORMAL, 0, true);  // Force deduplication

  assert(queue.size() == 2);

  // Enqueue duplicate - should deduplicate with the PENDING one
  queue.enqueue(
      Command(Commandtype::READ_ENCODER_CARRY),
      [&](bool, const Command &cmd) {
        callback2_count++;
        received_data2 = cmd.response;
      },
      Priority::NORMAL, 0, true);  // Force deduplication

  // Should still be size 2 (first command + deduplicated second/third)
  assert(queue.size() == 2);

  // Add a third duplicate
  queue.enqueue(
      Command(Commandtype::READ_ENCODER_CARRY),
      [&](bool, const Command &cmd) {
        callback3_count++;
        received_data3 = cmd.response;
      },
      Priority::NORMAL, 0, true);  // Force deduplication

  assert(queue.size() == 2);  // Still 2

  // Complete first command
  queue.update();
  queue.on_response(Command(Commandtype::READ_MOTOR_STATUS));

  // Now execute the merged command
  queue.update();
  std::vector<uint8_t> response_data = {0x12, 0x34, 0x56, 0x78};
  {
    Command cmd(Commandtype::READ_ENCODER_CARRY);
    cmd.response = response_data;
    queue.on_response(cmd);
  }

  assert(callback1_count == 1);
  assert(callback2_count == 1);
  assert(callback3_count == 1);
  assert(received_data1 == response_data);
  assert(received_data2 == response_data);
  assert(received_data3 == response_data);
  assert(queue.is_empty());

  std::cout << "  ✓ Duplicate read commands merged" << std::endl;
  std::cout << "  ✓ All callbacks invoked exactly once" << std::endl;
}

// ============================================================================
// TEST 6: No Deduplication for Different Commands
// ============================================================================
void test_no_deduplication_different() {
  std::cout << "\nTEST 6: No deduplication for different commands..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  // Enqueue two different read commands
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), [](bool, const Command &cmd) {});
  queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED), [](bool, const Command &cmd) {});

  assert(queue.size() == 2);

  std::cout << "  ✓ Different commands NOT deduplicated" << std::endl;
}

// ============================================================================
// TEST 7: No Deduplication for Write Commands
// ============================================================================
void test_no_deduplication_writes() {
  std::cout << "\nTEST 7: No deduplication for write commands..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  std::vector<uint8_t> data1 = {0x12, 0x34};
  std::vector<uint8_t> data2 = {0x12, 0x34};  // Same data

  // Enqueue two identical write commands
  queue.enqueue(Command(Commandtype::SET_ZERO, data1), [](bool, const Command &cmd) {});
  queue.enqueue(Command(Commandtype::SET_ZERO, data2), [](bool, const Command &cmd) {});

  // Should NOT deduplicate (writes must execute in order)
  assert(queue.size() == 2);

  std::cout << "  ✓ Write commands NOT deduplicated" << std::endl;
}

// ============================================================================
// TEST 8: Priority Queue (Front Insertion)
// ============================================================================
void test_priority_queue() {
  std::cout << "\nTEST 8: Priority queue handling..." << std::endl;
  test_millis = 0;

  MockTransport transport;
  CommandQueue queue(&transport);

  std::vector<int> callback_order;

  // Enqueue normal commands
  queue.enqueue(
      Command(Commandtype::READ_ENCODER_CARRY), [&](bool, const Command &cmd) { callback_order.push_back(1); },
      Priority::NORMAL);

  queue.enqueue(
      Command(Commandtype::READ_CURRENT_SPEED), [&](bool, const Command &cmd) { callback_order.push_back(2); },
      Priority::NORMAL);

  // Enqueue priority command (use READ_MOTOR_STATUS as priority, not EMERGENCY_STOP)
  queue.enqueue(
      Command(Commandtype::READ_MOTOR_STATUS), [&](bool, const Command &cmd) { callback_order.push_back(3); },
      Priority::NORMAL);

  // Enqueue a CRITICAL priority command
  queue.enqueue(
      Command(Commandtype::RELEASE_PROTECTION), [&](bool, const Command &cmd) { callback_order.push_back(99); },
      Priority::CRITICAL);  // Critical priority

  // Should be: 1 EXECUTING + 3 PENDING = 4 total
  // (READ_ENCODER_CARRY executing, READ_CURRENT_SPEED + READ_MOTOR_STATUS + RELEASE_PROTECTION pending)
  assert(queue.size() == 4);

  // First command starts executing
  queue.update();
  queue.on_response(Command(Commandtype::READ_ENCODER_CARRY));
  assert(callback_order[0] == 1);

  assert(transport.get_last_executed_commandtype() == Commandtype::RELEASE_PROTECTION);
  queue.on_response(Command(Commandtype::RELEASE_PROTECTION));
  assert(callback_order[1] == 99);

  // Normal commands retain FIFO order after CRITICAL.
  queue.update();
  queue.on_response(Command(Commandtype::READ_CURRENT_SPEED));
  assert(callback_order[2] == 2);

  // Then READ_MOTOR_STATUS
  queue.update();
  queue.on_response(Command(Commandtype::READ_MOTOR_STATUS));
  assert(callback_order[3] == 3);

  std::cout << "  ✓ CRITICAL precedes pending NORMAL commands at millis 0" << std::endl;
  std::cout << "  ✓ Executing command not interrupted" << std::endl;
}

// ============================================================================
// TEST 9: Timeout Detection
// ============================================================================
void test_timeout_detection() {
  std::cout << "\nTEST 9: Timeout detection..." << std::endl;

  test_millis = 0;  // Reset time

  MockTransport transport;
  CommandQueue queue(&transport, 500);  // 500ms timeout

  bool timeout_detected = false;
  std::vector<uint8_t> timeout_data;

  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), [&](bool success, const Command &cmd) {
    timeout_detected = !success;
    timeout_data = cmd.response;
  });

  // Command starts executing
  queue.update();
  assert(transport.read_count_ == 1);
  assert(queue.size() == 1);

  // Advance time but not past timeout
  test_millis = 400;
  queue.update();
  assert(!timeout_detected);  // Should not timeout yet
  assert(queue.size() == 1);

  // Advance time past timeout
  test_millis = 600;  // 600 > 500 timeout
  queue.update();

  // Command should timeout
  assert(timeout_detected);
  assert(timeout_data.empty());
  assert(queue.is_empty());

  std::cout << "  ✓ Timeout detected at correct time" << std::endl;
  std::cout << "  ✓ Callback invoked with success=false" << std::endl;

  test_millis = 0;  // Reset for other tests
}

// ============================================================================
// TEST 10: Timeout Allows Next Command
// ============================================================================
void test_timeout_recovery() {
  std::cout << "\nTEST 10: Timeout recovery and queue continuation..." << std::endl;

  test_millis = 0;

  MockTransport transport;
  CommandQueue queue(&transport, 500);

  bool first_timeout = false;
  bool second_success = false;

  // Enqueue two commands
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS),
                [&](bool success, const Command &cmd) { first_timeout = !success; });

  queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED),
                [&](bool success, const Command &cmd) { second_success = success; });

  assert(queue.size() == 2);

  // First command executes
  queue.update();
  assert(transport.read_count_ == 1);

  // Advance past timeout for first command
  test_millis = 600;
  queue.update();

  // First command should timeout
  assert(first_timeout);
  assert(queue.size() == 1);  // Second command still in queue

  // Second command should execute automatically
  assert(transport.read_count_ == 2);  // Second command sent

  // Respond to second command
  {
    Command cmd(Commandtype::READ_CURRENT_SPEED);
    cmd.response = {0xAA, 0xBB};
    queue.on_response(cmd);
  }

  assert(second_success);
  assert(queue.is_empty());

  std::cout << "  ✓ Timeout clears execution guard" << std::endl;
  std::cout << "  ✓ Next command executes automatically" << std::endl;

  test_millis = 0;
}

// ============================================================================
// TEST 11: Multiple Callbacks on Same Command (Deduplication)
// ============================================================================
void test_multiple_callbacks() {
  std::cout << "\nTEST 11: Multiple callbacks on deduplicated command..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  int callback_count = 0;
  std::vector<std::vector<uint8_t>> received_data;

  // Enqueue a blocker command first (will be EXECUTING)
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), [](bool, const Command &cmd) {});

  // Now enqueue same command 3 times (all will be PENDING and should be deduplicated)
  // NOTE: Deduplication is only automatic for BACKGROUND/IDLE priority, or must be forced with deduplicate=true
  for (int i = 0; i < 3; i++) {
    queue.enqueue(
        Command(Commandtype::READ_ENCODER_CARRY),
        [&](bool, const Command &cmd) {
          callback_count++;
          received_data.push_back(cmd.response);
        },
        Priority::NORMAL, 0, true);  // Force deduplication
  }

  // Should be: 1 EXECUTING (READ_MOTOR_STATUS) + 1 PENDING (deduplicated READ_ENCODER_CARRY) = 2
  assert(queue.size() == 2);

  // Complete blocker command
  queue.update();
  queue.on_response(Command(Commandtype::READ_MOTOR_STATUS));

  // Execute deduplicated command
  queue.update();
  std::vector<uint8_t> response = {0xFF, 0xFF};
  {
    Command cmd(Commandtype::READ_ENCODER_CARRY);
    cmd.response = response;
    queue.on_response(cmd);
  }

  assert(callback_count == 3);
  assert(received_data.size() == 3);
  for (const auto &data : received_data) {
    assert(data == response);
  }

  std::cout << "  ✓ All callbacks invoked exactly once" << std::endl;
  std::cout << "  ✓ Deduplicated command receives correct data" << std::endl;
}

// ============================================================================
// TEST 12: Update on Empty Queue (No Crash)
// ============================================================================
void test_update_empty_queue() {
  std::cout << "\nTEST 12: Update on empty queue..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  assert(queue.is_empty());

  // Should not crash or cause issues
  queue.update();
  queue.update();
  queue.update();

  assert(queue.is_empty());
  assert(transport.read_count_ == 0);

  std::cout << "  ✓ No crash on empty update" << std::endl;
}

// ============================================================================
// TEST 13: Error After Timeout (Ignore Late Response)
// ============================================================================
void test_late_response_ignored() {
  std::cout << "\nTEST 13: Late response ignored after timeout..." << std::endl;

  test_millis = 0;

  MockTransport transport;
  CommandQueue queue(&transport, 500);

  bool callback_called = false;
  int callback_count = 0;

  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), [&](bool success, const Command &cmd) {
    callback_count++;
    callback_called = !success;  // Timeout = !success
  });

  queue.update();

  // Trigger timeout
  test_millis = 600;
  queue.update();

  assert(callback_called);
  assert(callback_count == 1);
  assert(queue.is_empty());

  // Late response arrives (should be ignored)
  {
    Command cmd(Commandtype::READ_MOTOR_STATUS);
    cmd.response = {0x11, 0x22};
    queue.on_response(cmd);
  }

  // Callback should NOT be called again
  assert(callback_count == 1);  // Still 1, not 2

  std::cout << "  ✓ Late response ignored" << std::endl;
  std::cout << "  ✓ Callback not invoked twice" << std::endl;

  test_millis = 0;
}

// ============================================================================
// TEST 14: Mixed Read and Write Commands
// ============================================================================
void test_mixed_read_write() {
  std::cout << "\nTEST 14: Mixed read and write commands..." << std::endl;

  MockTransport transport;
  CommandQueue queue(&transport);

  std::vector<int> order;

  // Mix of reads and writes
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), [&](bool, const Command &cmd) { order.push_back(1); });

  std::vector<uint8_t> data = {0x00, 0x00};
  queue.enqueue(Command(Commandtype::SET_ZERO, data), [&](bool, const Command &cmd) { order.push_back(2); });

  queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED), [&](bool, const Command &cmd) { order.push_back(3); });

  assert(queue.size() == 3);

  // Execute in FIFO order
  queue.update();
  queue.on_response(Command(Commandtype::READ_ENCODER_CARRY));
  assert(order[0] == 1);

  queue.update();
  queue.on_response(Command(Commandtype::SET_ZERO));
  assert(order[1] == 2);

  queue.update();
  queue.on_response(Command(Commandtype::READ_CURRENT_SPEED));
  assert(order[2] == 3);

  std::cout << "  ✓ Mixed commands execute in order" << std::endl;
}

void test_coalesced_completion_outcomes() {
  enum class Outcome { SUCCESS, ERROR, TIMEOUT, CLEAR };
  for (auto priority : {Priority::NORMAL, Priority::BACKGROUND, Priority::IDLE}) {
    for (auto outcome : {Outcome::SUCCESS, Outcome::ERROR, Outcome::TIMEOUT, Outcome::CLEAR}) {
      for (int requesters : {0, 1, 3}) {
        test_millis = 0;
        MockTransport transport;
        CommandQueue queue(&transport, 50);
        int counts[3] = {};
        bool expect_success = outcome == Outcome::SUCCESS;
        Command response(Commandtype::READ_ENCODER_CARRY);
        response.response = {0x12, 0x34};
        std::optional<bool> dedup = priority == Priority::NORMAL ? std::optional<bool>(true) : std::nullopt;

        queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
        queue.enqueue(response, nullptr, priority, 0, dedup);
        for (int i = 0; i < requesters; ++i) {
          queue.enqueue(
              response,
              [&, i](bool success, const Command &cmd) {
                ++counts[i];
                assert(success == expect_success);
                assert(cmd.command_type == response.command_type);
                assert(cmd.response == (expect_success ? response.response : std::vector<uint8_t>{}));
              },
              priority, 0, dedup);
          queue.enqueue(response, nullptr, priority, 0, dedup);
        }
        queue.enqueue(response, nullptr, priority, 0, dedup);
        assert(queue.size() == 2);
        queue.update();
        assert(transport.execute_count_ == 1);

        if (outcome == Outcome::CLEAR) {
          queue.clear();
          assert(queue.size() == 1);
          transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
        } else {
          transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
          assert(transport.execute_count_ == 2);
          queue.update();
          assert(transport.execute_count_ == 2);
          if (outcome == Outcome::SUCCESS) {
            transport.simulate_response(response);
          } else if (outcome == Outcome::ERROR) {
            transport.simulate_error(response, ErrorCode::TIMEOUT);
          } else {
            test_millis = 50;
            queue.update();
            assert(counts[0] == 0);
            test_millis = 51;
            queue.update();
          }
        }
        assert(queue.is_empty());
        transport.simulate_response(response);
        transport.simulate_error(response, ErrorCode::TIMEOUT);
        queue.clear();
        queue.update();
        for (int i = 0; i < 3; ++i) {
          assert(counts[i] == (i < requesters ? 1 : 0));
        }
        assert(transport.execute_count_ == (outcome == Outcome::CLEAR ? 1 : 2));
      }
    }
  }
}

void test_pending_only_deduplication() {
  test_millis = 0;
  MockTransport transport;
  CommandQueue queue(&transport);
  int callbacks = 0;
  Command read(Commandtype::READ_ENCODER_CARRY);
  auto callback = [&](bool success, const Command &) {
    assert(success);
    ++callbacks;
  };
  queue.enqueue(read, callback, Priority::BACKGROUND);
  queue.enqueue(read, callback, Priority::BACKGROUND);
  queue.enqueue(read, callback, Priority::BACKGROUND);
  assert(queue.size() == 2);
  assert(transport.execute_count_ == 1);
  transport.simulate_response(read);
  assert(callbacks == 1);
  assert(transport.execute_count_ == 2);
  transport.simulate_response(read);
  assert(callbacks == 3);
  assert(queue.is_empty());

  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
  queue.enqueue(Command(Commandtype::SET_ZERO, {1}), nullptr, Priority::BACKGROUND);
  queue.enqueue(Command(Commandtype::SET_ZERO, {2}), nullptr, Priority::BACKGROUND);
  queue.enqueue(read, nullptr, Priority::BACKGROUND, 0, false);
  queue.enqueue(read, nullptr, Priority::BACKGROUND, 0, false);
  assert(queue.size() == 5);
  queue.clear();
  transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
  assert(queue.is_empty());
}

void test_delayed_coalesced_completion() {
  for (uint32_t start : {0U, UINT32_MAX - 49, UINT32_MAX - 99}) {
    for (bool error : {false, true}) {
      for (bool following : {false, true}) {
        test_millis = start;
        MockTransport transport;
        CommandQueue queue(&transport);
        int callbacks = 0;
        Command read(Commandtype::READ_ENCODER_CARRY);
        read.response = {7, 8};
        queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
        for (int i = 0; i < 3; ++i) {
          queue.enqueue(
              read,
              [&](bool success, const Command &cmd) {
                ++callbacks;
                assert(success == !error);
                assert(cmd.response == (error ? std::vector<uint8_t>{} : read.response));
                queue.update();
                assert(transport.execute_count_ == 2);
              },
              Priority::BACKGROUND, 100);
        }
        transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
        if (following) {
          queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED), nullptr);
        }
        if (error) {
          transport.simulate_error(read, ErrorCode::TIMEOUT);
        } else {
          transport.simulate_response(read);
        }
        if (!following) {
          queue.clear();
        }
        assert(callbacks == (error ? 3 : 0));
        assert(transport.execute_count_ == 2);
        test_millis = start + 99;
        queue.update();
        assert(callbacks == (error ? 3 : 0));
        assert(transport.execute_count_ == 2);
        test_millis = start + 100;
        queue.update();
        assert(callbacks == 3);
        assert(transport.execute_count_ == (following ? 3 : 2));
        if (following) {
          transport.simulate_response(Command(Commandtype::READ_CURRENT_SPEED));
        }
        queue.update();
        assert(callbacks == 3);
        assert(queue.is_empty());
      }
    }
  }
}

void test_reentrant_completion() {
  for (int outcome = 0; outcome < 4; ++outcome) {
    test_millis = 0;
    MockTransport transport;
    CommandQueue queue(&transport, 50);
    int first = 0, second = 0, cancelled = 0, fresh = 0;
    Command read(Commandtype::READ_ENCODER_CARRY);
    queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
    auto delay = outcome == 3 ? 10U : 0U;
    queue.enqueue(
        read,
        [&](bool success, const Command &) {
          ++first;
          assert(success == (outcome == 0 || outcome == 3));
          queue.clear();
          queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED), [&](bool ok, const Command &) {
            assert(ok);
            ++fresh;
          });
          queue.update();
          // Reentrant late events must not re-notify the completed command.
          queue.on_response(read);
          queue.on_error(read, ErrorCode::TIMEOUT);
          assert(transport.execute_count_ == 2);
          assert(second == 0);
        },
        Priority::NORMAL, delay, true);
    queue.enqueue(
        read,
        [&](bool success, const Command &) {
          ++second;
          assert(success == (outcome == 0 || outcome == 3));
          assert(first == 1);
          assert(transport.execute_count_ == 2);
        },
        Priority::NORMAL, delay, true);
    queue.enqueue(Command(Commandtype::SET_ZERO), [&](bool success, const Command &) {
      assert(!success);
      ++cancelled;
    });
    transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
    if (outcome == 1) {
      transport.simulate_error(read, ErrorCode::TIMEOUT);
    } else if (outcome == 2) {
      test_millis = 51;
      queue.update();
    } else {
      transport.simulate_response(read);
      if (outcome == 3) {
        assert(first == 0 && second == 0);
        test_millis = 10;
        queue.update();
      }
    }
    assert(first == 1 && second == 1 && cancelled == 1);
    assert(transport.execute_count_ == 3);
    transport.simulate_response(Command(Commandtype::READ_CURRENT_SPEED));
    assert(fresh == 1);
    assert(queue.is_empty());
  }
}

void test_reentrant_clear_and_delayed_last_callback() {
  for (bool complete_before_clear : {false, true}) {
    test_millis = 0;
    MockTransport transport;
    CommandQueue queue(&transport);
    int completed = 0, cancelled = 0, fresh = 0;
    queue.enqueue(
        Command(Commandtype::READ_MOTOR_STATUS),
        [&](bool success, const Command &) {
          assert(success);
          ++completed;
        },
        Priority::NORMAL, 10);
    for (int i = 0; i < 3; ++i) {
      queue.enqueue(
          Command(Commandtype::READ_ENCODER_CARRY),
          [&](bool success, const Command &) {
            assert(!success);
            ++cancelled;
            queue.clear();
            queue.update();
            assert(transport.execute_count_ == 1);
          },
          Priority::BACKGROUND);
    }
    queue.enqueue(Command(Commandtype::SET_ZERO), [&](bool success, const Command &) {
      assert(!success);
      ++cancelled;
      queue.enqueue(Command(Commandtype::READ_CURRENT_SPEED), [&](bool ok, const Command &) {
        assert(ok);
        ++fresh;
      });
      queue.update();
      assert(transport.execute_count_ == 1);
    });
    if (complete_before_clear) {
      transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
    }
    queue.clear();
    assert(cancelled == 4 && completed == 0);
    if (!complete_before_clear) {
      assert(queue.size() == 2);
      transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
    }
    assert(queue.size() == 1);
    test_millis = 10;
    queue.update();
    assert(completed == 1);
    assert(transport.execute_count_ == 2);
    transport.simulate_response(Command(Commandtype::READ_CURRENT_SPEED));
    assert(fresh == 1);
    queue.update();
    assert(completed == 1 && cancelled == 4);
  }
}

void test_deduplication_retains_priority_age_and_latest_delay() {
  test_millis = 0;
  MockTransport transport;
  CommandQueue queue(&transport, 100000);
  std::vector<int> order;
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
  auto enqueue = [&](int id, Priority priority, uint32_t delay, bool dedup) {
    queue.enqueue(
        Command(Commandtype::SET_ZERO),
        [&, id](bool success, const Command &) {
          assert(success);
          order.push_back(id);
        },
        priority, delay, dedup);
  };
  enqueue(1, Priority::IDLE, 30, true);
  test_millis = 10000;
  enqueue(2, Priority::BACKGROUND, 20, true);
  enqueue(3, Priority::NORMAL, 0, false);
  transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
  transport.simulate_response(Command(Commandtype::SET_ZERO));
  assert(order.empty());
  test_millis += 20;
  queue.update();
  assert((order == std::vector<int>{1, 2}));
  transport.simulate_response(Command(Commandtype::SET_ZERO));
  assert((order == std::vector<int>{1, 2, 3}));
  assert(queue.is_empty());

  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
  enqueue(4, Priority::NORMAL, 0, false);
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), nullptr, Priority::IDLE);
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), nullptr, Priority::SETUP, 0, true);
  queue.enqueue(Command(Commandtype::READ_ENCODER_CARRY), nullptr, Priority::IDLE);
  transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
  assert(transport.get_last_executed_commandtype() == Commandtype::READ_ENCODER_CARRY);
  transport.simulate_response(Command(Commandtype::READ_ENCODER_CARRY));
  transport.simulate_response(Command(Commandtype::SET_ZERO));
  assert((order == std::vector<int>{1, 2, 3, 4}));
}

void test_strict_priorities_and_fifo() {
  for (uint32_t start : {0U, 1U, UINT32_MAX - 5}) {
    test_millis = start;
    MockTransport transport;
    CommandQueue queue(&transport);
    std::vector<int> order;
    queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
    auto enqueue = [&](int id, Priority priority) {
      queue.enqueue(
          Command(Commandtype::SET_ZERO),
          [&, id](bool success, const Command &) {
            assert(success);
            order.push_back(id);
          },
          priority, 0, false);
    };
    enqueue(1, Priority::NORMAL);
    enqueue(2, Priority::NORMAL);
    enqueue(3, Priority::SETUP);
    enqueue(4, Priority::SETUP);
    enqueue(5, Priority::CRITICAL);
    enqueue(6, Priority::CRITICAL);
    enqueue(7, Priority::BACKGROUND);
    enqueue(8, Priority::IDLE);
    assert(transport.execute_count_ == 1);
    transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
    for (int i = 0; i < 8; ++i) {
      assert(transport.execute_count_ == i + 2);
      queue.update();
      assert(transport.execute_count_ == i + 2);
      transport.simulate_response(Command(Commandtype::SET_ZERO));
    }
    assert((order == std::vector<int>{5, 6, 3, 4, 1, 2, 7, 8}));
    assert(queue.is_empty());
  }
}

void test_priority_aging_and_rollover() {
  for (uint32_t start : {0U, UINT32_MAX - 5000}) {
    for (auto low : {Priority::BACKGROUND, Priority::IDLE}) {
      uint32_t penalty = low == Priority::BACKGROUND ? 10000 : 60000;
      for (uint32_t age : {penalty - 1, penalty, penalty + 1}) {
        test_millis = start;
        MockTransport transport;
        CommandQueue queue(&transport, 100000);
        std::vector<int> order;
        queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
        auto enqueue = [&](int id, Priority priority) {
          queue.enqueue(
              Command(Commandtype::SET_ZERO),
              [&, id](bool success, const Command &) {
                assert(success);
                order.push_back(id);
              },
              priority, 0, false);
        };
        enqueue(1, low);
        test_millis = start + age;
        enqueue(2, Priority::NORMAL);
        enqueue(3, Priority::NORMAL);
        enqueue(4, Priority::SETUP);
        enqueue(5, Priority::CRITICAL);
        transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
        for (int i = 0; i < 5; ++i) {
          transport.simulate_response(Command(Commandtype::SET_ZERO));
        }
        // At the exact aging boundary, insertion order wins the effective-time tie.
        auto expected = age < penalty ? std::vector<int>{5, 4, 2, 3, 1} : std::vector<int>{5, 4, 1, 2, 3};
        assert(order == expected);
        assert(queue.is_empty());
      }
    }
  }
}

void test_normal_fifo_across_rollover() {
  test_millis = UINT32_MAX - 1;
  MockTransport transport;
  CommandQueue queue(&transport);
  std::vector<int> order;
  queue.enqueue(Command(Commandtype::READ_MOTOR_STATUS), nullptr);
  queue.enqueue(Command(Commandtype::SET_ZERO), [&](bool, const Command &) { order.push_back(1); });
  test_millis = 1;
  queue.enqueue(Command(Commandtype::SET_ZERO), [&](bool, const Command &) { order.push_back(2); });
  transport.simulate_response(Command(Commandtype::READ_MOTOR_STATUS));
  transport.simulate_response(Command(Commandtype::SET_ZERO));
  transport.simulate_response(Command(Commandtype::SET_ZERO));
  assert((order == std::vector<int>{1, 2}));
}

void test_synchronous_transport_completion() {
  class SynchronousTransport : public MockTransport {
   public:
    Result execute_command(const Command &cmd) override {
      assert(!busy_);
      auto result = MockTransport::execute_command(cmd);
      auto payload = cmd.payload;
      simulate_response(cmd);
      assert(cmd.payload == payload);
      return result;
    }
  } transport;
  test_millis = 0;
  CommandQueue queue(&transport);
  int callbacks = 0;
  queue.enqueue(Command(Commandtype::SET_ZERO, {1, 2}), [&](bool success, const Command &) {
    assert(success);
    ++callbacks;
    queue.enqueue(Command(Commandtype::SET_ZERO, {3, 4}), [&](bool ok, const Command &) {
      assert(ok);
      ++callbacks;
    });
    queue.update();
    assert(transport.execute_count_ == 1);
  });
  assert(callbacks == 2);
  assert(transport.execute_count_ == 2);
  assert(queue.is_empty());
}

// ============================================================================
// Main
// ============================================================================
int main() {
  std::cout << "========================================" << std::endl;
  std::cout << "CommandQueue Unit Tests" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << std::endl;

  try {
    test_basic_enqueue();
    test_fifo_order();
    test_error_handling();
    test_clear();
    test_deduplication();
    test_no_deduplication_different();
    test_no_deduplication_writes();
    test_priority_queue();
    test_timeout_detection();
    test_timeout_recovery();
    test_multiple_callbacks();
    test_update_empty_queue();
    test_late_response_ignored();
    test_mixed_read_write();
    test_coalesced_completion_outcomes();
    test_pending_only_deduplication();
    test_delayed_coalesced_completion();
    test_reentrant_completion();
    test_reentrant_clear_and_delayed_last_callback();
    test_deduplication_retains_priority_age_and_latest_delay();
    test_strict_priorities_and_fifo();
    test_priority_aging_and_rollover();
    test_normal_fifo_across_rollover();
    test_synchronous_transport_completion();

    std::cout << "\n========================================" << std::endl;
    std::cout << "✅ All CommandQueue Tests Passed (24/24)!" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
    return 1;
  }
}
