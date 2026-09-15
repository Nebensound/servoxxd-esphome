#pragma once

#include "servoxxd.h"
#include "servoxxd_commands.h"
#include <vector>
#include <cstdint>
#include <cmath>

namespace esphome {
namespace servoxxd {
/**
 * @brief Modbus response decoder for ServoXxd
 *
 * Contains only decode functions for parsing hardware responses.
 * Each decoder validates that the correct Commandtype is provided.
 * Encode functions have been moved to CommandFactory.
 */
class CommandDecoder {
 private:
  static const char *TAG;

  /**
   * @brief Validate command type for decoder
   *
   * @param cmd Command object to validate
   * @param expected Expected command type
   * @return true if command type matches, false otherwise
   */
  static bool validate_command_type(const Command &cmd, Commandtype expected) {
    if (cmd.command_type != expected) {
      ESP_LOGE(TAG, "Invalid command type: expected 0x%04X, got 0x%04X", static_cast<uint16_t>(expected),
               cmd.register_address());
      return false;
    }
    return true;
  }

 public:
  // ============================================================================
  // RESPONSE DECODERS
  // ============================================================================

  /**
   * @brief Decode current motor speed
   *
   * @details Commandtype::READ_CURRENT_SPEED (0x32)
   * Function: 0x04 (Read Input Registers)
   * Response: 2 bytes - [speed_hi][speed_lo] (int16_t RPM)
   *
   * @param cmd Command object with command_type=READ_CURRENT_SPEED and response data
   * @param parent Parent ServoXxd pointer for speed context (required for steps/sec conversion)
   * @return Speed object with current motor speed in RPM, or default Speed on error
   */
  static Speed read_current_speed(const Command &cmd, const ServoXxd *parent) {
    if (!validate_command_type(cmd, Commandtype::READ_CURRENT_SPEED))
      return Speed(parent);

    const auto &data = cmd.response;
    if (data.size() < 2)
      return Speed(parent);
    int16_t rpm = static_cast<int16_t>((static_cast<int16_t>(data[0]) << 8) | data[1]);
    return Speed::from_rpm(rpm, parent);
  }

  /**
   * @brief Decode pulse count
   *
   * @details Commandtype::READ_PULSE_COUNT (0x33)
   * Function: 0x04 (Read Input Registers)
   * Response: 4 bytes - [count_b3][count_b2][count_b1][count_b0] (int32_t)
   *
   * @param cmd Command object with command_type=READ_PULSE_COUNT and response data
   * @param parent Parent ServoXxd pointer for position context (required for steps conversion)
   * @return Position object with pulse count in ticks, or default Position on error
   */
  static Position read_pulse_count(const Command &cmd, const ServoXxd *parent) {
    if (!validate_command_type(cmd, Commandtype::READ_PULSE_COUNT))
      return Position(parent);

    const auto &data = cmd.response;
    if (data.size() < 4)
      return Position(parent);
    int32_t ticks = (static_cast<int32_t>(data[0]) << 24) | (static_cast<int32_t>(data[1]) << 16) |
                    (static_cast<int32_t>(data[2]) << 8) | static_cast<int32_t>(data[3]);
    return Position::from_ticks(ticks, parent);
  }

  /**
   * @brief Decode encoder addition value
   *
   * @details Commandtype::READ_ENCODER_ADDITION (0x31)
   * Function: 0x04 (Read Input Registers)
   * Response: 6 bytes - int48_t position (signed)
   *
   * @param cmd Command object with command_type=READ_ENCODER_ADDITION and response data
   * @param parent Parent ServoXxd pointer for position context (required for steps conversion)
   * @return Position object with encoder position in ticks (int48_t), or default Position on error
   */
  static Position read_encoder_addition(const Command &cmd, const ServoXxd *parent) {
    if (!validate_command_type(cmd, Commandtype::READ_ENCODER_ADDITION))
      return Position(parent);

    const auto &data = cmd.response;
    if (data.size() < 6)
      return Position(parent);
    int64_t ticks = 0;
    for (size_t i = 0; i < 6; i++) {
      ticks = (ticks << 8) | data[i];
    }
    if (ticks & 0x800000000000LL) {
      ticks |= 0xFFFF000000000000LL;
    }
    return Position::from_ticks(ticks, parent);
  }

  /**
   * @brief Decode angle error
   *
   * @details Commandtype::READ_ANGLE_ERROR (0x39)
   * Function: 0x04 (Read Input Registers)
   * Response: Same format as pulse count (4 bytes)
   *
   * @param cmd Command object with command_type=READ_ANGLE_ERROR and response data
   * @param parent Parent ServoXxd pointer for position context (required for steps conversion)
   * @return Position object with angle error in ticks, or default Position on error
   */
  static Position read_angle_error(const Command &cmd, const ServoXxd *parent) {
    if (!validate_command_type(cmd, Commandtype::READ_ANGLE_ERROR))
      return Position(parent);

    const auto &data = cmd.response;
    if (data.size() < 4)
      return Position(parent);
    int32_t ticks = (static_cast<int32_t>(data[0]) << 24) | (static_cast<int32_t>(data[1]) << 16) |
                    (static_cast<int32_t>(data[2]) << 8) | static_cast<int32_t>(data[3]);
    return Position::from_ticks(ticks, parent);
  }

  /**
   * @brief Decode motor enable status (unused)
   *
   * @details Commandtype::READ_ENABLE_STATUS (0x3D)
   * Function: 0x04 (Read Input Registers)
   * Response: 2 bytes - [0x00][status] (0=disabled, 1=enabled)
   *
   * @param data Response data vector (2 bytes minimum)
   * @return true if motor is enabled, false otherwise
   */
  static bool read_enable_status(const std::vector<uint8_t> &data) {
    if (data.size() < 2)
      return false;
    return data[1] != 0;
  }

  struct IOPortStatus {
    bool in1{false};
    bool in2{false};
    bool out1{false};
    bool out2{false};
  };

  /**
   * @brief Decode IO port status
   *
   * @details Commandtype::READ_IO_STATUS (0x34)
   * Function: 0x04 (Read Input Registers)
   * Response: 2 bytes - [0x00][status] (bit0=IN1, bit1=IN2, bit2=OUT1, bit3=OUT2)
   *
   * @param data Response data vector (2 bytes minimum)
   * @return IOPortStatus struct with in1, in2, out1, out2 boolean values
   */
  static IOPortStatus read_io_port_status(const std::vector<uint8_t> &data) {
    IOPortStatus io{};
    if (data.size() >= 2) {
      uint8_t status = data[1];
      io.in1 = (status & 0x01) != 0;
      io.in2 = (status & 0x02) != 0;
      io.out1 = (status & 0x04) != 0;
      io.out2 = (status & 0x08) != 0;
    }
    return io;
  }

  struct ZeroingStatus {
    enum State { GOING_TO_ZERO = 0, SUCCESS = 1, FAILED = 2 };
    State state{GOING_TO_ZERO};
  };

  /**
   * @brief Decode zeroing/homing status
   *
   * @details Commandtype::READ_ZEROING_STATUS (0x3C)
   * Function: 0x04 (Read Input Registers)
   * Response: 2 bytes - [0x00][status] (0=GOING_TO_ZERO, 1=SUCCESS, 2=FAILED)
   *
   * @param data Response data vector (2 bytes minimum)
   * @return ZeroingStatus struct with state (GOING_TO_ZERO, SUCCESS, or FAILED)
   */
  static ZeroingStatus read_zeroing_status(const std::vector<uint8_t> &data) {
    ZeroingStatus zs{};
    if (data.size() >= 2) {
      uint8_t status = data[1];
      if (status == 0)
        zs.state = ZeroingStatus::GOING_TO_ZERO;
      else if (status == 1)
        zs.state = ZeroingStatus::SUCCESS;
      else
        zs.state = ZeroingStatus::FAILED;
    }
    return zs;
  }

  struct CommandResponse {
    enum Status { FAIL = 0, SUCCESS = 1, RUNNING = 2, ENDLIMIT_STOPPED = 3 };
    Status status{FAIL};
  };

  /**
   * @brief Decode command response status
   *
   * @details Generic response decoder for command execution status
   * Function: 0x06 (Write Single Register) response
   * Response: 1 byte - [status] (0=FAIL, 1=SUCCESS, 2=RUNNING, 3=ENDLIMIT_STOPPED)
   *
   * @param data Response data vector (1 byte minimum)
   * @return CommandResponse struct with status (FAIL, SUCCESS, RUNNING, or ENDLIMIT_STOPPED)
   */
  static CommandResponse read_command_response(const std::vector<uint8_t> &data) {
    CommandResponse cr{};
    if (!data.empty()) {
      uint8_t status = data[0];
      switch (status) {
        case 0:
          cr.status = CommandResponse::FAIL;
          break;
        case 1:
          cr.status = CommandResponse::SUCCESS;
          break;
        case 2:
          cr.status = CommandResponse::RUNNING;
          break;
        case 3:
          cr.status = CommandResponse::ENDLIMIT_STOPPED;
          break;
        default:
          cr.status = CommandResponse::FAIL;
      }
    }
    return cr;
  }

  /**
   * @brief Decode encoder value with carry/overflow tracking
   *
   * @details
   * Commandtype::READ_ENCODER_CARRY (0x30)
   * Function: 0x04 (Read Input Registers)
   * Response: 6 bytes - [carry_b3][carry_b2][carry_b1][carry_b0][value_hi][value_lo]
   *
   * Hardware returns carry (int32_t) + value (uint16_t, 0-0x3FFF).
   * Absolute position = carry × 0x4000 + value
   *
   * Example: carry=5, value=0x1234 → position = 0x14234 encoder ticks
   *
   * Note: Use read_encoder_addition() for direct int48_t position (Commandtype 0x31).
   *
   * @param cmd Command object with command_type=READ_ENCODER_CARRY and response data
   * @param parent Parent ServoXxd pointer for position conversion context
   * @return Position object with absolute encoder position in ticks, or default Position on error
   */
  static Position read_encoder_carry(const Command &cmd, const ServoXxd *parent) {
    if (!validate_command_type(cmd, Commandtype::READ_ENCODER_CARRY))
      return Position(parent);

    const auto &data = cmd.response;
    if (data.size() < 6)
      return Position(parent);

    int32_t carry = (static_cast<int32_t>(data[0]) << 24) | (static_cast<int32_t>(data[1]) << 16) |
                    (static_cast<int32_t>(data[2]) << 8) | static_cast<int32_t>(data[3]);
    uint16_t value = static_cast<uint16_t>((static_cast<uint16_t>(data[4]) << 8) | data[5]);

    // Combined position = carry × 0x4000 + value
    int64_t absolute_ticks = (static_cast<int64_t>(carry) * 0x4000LL) + static_cast<int64_t>(value);
    return Position::from_ticks(absolute_ticks, parent);
  }

  /**
   * @brief Motor status states
   *
   * Hardware response values for READ_MOTOR_STATUS (0xF1):
   * 0 = FAIL       - Motor read fail (motor idle/disabled)
   * 1 = STOP       - Motor is stopped
   * 2 = SPEED_UP   - Motor is accelerating
   * 3 = SPEED_DOWN - Motor is decelerating
   * 4 = FULL_SPEED - Motor at full speed
   * 5 = HOMING     - Motor is executing homing sequence
   * 6 = CALIBRATING - Motor is calibrating
   */
  enum class MotorStatus : uint8_t {
    FAIL = 0,
    STOP = 1,
    SPEED_UP = 2,
    SPEED_DOWN = 3,
    FULL_SPEED = 4,
    HOMING = 5,
    CALIBRATING = 6
  };

  /**
   * @brief Decode motor status
   *
   * @details Commandtype::READ_MOTOR_STATUS (0x3A)
   * Function: 0x04 (Read Input Registers)
   * Register: 0x003A
   * Response: 1 byte - [status] (0=FAIL, 1=STOP, 2=SPEED_UP, 3=SPEED_DOWN, 4=FULL_SPEED, 5=HOMING, 6=CALIBRATING)
   *
   * Hardware Manual: "Read motor motion status"
   * - FAIL (0): Motor read fail
   * - STOP (1): Motor standstill, ready for commands
   * - SPEED_UP (2): Motor accelerating
   * - SPEED_DOWN (3): Motor decelerating
   * - FULL_SPEED (4): Motor at constant full speed
   * - HOMING (5): Motor executing homing/zeroing sequence
   * - CALIBRATING (6): Motor calibrating
   *
   * @param cmd Command object with command_type=READ_MOTOR_STATUS and response data
   * @return MotorStatus enum (FAIL, STOP, SPEED_UP, SPEED_DOWN, FULL_SPEED, HOMING, or CALIBRATING)
   */
  static MotorStatus read_motor_status(const Command &cmd) {
    if (!validate_command_type(cmd, Commandtype::READ_MOTOR_STATUS))
      return MotorStatus::FAIL;

    const auto &data = cmd.response;
    // Response format: [Reserved:0x00][Status] - 2 bytes (1 Modbus register)
    if (data.size() < 2)
      return MotorStatus::FAIL;

    uint8_t status = data[1];  // Status is in second byte (data[0] is reserved)
    switch (status) {
      case 0:
        return MotorStatus::FAIL;
      case 1:
        return MotorStatus::STOP;
      case 2:
        return MotorStatus::SPEED_UP;
      case 3:
        return MotorStatus::SPEED_DOWN;
      case 4:
        return MotorStatus::FULL_SPEED;
      case 5:
        return MotorStatus::HOMING;
      case 6:
        return MotorStatus::CALIBRATING;
      default:
        ESP_LOGW(TAG, "Unknown motor status value: %u", status);
        return MotorStatus::FAIL;
    }
  }

  enum ZeroReturnStatus {
    IN_PROGRESS = 0,  // Go back to zero in progress
    SUCCESS = 1,      // Returned to zero successfully
    FAIL = 2          // Return to zero failed (timeout, obstacle, etc.)
  };

  /**
   * @brief Encode read zero return status command (no payload)
   *
   * @details Commandtype::READ_ZERO_RETURN_STATUS (0x3B)
   * Function: 0x04 (Read Input Registers)
   * Register: 0x003B
   * Payload: 0 bytes (read command)
   */
  static std::vector<uint8_t> encode_read_zero_return_status() {
    return {};  // No payload for read commands
  }

  /**
   * @brief Read the go back to zero status
   *
   * @details Commandtype::READ_ZERO_RETURN_STATUS (0x3B)
   * Function: 0x04 (Read Input Registers)
   * Response: 1 byte - [status] (0=IN_PROGRESS, 1=SUCCESS, 2=FAIL)
   * Hardware Manual: "Read the go back to zero status"
   * Note: This reads the status of automatic zero return (0_Mode), not endstop homing
   *
   * @param cmd Command object with command_type=READ_ZERO_RETURN_STATUS and response data
   * @return ZeroReturnStatus enum (IN_PROGRESS, SUCCESS, or FAIL)
   */
  static ZeroReturnStatus read_zero_return_status(const Command &cmd) {
    if (!validate_command_type(cmd, Commandtype::READ_ZERO_RETURN_STATUS))
      return ZeroReturnStatus::FAIL;

    const auto &data = cmd.response;
    if (data.empty())
      return ZeroReturnStatus::FAIL;

    uint8_t status = data[1];  // Status is in second byte (data[0] is reserved)
    switch (status) {
      case 0:
        return ZeroReturnStatus::IN_PROGRESS;
      case 1:
        return ZeroReturnStatus::SUCCESS;
      case 2:
        return ZeroReturnStatus::FAIL;
      default:
        return ZeroReturnStatus::FAIL;
    }
  }

  /**
   * @brief Decode protection status
   *
   * @details Commandtype::READ_PROTECTION_STATUS (0x3E)
   * Function: 0x04 (Read Input Registers)
   * Response: 2 bytes - [0x00][status] (0=OK, 1=Protected/Error)
   *
   * @param cmd Command object with command_type=READ_PROTECTION_STATUS and response data
   * @return true if motor is protected/error, false if OK
   */
  static bool read_protection_status(const Command &cmd) {
    if (!validate_command_type(cmd, Commandtype::READ_PROTECTION_STATUS))
      return false;

    const auto &data = cmd.response;
    // Response format: [Reserved:0x00][Status] - 2 bytes (1 Modbus register)
    // Status: 0=OK, 1=Protected
    return (data.size() >= 2 && data[1] != 0);
  }

  /**
   * @brief Decode all configuration parameters
   *
   * @details Commandtype::READ_ALL_CONFIG (0x1147)
   * Function: 0x04 (Read Input Registers)
   * Response: 38 bytes (19 registers) containing all motor configuration
   *
   * Register breakdown (matching write_all_config):
   * - REG1 (2B): Mode [mode][reserved]
   * - REG2 (2B): Hold current [hw_hold][reserved]
   * - REG3 (2B): Work current [hi][lo]
   * - REG4 (2B): Subdivision [subdivision][reserved]
   * - REG5 (2B): En + Dir [en_pin_active][shaft_reversed]
   * - REG6 (2B): AutoSDD + Protect [auto_screen_off][protect_enable]
   * - REG7 (2B): Mplyer + NULL [mplyer][reserved]
   * - REG8 (2B): Baud + Slave [baud_rate][slave_address]
   * - REG9 (2B): Group + Respond [group_address][respond_active]
   * - REG10 (2B): MODBUS + Key [modbus_enable][key_lock]
   * - REG11-13 (6B): Homing params [trigger][direction][speed_hi][speed_lo][null][endlimit]
   * - REG14-16 (8B): No-limit homing [reverse_angle(4)][mode(2)][current_ma(2)]
   * - REG17 (2B): Remap [null][limit_port_remap]
   * - REG18-19 (4B): 0_Mode [zero_mode][zero_task][zero_speed][zero_direction]
   *
   * @param cmd Command object with command_type=READ_ALL_CONFIG and response data
   * @param parent Parent ServoXxd pointer (REQUIRED for Speed/Position initialization)
   * @return ConfigData struct with decoded configuration parameters
   */
  static ConfigData read_all_config(const Command &cmd, ServoXxd *parent) {
    if (!validate_command_type(cmd, Commandtype::READ_ALL_CONFIG))
      return ConfigData(parent);

    const auto &data = cmd.response;
    // Datasheet: READ_ALL_CONFIG returns 19 registers = 38 data bytes
    // (byte_count = 0x26). Each register carries two 1-byte parameters.
    if (data.size() != 38) {
      ESP_LOGW(TAG, "Invalid READ_ALL_CONFIG response size: %zu bytes (expected 38)", data.size());
      return ConfigData(parent);
    }

    // Create ConfigData with required parent pointer
    ConfigData config(parent);
    // REG1: Mode + Hold current (%)
    config.mode = static_cast<ControlMode>(data[0]);
    uint8_t hw_hold = data[1];
    hw_hold = hw_hold > 8 ? 8 : hw_hold;  // clamp to valid range
    config.holding_current_percent = static_cast<HoldingCurrentPercent>(hw_hold);

    // REG2: Working current (mA, big-endian)
    config.working_current_ma = (static_cast<uint16_t>(data[2]) << 8) | data[3];

    // REG3: Subdivision + EN pin active level
    config.subdivision = data[4] == 0 ? 1 : data[4];  // avoid 0 → divide-by-zero later
    config.en_pin_active = static_cast<EnPinActive>(data[5]);

    // REG4: Direction + Auto screen off
    config.direction = static_cast<Direction>(data[6]);
    config.screen_mode = screen_mode_from_bool(data[7] != 0);

    // REG5: Protection + Interpolation
    config.protection = protection_mode_from_bool(data[8] != 0);
    config.interpolation = interpolation_mode_from_bool(data[9] != 0);

    // REG6: NULL + Baud rate (transport only) -> skip
    // data[10] reserved, data[11] baud rate

    // REG7: Slave address + Group address (transport) -> skip

    // REG8: Respond/Active flags (transport) -> skip

    // REG9: MODBUS enable (transport) + Key lock
    config.keypad_lock = keypad_lock_from_bool(data[17] != 0);

    // REG10: Homing trigger + Homing direction
    config.homing_trigger = static_cast<EndstopTrigger>(data[18]);
    config.homing_direction = static_cast<Direction>(data[19]);

    // REG11: Homing speed (RPM, big-endian)
    uint16_t homing_speed_rpm = (static_cast<uint16_t>(data[20]) << 8) | data[21];
    config.homing_speed = Speed::from_rpm(static_cast<float>(homing_speed_rpm), config.parent);

    // REG12: NULL + EndLimit enable
    config.endstop_limit = endstop_limit_from_bool(data[23] != 0);

    // REG13: Reserved (0x0000) -> ignore

    // REG14-16: Sensorless homing / protection parameters (best-effort mapping)
    // REG14: reverse angle (ticks, 16-bit)
    config.nolimit_reverse_angle_ticks.set_ticks(static_cast<int32_t>((data[26] << 8) | data[27]));
    // REG15: homing limit mode flag (non-zero => no-limit)
    config.homing_limit_mode = homing_limit_mode_from_bool(data[28] != 0);
    // REG16: sensorless homing current (mA, big-endian)
    config.nolimit_current_ma = (static_cast<uint16_t>(data[30]) << 8) | data[31];

    // REG17: limit port remap flag (byte 33)
    config.limit_port_mapping = limit_port_mapping_from_bool(data[33] != 0);

    // REG18-19: 0_Mode configuration
    config.zero_mode = static_cast<ZeroModeMode>(data[34]);
    config.zero_task = ZeroModeTask::CLEAN;  // Not encoded in READ_ALL_CONFIG; use safe default
    config.zero_speed = static_cast<ZeroingSpeed>(data[36]);
    config.zero_direction = static_cast<Direction>(data[37]);

    return config;
  }
};

}  // namespace servoxxd
}  // namespace esphome
