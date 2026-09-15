#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include "servoxxd_commands.h"

namespace esphome {
namespace servoxxd {

enum class ErrorCode {
  OK,
  TIMEOUT,
  PROTOCOL_ERROR,
  DEVICE_ERROR,
  INVALID_RESPONSE,
  BUSY,
  MODBUS_ERROR  // Motor rejected command with Modbus exception
};

struct Result {
  bool success{false};
  ErrorCode error_code{ErrorCode::OK};
};

class ITransport {
 public:
  virtual ~ITransport() = default;

  // Execute command (both read and write) - function code determines operation type
  virtual Result execute_command(const Command &cmd) = 0;

  // Check if transport is busy (waiting for response)
  virtual bool is_busy() const = 0;

  // Process state machine, timeouts, incoming data
  virtual void update() = 0;

  // Register callbacks for upper layers
  virtual void set_response_callback(std::function<void(const Command &)> cb) = 0;
  virtual void set_error_callback(std::function<void(const Command &, ErrorCode)> cb) = 0;
};

}  // namespace servoxxd
}  // namespace esphome
