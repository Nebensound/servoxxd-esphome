#include "servoxxd.h"
#include "servoxxd_modbus.h"
#include "servoxxd_command_factory.h"
#include "esphome/core/hal.h"
#include <cassert>
#include <iostream>

using namespace esphome;
using namespace esphome::servoxxd;

namespace esphome {
extern uint32_t test_millis_value;
}

struct Fixture {
  modbus::ModbusClientDevice device;
  ModbusTransport transport{&device};
  unsigned responses{0};
  unsigned errors{0};
  ErrorCode error{ErrorCode::OK};
  std::vector<uint8_t> payload;

  Fixture() {
    transport.set_response_callback([this](const Command &cmd) {
      responses++;
      payload = cmd.response;
    });
    transport.set_error_callback([this](const Command &, ErrorCode code) {
      errors++;
      error = code;
    });
  }

  void respond(const std::vector<uint8_t> &pdu) { transport.handle_modbus_response(device.last_request, pdu); }
};

void test_read_response() {
  Fixture f;
  assert(f.transport.execute_command(CommandFactory::read_current_speed()).success);
  assert((f.device.last_request == std::vector<uint8_t>{0x04, 0x00, 0x32, 0x00, 0x01}));
  assert(f.transport.is_busy());
  assert(!f.transport.execute_command(CommandFactory::restart()).success);
  f.respond({0x04, 0x02, 0xFF, 0xFE});
  assert(f.responses == 1 && f.errors == 0);
  assert((f.payload == std::vector<uint8_t>{0xFF, 0xFE}));
  assert(!f.transport.is_busy());
}

void test_write_payloads() {
  Fixture f;
  assert(f.transport.execute_command(CommandFactory::enable_motor(true)).success);
  assert((f.device.last_request == std::vector<uint8_t>{0x06, 0x00, 0xF3, 0x00, 0x01}));
  f.respond(f.device.last_request);
  assert(f.responses == 1 && f.payload.empty());

  assert(f.transport.execute_command(CommandFactory::restart()).success);
  assert((f.device.last_request == std::vector<uint8_t>{0x06, 0x00, 0x41, 0x00, 0x01}));
  f.respond(f.device.last_request);

  Command move(Commandtype::MOVE_POSITION_MODE_2, {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0});
  assert(f.transport.execute_command(move).success);
  assert((f.device.last_request ==
          std::vector<uint8_t>{0x10, 0x00, 0xFE, 0x00, 0x04, 0x08, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0}));
  f.respond({0x10, 0x00, 0xFE, 0x00, 0x04});
  assert(f.responses == 3 && f.errors == 0);

  assert(f.transport.execute_command(Command(Commandtype::SET_HOMING_PARAMETERS, {0x12, 0x34, 0x56})).success);
  assert((f.device.last_request == std::vector<uint8_t>{0x10, 0x00, 0x90, 0x00, 0x02, 0x04, 0x00, 0x12, 0x34, 0x56}));
  f.respond({0x10, 0x00, 0x90, 0x00, 0x02});
  assert(f.responses == 4 && f.errors == 0);
}

void test_errors_and_queue_rejection() {
  Fixture f;
  f.device.accept_requests = false;
  for (const auto &cmd : {CommandFactory::read_current_speed(), CommandFactory::restart(),
                          Command(Commandtype::MOVE_POSITION_MODE_2, {0, 0})}) {
    auto result = f.transport.execute_command(cmd);
    assert(!result.success && result.error_code == ErrorCode::BUSY);
    assert(!f.transport.is_busy());
  }
  assert(f.errors == 0 && f.responses == 0);
  f.device.accept_requests = true;
  assert(f.transport.execute_command(CommandFactory::restart()).success);
  f.transport.handle_modbus_error(f.device.last_request, modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
  assert(f.errors == 1 && f.error == ErrorCode::MODBUS_ERROR && !f.transport.is_busy());
  f.transport.handle_modbus_error(f.device.last_request, modbus::ExceptionCode::ILLEGAL_DATA_ADDRESS);
  assert(f.errors == 1);

  assert(f.transport.execute_command(CommandFactory::restart()).success);
  f.respond({0x06, 0x00, 0x41, 0xFF, 0xFF});
  assert(f.errors == 2 && f.error == ErrorCode::MODBUS_ERROR);

  auto malformed = f.transport.execute_command(Command(Commandtype::RESTART, {0, 1, 2}));
  assert(!malformed.success && malformed.error_code == ErrorCode::PROTOCOL_ERROR);
  assert(!f.transport.is_busy());
}

void test_invalid_and_unsolicited_responses() {
  Fixture f;
  f.respond({});
  assert(f.responses == 0 && f.errors == 0);
  for (const auto &response :
       {std::vector<uint8_t>{}, {0x04}, {0x04, 0x02, 0x01}, {0x04, 0x01, 0x00}, {0x06, 0x00, 0x00, 0x00, 0x00}}) {
    assert(f.transport.execute_command(CommandFactory::read_current_speed()).success);
    f.respond(response);
    assert(f.error == ErrorCode::PROTOCOL_ERROR && !f.transport.is_busy());
  }
  assert(f.errors == 5 && f.responses == 0);
  assert(f.transport.execute_command(CommandFactory::restart()).success);
  std::vector<uint8_t> unrelated{0x06, 0x00, 0x42, 0x00, 0x01};
  f.transport.handle_modbus_response(unrelated, unrelated);
  assert(f.transport.is_busy() && f.errors == 5);
  f.respond({0x06, 0x00, 0x41, 0x00, 0x02});
  assert(f.error == ErrorCode::INVALID_RESPONSE && f.errors == 6);

  assert(f.transport.execute_command(Command(Commandtype::MOVE_POSITION_MODE_2, {0, 1, 0, 2})).success);
  f.respond({0x10, 0x00, 0xFE, 0x00, 0x01});
  assert(f.error == ErrorCode::INVALID_RESPONSE && f.errors == 7);
}

void test_timeout_and_response_lifetime() {
  Fixture f;
  test_millis_value = 100;
  f.transport.set_timeout(1000);
  assert(f.transport.execute_command(CommandFactory::read_current_speed()).success);
  test_millis_value = 1100;
  f.transport.update();
  assert(f.transport.is_busy() && f.errors == 0);
  test_millis_value = 1101;
  f.transport.update();
  assert(!f.transport.is_busy() && f.errors == 1 && f.error == ErrorCode::TIMEOUT);
  f.respond({0x04, 0x02, 0, 1});
  assert(f.responses == 0);

  assert(f.transport.execute_command(CommandFactory::read_current_speed()).success);
  {
    std::vector<uint8_t> transient{0x04, 0x02, 0x12, 0x34};
    f.respond(transient);
    transient.assign(4, 0);
  }
  assert((f.payload == std::vector<uint8_t>{0x12, 0x34}));
  test_millis_value = 0;
}

void test_core_callback_bridge() {
  ServoXxd servo;
  const std::vector<uint8_t> request{0x04, 0x00, 0x32, 0x00, 0x01};
  const std::vector<uint8_t> response{0x04, 0x02, 0x00, 0x0A};
  // ESPHome may deliver a callback before setup; the bridge must not dereference a null transport.
  servo.on_response(request, response);
  servo.on_error(request, modbus::ExceptionCode::ILLEGAL_FUNCTION);
}

int main() {
  test_read_response();
  test_write_payloads();
  test_errors_and_queue_rejection();
  test_invalid_and_unsolicited_responses();
  test_timeout_and_response_lifetime();
  test_core_callback_bridge();
  std::cout << "Modbus client regression tests passed\n";
}
