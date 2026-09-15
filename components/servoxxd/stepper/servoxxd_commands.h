/**
 * @file servoxxd_commands.h
 * @brief Type-safe command identifiers for ServoXxd servo driver communication
 *
 * This header defines the Commandtype enum which maps 1:1 to command codes used by
 * the ServoXxd closed-loop servo driver. Commands are categorized into:
 * - Read Commands (0x30-0x3F): Query motor state and sensors
 * - Configuration Commands (0x40-0x9A): Setup motor parameters
 * - Movement Commands (0xF0-0xFF): Control motor motion
 * - Special Commands: Commands outside the standard range
 *
 * Note: Some command codes represent multiple operations depending on the data
 * payload. The CommandDecoder class handles encoding/decoding to differentiate
 * these cases.
 *
 * @see docs/specification/02d-layer4-transport.md for protocol details
 */

#pragma once

#include <cstdint>
#include <vector>

namespace esphome {
namespace servoxxd {

/**
 * @brief Type-safe hardware command identifiers
 *
 * Maps directly to ServoXxd command codes.
 * Used with ITransport and CommandDecoder for protocol-agnostic communication.
 */
enum class Commandtype : uint16_t {
  // ==================== Read Commands (0x0030-0x003F) ====================
  READ_ENCODER_CARRY = 0x0030,       /// Read encoder carry value (upper 32 bits of position)
  READ_ENCODER_ADDITION = 0x0031,    /// Read encoder addition value (lower 16 bits of position)
  READ_CURRENT_SPEED = 0x0032,       /// Read current motor speed in RPM
  READ_PULSE_COUNT = 0x0033,         /// Read pulse count (step counter)
  READ_IO_STATUS = 0x0034,           /// Read IO port status (limit switches, inputs)
  READ_ANGLE_ERROR = 0x0039,         /// Read angle error (position deviation)
  READ_ENABLE_STATUS = 0x003A,       /// Read enable pin status (0=disabled, 1=enabled)
  READ_ZERO_RETURN_STATUS = 0x003B,  /// Read the go back to zero status (0_Mode auto-return)
  RELEASE_PROTECTION = 0x003D,       /// Release protection state (clear error) - Register 0x003D
  READ_PROTECTION_STATUS = 0x003E,   /// Read protection status (over-current, stall, etc.)
  RESTART_CONTROLLER = 0x003F,       /// Restart/reset the controller

  // ==================== Configuration Commands (0x0040-0x009D) ====================
  RESTART = 0x0041,                      /// Restart/reset the motor controller
  SET_WORKING_CURRENT = 0x0044,          /// Set working current in mA (configuration)
  SET_HOME_PARAMS = 0x004A,              /// Set home parameters (direction, speed, etc.)
  CALIBRATE_ENCODER = 0x0080,            /// Calibrate encoder (zero position)
  SET_WORK_MODE = 0x0082,                /// Set work mode (CR_OPEN, SR_VFOC, etc.)
  SET_WORKING_CURRENT_RUNTIME = 0x0083,  /// Set working current in mA (runtime change)
  SET_SUBDIVISION = 0x0084,              /// Set subdivision (microstepping: 1, 2, 4, 8, 16, 32, 64, etc.)
  SET_EN_PIN_ACTIVE = 0x0085,            /// Set EN pin active level (0=LOW, 1=HIGH, 2=ALWAYS/Hold)
  SET_DIR_MOTOR_ROTATION = 0x0086,       /// Set the direction of motor rotation (0=CW, 1=CCW)
  SET_AUTO_SCREEN_OFF = 0x0087,          /// Set auto screen off (0=disabled, 1=enabled)
  SET_PROTECT_ENABLE = 0x0088,           /// Set stall protection enable (0=disabled, 1=enabled)
  SET_MPLYER = 0x0089,                   /// Set microstepping interpolation (0=disabled, 1=256x enabled)
  SET_LOCK_KEYS = 0x008F,                /// Set key lock (0=unlock, 1=lock)
  SET_HOMING_PARAMETERS = 0x0090,        /// Set ENDSTOP homing parameters (Fn 0x10, Reg 0x0090, 5 bytes)
  GO_HOME = 0x0091,                      /// Start homing sequence (go to home/zero position)
  SET_ZERO = 0x0092,                     /// Set current encoder position as zero reference
  SET_ENDLIMIT_ENABLE = 0x0093,          /// Set endstop limit checking enable (0=disabled, 1=enabled)
  SET_NOLIMIT_HOMING_PARAMS = 0x0094,    /// Set the parameter of "noLimit" go home (Fn 0x10, Reg 0x0094, 8 bytes)
  SET_LIMIT_PORT_REMAP = 0x0095,         /// Remap limit switch ports - swap IN1/IN2 (Fn 0x10, Reg 0x0095, 1 byte)
  SET_ZERO_MODE = 0x009A,                /// Set 0_Mode auto-return parameters (Fn 0x10, Reg 0x009A, 4 bytes)
  SET_HOLDING_CURRENT_PERCENT = 0x009B,  /// Set holding current percentage (0-8 for 10%-90%)
  SET_EN_TRIGGER_CONFIG = 0x009D,  /// Set EN trigger zero and position error protection (Fn 0x10, Reg 0x009D, 6 bytes)

  // ==================== Movement Commands (0x00F0-0x00FF) ====================
  READ_MOTOR_STATUS =
      0x00F1,  /// Read motor status (0=fail, 1=stop, 2=speed_up, 3=speed_down, 4=full_speed, 5=homing, 6=calibrating)
  ENABLE_MOTOR = 0x00F3,          /// Enable or disable motor (0x01=enable, 0x00=disable)
  MOVE_POSITION_MODE_3 = 0x00F4,  /// Position Mode 3: Move to absolute/relative position
  MOVE_POSITION_MODE_4 = 0x00F5,  /// Position Mode 4: Move to position with multi-segment profile
  MOVE_SPEED_MODE = 0x00F6,       /// Speed Mode: Constant velocity rotation (Speed=0 stops motor)
  EMERGENCY_STOP = 0x00F7,        /// Emergency stop - immediate halt
  MOVE_POSITION_MODE_1 = 0x00FD,  /// Position Mode 1: Move by pulse count (pulses=0 stops motor)
  MOVE_POSITION_MODE_2 = 0x00FE,  /// Position Mode 2: Move to absolute position
  STOP_POSITION_MODE_2 = 0x00FF,  /// Stop in Position Mode 2 (deceleration stop)

  // ==================== Bulk Read/Write Commands (extended register addresses) ====================
  // Note: These use full 16-bit Modbus register addresses
  WRITE_ALL_CONFIG = 0x1046,  /// Write all configuration parameters (Modbus Reg 0x1046, 38 bytes payload)
  READ_ALL_CONFIG = 0x1147,   /// Read all configuration parameters (Modbus Reg 0x1147, 38 bytes response)
  READ_ALL_STATUS = 0x1248,   /// Read all status parameters (Modbus Reg 0x1248, 28 bytes response)
};

/**
 * @brief Commandtype metadata and payload container
 *
 * Encapsulates all information needed to execute a Modbus command.
 * All metadata (function_code, register_address, expected_response_length)
 * is derived from the command enum value.
 */
struct Command {
  const Commandtype command_type;  /// Command type from Commandtype enum (immutable)
  std::vector<uint8_t> payload;    /// Payload data for write commands (empty for reads)
  std::vector<uint8_t> response;   /// Response data received from motor (populated after execution)

  Command(Commandtype cmd, const std::vector<uint8_t> &payload_data = {}) : command_type(cmd), payload(payload_data) {}

  /// Get Modbus function code (computed from command type)
  uint8_t function_code() const;

  /// Get register address (same as command code)
  uint16_t register_address() const;

  /// Get expected response length (computed from command type)
  uint8_t expected_response_length() const;
};

// ============================================================================
// Inline implementations (must be in header for inline expansion)
// ============================================================================

inline uint8_t Command::function_code() const {
  switch (command_type) {
    // Function 0x04 (Read Input Registers) - All read commands
    case Commandtype::READ_ENCODER_CARRY:
    case Commandtype::READ_ENCODER_ADDITION:
    case Commandtype::READ_CURRENT_SPEED:
    case Commandtype::READ_PULSE_COUNT:
    case Commandtype::READ_IO_STATUS:
    case Commandtype::READ_ANGLE_ERROR:
    case Commandtype::READ_ENABLE_STATUS:
    case Commandtype::READ_ZERO_RETURN_STATUS:
    case Commandtype::READ_PROTECTION_STATUS:  // 0x3E
    case Commandtype::READ_MOTOR_STATUS:       // 0xF1
    case Commandtype::READ_ALL_CONFIG:         // 0x1147: Read all configuration
    case Commandtype::READ_ALL_STATUS:         // 0x1248: Read all status
      return 0x04;

    // Function 0x10 (Write Multiple Registers) - Complex multi-parameter commands
    case Commandtype::SET_HOMING_PARAMETERS:      // 0x90: 5 bytes (trigger, direction, speed, endlimit)
    case Commandtype::SET_NOLIMIT_HOMING_PARAMS:  // 0x94: 8 bytes
    case Commandtype::SET_ZERO_MODE:              // 0x9A: 4 bytes (mode, enable, speed, direction)
    case Commandtype::SET_EN_TRIGGER_CONFIG:      // 0x9D: 6 bytes (g0Enable, pEnable, Tim, Errs)
    case Commandtype::WRITE_ALL_CONFIG:           // 0x1046: Write all configuration
    case Commandtype::MOVE_POSITION_MODE_1:       // 0xFD: 8 bytes (dir, acc, speed, pulses)
    case Commandtype::MOVE_POSITION_MODE_2:       // 0xFE: 8 bytes (acc, speed, absPulses)
    case Commandtype::MOVE_POSITION_MODE_3:       // 0xF4: variable bytes
    case Commandtype::MOVE_POSITION_MODE_4:       // 0xF5: variable bytes (multi-segment)
    case Commandtype::MOVE_SPEED_MODE:            // 0xF6: 4 bytes (dir, acc, speed)
      return 0x10;

    // Function 0x06 (Write Single Register) - All other write commands
    default:
      return 0x06;
  }
}

inline uint16_t Command::register_address() const {
  // Register address is directly encoded in the Commandtype enum
  return static_cast<uint16_t>(command_type);
}
inline uint8_t Command::expected_response_length() const {
  // Read command payload lengths
  // Note: Write commands return 0 by default in the switch
  switch (command_type) {
    // 6 bytes response (48-bit values)
    case Commandtype::READ_ENCODER_CARRY:
      return 6;  // carry (4 bytes) + value (2 bytes)
    case Commandtype::READ_ENCODER_ADDITION:
      return 6;  // int48_t position

    // 4 bytes response (32-bit values)
    case Commandtype::READ_PULSE_COUNT:
    case Commandtype::READ_ANGLE_ERROR:
      return 4;  // int32_t

    // 2 bytes response (16-bit values)
    case Commandtype::READ_CURRENT_SPEED:
    case Commandtype::READ_MOTOR_STATUS:
    case Commandtype::READ_PROTECTION_STATUS:
    case Commandtype::READ_IO_STATUS:
    case Commandtype::READ_ZERO_RETURN_STATUS:
    case Commandtype::READ_ENABLE_STATUS:
      return 2;  // uint16_t / int16_t

    // Bulk read responses
    case Commandtype::READ_ALL_CONFIG:
      return 38;  // 19 registers × 2 bytes = 38 bytes (all configuration parameters)
    case Commandtype::READ_ALL_STATUS:
      return 28;  // 14 registers × 2 bytes = 28 bytes (all status parameters)

    // All write commands have no response payload
    default:
      return 0;
  }
}

}  // namespace servoxxd
}  // namespace esphome
