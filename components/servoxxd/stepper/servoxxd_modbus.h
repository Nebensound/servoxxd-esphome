#pragma once

#include "esphome/core/component.h"
#include "esphome/components/modbus/modbus.h"
#include "servoxxd_transport.h"
#include "servoxxd_commands.h"
#include <vector>
#include <functional>
#include <queue>
#include <optional>

namespace esphome {
namespace servoxxd {

/**
 * @brief Modbus-RTU implementation of ITransport for ServoXxd communication
 *
 * Implements the ITransport interface using Modbus-RTU protocol over RS485.
 * Commandtype enum values map directly to Modbus register addresses.
 *
 * Protocol details:
 * - Read: Function 0x04 (Read Input Registers)
 * - Write: Function 0x06 (Write Single Register) or 0x10 (Write Multiple Registers)
 * - CRC16-MODBUS for frame integrity
 * - Configurable timeout (default 1000ms)
 *
 * @see docs/specification/02d-layer4-transport.md
 */
class ModbusTransport : public ITransport {
 public:
  /**
   * @brief Construct a new Modbus Transport object
   *
   * @param device ESPHome Modbus device instance
   */
  ModbusTransport(modbus::ModbusDevice *device);

  // ITransport interface implementation
  Result execute_command(const Command &cmd) override;
  bool is_busy() const override;
  void update() override;
  void set_response_callback(std::function<void(const Command &)> cb) override;
  void set_error_callback(std::function<void(const Command &, ErrorCode)> cb) override;

  /**
   * @brief Set command timeout in milliseconds
   * @param timeout_ms Timeout value (default: 1000ms)
   */
  void set_timeout(uint32_t timeout_ms) { timeout_ms_ = timeout_ms; }

  /**
   * @brief Handle Modbus response (read or write)
   * @param data Response data from Modbus device
   */
  void handle_response(const std::vector<uint8_t> &data);

  /**
   * @brief Handle Modbus error response (called when motor returns error frame)
   * @param function_code The function code that caused the error
   * @param exception_code The Modbus exception code (1=illegal function, 2=illegal address, etc.)
   */
  void handle_error_response(uint8_t function_code, uint8_t exception_code);

  /**
   * @brief Check if transport is waiting for a write response
   */
  bool is_waiting_write() const;

  /**
   * @brief Manually reset transport state to IDLE
   * @note Used during setup() when callbacks are not yet active
   */
  void reset_state() { state_ = State::IDLE; }

 private:
  enum class State {
    IDLE,           // No pending operation
    WAITING_WRITE,  // Waiting for write command response
    WAITING_READ    // Waiting for read command response
  };

  modbus::ModbusDevice *device_;
  State state_{State::IDLE};
  std::optional<Command> pending_command_;
  uint32_t timeout_ms_{1000};
  uint32_t timeout_start_ms_{0};

  std::function<void(const Command &)> response_callback_;
  std::function<void(const Command &, ErrorCode)> error_callback_;

  /**
   * @brief Check for timeout and invoke error callback
   */
  bool check_timeout();
};

}  // namespace servoxxd
}  // namespace esphome
