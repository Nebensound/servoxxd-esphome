/**
 * @file servoxxd_command_factory.h
 * @brief Factory functions for creating Command objects with inline encoding
 *
 * Provides self-documenting factory methods that combine Commandtype enum
 * with inline payload encoding. All encoding logic is contained within this
 * file for maximum clarity and maintainability.
 *
 * Usage:
 *   queue_->enqueue(CommandFactory::move_position_mode_2(pos, speed, accel), callback);
 *   queue_->enqueue(CommandFactory::set_subdivision(16), callback);
 *
 * This header must be included AFTER servoxxd.h to have access to all
 * required types (Direction, Speed, Position, etc.)
 */

#pragma once

#include "servoxxd_commands.h"
#include "esphome/core/helpers.h"
#include <cmath>

namespace esphome {
namespace servoxxd {
/**
 * @brief Factory functions for creating Command objects with inline encoding
 *
 * Benefits:
 * - Type-safe: Command type and payload encoding are always matched
 * - Self-documenting: Function names clearly indicate purpose
 * - DRY: No repetition of Commandtype enum in caller code
 * - Maintainable: All encoding logic in one place
 * - Uses ESPHome's built-in byte conversion helpers for consistency
 */
namespace CommandFactory {
// ============================================================================
// Internal Encoding Helpers (using ESPHome core helpers)
// ============================================================================

namespace detail {
inline void encode_uint8(std::vector<uint8_t> &data, uint8_t value) { data.push_back(value); }

inline void encode_uint16_be(std::vector<uint8_t> &data, uint16_t value) {
  // Encode as big-endian: high byte first, then low byte
  data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>(value & 0xFF));
}

inline void encode_int32_be(std::vector<uint8_t> &data, int32_t value) {
  // Encode as big-endian: most significant byte first
  data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>(value & 0xFF));
}

inline void encode_uint32_be(std::vector<uint8_t> &data, uint32_t value) {
  // Encode as big-endian: most significant byte first
  data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>(value & 0xFF));
}
}  // namespace detail

// ============================================================================
// Position Mode Commands
// ============================================================================

/**
 * @brief Position Mode 2: Absolute motion by pulses
 * @param position Target absolute position
 * @param speed Movement speed
 * @param accel Acceleration value
 * @return Command object with encoded payload (8 bytes)
 *
 * @details Payload: 8 bytes - [acc_hi][acc_lo][speed_hi][speed_lo][pos_b3][pos_b2][pos_b1][pos_b0]
 * NOTE: Mode 2 expects Pulses (depends on motor's current microstepping setting)
 * For microstepping-independent positioning, use move_position_mode_4() instead.
 */
inline Command move_position_mode_2(const Position &position, const Speed &speed, const Acceleration &accel) {
  std::vector<uint8_t> data;
  data.reserve(8);
  detail::encode_uint16_be(data, static_cast<uint16_t>(accel.acc_internal()));
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(speed.rpm_internal())));
  // Mode 2 uses pulses - need to convert from our internal ticks to motor's pulses
  // This requires knowing the motor's actual microstepping setting
  detail::encode_int32_be(data, static_cast<int32_t>(position.get_steps()));
  return Command(Commandtype::MOVE_POSITION_MODE_2, data);
}

/**
 * @brief Stop Position Mode 3 with deceleration
 * @param decel Deceleration value for controlled stop
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [deceleration]
 */
inline Command stop_position_mode_2(const Acceleration &decel) {
  // Use Mode 3 stop command (same register as Mode 2/3 stop)
  return Command(Commandtype::STOP_POSITION_MODE_2, {decel.acc_internal()});
}

/**
 * @brief Position Mode 1: Relative motion by pulses
 * @param direction Movement direction (CW or CCW)
 * @param speed Movement speed
 * @param accel Acceleration value
 * @param relative_position Relative position offset from current position
 * @return Command object with encoded payload (8 bytes)
 *
 * @details Payload: 8 bytes - [dir][speed_hi][speed_lo][acc][pulses_b3][pulses_b2][pulses_b1][pulses_b0]
 */
inline Command move_position_mode_1(Direction direction, const Speed &speed, const Acceleration &accel,
                                    const Position &relative_position) {
  std::vector<uint8_t> data;
  data.reserve(8);
  detail::encode_uint8(data, static_cast<uint8_t>(direction));
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(speed.rpm_internal())));
  detail::encode_uint8(data, accel.acc_internal());
  detail::encode_uint32_be(data, static_cast<uint32_t>(relative_position.get_ticks()));
  return Command(Commandtype::MOVE_POSITION_MODE_1, data);
}

/**
 * @brief Position Mode 3: Relative motion by axis
 * @param speed Movement speed
 * @param accel Acceleration value
 * @param relative_position Relative position offset from current position
 * @return Command object with encoded payload (7 bytes)
 *
 * @details Payload: 7 bytes - [speed_hi][speed_lo][acc][axis_b3][axis_b2][axis_b1][axis_b0]
 */
inline Command move_position_mode_3(const Speed &speed, const Acceleration &accel, const Position &relative_position) {
  std::vector<uint8_t> data;
  data.reserve(7);
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(speed.rpm_internal())));
  detail::encode_uint8(data, accel.acc_internal());
  detail::encode_int32_be(data, static_cast<int32_t>(relative_position.get_ticks()));
  return Command(Commandtype::MOVE_POSITION_MODE_3, data);
}

/**
 * @brief Position Mode 4: Absolute motion by axis
 * @param speed Movement speed
 * @param accel Acceleration value
 * @param absolute_position Target absolute position
 * @return Command object with encoded payload (7 bytes)
 *
 * @details Payload: 8 bytes - [acc_hi][acc_lo][speed_hi][speed_lo][axis_b3][axis_b2][axis_b1][axis_b0]
 */
inline Command move_position_mode_4(const Speed &speed, const Acceleration &accel, const Position &absolute_position) {
  std::vector<uint8_t> data;
  data.reserve(8);
  detail::encode_uint16_be(data, accel.acc_internal());
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(speed.rpm_internal())));
  detail::encode_int32_be(data, static_cast<int32_t>(absolute_position.get_ticks()));
  return Command(Commandtype::MOVE_POSITION_MODE_4, data);
}

// ============================================================================
// Speed Mode Commands
// ============================================================================

/**
 * @brief Speed mode movement - constant velocity rotation
 * @param speed Target speed (sign determines direction: + = CCW, - = CW)
 * @param accel Acceleration value
 * @return Command object with encoded payload (4 bytes)
 *
 * @details Payload: 4 bytes - [dir][speed_hi][speed_lo][acc]
 */
inline Command move_speed_mode(const Speed &speed, const Acceleration &accel) {
  std::vector<uint8_t> data;
  data.reserve(4);
  int16_t rpm = speed.rpm_internal();
  detail::encode_uint8(data, (rpm < 0) ? 0x00 : 0x01);
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(rpm)));
  detail::encode_uint8(data, accel.acc_internal());
  return Command(Commandtype::MOVE_SPEED_MODE, data);
}

/**
 * @brief Stop speed mode with deceleration
 * @param decel Deceleration value for controlled stop
 * @return Command object with encoded payload (4 bytes)
 *
 * @details Payload: 4 bytes - [dir=0x00][acc][speed_hi=0x00][speed_lo=0x00]
 */
inline Command stop_speed_mode(const Acceleration &decel) {
  std::vector<uint8_t> data;
  data.reserve(4);
  detail::encode_uint8(data, 0x00);                  // direction = 0 (stop)
  detail::encode_uint8(data, decel.acc_internal());  // deceleration
  detail::encode_uint16_be(data, 0x0000);            // speed = 0 (stop)
  return Command(Commandtype::MOVE_SPEED_MODE, data);
}

// ============================================================================
// Configuration Commands
// ============================================================================

/**
 * @brief Set working current
 * @param mA Working current in milliamperes (e.g., 2000 for 2A)
 * @return Command object with encoded payload (2 bytes)
 *
 * @details Payload: 2 bytes - [current_hi][current_lo]
 */
inline Command set_working_current(uint16_t mA) {
  std::vector<uint8_t> data;
  detail::encode_uint16_be(data, mA);
  return Command(Commandtype::SET_WORKING_CURRENT_RUNTIME, data);
}

/**
 * @brief Set microstepping subdivision
 * @param microsteps Microstep resolution (1-256, menu supports: 1,2,4,8,16,32,64,128,256)
 * @return Command object with encoded payload
 *
 * @details Payload: 1 byte - [microsteps]
 */
inline Command set_subdivision(uint16_t microsteps) {
  std::vector<uint8_t> data;
  detail::encode_uint16_be(data, microsteps);
  return Command(Commandtype::SET_SUBDIVISION, data);
}

/**
 * @brief Enable or disable motor
 * @param enable True to enable motor, false to disable
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x01=enable, 0x00=disable]
 */
inline Command enable_motor(bool enable) {
  return Command(Commandtype::ENABLE_MOTOR, {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

/**
 * @brief Set holding current percentage
 * @param percent Holding current percentage (PERCENT_10 to PERCENT_90)
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0-8 for 10%-90%]
 */
inline Command set_holding_current_percent(HoldingCurrentPercent percent) {
  return Command(Commandtype::SET_HOLDING_CURRENT_PERCENT, {static_cast<uint8_t>(percent)});
}

/**
 * @brief Set EN pin active level
 * @param mode Enable pin mode (LOW, HIGH, or ALWAYS)
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0=LOW, 1=HIGH, 2=ALWAYS]
 */
inline Command set_en_pin_active(EnPinActive mode) {
  return Command(Commandtype::SET_EN_PIN_ACTIVE, {static_cast<uint8_t>(mode)});
}

/**
 * @brief Set the direction of motor rotation
 * @param direction Motor rotation direction (CW or CCW)
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=CW, 0x01=CCW]
 */
inline Command set_dir_motor_rotation(Direction direction) {
  return Command(Commandtype::SET_DIR_MOTOR_ROTATION, {static_cast<uint8_t>(direction)});
}

/**
 * @brief Set auto screen off
 * @param enable True to enable auto screen off after 15s, false to disable
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=Disable, 0x01=Enable]
 */
inline Command set_auto_screen_off(ScreenMode mode) {
  return Command(Commandtype::SET_AUTO_SCREEN_OFF, {static_cast<uint8_t>(mode)});
}

/**
 * @brief Set stall protection enable
 * @param enable True to enable stall protection, false to disable
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=Disabled, 0x01=Enabled]
 */
inline Command set_protect_enable(bool enable) {
  return Command(Commandtype::SET_PROTECT_ENABLE, {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

/**
 * @brief Set microstepping interpolation (256x subdivision)
 * @param enable True to enable 256x interpolation, false to disable
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=Disabled, 0x01=256x Enabled]
 */
inline Command set_mplyer(bool enable) {
  return Command(Commandtype::SET_MPLYER, {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

/**
 * @brief Set key lock
 * @param lock True to lock hardware keys, false to unlock
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=unlock, 0x01=lock]
 */
inline Command set_lock_keys(KeypadLock lock) {
  return Command(Commandtype::SET_LOCK_KEYS, {static_cast<uint8_t>(lock)});
}

/**
 * @brief Set control mode
 * @param mode Control mode (CR_OPEN, CR_CLOSE, CR_vFOC, SR_OPEN, SR_CLOSE, SR_vFOC)
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [mode]
 */
inline Command set_control_mode(ControlMode mode) {
  return Command(Commandtype::SET_WORK_MODE, {static_cast<uint8_t>(mode)});
}

// ============================================================================
// Homing Commands
// ============================================================================

/**
 * @brief Set ENDSTOP homing parameters (0x90)
 * @param trigger Endstop trigger level (LOW or HIGH)
 * @param direction Homing direction (CW or CCW)
 * @param speed Homing speed
 * @param endlimit_enable True to enable endstop limit function
 * @return Command object with encoded payload (6 bytes, padded for Modbus 0x10)
 *
 * @details Payload: 6 bytes - [hmTrig][hmDir][HmSpeed_hi][HmSpeed_lo][EndLimit][padding]
 * Modbus 0x10 requires even byte count (multiples of 2 bytes per register).
 * Hardware format: 5 bytes content + 1 padding byte = 6 bytes = 3 registers
 */
inline Command set_homing_parameters(EndstopTrigger trigger, Direction direction, const Speed &speed,
                                     bool endlimit_enable) {
  std::vector<uint8_t> data;
  data.reserve(6);  // 6 bytes = 3 Modbus registers
  detail::encode_uint8(data, static_cast<uint8_t>(trigger));
  detail::encode_uint8(data, static_cast<uint8_t>(direction));
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(speed.rpm_internal())));
  detail::encode_uint8(data, endlimit_enable ? 1 : 0);
  detail::encode_uint8(data, 0x00);  // Padding byte (prevents Modbus auto-padding at wrong position)
  return Command(Commandtype::SET_HOMING_PARAMETERS, data);
}

/**
 * @brief Set no-limit (sensorless) homing parameters
 * @param reverse_angle Reverse movement angle after detecting stall (in ticks, typically 2000)
 * @param sensorless_enabled True to enable sensorless homing mode
 * @param home_current_ma Current threshold for stall detection in mA (default: 100)
 * @return Command object with encoded payload (8 bytes)
 *
 * @details Payload: 8 bytes - [retValue(4)][mode(2)][ma(2)]
 */
inline Command set_nolimit_homing_params(const Position &reverse_angle, bool sensorless_enabled = false,
                                         uint16_t home_current_ma = 1000) {
  std::vector<uint8_t> data;
  data.reserve(8);
  detail::encode_uint32_be(data, reverse_angle.get_ticks());
  detail::encode_uint16_be(data, sensorless_enabled ? 1 : 0);
  detail::encode_uint16_be(data, home_current_ma);
  return Command(Commandtype::SET_NOLIMIT_HOMING_PARAMS, data);
}

/**
 * @brief Set endstop limit checking enable
 * @param enable True to enable endstop limit checking, false to disable
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=Disabled, 0x01=Enabled]
 */
inline Command set_endlimit_enable(bool enable) {
  return Command(Commandtype::SET_ENDLIMIT_ENABLE, {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

/**
 * @brief Initiate homing sequence (go home)
 * @return Command object with 2-byte payload (0x0001 = start homing)
 *
 * @details Initiates the homing sequence using previously configured parameters.
 * Payload: 2 bytes - [0x00][0x01]
 */
inline Command go_home() {
  std::vector<uint8_t> data;
  data.reserve(2);
  detail::encode_uint16_be(data, 0x0001);  // 0x0001 = start homing
  return Command(Commandtype::GO_HOME, data);
}

/**
 * @brief Set 0_Mode auto-return parameters
 * @param mode Zero mode operation (DISABLED, DIR_MODE, or NEAR_MODE)
 * @param clean_set Task to perform (CLEAN to clear zero, SET to set zero)
 * @param speed Zeroing speed (0-4, slowest to fastest)
 * @param direction Zeroing direction (CW or CCW)
 * @return Command object with encoded payload (4 bytes)
 *
 * @details Payload: 4 bytes - [0_Mode][Set 0][0_Speed][0_Dir]
 */
inline Command set_zero_mode(ZeroModeMode mode = ZeroModeMode::MODE_DISABLED,
                             ZeroModeTask clean_set = ZeroModeTask::CLEAN, ZeroingSpeed speed = ZeroingSpeed::MEDIUM,
                             Direction direction = Direction::CW) {
  std::vector<uint8_t> data;
  data.reserve(4);
  detail::encode_uint8(data, static_cast<uint8_t>(mode));
  detail::encode_uint8(data, static_cast<uint8_t>(clean_set));
  detail::encode_uint8(data, static_cast<uint8_t>(speed));
  detail::encode_uint8(data, static_cast<uint8_t>(direction));
  return Command(Commandtype::SET_ZERO_MODE, data);
}

/**
 * @brief Remap limit switch ports
 * @param enable True to swap IN1/IN2 ports (IN1→En, IN2→Dir), false for normal mapping
 * @return Command object with encoded payload (1 byte)
 *
 * @details Payload: 1 byte - [0x00=normal, 0x01=swapped]
 */
inline Command set_limit_port_remap(bool enable) {
  return Command(Commandtype::SET_LIMIT_PORT_REMAP, {static_cast<uint8_t>(enable ? 0x01 : 0x00)});
}

/**
 * @brief Set EN trigger zero and position error protection parameters
 * @param en_trigger_zero_enable Enable EN trigger return to zero function (200ms pulse triggers single-turn zero)
 * @param position_error_protection_enable Enable position error protection
 * @param error_time_ms Error statistics time length (in ~15ms units, default: 100 = ~1.5s)
 * @param error_threshold Number of errors before protection triggers (28000 = 360° misalignment)
 * @return Command object with encoded payload (6 bytes)
 *
 * @details Payload: 6 bytes - [g0Enable][pEnable][Tim_hi][Tim_lo][Errs_hi][Errs_lo]
 * - g0Enable: 0=disable EN trigger zero, 1=enable (motor auto-zeros on ~200ms EN pulse)
 * - pEnable: 0=disable position error protection, 1=enable
 * - Tim: uint16_t error statistics time (1 unit ≈ 15ms)
 * - Errs: uint16_t error threshold (28000 = 360° misalignment)
 */
inline Command set_en_trigger_config(bool en_trigger_zero_enable = false, bool position_error_protection_enable = false,
                                     uint16_t error_time_units = 100, uint16_t error_threshold = 28000) {
  std::vector<uint8_t> data;
  data.reserve(6);
  detail::encode_uint8(data, en_trigger_zero_enable ? 0x01 : 0x00);
  detail::encode_uint8(data, position_error_protection_enable ? 0x01 : 0x00);
  detail::encode_uint16_be(data, error_time_units);
  detail::encode_uint16_be(data, error_threshold);
  return Command(Commandtype::SET_EN_TRIGGER_CONFIG, data);
}

// ============================================================================
// System Commands
// ============================================================================

/**
 * @brief Restart motor controller
 * @return Command object with encoded payload (2 bytes)
 *
 * @details Performs a software reset of the motor controller.
 * Payload: 2 bytes - [0x00][0x01] (value 0x0001 = 1 in big-endian)
 */
inline Command restart() { return Command(Commandtype::RESTART, {0x00, 0x01}); }

/**
 * @brief Calibrate encoder
 * @return Command object with encoded payload (1 byte)
 *
 * @details Initiates encoder calibration sequence. Motor must be unloaded.
 * Payload: 1 byte - [0x00]
 */
inline Command calibrate_encoder() { return Command(Commandtype::CALIBRATE_ENCODER, {0x00}); }

/**
 * @brief Emergency stop
 * @return Command object with no payload
 *
 * @details Immediately stops motor movement without deceleration.
 * Payload: 0 bytes
 */
inline Command emergency_stop() { return Command(Commandtype::EMERGENCY_STOP); }

/**
 * @brief Release motor protection/error state
 * @return Command object with 0x0001 payload
 *
 * @details Clears motor protection/error state (e.g., stall detection, FAIL state).
 * Payload: 2 bytes (0x0001)
 */
inline Command release_protection() {
  std::vector<uint8_t> data;
  data.reserve(2);
  detail::encode_uint16_be(data, 0x0001);  // 0x0001 = release protection
  return Command(Commandtype::RELEASE_PROTECTION, data);
}

/**
 * @brief Set current position as zero
 * @return Command object with no payload
 *
 * @details Sets the current encoder position as zero reference point.
 * Payload: 0 bytes
 */
inline Command set_zero() { return Command(Commandtype::SET_ZERO); }

// ============================================================================
// Read Commands (no payload needed)
// ============================================================================

/**
 * @brief Read encoder carry value
 * @return Command object for read operation
 *
 * @details Response: 6 bytes - carry (int32_t) + value (uint16_t)
 */
inline Command read_encoder_carry() { return Command(Commandtype::READ_ENCODER_CARRY); }

/**
 * @brief Read current speed
 * @return Command object for read operation
 *
 * @details Response: 2 bytes - speed (int16_t RPM, positive=CCW, negative=CW)
 */
inline Command read_current_speed() { return Command(Commandtype::READ_CURRENT_SPEED); }

/**
 * @brief Read motor status
 * @return Command object for read operation
 *
 * @details Response: 1 byte - status (1=STOP, 2=ACCEL, 3=DECEL, 4=FULL_SPEED, 5=HOMING, 6=CAL)
 */
inline Command read_motor_status() { return Command(Commandtype::READ_MOTOR_STATUS); }

/**
 * @brief Read protection status
 * @return Command object for read operation
 *
 * @details Response: 1 byte - status (0=OK, 1=Protected/Error)
 */
inline Command read_protection_status() { return Command(Commandtype::READ_PROTECTION_STATUS); }

/**
 * @brief Read zero return status
 * @return Command object for read operation
 *
 * @details Response: 1 byte - status (0=IN_PROGRESS, 1=SUCCESS, 2=FAIL)
 */
inline Command read_zero_return_status() { return Command(Commandtype::READ_ZERO_RETURN_STATUS); }

// ============================================================================
// Bulk Read/Write Commands
// ============================================================================

/**
 * @brief Write all configuration parameters (bulk write)
 * @param mode Control mode (SR_OPEN, SR_CLOSE, SR_VFOC, etc.)
 * @param holding_current_percent Holding current percentage (0-100)
 * @param working_current_ma Working current in mA
 * @param subdivision Microstepping subdivision (1, 2, 4, 8, 16, etc.)
 * @param en_pin_active EN pin active level
 * @param shaft_reversed True if shaft direction is reversed
 * @param auto_screen_off Auto screen off after 15s
 * @param protect_enable Protection enable flag
 * @param mplyer 256x subdivision interpolation enable
 * @param baud_rate Baud rate code (from transport layer)
 * @param slave_address Slave address (from transport layer/ModbusDevice)
 * @param group_address Group address (0x00-0xFF)
 * @param respond_enable Response enable flag
 * @param active_enable Active reporting enable flag
 * @param modbus_enable MODBUS-RTU protocol enable
 * @param key_lock Lock hardware keys
 * @param homing_trigger Homing trigger level (for ENDSTOP mode)
 * @param homing_direction Homing direction
 * @param homing_speed Homing speed
 * @param endlimit_enable Endstop limit enable
 * @param nolimit_reverse_angle Reverse angle for sensorless homing
 * @param nolimit_mode Sensorless homing mode enable
 * @param nolimit_current_ma Current threshold for sensorless homing
 * @param limit_port_remap Remap limit switch ports
 * @param zero_mode 0_Mode configuration
 * @param zero_task Zero task (CLEAN or SET)
 * @param zero_speed Zeroing speed (0-4)
 * @param zero_direction Zeroing direction
 * @return Command object with encoded payload (38 bytes)
 *
 * @details Writes all configuration parameters in a single Modbus transaction.
 * Register address: 0x1046
 * Note: This is an advanced command. Use individual setters for normal configuration.
 */
inline Command write_all_config(ControlMode mode, uint8_t holding_current_percent, uint16_t working_current_ma,
                                uint16_t subdivision, EnPinActive en_pin_active, bool shaft_reversed,
                                bool auto_screen_off, uint8_t protect_enable, bool mplyer, uint8_t baud_rate,
                                uint8_t slave_address, uint8_t group_address, bool respond_enable, bool active_enable,
                                bool modbus_enable, bool key_lock, EndstopTrigger homing_trigger,
                                Direction homing_direction, const Speed &homing_speed, bool endlimit_enable,
                                const Position &nolimit_reverse_angle, bool nolimit_mode, uint16_t nolimit_current_ma,
                                bool limit_port_remap, ZeroModeMode zero_mode, ZeroModeTask zero_task,
                                ZeroingSpeed zero_speed, Direction zero_direction) {
  std::vector<uint8_t> data;
  data.reserve(38);

  // REG1: Mode (2 bytes)
  detail::encode_uint8(data, static_cast<uint8_t>(mode));
  detail::encode_uint8(data, 0x00);  // Reserved

  // REG2: Hold current (2 bytes)
  uint8_t hw_hold =
      (holding_current_percent >= 90) ? 8 : ((holding_current_percent >= 20) ? (holding_current_percent / 10) - 1 : 0);
  detail::encode_uint8(data, hw_hold);
  detail::encode_uint8(data, 0x00);  // Reserved

  // REG3: Work current (2 bytes)
  detail::encode_uint16_be(data, working_current_ma);

  // REG4: Subdivision (2 bytes)
  detail::encode_uint8(data, subdivision);
  detail::encode_uint8(data, 0x00);  // Reserved

  // REG5: En + Dir (2 bytes)
  detail::encode_uint8(data, static_cast<uint8_t>(en_pin_active));
  detail::encode_uint8(data, shaft_reversed ? 0x01 : 0x00);

  // REG6: AutoSDD + Protect (2 bytes)
  detail::encode_uint8(data, auto_screen_off ? 0x01 : 0x00);
  detail::encode_uint8(data, protect_enable);

  // REG7: Mplyer + NULL (2 bytes)
  detail::encode_uint8(data, mplyer ? 0x01 : 0x00);
  detail::encode_uint8(data, 0x00);  // Reserved

  // REG8: Baud rate + Slave address (2 bytes)
  detail::encode_uint8(data, baud_rate);
  detail::encode_uint8(data, slave_address);

  // REG9: Group address + Respond/Active (2 bytes)
  detail::encode_uint8(data, group_address);
  uint8_t respond_active = (respond_enable ? 0x01 : 0x00) | ((active_enable ? 0x01 : 0x00) << 8);
  detail::encode_uint8(data, respond_active);

  // REG10: MODBUS + Key lock (2 bytes)
  detail::encode_uint8(data, modbus_enable ? 0x01 : 0x00);
  detail::encode_uint8(data, key_lock ? 0x01 : 0x00);

  // REG11-13: Homing parameters (6 bytes)
  detail::encode_uint8(data, static_cast<uint8_t>(homing_trigger));
  detail::encode_uint8(data, static_cast<uint8_t>(homing_direction));
  detail::encode_uint16_be(data, static_cast<uint16_t>(std::abs(homing_speed.rpm_internal())));
  detail::encode_uint8(data, 0x00);  // NULL
  detail::encode_uint8(data, endlimit_enable ? 0x01 : 0x00);

  // REG14-16: No-limit homing (8 bytes)
  detail::encode_uint32_be(data, nolimit_reverse_angle.get_ticks());
  detail::encode_uint16_be(data, nolimit_mode ? 0x01 : 0x00);
  detail::encode_uint16_be(data, nolimit_current_ma);

  // REG17: Remap + NULL (2 bytes)
  detail::encode_uint8(data, 0x00);  // NULL
  detail::encode_uint8(data, limit_port_remap ? 0x01 : 0x00);

  // REG18-19: 0_Mode parameters (4 bytes)
  detail::encode_uint8(data, static_cast<uint8_t>(zero_mode));
  detail::encode_uint8(data, static_cast<uint8_t>(zero_task));
  detail::encode_uint8(data, static_cast<uint8_t>(zero_speed));
  detail::encode_uint8(data, static_cast<uint8_t>(zero_direction));

  return Command(Commandtype::WRITE_ALL_CONFIG, data);
}

/**
 * @brief Read all configuration parameters (bulk read)
 * @return Command object for read operation
 *
 * @details Reads all configuration parameters in a single Modbus transaction:
 * Response: 38 bytes (19 registers) containing all motor configuration.
 * See write_all_config() for detailed register breakdown.
 *
 * This is useful for:
 * - Verifying configuration after setup
 * - Backup/restore of motor configuration
 * - Debugging configuration mismatches
 *
 * Register address: 0x1147
 */
inline Command read_all_config() { return Command(Commandtype::READ_ALL_CONFIG); }

/**
 * @brief Read all status parameters (bulk read)
 * @return Command object for read operation
 *
 * @details Reads all status information in a single Modbus transaction:
 * Response: 28 bytes (14 registers) containing:
 * - REG1: Motor status (1 byte) + IO status (1 byte)
 * - REG2-3: Encoder value (4 bytes)
 * - REG4-5: Speed (4 bytes)
 * - REG6-8: Pulses (6 bytes)
 * - REG9-11: RAW encoder value (6 bytes)
 * - REG12-13: Error (4 bytes)
 * - REG14: En status (1 byte) + 0_status (1 byte) + Protect status (1 byte) + NULL (1 byte)
 *
 * This is much more efficient than reading each status individually.
 * Register address: 0x1248
 */
inline Command read_all_status() { return Command(Commandtype::READ_ALL_STATUS); }

}  // namespace CommandFactory

}  // namespace servoxxd
}  // namespace esphome
