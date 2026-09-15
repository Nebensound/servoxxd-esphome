#include "servoxxd_modbus.h"
#include "servoxxd_command_decoder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace servoxxd {

static const char *const TAG = "servoxxd.modbus";

ModbusTransport::ModbusTransport(modbus::ModbusDevice *device) : device_(device) {}

Result ModbusTransport::execute_command(const Command &cmd) {
  if (state_ != State::IDLE) {
    ESP_LOGW(TAG, "Transport busy, cannot execute command 0x%02X", static_cast<uint8_t>(cmd.command_type));
    return {false, ErrorCode::BUSY};
  }

  uint16_t register_address = cmd.register_address();
  uint8_t function_code = cmd.function_code();

  switch (function_code) {
    case 0x04:  // Read Input Registers
    {
      uint8_t expected_payload_length = cmd.expected_response_length();
      uint16_t register_count = expected_payload_length / 2;  // Modbus registers are 16-bit

      device_->send(function_code, register_address, register_count, 0, nullptr);

      state_ = State::WAITING_READ;
      pending_command_.emplace(cmd);
      timeout_start_ms_ = millis();

      break;
    }

    case 0x06:
    case 0x10: {
      const std::vector<uint8_t> &data = cmd.payload;

      // Pad data if necessary (Modbus registers are 16-bit, BIG-ENDIAN)
      // For single-byte payloads: prepend 0x00 (not append) to maintain big-endian order
      // Example: {16} → {0, 16} (0x0010) NOT {16, 0} (0x1000)
      std::vector<uint8_t> padded;
      if (data.size() % 2 != 0) {
        padded.push_back(0x00);  // High byte first (big-endian)
        padded.insert(padded.end(), data.begin(), data.end());
      } else {
        padded.assign(data.begin(), data.end());
      }

      uint16_t register_count = padded.size() / 2;

      if (function_code == 0x06) {
        if (padded.size() != 2) {
          ESP_LOGE(TAG, "0x06 requires exactly 1 register (2 bytes), got %u", (unsigned) padded.size());
          return {false, ErrorCode::PROTOCOL_ERROR};
        }
        // number_of_entities must be > 0: newer ESPHome refuses an empty PDU for rc = 0
        // (value is otherwise ignored for 0x06 - the two payload bytes are the register value)
        device_->send(0x06, register_address, 1, 2, padded.data());
      } else {
        // 0x10: MUST NOT use rc=0
        device_->send(0x10, register_address, register_count, padded.size(), padded.data());
      }

      state_ = State::WAITING_WRITE;
      pending_command_.emplace(cmd);
      timeout_start_ms_ = millis();

      break;
    }

    default:
      ESP_LOGE(TAG, "Invalid function code 0x%02X for command 0x%02X", function_code,
               static_cast<uint8_t>(cmd.command_type));
      return {false, ErrorCode::PROTOCOL_ERROR};
  }

  return {true, ErrorCode::OK};
}

bool ModbusTransport::is_busy() const { return state_ != State::IDLE; }

bool ModbusTransport::is_waiting_write() const { return state_ == State::WAITING_WRITE; }

void ModbusTransport::update() {
  if (state_ == State::IDLE) {
    return;
  }

  // Check for timeout
  if (check_timeout()) {
    ESP_LOGW(TAG, "Command 0x%02X timed out", static_cast<uint8_t>(pending_command_->command_type));
    state_ = State::IDLE;
    if (error_callback_) {
      error_callback_(pending_command_.value(), ErrorCode::TIMEOUT);
    }
    return;
  }

  // Note: ESPHome's ModbusDevice handles response parsing via callbacks
  // Upper layers (CommandQueue) should call on_modbus_data() / on_modbus_error()
  // This implementation provides a simplified synchronous-style wrapper
}

void ModbusTransport::set_response_callback(std::function<void(const Command &)> cb) { response_callback_ = cb; }

void ModbusTransport::set_error_callback(std::function<void(const Command &, ErrorCode)> cb) { error_callback_ = cb; }

bool ModbusTransport::check_timeout() { return (millis() - timeout_start_ms_) > timeout_ms_; }

void ModbusTransport::handle_response(const std::vector<uint8_t> &data) {
  // IMPORTANT: ESPHome's ModbusDevice::on_modbus_data() provides ONLY the payload data!
  // Function code, slave address, and CRC are already processed/validated by ESPHome.
  //
  // Expected payload formats (WITHOUT function code):
  // - Read (0x04):  [data...] (just the raw register values)
  // - Write (0x06): [addr_hi][addr_lo][value_hi][value_lo]
  // - Write (0x10): [addr_hi][addr_lo][count_hi][count_lo]
  ;
  uint8_t function_code = pending_command_->function_code();
  std::vector<uint8_t> send_payload = pending_command_->payload;
  // Recreate the padded payload we actually transmitted so validation
  // works for 1-byte values that were expanded to 2 bytes during send().
  std::vector<uint8_t> expected_payload;
  if ((function_code == 0x06 || function_code == 0x10) && (send_payload.size() % 2 != 0)) {
    expected_payload.push_back(0x00);  // high byte placeholder
  }
  expected_payload.insert(expected_payload.end(), send_payload.begin(), send_payload.end());

  if (data.size() < 1) {
    ESP_LOGW(TAG, "Response too short: %d bytes", data.size());
    state_ = State::IDLE;
    if (error_callback_) {
      error_callback_(pending_command_.value(), ErrorCode::PROTOCOL_ERROR);
    }
    return;
  }

  if (function_code == 0x04) {
    // Read Input Registers (Function 0x04)
    // ESPHome payload: Just the raw data bytes (NO byte_count prefix!)
    // The motor sends: [0x04][byte_count][data...], but ESPHome strips both function and byte_count

    uint8_t expected_payload_length = pending_command_->expected_response_length();
    if (data.size() != expected_payload_length) {
      ESP_LOGW(TAG, "Read response size mismatch: expected %d bytes, received %d", expected_payload_length,
               data.size());
      state_ = State::IDLE;
      if (error_callback_) {
        error_callback_(pending_command_.value(), ErrorCode::PROTOCOL_ERROR);
      }
      return;
    }

    // Response data is valid - set response and pass to callback
    Command cmd_with_response = pending_command_.value();
    cmd_with_response.response = data;

    state_ = State::IDLE;
    if (response_callback_) {
      response_callback_(cmd_with_response);
    }
  } else if (function_code == 0x06 || function_code == 0x10) {
    // Write Single Register (0x06) or Write Multiple Registers (0x10)
    // Payload format: [addr_hi][addr_lo][value/count_hi][value/count_lo]

    if (data.size() < 4) {
      ESP_LOGW(TAG, "Write response too short: %d bytes (expected 4)", data.size());
      state_ = State::IDLE;
      if (error_callback_) {
        error_callback_(pending_command_.value(), ErrorCode::PROTOCOL_ERROR);
      }
      return;
    }

    // Extract and validate register address
    uint16_t response_register = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    uint16_t expected_register = pending_command_.value().register_address();
    if (response_register != expected_register) {
      ESP_LOGW(TAG, "Write response register mismatch: sent 0x%04X, received 0x%04X", expected_register,
               response_register);
      state_ = State::IDLE;
      if (error_callback_) {
        error_callback_(pending_command_.value(), ErrorCode::PROTOCOL_ERROR);
      }
      return;
    }

    // Function-specific logging
    switch (function_code) {
      case 0x06: {
        // Check if motor rejected the command (returns 0xFFFF for failed writes)
        uint16_t response_value = (static_cast<uint16_t>(data[2]) << 8) | data[3];
        if (response_value == 0xFFFF) {
          ESP_LOGW(TAG, "Motor rejected command for register 0x%04X (response: 0xFFFF = write failed)",
                   response_register);
          state_ = State::IDLE;
          if (error_callback_) {
            error_callback_(pending_command_.value(), ErrorCode::MODBUS_ERROR);
          }
          return;
        }

        // Validate value bytes only (data[2:3] should match send_payload)
        // Response format: [addr_hi][addr_lo][value_hi][value_lo]
        if (expected_payload.size() != 2) {
          ESP_LOGW(TAG, "Invalid payload size for 0x06: %d bytes (expected 2)", expected_payload.size());
          state_ = State::IDLE;
          if (error_callback_) {
            error_callback_(pending_command_.value(), ErrorCode::PROTOCOL_ERROR);
          }
          return;
        }

        if (expected_payload[0] != data[2] || expected_payload[1] != data[3]) {
          ESP_LOGW(TAG, "Write response value mismatch for register 0x%04X", response_register);
          ESP_LOGW(TAG, "  Sent value: [%02X %02X], Received value: [%02X %02X]", expected_payload[0],
                   expected_payload[1], data[2], data[3]);
          state_ = State::IDLE;
          if (error_callback_) {
            error_callback_(pending_command_.value(), ErrorCode::INVALID_RESPONSE);
          }
          return;
        }
        break;
      }
      case 0x10: {
        // Response contains register count
        uint16_t response_count = (static_cast<uint16_t>(data[2]) << 8) | data[3];
        break;
      }
      default:
        break;
    }

    // Write response validated successfully
    Command cmd_with_response = pending_command_.value();
    cmd_with_response.response = std::vector<uint8_t>{};  // Empty response for writes

    state_ = State::IDLE;
    if (response_callback_) {
      response_callback_(cmd_with_response);
    }
  } else {
    ESP_LOGW(TAG, "Unexpected function code 0x%02X in handle_response", function_code);
    state_ = State::IDLE;
    if (error_callback_) {
      error_callback_(Command(pending_command_->command_type), ErrorCode::PROTOCOL_ERROR);
    }
  }
}

void ModbusTransport::handle_error_response(uint8_t function_code, uint8_t exception_code) {
  // Modbus error response received - clear transport state immediately!
  // This prevents the 4-second timeout wait when motor rejects a command
  ESP_LOGW(TAG, "Modbus error for command 0x%02X: function=0x%02X, exception=%d",
           static_cast<uint8_t>(pending_command_->command_type), function_code, exception_code);

  state_ = State::IDLE;

  // Invoke error callback
  if (error_callback_) {
    error_callback_(pending_command_.value(), ErrorCode::MODBUS_ERROR);
  }
}

}  // namespace servoxxd
}  // namespace esphome
