#include "servoxxd.h"
#include "servoxxd_command_factory.h"
#include "servoxxd_stepper_engine.h"
#include "servoxxd_command_decoder.h"
#include "servoxxd_commands.h"
#include "servoxxd_modbus.h"  // Layer 4 implementation (only in .cpp)
#include <cmath>

namespace esphome {
namespace servoxxd {

static const char *const TAG = "servoxxd";

// ============================================================================
// State Query Methods
// ============================================================================

State ServoXxd::get_state() {
  if (engine_ != nullptr) {
    return engine_->get_state();
  }
  return State::SettingUp;  // Default state before engine initialized
}

std::string ServoXxd::get_state_as_string() {
  if (engine_ != nullptr) {
    return engine_->get_state_string();
  }
  return "Not Initialized";
}

// ============================================================================
// Action-API Methods
// ============================================================================

void ServoXxd::set_control_mode(ControlMode mode) {
  this->config_.mode = mode;

  // Only send to hardware if setup is complete
  if (this->setup_state_ != SetupState::COMPLETED || this->engine_ == nullptr)
    return;

  // Critical setting - requires motor restart and full reconfiguration
  // setup_motor() will read config_.mode and send it to hardware
  ESP_LOGW(TAG, "Control mode changed - restarting motor...");
  this->engine_->setup_motor();
}

// ============================================================================
// Stepper Compatibility Methods
// ============================================================================

void ServoXxd::set_target(int32_t steps) {
  // Convert int32_t steps to Position object
  Position target = Position::from_steps(steps, this);
  // Update target position (base class and internal)
  this->set_target_pos(target);
  // Trigger movement immediately with default speed/acceleration
  // This is called by ESPHome stepper.set_target action
  if (this->setup_state_ == SetupState::COMPLETED && this->engine_ != nullptr) {
    this->move_to(target);
  }
}

void ServoXxd::set_max_speed(float speed) {
  // Convert float steps/s to Speed object and delegate to set_speed
  // ESPHome uses steps/s for stepper speed
  Speed speed_obj(speed, SpeedUnit::STEPS_PER_SEC, this);
  this->set_speed(speed_obj);
  // Also update base class member for compatibility
  this->max_speed_ = speed;
}

void ServoXxd::set_deceleration(float decel) {
  // ServoXxd uses same value for acceleration and deceleration
  // Convert to Acceleration object (ESPHome uses steps/s^2)
  Acceleration accel_obj(decel, AccelerationUnit::STEPS_PER_SEC_SQ, this);
  this->set_acceleration(accel_obj);
  // Also update base class member for compatibility
  this->deceleration_ = decel;
}

void ServoXxd::set_acceleration(float accel) {
  // Convert to Acceleration object (ESPHome uses steps/s^2)
  Acceleration accel_obj(accel, AccelerationUnit::STEPS_PER_SEC_SQ, this);
  this->set_acceleration(accel_obj);
  // Also update base class member for compatibility
  this->acceleration_ = accel;
}

// ============================================================================
// Action-API Methods
// ============================================================================

void ServoXxd::set_speed(const Speed &speed) { this->default_speed_ = speed; }

void ServoXxd::set_microsteps(uint8_t microsteps) {
  // Validate microstepping value (1-256 per spec)
  if (microsteps < 1) {
    ESP_LOGE(TAG, "Invalid microsteps: %u (must be 1-256)", microsteps);
    return;
  }

  if (this->config_.subdivision == microsteps) {
    return;
  }

  // Update config
  this->config_.subdivision = microsteps;

  // Only send to hardware if setup is complete
  if (this->setup_state_ != SetupState::COMPLETED || this->engine_ == nullptr)
    return;

  // Critical setting - requires motor restart and full reconfiguration
  // Steps per revolution effectively changes, which affects all unit conversions
  ESP_LOGW(TAG, "Microstepping changed to %u - restarting motor...", microsteps);
  this->engine_->setup_motor();
}

void ServoXxd::set_acceleration(const Acceleration &accel) { this->default_acceleration_ = accel; }

void ServoXxd::set_zero() {
  // Validate that virtual homing is configured
  if (this->homing_.mode != HomingMode::VIRTUAL) {
    ESP_LOGE(TAG, "set_zero: Only valid with homing.mode: VIRTUAL (current mode: %s)",
             this->homing_.mode == HomingMode::ENDSTOP ? "ENDSTOP" : "SENSORLESS");
    return;
  }

  // Delegate to StepperEngine - position will be updated via callback after confirmation
  this->engine_->set_zero();
}

void ServoXxd::report_position(const Position &pos) {
  // TODO: Offset-Feature wird später implementiert
  // Für jetzt: Setze einfach die aktuelle Position direkt
  set_current_pos(pos);
  ESP_LOGW(TAG, "report_position: Feature not yet implemented - just setting current_position to %.0f steps",
           pos.get_steps());
}

// ============================================================================
// Position Synchronization Helpers
// ============================================================================

void ServoXxd::set_current_pos(const Position &pos) {
  this->current_pos_ = pos;
  this->current_position = static_cast<int32_t>(pos.get_steps());
}

void ServoXxd::set_target_pos(const Position &pos) {
  this->target_pos_ = pos;
  this->target_position = static_cast<int32_t>(pos.get_steps());
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ServoXxd::ServoXxd() : config_(this) {  // ConfigData REQUIRES parent pointer - no defaults allowed
  // config_.parent is now set via constructor
  // Speed/Position objects (homing_speed, nolimit_reverse_angle_ticks) are initialized with parent via ConfigData
  // constructor

  // Note: homing_ union will be initialized by Python setters from YAML configuration
  // Note: transport_ and engine_ are created in setup() after all setters have run
}

ServoXxd::~ServoXxd() {
  if (this->engine_ != nullptr) {
    delete this->engine_;
    this->engine_ = nullptr;
  }
  if (this->transport_ != nullptr) {
    delete this->transport_;
    this->transport_ = nullptr;
  }
}

// ============================================================================
// Pure Delegation Methods (Action-API)
// ============================================================================

void ServoXxd::release_protection() {
  if (this->engine_ != nullptr)
    this->engine_->release_protection();
}

void ServoXxd::restart() {
  if (this->engine_ != nullptr)
    this->engine_->restart();
}

void ServoXxd::calibrate() {
  if (this->engine_ != nullptr)
    this->engine_->calibrate();
}

void ServoXxd::key_lock() {
  if (this->engine_ != nullptr)
    this->engine_->key_lock();
}

void ServoXxd::key_unlock() {
  if (this->engine_ != nullptr)
    this->engine_->key_unlock();
}

void ServoXxd::home() {
  if (this->operating_mode_ != OperatingMode::POSITION) {
    ESP_LOGE(TAG, "home: Only valid in POSITION mode (current mode: SPEED)");
    return;
  }
  if (this->engine_ != nullptr)
    this->engine_->home();
}

std::string ServoXxd::get_state_string() const {
  if (this->engine_ == nullptr) {
    return "Unknown";
  }
  return this->engine_->get_state_string();
}

std::string ServoXxd::get_setup_state_string() const {
  switch (this->setup_state_) {
    case SetupState::NOT_STARTED:
      return "NOT_STARTED";
    case SetupState::IN_PROGRESS:
      return "IN_PROGRESS";
    case SetupState::COMPLETED:
      return "COMPLETED";
    case SetupState::FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

void ServoXxd::stop(std::optional<Acceleration> decel) {
  if (this->engine_ == nullptr)
    return;

  Acceleration actual_decel = decel.has_value() ? decel.value() : this->default_acceleration_;
  this->engine_->stop(actual_decel);
}

void ServoXxd::run_continuous(std::optional<Speed> speed, std::optional<Acceleration> accel) {
  // Speed Mode validation
  if (this->operating_mode_ != OperatingMode::SPEED) {
    ESP_LOGE(TAG, "run_continuous: Only valid in SPEED mode (current mode: POSITION)");
    return;
  }

  if (this->engine_ == nullptr)
    return;

  Speed actual_speed = speed.has_value() ? speed.value() : this->default_speed_;
  Acceleration actual_accel = accel.has_value() ? accel.value() : this->default_acceleration_;
  this->engine_->run_continuous(actual_speed, actual_accel);
}

void ServoXxd::emergency_stop() {
  if (this->engine_ != nullptr)
    this->engine_->emergency_stop();
}

void ServoXxd::enable() {
  if (this->engine_ != nullptr)
    this->engine_->enable();
}

void ServoXxd::disable() {
  if (this->engine_ != nullptr)
    this->engine_->disable();
}

// ============================================================================
// Component Lifecycle
// ============================================================================

void ServoXxd::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ServoXxd Modbus...");

  // Validate configuration
  if (this->config_.subdivision < 1) {
    ESP_LOGE(TAG, "Invalid microstepping: %u (must be >= 1)", this->config_.subdivision);
    this->mark_failed();
    return;
  }

  // Synchronize homing_ values to config_ for proper comparison in get_update_command_types()
  // homing_ is set by YAML setters, config_ is used for hardware comparison
  if (this->homing_.mode == HomingMode::ENDSTOP) {
    this->config_.homing_trigger = this->homing_.endstop_trigger;
    this->config_.homing_direction = (this->homing_.direction == HomingDirection::CW) ? Direction::CW : Direction::CCW;
    this->config_.homing_speed = this->homing_.speed;
  }

  // Create Layer 4: ModbusTransport
  this->transport_ = new ModbusTransport(this);
  if (this->transport_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate ModbusTransport");
    this->mark_failed();
    return;
  }

  // Create Layer 2: StepperEngine with CommandQueue (Layer 3)
  this->engine_ = new StepperEngine(this, this->transport_);
  if (this->engine_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate StepperEngine");
    this->mark_failed();
    return;
  }

  // Start async motor initialization
  ESP_LOGCONFIG(TAG, "Starting motor initialization (async)...");
  this->setup_state_ = SetupState::IN_PROGRESS;
  this->setup_start_time_ = millis();
  this->engine_->setup_motor();

  // Note: setup() returns immediately, motor init runs in background
  // ESPHome will call can_proceed() repeatedly to check if we're ready
  // Hardware polling starts only after motor initialization completes
  // "Setup complete" message will be logged in setup_motor() completion callback

  // Note: Homing at startup is handled by motor's 0_Mode feature (Commandtype 0x9A)
  // When homing.at_startup=true and homing.mode=VIRTUAL, the motor automatically
  // returns to zero position after restart. No ESPHome-side action required.
  // For ENDSTOP/SENSORLESS modes, homing must be triggered manually via home() action.
}

void ServoXxd::loop() {
  // Lightweight loop: State machine updates only
  // Position sync: set_interval (100ms) - updates ESPHome base class
  // Hardware polling: Engine::update() - manages own timing (poll_interval_ms_)
  if (this->engine_ != nullptr) {
    // State machine update (high frequency for smooth motion control)
    // Also processes CommandQueue and hardware polling (poll_interval_ms_)
    this->engine_->update();
  }

  // Only process user commands after setup is complete
  if (this->setup_state_ != SetupState::COMPLETED) {
    // Setup still in progress - check for timeout
    if (this->setup_state_ == SetupState::IN_PROGRESS) {
      const uint32_t SETUP_TIMEOUT_MS = 10000;  // 10s timeout (motor restart takes 4s)
      uint32_t now = millis();

      if (now - this->setup_start_time_ > SETUP_TIMEOUT_MS) {
        ESP_LOGE(TAG, "Motor setup timeout after %ums", SETUP_TIMEOUT_MS);
        this->setup_state_ = SetupState::FAILED;
        this->mark_failed();
      }
    }
    return;  // Don't process user commands yet
  }

  // Note: No need to poll target_position here
  // ESPHome stepper.set_target action calls set_target(), which directly triggers move_to()
}

void ServoXxd::dump_config() {
  ESP_LOGCONFIG(TAG, "ServoXxd Modbus Stepper:");
  LOG_STEPPER(this);

  // Operating mode (determines available features)
  [[maybe_unused]] const char *op_modes[] = {"POSITION", "SPEED"};
  ESP_LOGCONFIG(TAG, "  Operating Mode: %s", op_modes[static_cast<uint8_t>(this->operating_mode_)]);

  // Control mode (hardware loop type)
  [[maybe_unused]] const char *ctrl_modes[] = {"", "", "", "SR_OPEN", "SR_CLOSE", "SR_VFOC"};
  ESP_LOGCONFIG(TAG, "  Control Mode: %s", ctrl_modes[static_cast<uint8_t>(this->config_.mode)]);

  // Motor configuration
  ESP_LOGCONFIG(TAG, "  Base Steps per Revolution: %.0f (hardware constant)", BASE_STEPS_PER_REVOLUTION);
  ESP_LOGCONFIG(TAG, "  Microstepping: %u", this->config_.subdivision);
  ESP_LOGCONFIG(TAG, "  Effective Steps per Revolution: %.0f", get_effective_steps_per_revolution());

  // Current settings (only effective in SR_OPEN and SR_CLOSE modes)
  if (this->config_.mode != ControlMode::SR_VFOC) {
    ESP_LOGCONFIG(TAG, "  Working Current: %u mA", this->config_.working_current_ma);
    ESP_LOGCONFIG(TAG, "  Holding Current: %u%% of working", this->config_.holding_current_percent);
  }

  // Motor behavior
  ESP_LOGCONFIG(TAG, "  Direction: %s", direction_to_string(this->config_.direction));
  [[maybe_unused]] const char *en_modes[] = {"LOW", "HIGH", "ALWAYS"};
  ESP_LOGCONFIG(TAG, "  EN Pin Active: %s", en_modes[static_cast<uint8_t>(this->config_.en_pin_active)]);
  ESP_LOGCONFIG(TAG, "  Auto Screen Off: %s", screen_mode_to_string(this->config_.screen_mode));
  ESP_LOGCONFIG(TAG, "  Lock Keys at Startup: %s", keypad_lock_to_string(this->config_.keypad_lock));

  // Homing configuration (only in POSITION mode)
  if (this->operating_mode_ == OperatingMode::POSITION) {
    if (this->homing_.mode != HomingMode::NO_HOMING) {
      [[maybe_unused]] const char *homing_modes[] = {"NO_HOMING", "ENDSTOP", "SENSORLESS", "VIRTUAL"};
      ESP_LOGCONFIG(TAG, "  Homing Mode: %s", homing_modes[static_cast<uint8_t>(this->homing_.mode)]);
      ESP_LOGCONFIG(TAG, "  Homing at Startup: %s", this->homing_.at_startup ? "YES" : "NO");

      [[maybe_unused]] const char *homing_dirs[] = {"CW", "CCW", "NEAREST"};
      ESP_LOGCONFIG(TAG, "  Homing Direction: %s", homing_dirs[static_cast<uint8_t>(this->homing_.direction)]);

      // Speed formatting depends on mode
      if (this->homing_.mode == HomingMode::VIRTUAL) {
        [[maybe_unused]] const char *speed_levels[] = {"VERY_SLOW", "SLOW", "MEDIUM", "FAST", "VERY_FAST"};
        ESP_LOGCONFIG(TAG, "  Homing Speed: %s (level %u)", speed_levels[static_cast<uint8_t>(this->homing_.level)],
                      static_cast<uint8_t>(this->homing_.level));
      } else {
        ESP_LOGCONFIG(TAG, "  Homing Speed: %.1f RPM", this->homing_.speed.rpm());
      }

      // Mode-specific settings
      if (this->homing_.mode == HomingMode::ENDSTOP) {
        [[maybe_unused]] const char *endstop_triggers[] = {"LOW", "HIGH"};
        ESP_LOGCONFIG(TAG, "  Endstop Trigger: %s",
                      endstop_triggers[static_cast<uint8_t>(this->homing_.endstop_trigger)]);
      } else if (this->homing_.mode == HomingMode::SENSORLESS) {
        ESP_LOGCONFIG(TAG, "  Homing Current: %u mA", this->homing_.current_ma);
      }
    } else {
      ESP_LOGCONFIG(TAG, "  Homing: Not configured");
    }
  }

  // Default motion parameters
  ESP_LOGCONFIG(TAG, "  Default Speed: %.1f RPM (%.0f steps/s)", this->default_speed_.rpm(),
                this->default_speed_.steps_per_sec());
  ESP_LOGCONFIG(TAG, "  Default Acceleration: %.1f RPM/s (%.0f steps/s^2)",
                this->default_acceleration_.get_rpm_per_sec(), this->default_acceleration_.get_steps_per_sec2());

  // Motion parameters (from Stepper base class)
  ESP_LOGCONFIG(TAG, "  Base Class Acceleration: %.0f steps/s^2", this->acceleration_);
  ESP_LOGCONFIG(TAG, "  Base Class Deceleration: %.0f steps/s^2", this->deceleration_);
  ESP_LOGCONFIG(TAG, "  Base Class Max Speed: %.0f steps/s", this->max_speed_);

  // Power management (only in POSITION mode)
  if (this->operating_mode_ == OperatingMode::POSITION) {
    ESP_LOGCONFIG(TAG, "  Sleep When Done: %s", this->sleep_when_done_ ? "YES" : "NO");
  }

  // Current state
  ESP_LOGCONFIG(TAG, "  Current Position: %d steps (%.2f rev)", this->current_position,
                this->current_pos_.revolutions());
  ESP_LOGCONFIG(TAG, "  Target Position: %d steps (%.2f rev)", this->target_position, this->target_pos_.revolutions());
  ESP_LOGCONFIG(TAG, "  Position Offset: %.0f steps (%.2f rev)", this->position_offset_.get_steps(),
                this->position_offset_.revolutions());
  ESP_LOGCONFIG(TAG, "  Current Speed: %.1f steps/s (%.1f RPM)", this->current_speed_,
                this->current_speed_ * 60.0f / get_effective_steps_per_revolution());

  // Engine state
  if (this->engine_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Engine State: %s", this->engine_->state_to_string(this->engine_->get_state()));
    ESP_LOGCONFIG(TAG, "  Is Moving: %s", this->engine_->is_moving() ? "YES" : "NO");
  } else {
    ESP_LOGCONFIG(TAG, "  StepperEngine: Not initialized");
  }
}

// ============================================================================
// Modbus Callbacks
// ============================================================================

void ServoXxd::on_modbus_data(const std::vector<uint8_t> &data) {
  // Forward response data to transport layer
  if (this->transport_ != nullptr) {
    this->transport_->handle_response(data);
  } else {
    ESP_LOGW(TAG, "on_modbus_data() called but transport is null - received %zu bytes", data.size());
  }
}

void ServoXxd::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  // Forward error to ModbusTransport to clear "busy" state immediately
  // This prevents 4-second timeout wait after motor rejects a command
  // Note: Detailed logging happens in handle_error_response() with command context
  if (this->engine_) {
    auto *modbus_transport = static_cast<ModbusTransport *>(this->engine_->get_transport());
    if (modbus_transport) {
      modbus_transport->handle_error_response(function_code, exception_code);
    }
  }
}

// ============================================================================
// Public API - Action Methods (Minimal Implementation)
// ============================================================================

void ServoXxd::move_to(const Position &position, std::optional<Speed> speed, std::optional<Acceleration> accel) {
  // Position Mode validation
  if (this->operating_mode_ != OperatingMode::POSITION) {
    ESP_LOGE(TAG, "move_to: Only valid in POSITION mode (current mode: SPEED)");
    return;
  }

  // Use default values if not provided
  Speed actual_speed = speed.has_value() ? speed.value() : this->default_speed_;
  Acceleration actual_accel = accel.has_value() ? accel.value() : this->default_acceleration_;

  this->engine_->move_to(position, actual_speed, actual_accel);
}

// ============================================================================
// ConfigData::get_update_command_types - Generate list of command types to update config
// ============================================================================
std::vector<Commandtype> ConfigData::get_update_command_types(const ConfigData &desired) const {
  // Step 1: Collect all changed parameters with their corresponding Commandtype
  std::vector<Commandtype> changed_commands;

  // Check each ConfigData field and add Commandtype if changed
  if (this->mode != desired.mode) {
    changed_commands.push_back(Commandtype::SET_WORK_MODE);
  }

  if (this->holding_current_percent != desired.holding_current_percent) {
    changed_commands.push_back(Commandtype::SET_HOLDING_CURRENT_PERCENT);
  }

  if (this->working_current_ma != desired.working_current_ma) {
    changed_commands.push_back(Commandtype::SET_WORKING_CURRENT_RUNTIME);
  }

  if (this->subdivision != desired.subdivision) {
    changed_commands.push_back(Commandtype::SET_SUBDIVISION);
  }

  if (this->en_pin_active != desired.en_pin_active) {
    changed_commands.push_back(Commandtype::SET_EN_PIN_ACTIVE);
  }

  if (this->direction != desired.direction) {
    changed_commands.push_back(Commandtype::SET_DIR_MOTOR_ROTATION);
  }

  if (this->screen_mode != desired.screen_mode) {
    changed_commands.push_back(Commandtype::SET_AUTO_SCREEN_OFF);
  }

  if (this->protection != desired.protection) {
    changed_commands.push_back(Commandtype::SET_PROTECT_ENABLE);
  }

  if (this->interpolation != desired.interpolation) {
    changed_commands.push_back(Commandtype::SET_MPLYER);
  }

  if (this->keypad_lock != desired.keypad_lock) {
    changed_commands.push_back(Commandtype::SET_LOCK_KEYS);
  }

  // Homing parameters (composite check)
  if (this->homing_trigger != desired.homing_trigger || this->homing_direction != desired.homing_direction ||
      this->homing_speed.rpm() != desired.homing_speed.rpm()) {
    changed_commands.push_back(Commandtype::SET_HOMING_PARAMETERS);
  }

  if (this->endstop_limit != desired.endstop_limit) {
    changed_commands.push_back(Commandtype::SET_ENDLIMIT_ENABLE);
  }

  // No-limit homing parameters (composite check)
  if (this->nolimit_reverse_angle_ticks.get_ticks() != desired.nolimit_reverse_angle_ticks.get_ticks() ||
      this->homing_limit_mode != desired.homing_limit_mode || this->nolimit_current_ma != desired.nolimit_current_ma) {
    changed_commands.push_back(Commandtype::SET_NOLIMIT_HOMING_PARAMS);
  }

  if (this->limit_port_mapping != desired.limit_port_mapping) {
    changed_commands.push_back(Commandtype::SET_LIMIT_PORT_REMAP);
  }

  // Zero mode parameters (composite check)
  if (this->zero_mode != desired.zero_mode || this->zero_task != desired.zero_task ||
      this->zero_speed != desired.zero_speed || this->zero_direction != desired.zero_direction) {
    changed_commands.push_back(Commandtype::SET_ZERO_MODE);
  }

  // Always set EN trigger config (safety feature, always configured)
  changed_commands.push_back(Commandtype::SET_EN_TRIGGER_CONFIG);

  // Step 2: Define priority order for commands (optimized for dependency chain)
  // Commands are executed in this order regardless of which parameters changed
  static const std::vector<Commandtype> priority_order = {
      // 1. Basic hardware settings (must be set before motion parameters)
      Commandtype::SET_SUBDIVISION,
      Commandtype::SET_EN_PIN_ACTIVE,
      Commandtype::SET_DIR_MOTOR_ROTATION,
      Commandtype::SET_AUTO_SCREEN_OFF,
      Commandtype::SET_LOCK_KEYS,
      Commandtype::SET_PROTECT_ENABLE,
      Commandtype::SET_MPLYER,

      // 2. Safety features (before control mode changes)
      Commandtype::SET_EN_TRIGGER_CONFIG,

      // 3. Control mode (requires basic settings to be stable)
      Commandtype::SET_WORK_MODE,

      // 4. Current settings (depend on control mode)
      Commandtype::SET_WORKING_CURRENT_RUNTIME,
      Commandtype::SET_HOLDING_CURRENT_PERCENT,

      // 5. Homing configuration
      Commandtype::SET_HOMING_PARAMETERS,
      Commandtype::SET_ENDLIMIT_ENABLE,
      Commandtype::SET_NOLIMIT_HOMING_PARAMS,

      // 6. Special features
      Commandtype::SET_ZERO_MODE,
      Commandtype::SET_LIMIT_PORT_REMAP,
  };

  // Step 3: Sort changed_commands according to priority_order
  std::vector<Commandtype> sorted_commands;
  sorted_commands.reserve(changed_commands.size());

  for (const auto &priority_cmd : priority_order) {
    // Check if this priority command is in changed_commands
    auto it = std::find(changed_commands.begin(), changed_commands.end(), priority_cmd);
    if (it != changed_commands.end()) {
      sorted_commands.push_back(priority_cmd);
    }
  }

  return sorted_commands;
}

}  // namespace servoxxd
}  // namespace esphome
