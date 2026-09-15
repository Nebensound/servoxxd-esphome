#pragma once

// Undefine Arduino macros that conflict with our method names
#ifdef degrees
#undef degrees
#endif
#ifdef radians
#undef radians
#endif

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/stepper/stepper.h"
#include "esphome/core/component.h"
#include "servoxxd_speed.h"
#include "servoxxd_acceleration.h"
#include "servoxxd_position.h"
#include <optional>
#include <span>
#include <string>

namespace esphome {
namespace servoxxd {

// Forward declarations (Layer 2)
class StepperEngine;

// Forward declarations (Layer 4)
// Note: ServoXxd is tightly coupled to ModbusTransport because it inherits from
// ModbusClientDevice and bridges ESPHome's Modbus callbacks to the transport layer.
class ModbusTransport;
enum class Commandtype : uint16_t;

// State enum (defined in servoxxd_stepper_engine.h)
enum class State;

// Setup completion states
enum class SetupState : uint8_t {
  NOT_STARTED,  // setup() not yet called
  IN_PROGRESS,  // setup_motor() running
  COMPLETED,    // setup_motor() succeeded
  FAILED        // setup_motor() failed
};

// Enum definitions for YAML configuration
enum class ServoType : uint8_t {
  SERVO28D,
  SERVO35D,
  SERVO42D,
  SERVO57D,
};

enum class ControlMode : uint8_t {
  CR_OPEN = 0,   // CR open loop mode (pulse interface)
  CR_CLOSE = 1,  // CR closed loop mode (pulse interface)
  CR_VFOC = 2,   // CR vector FOC mode (pulse interface)
  SR_OPEN = 3,   // SR open loop mode (serial interface)
  SR_CLOSE = 4,  // SR closed loop mode (serial interface)
  SR_VFOC = 5,   // SR vector FOC mode (serial interface)
};

// Helper function to convert ControlMode enum to human-readable string
inline const char *control_mode_to_string(ControlMode mode) {
  static const char *names[] = {"CR_OPEN", "CR_CLOSE", "CR_vFOC", "SR_OPEN", "SR_CLOSE", "SR_vFOC"};
  uint8_t idx = static_cast<uint8_t>(mode);
  return (idx < 6) ? names[idx] : "UNKNOWN";
}

enum class EnPinActive : uint8_t {
  EN_LOW = 0,     // EN pin active low (motor enabled when LOW)
  EN_HIGH = 1,    // EN pin active high (motor enabled when HIGH)
  EN_ALWAYS = 2,  // Motor always enabled (ignore EN pin)
};

// Helper function to convert EnPinActive enum to human-readable string
inline const char *en_pin_active_to_string(EnPinActive mode) {
  static const char *names[] = {"LOW", "HIGH", "ALWAYS"};
  uint8_t idx = static_cast<uint8_t>(mode);
  return (idx < 3) ? names[idx] : "UNKNOWN";
}

enum class OperatingMode : uint8_t {
  POSITION = 0,  // Position control mode
  SPEED = 1,     // Speed control mode
};

enum class EndstopTrigger : uint8_t {
  TRIGGER_LOW = 0,   // Endstop triggers on LOW signal
  TRIGGER_HIGH = 1,  // Endstop triggers on HIGH signal
};

enum class HomingMode : uint8_t {
  NO_HOMING = 0,   // No homing configured
  ENDSTOP = 1,     // Homing with physical endstop switch (used limit switch)
  SENSORLESS = 2,  // Sensorless homing using stall detection (no limit switch)
  VIRTUAL = 3,     // Virtual homing (software move to position 0)
};

enum class HomingDirection : uint8_t {
  CW = 0,       // Clockwise
  CCW = 1,      // Counter-clockwise
  NEAREST = 2,  // Nearest direction
};

enum class Direction : uint8_t {
  CW = 0,   // Clockwise
  CCW = 1,  // Counter-clockwise
};

// Helper function to convert Direction enum to human-readable string
inline const char *direction_to_string(Direction dir) {
  static const char *names[] = {"CW", "CCW"};
  uint8_t idx = static_cast<uint8_t>(dir);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

enum class ZeroingSpeed : uint8_t {
  VERY_SLOW = 0,
  SLOW = 1,
  MEDIUM = 2,
  FAST = 3,
  VERY_FAST = 4,
};

enum class ZeroModeMode : uint8_t { MODE_DISABLED = 0x00, DIR_MODE = 0x01, NEAR_MODE = 0x02 };

enum class ZeroModeTask : uint8_t { CLEAN = 0x00, SET = 0x01 };

enum class HoldingCurrentPercent : uint8_t {
  PERCENT_10 = 0,  // 10% of working current
  PERCENT_20 = 1,  // 20% of working current
  PERCENT_30 = 2,  // 30% of working current
  PERCENT_40 = 3,  // 40% of working current
  PERCENT_50 = 4,  // 50% of working current (default)
  PERCENT_60 = 5,  // 60% of working current
  PERCENT_70 = 6,  // 70% of working current
  PERCENT_80 = 7,  // 80% of working current
  PERCENT_90 = 8,  // 90% of working current
};

// Helper function to convert HoldingCurrentPercent enum to human-readable string
inline const char *holding_current_percent_to_string(HoldingCurrentPercent percent) {
  static const char *names[] = {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%"};
  uint8_t idx = static_cast<uint8_t>(percent);
  return (idx < 9) ? names[idx] : "UNKNOWN";
}

// Helper function to get actual percentage value from enum
inline uint8_t holding_current_percent_to_value(HoldingCurrentPercent percent) {
  return (static_cast<uint8_t>(percent) + 1) * 10;  // 0→10%, 1→20%, ..., 8→90%
}

enum class ScreenMode : uint8_t {
  ALWAYS_ON = 0,  // Screen stays on permanently
  AUTO_OFF = 1,   // Screen automatically turns off when idle
};

// Helper function to convert ScreenMode enum to human-readable string
inline const char *screen_mode_to_string(ScreenMode mode) {
  static const char *names[] = {"ALWAYS_ON", "AUTO_OFF"};
  uint8_t idx = static_cast<uint8_t>(mode);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert ScreenMode enum to bool (for hardware)
inline bool screen_mode_to_bool(ScreenMode mode) { return mode == ScreenMode::AUTO_OFF; }

// Helper function to convert bool to ScreenMode enum (from hardware)
inline ScreenMode screen_mode_from_bool(bool auto_off) {
  return auto_off ? ScreenMode::AUTO_OFF : ScreenMode::ALWAYS_ON;
}

enum class ProtectionMode : uint8_t {
  PROTECTION_OFF = 0,  // Stall protection disabled
  PROTECTION_ON = 1,   // Stall protection enabled
};

// Helper function to convert ProtectionMode enum to human-readable string
inline const char *protection_mode_to_string(ProtectionMode mode) {
  static const char *names[] = {"PROTECTION_OFF", "PROTECTION_ON"};
  uint8_t idx = static_cast<uint8_t>(mode);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert ProtectionMode enum to bool (for hardware)
inline bool protection_mode_to_bool(ProtectionMode mode) { return mode == ProtectionMode::PROTECTION_ON; }

// Helper function to convert bool to ProtectionMode enum (from hardware)
inline ProtectionMode protection_mode_from_bool(bool enabled) {
  return enabled ? ProtectionMode::PROTECTION_ON : ProtectionMode::PROTECTION_OFF;
}

enum class InterpolationMode : uint8_t {
  INTERP_OFF = 0,   // No interpolation
  INTERP_256X = 1,  // 256x microstepping interpolation
};

// Helper function to convert InterpolationMode enum to human-readable string
inline const char *interpolation_mode_to_string(InterpolationMode mode) {
  static const char *names[] = {"INTERP_OFF", "INTERP_256X"};
  uint8_t idx = static_cast<uint8_t>(mode);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert InterpolationMode enum to bool (for hardware)
inline bool interpolation_mode_to_bool(InterpolationMode mode) { return mode == InterpolationMode::INTERP_256X; }

// Helper function to convert bool to InterpolationMode enum (from hardware)
inline InterpolationMode interpolation_mode_from_bool(bool enabled) {
  return enabled ? InterpolationMode::INTERP_256X : InterpolationMode::INTERP_OFF;
}

enum class KeypadLock : uint8_t {
  UNLOCKED = 0,  // Physical keypad is unlocked
  LOCKED = 1,    // Physical keypad is locked
};

// Helper function to convert KeypadLock enum to human-readable string
inline const char *keypad_lock_to_string(KeypadLock lock) {
  static const char *names[] = {"UNLOCKED", "LOCKED"};
  uint8_t idx = static_cast<uint8_t>(lock);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert KeypadLock enum to bool (for hardware)
inline bool keypad_lock_to_bool(KeypadLock lock) { return lock == KeypadLock::LOCKED; }

// Helper function to convert bool to KeypadLock enum (from hardware)
inline KeypadLock keypad_lock_from_bool(bool locked) { return locked ? KeypadLock::LOCKED : KeypadLock::UNLOCKED; }

enum class EndstopLimit : uint8_t {
  LIMIT_OFF = 0,  // Endstop limit checking disabled
  LIMIT_ON = 1,   // Endstop limit checking enabled
};

// Helper function to convert EndstopLimit enum to human-readable string
inline const char *endstop_limit_to_string(EndstopLimit limit) {
  static const char *names[] = {"LIMIT_OFF", "LIMIT_ON"};
  uint8_t idx = static_cast<uint8_t>(limit);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert EndstopLimit enum to bool (for hardware)
inline bool endstop_limit_to_bool(EndstopLimit limit) { return limit == EndstopLimit::LIMIT_ON; }

// Helper function to convert bool to EndstopLimit enum (from hardware)
inline EndstopLimit endstop_limit_from_bool(bool enabled) {
  return enabled ? EndstopLimit::LIMIT_ON : EndstopLimit::LIMIT_OFF;
}

enum class HomingLimitMode : uint8_t {
  WITH_LIMIT = 0,  // Homing with limit switch
  NO_LIMIT = 1,    // Homing without limit switch (sensorless)
};

// Helper function to convert HomingLimitMode enum to human-readable string
inline const char *homing_limit_mode_to_string(HomingLimitMode mode) {
  static const char *names[] = {"WITH_LIMIT", "NO_LIMIT"};
  uint8_t idx = static_cast<uint8_t>(mode);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert HomingLimitMode enum to bool (for hardware)
inline bool homing_limit_mode_to_bool(HomingLimitMode mode) { return mode == HomingLimitMode::NO_LIMIT; }

// Helper function to convert bool to HomingLimitMode enum (from hardware)
inline HomingLimitMode homing_limit_mode_from_bool(bool no_limit) {
  return no_limit ? HomingLimitMode::NO_LIMIT : HomingLimitMode::WITH_LIMIT;
}

enum class LimitPortMapping : uint8_t {
  MAPPING_DEFAULT = 0,   // Default limit port mapping
  MAPPING_REMAPPED = 1,  // Limit ports remapped
};

// Helper function to convert LimitPortMapping enum to human-readable string
inline const char *limit_port_mapping_to_string(LimitPortMapping mapping) {
  static const char *names[] = {"MAPPING_DEFAULT", "MAPPING_REMAPPED"};
  uint8_t idx = static_cast<uint8_t>(mapping);
  return (idx < 2) ? names[idx] : "UNKNOWN";
}

// Helper function to convert LimitPortMapping enum to bool (for hardware)
inline bool limit_port_mapping_to_bool(LimitPortMapping mapping) {
  return mapping == LimitPortMapping::MAPPING_REMAPPED;
}

// Helper function to convert bool to LimitPortMapping enum (from hardware)
inline LimitPortMapping limit_port_mapping_from_bool(bool remapped) {
  return remapped ? LimitPortMapping::MAPPING_REMAPPED : LimitPortMapping::MAPPING_DEFAULT;
}

// Forward declaration for HomingConfig (defined after class for access to Speed)
class ServoXxd;

/**
 * @brief Motor configuration data structure
 *
 * Stores all motor configuration parameters in a single structure.
 * This serves as the single source of truth for motor settings and
 * matches the hardware READ_ALL_CONFIG response format.
 *
 * Requires parent pointer for Speed/Position object initialization.
 * Default values ensure consistent motor behavior after setup.
 */
struct ConfigData {
  ServoXxd *parent;  ///< Parent pointer for Speed/Position object construction (REQUIRED - no default)

  // TODO: Consider whether these fields should have defaults or be required constructor parameters:
  // - working_current_ma: Motor-specific, varies by model (28D/35D/42D/57D)
  // - subdivision: Application-specific, depends on required resolution
  // - homing_trigger: Hardware-specific, depends on endstop wiring
  // - homing_direction: Mechanical setup specific
  // - homing_speed: Application-specific, depends on mechanical constraints
  // For now keeping defaults for backwards compatibility and convenience

  ControlMode mode{ControlMode::SR_VFOC};  ///< Control mode (default: SR_VFOC)
  HoldingCurrentPercent holding_current_percent{HoldingCurrentPercent::PERCENT_50};  ///< Holding current (default: 50%)
  uint16_t working_current_ma{2000};                                ///< Working current in mA (default: 2000 mA)
  uint16_t subdivision{16};                                         ///< Microstepping subdivisions 1-256 (default: 16)
  EnPinActive en_pin_active{EnPinActive::EN_LOW};                   ///< EN pin active level (default: active LOW)
  Direction direction{Direction::CW};                               ///< Motor shaft rotation direction (default: CW)
  ScreenMode screen_mode{ScreenMode::AUTO_OFF};                     ///< Screen power mode (default: auto off)
  ProtectionMode protection{ProtectionMode::PROTECTION_OFF};        ///< Stall protection mode (default: disabled)
  InterpolationMode interpolation{InterpolationMode::INTERP_256X};  ///< Microstepping interpolation (default: 256x)
  KeypadLock keypad_lock{KeypadLock::UNLOCKED};                     ///< Physical keypad lock (default: unlocked)
  EndstopTrigger homing_trigger{EndstopTrigger::TRIGGER_LOW};       ///< Homing endstop trigger level
  Direction homing_direction{Direction::CW};                        ///< Homing movement direction
  Speed homing_speed;                                               ///< Homing speed (initialized with parent)
  EndstopLimit endstop_limit{EndstopLimit::LIMIT_OFF};              ///< Endstop limit checking (default: disabled)
  Position nolimit_reverse_angle_ticks;                             ///< No-limit reverse angle as Position object
  HomingLimitMode homing_limit_mode{HomingLimitMode::WITH_LIMIT};   ///< Homing with/without limit (default: with limit)
  uint16_t nolimit_current_ma{1000};                                ///< No-limit homing current in mA
  LimitPortMapping limit_port_mapping{LimitPortMapping::MAPPING_DEFAULT};  ///< Limit port mapping (default: default)
  ZeroModeMode zero_mode{ZeroModeMode::MODE_DISABLED};                     ///< 0_Mode configuration
  ZeroModeTask zero_task{ZeroModeTask::CLEAN};                             ///< Zero task
  ZeroingSpeed zero_speed{ZeroingSpeed::MEDIUM};                           ///< Zero speed
  Direction zero_direction{Direction::CW};

  // Constructor - parent is REQUIRED (no default value)
  explicit ConfigData(ServoXxd *parent_ptr)
      : parent(parent_ptr), homing_speed(parent_ptr), nolimit_reverse_angle_ticks(parent_ptr) {}

  // Delete default constructor to enforce parent requirement
  ConfigData() = delete;

  /**
   * @brief Generate list of command types needed to update configuration
   * @param desired The desired configuration to achieve
   * @return Vector of command types to execute, in optimal order
   *
   * Compares this (current) configuration with desired configuration
   * and returns only the command types needed to update differing values.
   * Commands are ordered logically: basic settings first, then homing, then special features.
   *
   * The returned command types should be used with a switch statement
   * to create and execute the actual commands with appropriate parameters.
   */
  std::vector<Commandtype> get_update_command_types(const ConfigData &desired) const;
};

/**
 * @brief Homing configuration structure
 *
 * Stores all homing-related parameters.
 * Mode determines which speed field is active (union).
 */
struct HomingConfig {
  HomingMode mode{HomingMode::NO_HOMING};          ///< Homing mode (determines which speed field is used)
  bool at_startup{false};                          ///< Perform homing at startup
  HomingDirection direction{HomingDirection::CW};  ///< Homing direction

  // Speed - union of two types (mode determines which is active):
  // - VIRTUAL: speed_level (ZeroingSpeed enum)
  // - ENDSTOP/SENSORLESS: speed (Speed object)
  union {
    Speed speed;         ///< For ENDSTOP/SENSORLESS modes
    ZeroingSpeed level;  ///< For VIRTUAL mode
  };

  EndstopTrigger endstop_trigger{EndstopTrigger::TRIGGER_LOW};  ///< For ENDSTOP mode
  uint16_t current_ma{0};                                       ///< For SENSORLESS mode (0 = use defaults)

  // Constructor - requires parent pointer for Speed initialization
  // Note: Will be properly initialized in ServoXxd constructor
  HomingConfig() : level(ZeroingSpeed::MEDIUM) {}  // Temporary - will be overwritten by ServoXxd constructor

  // Destructor - clean up Speed if that's the active member
  ~HomingConfig() {
    if (mode != HomingMode::VIRTUAL)
      speed.~Speed();
  }

  // Copy constructor
  HomingConfig(const HomingConfig &other)
      : mode(other.mode),
        at_startup(other.at_startup),
        direction(other.direction),
        endstop_trigger(other.endstop_trigger),
        current_ma(other.current_ma) {
    if (mode == HomingMode::VIRTUAL)
      level = other.level;
    else
      new (&speed) Speed(other.speed);
  }

  // Copy assignment
  HomingConfig &operator=(const HomingConfig &other) {
    if (this != &other) {
      // Destroy old Speed if needed
      if (mode != HomingMode::VIRTUAL)
        speed.~Speed();

      mode = other.mode;
      at_startup = other.at_startup;
      direction = other.direction;
      endstop_trigger = other.endstop_trigger;
      current_ma = other.current_ma;

      // Copy union member based on new mode
      if (mode == HomingMode::VIRTUAL)
        level = other.level;
      else
        new (&speed) Speed(other.speed);
    }
    return *this;
  }
};

/**
 * @brief Main component class for ServoXxd stepper motors
 *
 * This is the facade class that integrates with ESPHome. It inherits from:
 * - stepper::Stepper: Provides ESPHome stepper interface
 * - modbus::ModbusClientDevice: Enables Modbus communication
 * - Component: ESPHome lifecycle management
 *
 * **Architecture:**
 * - Facade pattern: Public API for YAML configuration and actions
 * - Delegates all movement logic to StepperEngine (state machine)
 * - Manages configuration parameters (steps_per_rev, microstepping, currents, etc.)
 * - Synchronizes with ESPHome base class (current_position, target_position, max_speed)
 *
 * **Responsibilities:**
 * - Component lifecycle (setup, loop, dump_config)
 * - YAML configuration validation
 * - Public API for actions (move_to, home, stop, run_continuous, etc.)
 * - Modbus communication setup
 * - Helper methods for unit conversions (steps ↔ ticks)
 */
class ServoXxd : virtual public Component, public stepper::Stepper, public modbus::ModbusClientDevice {
 public:
  // ==== Stepper Compatibility Methods ====
  // These methods provide compatibility with ESPHome's stepper interface
  // and delegate to ServoXxd's Position/Speed/Acceleration objects
  void set_target(int32_t steps);      // Delegate to ServoXxd's Position tracking
  void set_max_speed(float speed);     // Delegate to ServoXxd's Speed objects
  void set_deceleration(float decel);  // Delegate to ServoXxd's Acceleration (decel = accel)
  void set_acceleration(float accel);  // Delegate to ServoXxd's Acceleration

  // ==== Action-API Methods ====
  void set_control_mode(ControlMode mode);           // Change control mode at runtime (sends Commandtype 0x82)
  void set_speed(const Speed &speed);                // Update default speed for movements
  void set_acceleration(const Acceleration &accel);  // Update default acceleration
  void set_zero();                                   // Store current position as zero (VIRTUAL homing)
  void report_position(const Position &pos);         // Set position offset for zeroing

  // Position synchronization helpers (keep internal Position objects in sync with base class int32_t members)
  void set_current_pos(const Position &pos);  ///< Update current_pos_ and sync base class current_position
  void set_target_pos(const Position &pos);   ///< Update target_pos_ and sync base class target_position

  // Pure delegation methods (declared here, implemented in .cpp to avoid incomplete type errors)
  void release_protection();  ///< Clear protection state
  void restart();             ///< Restart motor controller
  void calibrate();           ///< Start encoder calibration
  void key_lock();            ///< Lock physical buttons
  void key_unlock();          ///< Unlock physical buttons
 public:
  ServoXxd();   // Implemented in .cpp to initialize homing_.speed with valid parent pointer
  ~ServoXxd();  // Implemented in .cpp to avoid incomplete type

  // ============================================================================
  // Component Lifecycle
  // ============================================================================

  /**
   * @brief Initialize the component
   *
   * - Validates configuration (steps_per_rev > 0)
   * - Creates ITransport implementation and StepperEngine
   * - Enqueues initial configuration commands
   * - Sets up periodic position synchronization
   */
  void setup() override;

  /**
   * @brief Called repeatedly by ESPHome
   *
   * - Calls StepperEngine::update() for state machine and hardware polling
   * - Checks for external target_position changes
   * - Position sync handled via set_interval (100ms)
   */
  void loop() override;

  /**
   * @brief Log configuration to console
   *
   * Logs:
   * - Operating mode (POSITION/SPEED) and control mode (SR_OPEN/SR_CLOSE/SR_VFOC)
   * - Motor configuration (steps/rev, microstepping, currents)
   * - Homing configuration (mode-specific settings)
   * - Motion parameters (speed, acceleration)
   * - Current state (position, speed, engine state)
   */
  void dump_config() override;

  // ============================================================================
  // Configuration (called from Python/YAML)
  // ============================================================================

  /**
   * @brief Get base steps per revolution (hardware constant)
   *
   * This is the motor's base step count without microstepping.
   * Fixed at 200 for 1.8° motors (only supported type for ServoXXD).
   *
   * @return Base steps per revolution (always 200.0f)
   */
  static constexpr float get_base_steps_per_revolution() { return BASE_STEPS_PER_REVOLUTION; }

  /**
   * @brief Get effective steps per revolution (base steps × microsteps)
   *
   * This is the actual resolution used for all position calculations.
   * Example: 200 base steps × 16 microsteps = 3200 effective steps
   *
   * @return Effective steps per revolution
   */
  float get_effective_steps_per_revolution() const { return BASE_STEPS_PER_REVOLUTION * config_.subdivision; }

  /**
   * @brief Get steps per revolution (deprecated, use get_effective_steps_per_revolution)
   *
   * For backward compatibility with Speed/Acceleration/Position classes.
   *
   * @return Effective steps per revolution
   */
  virtual float get_steps_per_revolution() const { return get_effective_steps_per_revolution(); }

  /**
   * @brief Get State
   *
   * @return State Current state of the motor state machine
   */
  State get_state();

  /**
   * @brief Get state as string
   *
   * @return std::string Current state as string
   */
  std::string get_state_as_string();

  /**
   * @brief Set microstepping subdivision (per specification)
   *
   * Valid values: 1-256 (hardware supports any value in this range)
   * Affects Speed class hardware compensation (rpm_for_hardware).
   * Critical setting - triggers motor restart if changed after setup.
   */
  void set_microsteps(uint16_t microsteps);

  /**
   * @brief Get current microstepping mode
   *
   * Used by Speed class for hardware compensation.
   * @return Configured subdivision (1-256), without wire-format narrowing.
   */
  virtual uint16_t get_microstepping() const { return config_.subdivision; }

  /**
   * @brief Get current position as float (precise value with microsteps)
   *
   * Returns the precise position from internal Position object.
   * Unlike current_position (int32_t), this preserves fractional steps.
   *
   * @return float Current position in steps (with fractional part)
   */
  float get_current_position_steps() const {
    return static_cast<float>(current_pos_.get_double_unit(PositionUnit::STEPS));
  }

  /**
   * @brief Get current position as float (precise value with microsteps)
   *
   * Returns the precise position from internal Position object.
   * Unlike current_position (int32_t), this preserves fractional steps.
   *
   * @return float Current position in steps (with fractional part)
   */
  float get_current_position_revolutions() const {
    return static_cast<float>(current_pos_.get_double_unit(PositionUnit::REVOLUTIONS));
  }

  /**
   * @brief Get target position as float (precise value with microsteps)
   *
   * Returns the precise target position from internal Position object.
   * Unlike target_position (int32_t), this preserves fractional steps.
   *
   * @return float Target position in steps (with fractional part)
   */
  float get_target_position_steps() const {
    return static_cast<float>(target_pos_.get_double_unit(PositionUnit::STEPS));
  }

  /**
   * @brief Get current operating mode
   *
   * Used by StepperEngine for mode validation.
   */
  OperatingMode get_operating_mode() const { return operating_mode_; }

  /**
   * @brief Get control mode (hardware loop type)
   *
   * Used by StepperEngine for setup commands.
   */
  ControlMode get_control_mode() const { return config_.mode; }

  /**
   * @brief Get working current in mA
   *
   * Used by StepperEngine for motor setup.
   */
  uint16_t get_working_current() const { return config_.working_current_ma; }

  /**
   * @brief Get holding current percentage
   *
   * Used by StepperEngine for motor setup.
   */
  HoldingCurrentPercent get_holding_current_percent() const { return config_.holding_current_percent; }

  /**
   * @brief Get EN pin active mode
   *
   * Used by StepperEngine for motor setup.
   */
  EnPinActive get_en_pin_active() const { return config_.en_pin_active; }

  /**
   * @brief Get auto screen off setting
   *
   * Used by StepperEngine for motor setup.
   */
  ScreenMode get_auto_screen_off() const { return config_.screen_mode; }

  /**
   * @brief Get lock keys at startup setting
   *
   * Used by StepperEngine for motor setup.
   */
  KeypadLock get_lock_keys_at_startup() const { return config_.keypad_lock; }

  /**
   * @brief Get homing configuration
   *
   * Used by StepperEngine for homing commands.
   */
  const HomingConfig &get_homing_config() const { return homing_; }

  /**
   * @brief Get current state as string
   *
   * Returns the current state of the motor state machine:
   * "Disabled", "Idle", "Moving", "Running", "Homing", "Calibrating", "Stopping", "Error"
   */
  std::string get_state_string() const;

  /**
   * @brief Get current setup state as string
   *
   * Returns the current setup state:
   * "NOT_STARTED", "IN_PROGRESS", "COMPLETED", "FAILED"
   */
  std::string get_setup_state_string() const;

  /**
   * @brief Set homing mode (ENDSTOP, SENSORLESS, VIRTUAL)
   *
   * Called from Python/YAML. Destroys old union member and constructs new one.
   */
  void set_homing_mode(HomingMode mode) {
    if (homing_.mode == mode)
      return;  // No change

    // Destroy old union member (if previously not NO_HOMING or VIRTUAL)
    if (homing_.mode != HomingMode::NO_HOMING && homing_.mode != HomingMode::VIRTUAL)
      homing_.speed.~Speed();

    // Update mode
    homing_.mode = mode;

    // Construct new union member
    if (mode == HomingMode::VIRTUAL)
      homing_.level = ZeroingSpeed::MEDIUM;  // Default to MEDIUM
    else if (mode != HomingMode::NO_HOMING)
      new (&homing_.speed) Speed(100.0f, SpeedUnit::RPM, this);  // Default speed
  }

  /**
   * @brief Set homing at startup flag
   */
  void set_homing_at_startup(bool enable) { homing_.at_startup = enable; }

  /**
   * @brief Set homing direction (CW, CCW, NEAREST)
   */
  void set_homing_direction(HomingDirection dir) { homing_.direction = dir; }

  /**
   * @brief Set homing speed for ENDSTOP/SENSORLESS modes
   *
   * Called from Python/YAML with Speed object.
   */
  void set_homing_speed(const Speed &speed) {
    if (homing_.mode == HomingMode::VIRTUAL) {
      ESP_LOGW("servoxxd", "set_homing_speed: ignored for VIRTUAL mode (use set_homing_speed_level)");
      return;
    }
    // Reconstruct Speed object with new value
    homing_.speed.~Speed();
    new (&homing_.speed) Speed(speed);
  }

  /**
   * @brief Set homing speed level for VIRTUAL mode
   *
   * @param level ZeroingSpeed enum (VERY_SLOW, SLOW, MEDIUM, FAST, VERY_FAST)
   */
  void set_homing_speed_level(ZeroingSpeed level) {
    if (homing_.mode != HomingMode::VIRTUAL) {
      ESP_LOGW("servoxxd", "set_homing_speed_level: ignored for non-VIRTUAL mode (use set_homing_speed)");
      return;
    }
    homing_.level = level;
  }

  /**
   * @brief Set endstop trigger mode (HIGH, LOW) for ENDSTOP mode
   */
  void set_homing_endstop_trigger(EndstopTrigger trigger) { homing_.endstop_trigger = trigger; }

  /**
   * @brief Set homing current threshold for SENSORLESS mode
   *
   * @param current_milliamps Current in mA (0-5200 depending on servo_type)
   */
  void set_homing_current(uint16_t current_milliamps) { homing_.current_ma = current_milliamps; }

  /**
   * @brief Get default speed
   */
  const Speed &get_default_speed() const { return default_speed_; }

  /**
   * @brief Get default acceleration
   */
  const Acceleration &get_default_acceleration() const { return default_acceleration_; }

  /**
   * @brief Set speed from value and unit (called from Python/YAML)
   * Creates a Speed object internally for configuration.
   */
  void set_speed(float value, SpeedUnit unit) {
    // Store as Speed object for later use
    // This will be used as default/max speed for movements
    default_speed_ = Speed(value, unit, this);
  }

  /**
   * @brief Set acceleration from value and unit (called from Python/YAML)
   * Creates an Acceleration object internally for configuration.
   */
  void set_acceleration(float value, AccelerationUnit unit) { default_acceleration_ = Acceleration(value, unit, this); }

  // Configuration setters for motor parameters
  void set_address(uint8_t addr) { this->address_ = addr; }
  void set_servo_type(ServoType type [[maybe_unused]]) { /* Store servo type */ }
  void set_working_current(uint16_t ma) { config_.working_current_ma = ma; }
  void set_holding_current_percent(HoldingCurrentPercent percent) { config_.holding_current_percent = percent; }
  void set_en_pin_active(EnPinActive value) { config_.en_pin_active = value; }
  void set_auto_screen_off(ScreenMode mode) { config_.screen_mode = mode; }
  void set_lock_keys_at_startup(KeypadLock lock) { config_.keypad_lock = lock; }
  void set_mode(OperatingMode mode) { operating_mode_ = mode; }
  void set_sleep_when_done(uint32_t ms [[maybe_unused]]) { /* Store sleep delay */ }

  // ============================================================================
  // Public API (called from Actions)
  // ============================================================================

  /**
   * @brief Move to absolute position
   *
   * Delegates to StepperEngine with default values if parameters not provided.
   * Only valid in POSITION mode.
   */
  void move_to(const Position &position, std::optional<Speed> speed = std::nullopt,
               std::optional<Acceleration> accel = std::nullopt);

  /**
   * @brief Start homing sequence
   *
   * Delegates to StepperEngine. Behavior depends on homing_.mode configuration.
   * Only valid in POSITION mode.
   */
  void home();

  /**
   * @brief Stop motor with deceleration
   *
   * Works in both Position and Speed modes.
   */
  void stop(std::optional<Acceleration> decel = std::nullopt);

  /**
   * @brief Run continuously at specified speed
   *
   * Delegates to StepperEngine with default values if parameters not provided.
   * Only valid in SPEED mode.
   */
  void run_continuous(std::optional<Speed> speed = std::nullopt, std::optional<Acceleration> accel = std::nullopt);

  /**
   * @brief Emergency stop (immediate halt, no deceleration)
   */
  void emergency_stop();

  /**
   * @brief Enable motor
   */
  void enable();

  /**
   * @brief Disable motor
   */
  void disable();

  // ============================================================================
  // Modbus Callbacks (called by ModbusClientDevice base class)
  // ============================================================================

  /**
   * @brief Handle Modbus response
   *
   * Forwards PDUs to ModbusTransport::handle_modbus_response() for command completion.
   */
  void on_response(std::span<const uint8_t> request_pdu, std::span<const uint8_t> response_pdu) override;

  /**
   * @brief Handle Modbus error
   *
   * Logs error details.
   */
  void on_error(std::span<const uint8_t> request_pdu, modbus::ExceptionCode exception_code) override;

  // ============================================================================
  // Helper Methods (used by unit type classes and StepperEngine)
  // ============================================================================

  /**
   * @brief Step/Tick conversion is handled by the Position class
   *
   * The Position class provides tested and validated conversion between
   * steps and encoder ticks. Do not duplicate this logic here.
   *
   * Usage examples:
   *
   * Steps → Ticks:
   *   Position pos(steps_value, PositionUnit::STEPS, this);
   *   int64_t total_ticks = pos.ticks_total();
   *   int32_t revs = pos.revolutions();
   *   uint16_t angle = pos.angle_ticks();
   *
   * Ticks → Steps:
   *   Position pos = Position::from_ticks_total(total_ticks);
   *   int32_t steps = pos.steps();
   *
   * Split Format → Steps:
   *   Position pos = Position::from_parts(revolutions, angle_ticks);
   *   int32_t steps = pos.steps();
   *
   * Benefits of using Position class:
   * - Handles split format (revolutions + angle_ticks)
   * - Automatic carry/borrow normalization
   * - All unit conversions in one place
   * - Comprehensive unit tests
   *
   * @see Position for implementation details
   */

 private:
  // Core components (4-layer architecture)
  // Note: ServoXxd uses ModbusTransport specifically because it inherits from ModbusClientDevice.
  // The ESPHome ModbusClientDevice callbacks (on_response, on_error) are forwarded
  // to ModbusTransport-specific methods. If Serial transport is added in the future,
  // a new SerialXxd class would be created that uses SerialTransport*.
  ModbusTransport *transport_{nullptr};  // Layer 4: Modbus-specific transport
  StepperEngine *engine_{nullptr};       // Layer 2: State machine & movement logic

  // Motor configuration (single source of truth)
  ConfigData config_;  ///< All motor configuration parameters

  /// Hardware constant: Base steps per revolution for 1.8° motors (200 steps)
  /// ServoXXD hardware only supports 200-step motors (confirmed by community reports)
  static constexpr float BASE_STEPS_PER_REVOLUTION = 200.0f;

  // Homing configuration
  HomingConfig homing_;

  // Default motion parameters
  Speed default_speed_{100.0f, SpeedUnit::RPM, this};  ///< Default/max speed for movements
  Acceleration default_acceleration_{1000.0f, AccelerationUnit::RPM_PER_SEC, this};  ///< Default acceleration

  // Position tracking (internal Position objects - primary source of truth)
  Position current_pos_{0.0f, PositionUnit::STEPS, this};      ///< Current position (raw encoder + offset)
  Position target_pos_{0.0f, PositionUnit::STEPS, this};       ///< Target position for moves
  Position position_offset_{0.0f, PositionUnit::STEPS, this};  ///< Offset for report_position() zeroing

  // Operating mode
  OperatingMode operating_mode_{OperatingMode::POSITION};  ///< Current operating mode (POSITION or SPEED)

  // Async setup state tracking
  SetupState setup_state_{SetupState::NOT_STARTED};
  uint32_t setup_start_time_{0};  // Time when setup_motor() started

  // Sleep configuration
  bool sleep_when_done_{false};  ///< Enter sleep mode after motion complete

  friend class Speed;
  friend class Acceleration;
  friend class Position;
  friend class StepperEngine;
};

}  // namespace servoxxd
}  // namespace esphome
