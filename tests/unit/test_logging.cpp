#include "servoxxd.h"
#include "servoxxd_command_factory.h"
#include "servoxxd_stepper_engine.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#ifndef SERVOXXD_TEST_LOGGING
#error "Build this regression with SERVOXXD_TEST_LOGGING and -Werror=format"
#endif

namespace esphome {
extern uint32_t test_millis_value;
}

using namespace esphome::servoxxd;
using esphome::test_log_messages;
using esphome::test_millis_value;

class LoggingServo : public ServoXxd {
 public:
  float get_steps_per_revolution() const override { return 3200.0f; }
};

class LoggingTransport : public ITransport {
 public:
  Result execute_command(const Command &) override { return {true, ErrorCode::OK}; }
  bool is_busy() const override { return false; }
  void update() override {}
  void set_response_callback(std::function<void(const Command &)> cb) override { response_callback = cb; }
  void set_error_callback(std::function<void(const Command &, ErrorCode)>) override {}

  std::function<void(const Command &)> response_callback;
};

void expect_log(const std::string &message) {
  for (const auto &log : test_log_messages) {
    if (log.message.find(message) != std::string::npos)
      return;
  }
  std::cerr << "Missing log: " << message << '\n';
  std::abort();
}

void test_all_log_levels() {
  test_log_messages.clear();
  ESP_LOGE("test", "error %d", 1);
  ESP_LOGW("test", "warn %d", 2);
  ESP_LOGI("test", "info %d", 3);
  ESP_LOGCONFIG("test", "config %d", 4);
  ESP_LOGD("test", "debug %d", 5);
  ESP_LOGV("test", "verbose %d", 6);
  ESP_LOGVV("test", "very verbose %d", 7);
  assert(test_log_messages.size() == 7);
  for (size_t i = 0; i < test_log_messages.size(); ++i) {
    assert(test_log_messages[i].level == static_cast<int>(i + 1));
  }
  expect_log("error 1");
  expect_log("warn 2");
  expect_log("info 3");
  expect_log("config 4");
  expect_log("debug 5");
  expect_log("verbose 6");
  expect_log("very verbose 7");
}

void test_config_and_report_position() {
  LoggingServo servo;
  servo.set_control_mode(ControlMode::SR_CLOSE);
  servo.set_current_pos(Position::from_revolutions(-1.25, &servo));
  servo.set_target_pos(Position::from_revolutions(2.75, &servo));
  test_log_messages.clear();
  servo.dump_config();
  expect_log("Holding Current: 50% of working");
  expect_log("Current Position: -4000 steps (-1.25 rev)");
  expect_log("Target Position: 8800 steps (2.75 rev)");
  expect_log("Position Offset: 0 steps (0.00 rev)");

  for (unsigned encoded = 0; encoded < 9; ++encoded) {
    servo.set_holding_current_percent(static_cast<HoldingCurrentPercent>(encoded));
    test_log_messages.clear();
    servo.dump_config();
    expect_log("Holding Current: " + std::to_string((encoded + 1) * 10) + "% of working");
  }

  test_log_messages.clear();
  servo.report_position(Position::from_revolutions(-1.25, &servo));
  expect_log("current_position to -4000 steps");
  assert(servo.current_position == -4000);

  const uint8_t response[] = {0, 1, 2};
  servo.on_response({}, response);
  expect_log("transport is null - received 3 bytes");
}

void test_position_without_parent() {
  test_log_messages.clear();
  Position position = Position::from_revolutions(-123456789.25, nullptr);
  assert(position.get_steps() == 0);
  expect_log("revs=-123456790, angle_ticks=12288");
}

void test_queue_logging() {
  test_millis_value = 0;
  test_log_messages.clear();
  LoggingTransport transport;
  CommandQueue queue(&transport, UINT32_C(3000000000));
  expect_log("CommandQueue initialized: timeout=3000000000ms");
  queue.enqueue(CommandFactory::read_motor_status(), nullptr);
  test_millis_value = UINT32_C(3000000001);
  queue.update();
  expect_log("timed out after 3000000001ms (timeout=3000000000ms)");

  queue.enqueue(CommandFactory::read_motor_status(), nullptr);
  queue.enqueue(CommandFactory::read_current_speed(), nullptr);
  queue.enqueue(CommandFactory::read_protection_status(), nullptr);
  queue.clear();
  expect_log("Clearing 2 pending commands");
}

void test_engine_timeouts() {
  struct TimeoutCase {
    State state;
    uint32_t elapsed;
    const char *message;
  };
  const TimeoutCase cases[] = {
      {State::SettingUp, 30001, "Setup timeout after 30001 ms"},
      {State::Homing, 600001, "Homing timeout after 600001 ms"},
      {State::Calibrating, 120001, "Calibration timeout after 120001 ms"},
      {State::Stopping, 50001, "Stopping timeout after 50001 ms"},
  };
  for (const auto &test : cases) {
    test_millis_value = 0;
    LoggingServo servo;
    StepperEngine engine(&servo);
    engine.set_state(test.state);
    test_log_messages.clear();
    test_millis_value = test.elapsed;
    engine.update();
    expect_log(test.message);
  }
}

void test_protection_positions() {
  test_millis_value = 0;
  LoggingServo servo;
  servo.set_current_pos(Position::from_revolutions(-1000000.25, &servo));
  servo.set_target_pos(Position::from_revolutions(1000000.75, &servo));
  LoggingTransport transport;
  StepperEngine engine(&servo, &transport);
  engine.set_state(State::Idle);
  test_log_messages.clear();
  engine.poll_protection_status();
  Command response = CommandFactory::read_protection_status();
  response.response = {0, 1};
  transport.response_callback(response);
  expect_log("Current position: -3200000800 steps (-1000000.25 rev), "
             "Target position: 3200002400 steps (1000000.75 rev)");
  assert(engine.get_state() == State::Error);
}

void test_core_setup_timeout() {
  test_millis_value = 0;
  LoggingServo servo;
  servo.setup();
  test_log_messages.clear();
  test_millis_value = 10001;
  servo.loop();
  expect_log("Motor setup timeout after 10000ms");
  assert(servo.is_failed());
}

int main() {
  test_all_log_levels();
  test_config_and_report_position();
  test_position_without_parent();
  test_queue_logging();
  test_engine_timeouts();
  test_protection_positions();
  test_core_setup_timeout();
  std::cout << "Logging format and numeric regression tests passed\n";
}
