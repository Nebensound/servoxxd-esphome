#include "servoxxd_stepper_engine.h"
#include "servoxxd.h"
#include "servoxxd_command_decoder.h"
#include "servoxxd_commands.h"
#include "servoxxd_command_factory.h"
#include "servoxxd_transport.h"
#include <cmath>
#include <cinttypes>
#include <cstring>

namespace esphome {
namespace servoxxd {

static const char *TAG_ENGINE = "servoxxd.stepper_engine";

// ============================================================================
// Constructor / Destructor
// ============================================================================

StepperEngine::StepperEngine(ServoXxd *parent, ITransport *transport, uint32_t command_timeout_ms)
    : parent_(parent),
      queue_(nullptr),
      state_(State::SettingUp),
      emergency_flag_(false),
      current_speed_(0.0f, SpeedUnit::RPM, parent),
      protection_triggered_(false),
      state_enter_time_(0),
      disable_pending_(false) {
  // Initialize state entry time to current time for proper timeout tracking
  state_enter_time_ = millis();

  // Create CommandQueue (Layer 3) if transport provided
  if (transport != nullptr) {
    queue_ = new CommandQueue(transport, command_timeout_ms);
  }
}

StepperEngine::~StepperEngine() {
  if (queue_) {
    delete queue_;
    queue_ = nullptr;
  }
}

// ============================================================================
// Hardware Polling Methods
// ============================================================================

void StepperEngine::poll_motor_speed() {
  // Enqueue read command for motor speed (Commandtype 0x32 READ_CURRENT_SPEED)
  // Expected response: speed_rpm (int16_t) = 2 bytes

  queue_->enqueue(
      CommandFactory::read_current_speed(),
      [this](bool success, const Command &cmd) {
        if (success) {
          Speed speed = CommandDecoder::read_current_speed(cmd, parent_);
          process_speed_update(speed);
        }
      },
      Priority::BACKGROUND);
}

void StepperEngine::poll_motor_status() {
  // Enqueue read command for motor status (Commandtype 0xF1 READ_MOTOR_STATUS)
  // Expected response: status (uint8_t: 0=fail, 1=stop, 2=speed_up, 3=speed_down, 4=full_speed, 5=homing,
  // 6=calibrating)
  queue_->enqueue(
      CommandFactory::read_motor_status(),
      [this](bool success, const Command &cmd) {
        if (!success) {
          ESP_LOGW(TAG_ENGINE, "Failed to read motor status");
          return;
        }

        CommandDecoder::MotorStatus status = CommandDecoder::read_motor_status(cmd);
        process_motor_status_update(status);
      },
      Priority::BACKGROUND);
}

void StepperEngine::poll_protection_status() {
  // Enqueue read command for protection status (Commandtype 0x3E READ_PROTECTION_STATUS)
  // Expected response: protection (uint8_t, 0 = OK, 1 = protected) = 2 bytes (1 register)
  queue_->enqueue(
      CommandFactory::read_protection_status(),
      [this](bool success, const Command &cmd) {
        if (success) {
          bool protected_state = CommandDecoder::read_protection_status(cmd);
          process_protection_update(protected_state ? 1 : 0);
        }
      },
      Priority::BACKGROUND);
}

// ============================================================================
// Motor Setup
// ============================================================================

void StepperEngine::setup_motor() {
  // Explicitly transition to SettingUp state
  transition_to(State::SettingUp);

  if (!queue_) {
    parent_->status_set_error(LOG_STR("CommandQueue not initialized"));
    parent_->mark_failed();
    return;
  }

  // 0. Restart motor to ensure clean state (Commandtype 0x41 RESTART)
  // Motor needs ~3-4s to reboot before accepting configuration commands
  queue_->enqueue(
      CommandFactory::restart(),
      [](bool success, const Command &) {
        if (!success) {
          ESP_LOGW(TAG_ENGINE, "Failed to restart motor");
        }
      },
      Priority::SETUP,
      4000);  // Wait 4 seconds after restart

  // 1. Read all current configuration from motor (after restart)
  // This allows us to verify the motor's current state before applying new settings
  queue_->enqueue(
      CommandFactory::read_all_config(),
      [this](bool success, const Command &cmd) {
        if (!success) {
          ESP_LOGE(TAG_ENGINE, "✗ Failed to read motor configuration - aborting setup!");
          transition_to(State::Error);
          parent_->status_set_error(LOG_STR("Failed to read motor configuration"));
          parent_->mark_failed();
          // Clear all pending commands to abort setup sequence
          if (queue_) {
            queue_->clear();
          }
          return;
        }

        // Decode configuration values
        auto config = CommandDecoder::read_all_config(cmd, parent_);

        // Generate list of command types needed to update configuration
        // Get desired config from parent
        ConfigData desired_config = parent_->config_;
        // Get list of commands that need to be executed
        std::vector<Commandtype> update_commands = config.get_update_command_types(desired_config);

        if (!update_commands.empty()) {
          // Process each command type in the optimal order (already sorted by get_update_command_types)
          for (const auto &cmd_type : update_commands) {
            switch (cmd_type) {
              case Commandtype::SET_WORK_MODE: {
                ControlMode desired_mode = desired_config.mode;

                queue_->enqueue(
                    CommandFactory::set_control_mode(desired_config.mode),
                    [this, desired_mode](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update control mode");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update control mode"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_HOLDING_CURRENT_PERCENT: {
                HoldingCurrentPercent desired_holding_current = desired_config.holding_current_percent;

                queue_->enqueue(
                    CommandFactory::set_holding_current_percent(desired_holding_current),
                    [this, desired_holding_current](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update holding current");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update holding current"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_WORKING_CURRENT_RUNTIME: {
                uint16_t desired_current_ma = desired_config.working_current_ma;

                queue_->enqueue(
                    CommandFactory::set_working_current(desired_current_ma),
                    [this, desired_current_ma](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update working current");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update working current"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_SUBDIVISION: {
                uint8_t desired_microstepping = desired_config.subdivision;

                queue_->enqueue(
                    CommandFactory::set_subdivision(desired_microstepping),
                    [this, desired_microstepping](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        // Motor rejected SET_SUBDIVISION command
                        // This is expected in vFOC modes (hardware limitation per manual)
                        ESP_LOGE(TAG_ENGINE, "Failed to update microstepping to %u - Motor in vFOC mode?",
                                 desired_microstepping);
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update microstepping"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_EN_PIN_ACTIVE: {
                EnPinActive desired_en_pin = desired_config.en_pin_active;

                queue_->enqueue(
                    CommandFactory::set_en_pin_active(desired_en_pin),
                    [this, desired_en_pin](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update EN pin active level");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update EN pin active level"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_DIR_MOTOR_ROTATION: {
                Direction desired_direction = desired_config.direction;

                queue_->enqueue(
                    CommandFactory::set_dir_motor_rotation(desired_direction),
                    [this, desired_direction](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update direction");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update direction"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_AUTO_SCREEN_OFF: {
                ScreenMode desired_screen_mode = desired_config.screen_mode;

                queue_->enqueue(
                    CommandFactory::set_auto_screen_off(desired_screen_mode),
                    [this, desired_screen_mode](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update auto screen off");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update auto screen off"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_PROTECT_ENABLE: {
                ProtectionMode desired_protection = desired_config.protection;

                queue_->enqueue(
                    CommandFactory::set_protect_enable(protection_mode_to_bool(desired_protection)),
                    [this, desired_protection](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update protection mode");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update protection mode"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_LOCK_KEYS: {
                KeypadLock desired_keypad_lock = desired_config.keypad_lock;

                queue_->enqueue(
                    CommandFactory::set_lock_keys(desired_keypad_lock),
                    [this, desired_keypad_lock](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to update key lock");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to update key lock"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_EN_TRIGGER_CONFIG: {
                queue_->enqueue(
                    CommandFactory::set_en_trigger_config(),
                    [this](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to set EN trigger configuration");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to set EN trigger configuration"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_HOMING_PARAMETERS: {
                // Only configure if ENDSTOP mode is enabled
                auto &homing = parent_->homing_;
                if (homing.mode != HomingMode::ENDSTOP) {
                  break;  // Skip if not ENDSTOP mode
                }

                // Convert HomingDirection to Direction
                Direction dir = (homing.direction == HomingDirection::CW) ? Direction::CW : Direction::CCW;

                // Configure homing parameters (EndLimit=false allows motor to rotate past endstop)
                queue_->enqueue(
                    CommandFactory::set_homing_parameters(homing.endstop_trigger, dir, homing.speed, false),
                    [this](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to configure homing parameters");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to configure homing parameters"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                        return;
                      }

                      // Manual requires GoHome after setting/changing homing parameters
                      // Execute GoHome immediately after successful parameter configuration
                      ESP_LOGI(TAG_ENGINE, "Homing parameters set - executing GoHome as required by hardware");
                      queue_->enqueue(
                          CommandFactory::go_home(),
                          [this](bool go_home_success, const Command &) {
                            if (go_home_success) {
                              ESP_LOGI(TAG_ENGINE, "GoHome started after parameter configuration");
                              transition_to(State::Homing);
                            } else {
                              ESP_LOGW(TAG_ENGINE, "GoHome after parameter configuration failed");
                              // Don't fail setup - homing can be retried later
                            }
                          },
                          Priority::SETUP);
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_NOLIMIT_HOMING_PARAMS: {
                // Only configure when SENSORLESS homing is selected
                auto &homing = parent_->homing_;
                if (homing.mode != HomingMode::SENSORLESS) {
                  break;
                }

                // Fallback to typical reverse angle if none provided
                Position reverse_angle = desired_config.nolimit_reverse_angle_ticks;
                if (reverse_angle.get_ticks() == 0) {
                  reverse_angle = Position::from_ticks(2000, parent_);
                }

                // Use explicit homing current when provided, otherwise default config value
                uint16_t homing_current =
                    homing.current_ma != 0 ? homing.current_ma : desired_config.nolimit_current_ma;

                queue_->enqueue(
                    CommandFactory::set_nolimit_homing_params(reverse_angle, true, homing_current),
                    [this](bool success, const Command &) {
                      if (state_ == State::Error) {
                        return;  // Setup already aborted
                      }
                      if (!success) {
                        ESP_LOGE(TAG_ENGINE, "Failed to configure sensorless homing parameters");
                        transition_to(State::Error);
                        parent_->status_set_error(LOG_STR("Failed to configure sensorless homing parameters"));
                        parent_->mark_failed();
                        if (queue_) {
                          queue_->clear();
                        }
                      }
                    },
                    Priority::SETUP);
                break;
              }

              case Commandtype::SET_LIMIT_PORT_REMAP:
              case Commandtype::SET_ZERO_MODE:
                // TODO: Implement handlers for these command types
                ESP_LOGW(TAG_ENGINE, "  Command type 0x%04X not yet implemented", static_cast<uint16_t>(cmd_type));
                break;

              default:
                ESP_LOGW(TAG_ENGINE, "  Unknown command type: 0x%04X", static_cast<uint16_t>(cmd_type));
                break;
            }
          }
        }

        // Enqueue setup completion marker AFTER all config updates
        // Queue guarantees FIFO order, so this runs after all config commands
        queue_->enqueue(
            CommandFactory::read_motor_status(),
            [this](bool success, const Command &) {
              // If already in Error state, setup was aborted by earlier failure
              if (state_ == State::Error) {
                return;  // Don't overwrite original error message
              }

              if (!success) {
                ESP_LOGE(TAG_ENGINE, "✗ Motor setup failed - final status check unsuccessful");
                transition_to(State::Error);
                parent_->setup_state_ = SetupState::FAILED;
                parent_->status_set_error(LOG_STR("Motor setup failed"));
                parent_->mark_failed();
                return;
              }

              // All setup commands completed successfully
              ESP_LOGI(TAG_ENGINE, "✓ Motor setup completed successfully");

              // Signal ESPHome that setup is complete
              parent_->setup_state_ = SetupState::COMPLETED;

              // Hardware polling is handled in update() with rate limiting

              ESP_LOGCONFIG("servoxxd", "ServoXxd Modbus setup complete");

              // Check if homing was already started during setup (SET_HOMING_PARAMETERS triggers GoHome)
              // In that case, state is already Homing - don't transition to Idle, let homing complete
              if (state_ == State::Homing) {
                ESP_LOGI(TAG_ENGINE, "Homing in progress from setup - waiting for completion");
                return;  // Homing will transition to Idle via process_motor_status_update()
              }

              // Execute homing at startup if configured (and not already started)
              if (parent_->homing_.at_startup && parent_->homing_.mode != HomingMode::NO_HOMING) {
                ESP_LOGI(TAG_ENGINE, "Executing homing at startup (mode=%d)", static_cast<int>(parent_->homing_.mode));
                // Start homing immediately after setup completes (skip Idle state)
                this->home();
              } else {
                // Only transition to Idle if no homing at startup
                if (state_ == State::SettingUp) {
                  transition_to(State::Idle);
                }
              }
            },
            Priority::SETUP);
      },
      Priority::SETUP);

  ESP_LOGCONFIG(TAG_ENGINE, "Setup: Motor initialization sequence enqueued");
}

// ============================================================================
// Main Update Loop
// ============================================================================

void StepperEngine::update() {
  // 1. Update CommandQueue (process timeouts, execute next command)
  if (queue_) {
    queue_->update();
  }

  // 2. Check state-specific timeouts
  check_state_timeouts();

  // 3. Process buffered commands
  if (disable_pending_ && state_ == State::Idle) {
    disable_pending_ = false;
    disable();  // Execute buffered disable
  }

  // 4. Hardware polling with rate limiting (max every 200ms)
  uint32_t now = millis();
  if (now - last_poll_time_ >= 200) {
    last_poll_time_ = now;
    poll_hardware();
  }
}

// ============================================================================
// Hardware Polling
// ============================================================================

void StepperEngine::poll_hardware() {
  if (parent_->setup_state_ == SetupState::COMPLETED) {
    // Poll all status values in sequence
    poll_encoder_position();
    poll_motor_speed();
    poll_motor_status();
    // TODO: Register 0x3E might not exist in hardware - investigate
    // poll_protection_status();
  }
}

// ============================================================================
// Movement Commands
// ============================================================================

void StepperEngine::move_to(const Position &target, std::optional<Speed> speed, std::optional<Acceleration> accel) {
  // Use default values if not provided
  Speed speed_units = speed.has_value() ? speed.value() : parent_->get_default_speed();
  Acceleration accel_units = accel.has_value() ? accel.value() : parent_->get_default_acceleration();

  // Validation: only allowed in Idle state (Position Mode)
  if (!validate_command(__func__, {State::Idle, State::Moving, State::Stopping})) {
    return;
  }

  // Update target position for target-reached detection (must be done before is_target_reached check)
  parent_->set_target_pos(target);

  // Check if already at target position - skip command if no movement needed
  if (is_target_reached()) {
    ESP_LOGD(TAG_ENGINE, "move_to: Already at target position - skipping");
    return;
  }

  // Send move command to hardware using Mode 4 (absolute by encoder ticks)
  // Mode 4 uses absolute encoder position - target is sent directly to hardware
  queue_->enqueue(CommandFactory::move_position_mode_4(speed_units, accel_units, target),
                  [this](bool success, const Command &) {
                    if (!success) {
                      ESP_LOGW(TAG_ENGINE, "move_to: Failed to send move command to hardware");
                      return;
                    }

                    // Only transition to Moving if not already in motion
                    // During Moving/Stopping: just update target (override behavior)
                    if (state_ == State::Moving) {
                      return;
                    }
                    transition_to(State::Moving);
                  });
}

void StepperEngine::stop(std::optional<Acceleration> decel) {
  // Validation: allowed in Moving, Running, Homing, Calibrating, Stopping states
  if (state_ == State::Idle) {
    return;
  }

  if (!validate_command(__func__,
                        {State::Moving, State::Running, State::Homing, State::Calibrating, State::Stopping})) {
    return;
  }

  // Send stop command via queue (Commandtype 0xFE STOP_POSITION_MODE_2)
  Acceleration decel_units = decel.has_value() ? decel.value() : parent_->get_default_acceleration();
  queue_->enqueue(CommandFactory::stop_position_mode_2(decel_units), nullptr);

  transition_to(State::Stopping);
}

void StepperEngine::emergency_stop() {
  ESP_LOGW(TAG_ENGINE, "emergency_stop(): Immediate halt, clearing queue");

  emergency_flag_ = true;

  // Clear command queue and send emergency stop
  if (queue_) {
    queue_->clear();  // Clear all pending commands

    // Send emergency stop command to hardware (Commandtype 0xF7 EMERGENCY_STOP)
    queue_->enqueue(CommandFactory::emergency_stop(), nullptr, Priority::CRITICAL);
  }

  transition_to(State::Error);
}

void StepperEngine::home() {
  // Get homing configuration from parent
  auto &homing = parent_->homing_;

  // Check if homing is configured
  if (homing.mode == HomingMode::NO_HOMING) {
    ESP_LOGW(TAG_ENGINE, "home(): No homing configured - action ignored");
    return;
  }

  // Note: Homing parameters are already configured in setup_motor()
  // This method only triggers the homing sequence

  switch (homing.mode) {
    case HomingMode::VIRTUAL: {
      // TODO: Implement VIRTUAL homing - Move to position 0 using move_to()
      ESP_LOGW(TAG_ENGINE, "VIRTUAL homing not yet implemented");
      /*
      // Virtual homing: Move to position 0 using normal positioning
      // Movement limited to ±180° (one revolution max)

      Position target = Position::from_steps(0, parent_);

      // Use ZeroingSpeed level to select appropriate speed
      // Map VERY_SLOW(0)→60 RPM, SLOW(1)→120, MEDIUM(2)→180, FAST(3)→240, VERY_FAST(4)→300
      float rpm = 60.0f + (static_cast<uint8_t>(homing.level) * 60.0f);
      Speed speed(rpm, SpeedUnit::RPM, parent_);

      // Get current position
      Position current = Position::from_steps(parent_->current_position, parent_);
      float current_steps = current.get_steps();
      float steps_per_rev = parent_->get_steps_per_revolution();

      // Calculate delta to position 0
      float delta = 0.0f - current_steps;

      // Normalize delta to [-steps_per_rev/2, +steps_per_rev/2] for NEAREST behavior
      while (delta > steps_per_rev / 2.0f)
        delta -= steps_per_rev;
      while (delta < -steps_per_rev / 2.0f)
        delta += steps_per_rev;

      // Store shortest path distance
      float shortest_distance = std::abs(delta);

      // Apply direction constraint
      if (homing.direction == HomingDirection::CW)
      {
        // Force clockwise: if shortest path is CCW (delta < 0), go the long way CW
        if (delta < 0)
          delta = steps_per_rev + delta; // Positive = CW
      }
      else if (homing.direction == HomingDirection::CCW)
      {
        // Force counter-clockwise: if shortest path is CW (delta > 0), go the long way CCW
        if (delta > 0)
          delta = delta - steps_per_rev; // Negative = CCW
      }
      // NEAREST: delta already contains shortest path

      // Validate ±180° constraint
      // Only NEAREST is guaranteed to be within one revolution
      // CW/CCW can require up to a full revolution if forcing the "wrong" direction
      if (homing.direction == HomingDirection::NEAREST)
      {
        // NEAREST is always ≤ 180°
        if (shortest_distance > steps_per_rev / 2.0f + 1.0f) // +1 for float tolerance
        {
          ESP_LOGE(TAG_ENGINE, "✗ VIRTUAL homing: Internal error - NEAREST path exceeds 180°");
          return;
        }
      }
      else
      {
        // CW/CCW: Warn if forced direction requires >180°
        if (std::abs(delta) > steps_per_rev / 2.0f)
        {
          ESP_LOGW(TAG_ENGINE, "VIRTUAL homing: Forced %s direction requires %.1f° movement (>180°)",
                   homing.direction == HomingDirection::CW ? "CW" : "CCW",
                   std::abs(delta) / steps_per_rev * 360.0f);
        }

        // Hard limit: Cannot move more than one full revolution
        if (std::abs(delta) > steps_per_rev)
        {
          ESP_LOGE(TAG_ENGINE, "✗ VIRTUAL homing: Movement exceeds one revolution (%.1f steps > %.1f)",
                   std::abs(delta), steps_per_rev);
          ESP_LOGE(TAG_ENGINE, "  Current position too far from zero - use set_zero() first");
          return;
        }
      }

      // Move to position 0
      move_to(target, speed, parent_->get_default_acceleration());

      */
      break;
    }

    case HomingMode::ENDSTOP: {
      // ENDSTOP homing: Trigger homing sequence (parameters already set in setup)

      queue_->enqueue(
          CommandFactory::go_home(),
          [this](bool success, const Command &) {
            if (success) {
              // Transition to Homing state - poll_homing_status() will monitor completion
              transition_to(State::Homing);
            } else {
              transition_to(State::Error);
              ESP_LOGW(TAG_ENGINE, "✗ Failed to start ENDSTOP homing");
            }
          },
          Priority::SETUP, 1000);  // 1 second timeout for go_home command
      break;
    }

    case HomingMode::SENSORLESS: {
      // TODO: Implement SENSORLESS homing - Use stall detection with no-limit parameters
      ESP_LOGW(TAG_ENGINE, "SENSORLESS homing not yet implemented");
      /*
      // SENSORLESS homing: Start movement (stall detection parameters already set)

      // Move in homing direction until stall detected
      int32_t large_target = (homing.direction == HomingDirection::CW) ? 1000000 : -1000000;
      Position target = Position::from_steps(large_target, parent_);

      queue_->enqueue(CommandFactory::move_position_mode_3(
          homing.speed,
          parent_->get_default_acceleration(),
          target),
                      [this](bool success, const Command &)
                      {
                        if (success)
                        {
                        }
                        else
                        {
                          ESP_LOGW(TAG_ENGINE, "✗ Failed to start SENSORLESS homing");
                          transition_to(State::Error);
                        }
                      });
      */
      break;
    }

    default:
      ESP_LOGE(TAG_ENGINE, "home(): Invalid homing mode: %d", static_cast<int>(homing.mode));
      return;
  }

  // Note: State transition to Homing happens in the callback for ENDSTOP mode
  // For VIRTUAL/SENSORLESS modes (when implemented), they handle transitions themselves
}

void StepperEngine::run_continuous(std::optional<Speed> speed, std::optional<Acceleration> accel) {
  // Validation: allowed in Idle or Running states (Speed Mode)
  if (!validate_command(__func__, {State::Idle, State::Running})) {
    return;
  }

  // Use default values if not provided (requires parent defaults)
  Speed speed_obj = speed.has_value() ? speed.value() : parent_->get_default_speed();
  Acceleration accel_obj = accel.has_value() ? accel.value() : parent_->get_default_acceleration();

  // Send speed command via queue (Commandtype 0xF6 MOVE_SPEED_MODE)
  queue_->enqueue(CommandFactory::move_speed_mode(speed_obj, accel_obj), nullptr);

  transition_to(State::Running);
}

// ============================================================================
// Configuration Commands
// ============================================================================

void StepperEngine::enable() {
  // Allow enable from Disabled, Idle, or Error states
  if (!validate_command(__func__, {State::Disabled, State::Idle})) {
    return;
  }

  // If already in Idle state, motor is already enabled
  if (state_ == State::Idle) {
    return;
  }

  // Send enable command via queue (Commandtype 0xF3 ENABLE_MOTOR)
  queue_->enqueue(CommandFactory::enable_motor(true), nullptr);

  transition_to(State::Idle);
}

void StepperEngine::disable() {
  // Validate allowed states (explicitly reject Homing and Calibrating)
  if (!validate_command(__func__,
                        {State::Idle, State::Disabled, State::Error, State::Moving, State::Running, State::Stopping})) {
    return;
  }

  // During motion: stop first, then disable will be triggered after stop completes
  if (state_ == State::Moving || state_ == State::Running || state_ == State::Stopping) {
    ESP_LOGI(TAG_ENGINE, "disable(): Motor in motion - stopping motor first, disable will follow after stop completes");
    disable_pending_ = true;
    stop();  // Stop first, disable() will be called again from update() when Idle is reached
    return;
  }

  // Already disabled - no-op
  if (state_ == State::Disabled) {
    return;
  }

  // Send disable command via queue (Commandtype 0xF3 ENABLE_MOTOR with false)
  queue_->enqueue(CommandFactory::enable_motor(false), nullptr);

  transition_to(State::Disabled);
}

void StepperEngine::release_protection() {
  if (!validate_command(__func__, {State::Error, State::Idle, State::Disabled})) {
    return;
  }

  // Always clear internal flags
  protection_triggered_ = false;
  emergency_flag_ = false;

  // Send release protection command via queue (Commandtype 0x3D RELEASE_PROTECTION)
  queue_->enqueue(CommandFactory::release_protection(), [](bool success, const Command &) {
    if (success) {
    } else {
      ESP_LOGW(TAG_ENGINE, "✗ Failed to send release protection");
    }
  });

  // Transition from Error to Idle to allow re-enabling
  if (state_ == State::Error) {
    transition_to(State::Idle);
  }
}

void StepperEngine::restart() {
  // Clear all errors
  protection_triggered_ = false;
  emergency_flag_ = false;

  // Clear queue
  if (queue_) {
    queue_->clear();
  }

  // Send restart command to hardware (Commandtype 0x41 RESTART with value 0x0001)
  // Motor needs 3-4 seconds to fully restart - use queue delay mechanism
  queue_->enqueue(CommandFactory::restart(), nullptr, Priority::NORMAL, 4000);

  transition_to(State::Idle);
}

void StepperEngine::calibrate() {
  if (!validate_command(__func__, {State::Idle, State::Disabled})) {
    return;
  }

  // Send calibrate encoder command via queue (Commandtype 0x80 CALIBRATE_ENCODER)
  queue_->enqueue(CommandFactory::calibrate_encoder(), [this](bool success, const Command &) {
    if (success) {
    } else {
      ESP_LOGW(TAG_ENGINE, "✗ Failed to start calibration");
      transition_to(State::Error);
    }
  });

  transition_to(State::Calibrating);
}

void StepperEngine::key_lock() {
  // Send key lock command via queue (Commandtype 0x8F SET_LOCK_KEYS)
  queue_->enqueue(CommandFactory::set_lock_keys(KeypadLock::LOCKED), nullptr);
}

void StepperEngine::key_unlock() {
  // Send key unlock command via queue (Commandtype 0x8F SET_LOCK_KEYS)
  queue_->enqueue(CommandFactory::set_lock_keys(KeypadLock::UNLOCKED), nullptr);
}

void StepperEngine::set_zero() {
  if (!validate_command(__func__, {State::Idle})) {
    return;
  }

  // Update position tracking only after hardware confirms
  queue_->enqueue(CommandFactory::set_zero(), [this](bool success, const Command &) {
    if (!success) {
      ESP_LOGW(TAG_ENGINE, "set_zero: Hardware command failed");
      return;
    }

    // Hardware confirmed - reset position tracking to zero
    parent_->position_offset_ = Position(0.0f, PositionUnit::STEPS, parent_);
    parent_->set_current_pos(Position(0.0f, PositionUnit::STEPS, parent_));

    ESP_LOGI(TAG_ENGINE, "set_zero: Hardware confirmed, encoder and offset reset to zero");
  });
}

// ============================================================================
// Status Queries
// ============================================================================

bool StepperEngine::is_moving() const {
  return state_ == State::Moving || state_ == State::Running || state_ == State::Homing ||
         state_ == State::Calibrating || state_ == State::Stopping;
}

const char *StepperEngine::state_to_string(State state) {
  switch (state) {
    case State::Disabled:
      return "Disabled";
    case State::SettingUp:
      return "SettingUp";
    case State::Idle:
      return "Idle";
    case State::Moving:
      return "Moving";
    case State::Running:
      return "Running";
    case State::Homing:
      return "Homing";
    case State::Calibrating:
      return "Calibrating";
    case State::Stopping:
      return "Stopping";
    case State::Error:
      return "Error";
    default:
      return "Unknown";
  }
}

// ============================================================================
// Transport Access
// ============================================================================

ITransport *StepperEngine::get_transport() const { return queue_ ? queue_->get_transport() : nullptr; }

// ============================================================================
// Private Methods - State Machine
// ============================================================================

void StepperEngine::transition_to(State new_state) {
  if (state_ == new_state) {
    return;  // No change
  }

  [[maybe_unused]] State old_state = state_;
  state_ = new_state;
  state_enter_time_ = millis();  // Track state entry time for timeout monitoring

  ESP_LOGD(TAG_ENGINE, "State transition: %s → %s", state_to_string(old_state), state_to_string(new_state));

  // State-specific initialization
  switch (new_state) {
    case State::Moving:
      // Start polling position more frequently (optional)
      break;

    case State::Idle:
      // Reset target position
      parent_->target_pos_ = parent_->current_pos_;
      // Reset recovery timer when leaving Error state
      last_recovery_attempt_time_ = 0;
      break;

    case State::Error:
      // Stop all motion immediately
      if (queue_) {
        queue_->clear();
      }
      // Reset recovery timer for fresh start
      last_recovery_attempt_time_ = 0;
      break;

    default:
      // Reset recovery timer when leaving Error state
      if (old_state == State::Error) {
        last_recovery_attempt_time_ = 0;
      }
      break;
  }
}

bool StepperEngine::validate_command(const char *func_name, std::initializer_list<State> allowed_states) {
  // Special handling for SettingUp state: Only allow critical commands
  if (state_ == State::SettingUp) {
    // During setup, only allow emergency stop, normal stop, and protection release
    bool is_critical = (strcmp(func_name, "emergency_stop") == 0);

    if (!is_critical) {
      ESP_LOGW(TAG_ENGINE, "%s(): Rejected during setup - motor still initializing", func_name);
      return false;
    }
    // Critical command during SettingUp - allow it
    return true;
  }

  for (State allowed : allowed_states) {
    if (state_ == allowed) {
      return true;  // Command allowed
    }
  }

  // Command not allowed in current state
  ESP_LOGW(TAG_ENGINE, "%s(): Rejected (state=%s)", func_name, state_to_string(state_));
  return false;
}

void StepperEngine::check_state_timeouts() {
  // State-specific timeout monitoring
  uint32_t now = millis();
  uint32_t state_duration = now - state_enter_time_;

  switch (state_) {
    case State::SettingUp:
      // Maximum setup duration: 30 seconds
      if (state_duration > 30000) {
        ESP_LOGE(TAG_ENGINE, "Setup timeout after %" PRIu32 " ms", state_duration);
        handle_error("Setup timeout - motor not responding");
      }
      break;

    case State::Homing:
      // Maximum homing duration: 600 seconds
      if (state_duration > 600000) {
        ESP_LOGE(TAG_ENGINE, "Homing timeout after %" PRIu32 " ms", state_duration);
        handle_error("Homing timeout");
      }
      break;

    case State::Calibrating:
      // Maximum calibration duration: 120 seconds
      if (state_duration > 120000) {
        ESP_LOGE(TAG_ENGINE, "Calibration timeout after %" PRIu32 " ms", state_duration);
        handle_error("Calibration timeout");
      }
      break;

    case State::Stopping:
      // Maximum stop duration: 50 seconds
      if (state_duration > 50000) {
        ESP_LOGE(TAG_ENGINE, "Stopping timeout after %" PRIu32 " ms", state_duration);
        // Force transition to Idle even if not at standstill
        transition_to(State::Idle);
      }
      break;

    case State::Error:
      // Auto-recovery: After ERROR_RECOVERY_DELAY_MS, attempt recovery
      if (state_duration > ERROR_RECOVERY_DELAY_MS) {
        uint32_t time_since_last_attempt = now - last_recovery_attempt_time_;
        // First attempt or interval elapsed since last attempt
        if (last_recovery_attempt_time_ == 0 || time_since_last_attempt > ERROR_RECOVERY_INTERVAL_MS) {
          attempt_error_recovery();
        }
      }
      break;

    default:
      // No timeout for other states
      break;
  }
}

void StepperEngine::poll_encoder_position(std::function<void(const Position &)> callback) {
  // Enqueue read command for encoder position (Commandtype 0x30 READ_ENCODER_CARRY)
  // Expected response: carry (int32_t) + value (uint16_t) = 6 bytes
  queue_->enqueue(
      CommandFactory::read_encoder_carry(),
      [this, callback](bool success, const Command &cmd) {
        if (success) {
          auto position = CommandDecoder::read_encoder_carry(cmd, parent_);
          process_encoder_update(position);
          parent_->set_current_pos(position);
        }
      },
      Priority::BACKGROUND);
}

// ============================================================================
// Private Methods - Event Processing
// ============================================================================

void StepperEngine::process_encoder_update(const Position &position) {
  Position old_position = parent_->current_pos_;
  parent_->set_current_pos(position);

  // Check if target reached (in Moving state)
  // if (state_ == State::Moving && is_target_reached()) {
  //   transition_to(State::Idle);
  // }
}

void StepperEngine::process_speed_update(const Speed &speed) {
  Speed old_speed = current_speed_;
  current_speed_ = speed;
}

void StepperEngine::process_motor_status_update(CommandDecoder::MotorStatus status) {
  // Hardware status validates Engine state - Engine state is leading
  // Only handle critical errors or completion signals
  switch (status) {
    case CommandDecoder::MotorStatus::FAIL:
      // Hardware docs: FAIL (status=0) means "read fail" - no valid status available
      // This is NORMAL when motor is idle/disabled, not an error condition
      break;

    case CommandDecoder::MotorStatus::STOP:
      // Hardware reports standstill - validate against Engine expectations
      // If Engine expects motion but hardware stopped → Error
      if (state_ == State::Moving || state_ == State::Running) {
        ESP_LOGW(TAG_ENGINE, "Unexpected stop: Engine expected motion but hardware stopped");
        transition_to(State::Stopping);
      }
      // If Engine is Stopping and hardware confirms → Idle
      else if (state_ == State::Stopping) {
        transition_to(State::Idle);
      }
      // If Engine is Homing and hardware stopped → Homing completed successfully
      else if (state_ == State::Homing) {
        ESP_LOGI(TAG_ENGINE, "✓ Homing completed successfully (status: HOMING → STOP)");
        // Reset position to zero after successful homing
        Position zero_pos = Position::from_steps(0, parent_);
        parent_->set_current_pos(zero_pos);
        parent_->set_target_pos(zero_pos);
        homed_ = true;
        transition_to(State::Idle);
      }
      // If Engine is Calibrating and hardware stopped → Calibration completed
      else if (state_ == State::Calibrating) {
        ESP_LOGI(TAG_ENGINE, "✓ Calibration completed (status: CALIBRATING → STOP)");
        transition_to(State::Idle);
      }
      break;

    case CommandDecoder::MotorStatus::SPEED_UP:
    case CommandDecoder::MotorStatus::SPEED_DOWN:
    case CommandDecoder::MotorStatus::FULL_SPEED:
      // Hardware reports motion - validate against Engine expectations
      // If Engine expects Idle but hardware moving → Inconsistency warning
      if (state_ == State::Idle || state_ == State::Disabled) {
        ESP_LOGW(TAG_ENGINE, "Unexpected motion: Hardware moving but engine state is %s", state_to_string(state_));
        // Don't change state - let engine commands control state
      }
      break;

    case CommandDecoder::MotorStatus::HOMING:
      // Hardware reports homing in progress
      if (state_ == State::Homing) {
        // Motor is still homing - stay in Homing state
        // Completion is detected when hardware transitions to STOP (handled above)
        // Do NOT check position here - position may not be zero yet during homing
      }
      // If Engine is NOT in Homing state → Someone else started homing (physical buttons?)
      // CRITICAL: Ignore hardware status sync during SettingUp to prevent state overwrites
      else if (state_ != State::SettingUp) {
        ESP_LOGW(TAG_ENGINE, "Unexpected homing: Hardware homing but engine state is %s", state_to_string(state_));
        ESP_LOGW(TAG_ENGINE, "  Possible cause: Manual homing via physical buttons");
        // Sync engine state to hardware reality
        transition_to(State::Homing);
      }
      break;

    case CommandDecoder::MotorStatus::CALIBRATING:
      // Hardware reports calibration - validate against Engine expectations
      // If Engine is NOT in Calibrating state → Manual calibration started
      // CRITICAL: Ignore hardware status sync during SettingUp to prevent state overwrites
      if (state_ != State::Calibrating && state_ != State::SettingUp) {
        ESP_LOGW(TAG_ENGINE, "Unexpected calibration: Hardware calibrating but engine state is %s",
                 state_to_string(state_));
        // Sync engine state to hardware reality
        transition_to(State::Calibrating);
      }
      break;
    default:
      ESP_LOGW(TAG_ENGINE, "Unknown motor status received: %d", static_cast<int>(status));
      break;
  }
}

void StepperEngine::process_protection_update(uint8_t protected_status) {
  bool old_protection = protection_triggered_;
  protection_triggered_ = (protected_status != 0);

  // Transition to Error state if protection triggered
  if (protection_triggered_ && !old_protection) {
    ESP_LOGE(TAG_ENGINE, "Protection triggered! Status=0x%02X", protected_status);
    ESP_LOGE(TAG_ENGINE, "  Current state: %s", state_to_string(state_));
    ESP_LOGE(TAG_ENGINE,
             "  Current position: %" PRId64 " steps (%.2f rev), Target position: %" PRId64 " steps (%.2f rev)",
             parent_->current_pos_.get_steps(), parent_->current_pos_.get_revolutions(),
             parent_->target_pos_.get_steps(), parent_->target_pos_.get_revolutions());
    ESP_LOGE(TAG_ENGINE, "  Current speed: %.2f RPM", current_speed_.rpm());
    handle_error("Locked-rotor protection triggered");
  }
}

bool StepperEngine::is_target_reached() {
  // Check if current position is within tolerance of target
  Position delta = parent_->current_pos_ - parent_->target_pos_;
  Position tolerance = Position::from_ticks(1.0f, parent_);
  // Use absolute value to check distance in both directions
  return delta.abs() <= tolerance;
}

void StepperEngine::attempt_error_recovery() {
  // Only attempt recovery in Error state
  if (state_ != State::Error) {
    return;
  }

  // Update last attempt time
  last_recovery_attempt_time_ = millis();

  ESP_LOGW(TAG_ENGINE, "Attempting automatic error recovery...");

  // Query motor status to check actual hardware state
  queue_->enqueue(
      CommandFactory::read_motor_status(),
      [this](bool success, const Command &cmd) {
        if (!success) {
          ESP_LOGW(TAG_ENGINE, "Recovery: Failed to query motor status - will retry later");
          return;
        }

        CommandDecoder::MotorStatus status = CommandDecoder::read_motor_status(cmd);

        // Check if motor is in a recoverable state
        if (status == CommandDecoder::MotorStatus::STOP || status == CommandDecoder::MotorStatus::FAIL) {
          // Motor is stopped or idle - attempt release_protection
          ESP_LOGI(TAG_ENGINE, "Recovery: Motor status OK (%d) - releasing protection", static_cast<int>(status));

          // Clear internal flags and release protection
          protection_triggered_ = false;
          emergency_flag_ = false;

          queue_->enqueue(
              CommandFactory::release_protection(),
              [this](bool release_success, const Command &) {
                if (release_success) {
                  ESP_LOGI(TAG_ENGINE, "Recovery: Protection released - returning to Idle");
                  transition_to(State::Idle);
                  // Startup homing never completed (e.g. it failed) - position reference is unknown,
                  // so re-run homing instead of accepting moves against an unreferenced position
                  if (!homed_ && parent_->homing_.at_startup && parent_->homing_.mode != HomingMode::NO_HOMING) {
                    ESP_LOGI(TAG_ENGINE, "Recovery: Startup homing not completed - retrying homing");
                    this->home();
                  }
                } else {
                  ESP_LOGW(TAG_ENGINE, "Recovery: Failed to release protection - will retry later");
                }
              },
              Priority::SETUP);  // Recovery commands get high priority
        } else {
          // Motor is busy (moving, homing, calibrating) - wait for it to finish
          ESP_LOGW(TAG_ENGINE, "Recovery: Motor busy (status=%d) - waiting for completion", static_cast<int>(status));
        }
      },
      Priority::SETUP);  // Use SETUP priority - recovery should happen before normal commands
}

void StepperEngine::handle_error(const char *error_message) {
  ESP_LOGE(TAG_ENGINE, "Error: %s", error_message);
  transition_to(State::Error);
}

}  // namespace servoxxd
}  // namespace esphome
