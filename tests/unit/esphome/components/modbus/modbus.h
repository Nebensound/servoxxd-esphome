#pragma once

// Mock ESPHome Modbus header for unit testing

#include <cstdint>
#include <span>
#include <vector>

namespace esphome {
namespace modbus {

// Modbus function codes
constexpr uint8_t FUNC_PRESET_MULTIPLE_REGISTERS = 0x10;

enum class ExceptionCode : uint8_t { ILLEGAL_FUNCTION = 1, ILLEGAL_DATA_ADDRESS = 2 };

class ModbusClientDevice {
 public:
  virtual ~ModbusClientDevice() = default;

  // Mock methods for unit testing
  void set_address(uint8_t address) { address_ = address; }
  uint8_t get_address() const { return address_; }

  bool read_input_registers(uint16_t address, uint16_t count) {
    last_request = {0x04};
    append_word_(address);
    append_word_(count);
    return accept_requests;
  }
  bool write_single_register(uint16_t address, uint16_t value) {
    last_request = {0x06};
    append_word_(address);
    append_word_(value);
    return accept_requests;
  }
  bool write_multiple_registers(uint16_t address, std::span<const uint16_t> values) {
    last_request = {0x10};
    append_word_(address);
    append_word_(values.size());
    last_request.push_back(values.size() * 2);
    for (auto value : values) {
      append_word_(value);
    }
    return accept_requests && !values.empty();
  }

  virtual void on_response(std::span<const uint8_t>, std::span<const uint8_t>) {}
  virtual void on_error(std::span<const uint8_t>, ExceptionCode) {}

  bool accept_requests{true};
  std::vector<uint8_t> last_request;

 protected:
  void append_word_(uint16_t value) {
    last_request.push_back(value >> 8);
    last_request.push_back(value & 0xFF);
  }
  uint8_t address_{0};
};

}  // namespace modbus
}  // namespace esphome
