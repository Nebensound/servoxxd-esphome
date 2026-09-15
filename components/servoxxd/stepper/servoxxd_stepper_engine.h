#pragma once

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "servoxxd_command_queue.h"
#include "servoxxd_commands.h"
#include "servoxxd_command_decoder.h"
#include "servoxxd_transport.h"
#include "servoxxd_position.h"
#include "servoxxd_speed.h"
#include "servoxxd_acceleration.h"
#include <functional>
#include <cmath>
#include <optional>
#include <string>

namespace esphome {
namespace servoxxd {

// Forward declaration
class ServoXxd;

// Import types from servoxxd_modbus
using servoxxd::Acceleration;
using servoxxd::AccelerationUnit;
using servoxxd::CommandQueue;
using servoxxd::Position;
using servoxxd::PositionUnit;
using servoxxd::Speed;
using servoxxd::SpeedUnit;

/**
 * @brief State machine states
 *
 * States define what operations are allowed and how the motor responds to commands.
 * See specification 02-cpp-interface.md for complete state transition table.
 */
enum class State {
  Disabled,     // Motor disabled, no motion possible
  SettingUp,    // Motor initialization in progress (setup_motor running)
  Idle,         // Motor ready, waiting for commands
  Moving,       // Position movement in progress (Position Mode only)
  Running,      // Continuous rotation in progress (Speed Mode only)
  Homing,       // Homing process in progress
  Calibrating,  // Encoder calibration in progress
  Stopping,     // Controlled stop in progress (with deceleration)
  Error         // Error occurred (Protection, Modbus error, Timeout)
};

/**
 * @brief Core movement and state machine logic for Servo42D motor
 *
 * StepperEngine encapsulates all movement, homing, and stop logic as state machine.
 * It manages the CommandQueue, processes Modbus callbacks, and handles state transitions.
 *
 * Responsibilities:
 * - Processes all movement commands (move_to, stop, home, run_continuous)
 * - Monitors and controls internal states (Idle, Moving, Homing, Error, Disabled)
 * - Manages CommandQueue and processes Modbus callbacks (response, error, timeout)
 * - Regularly updates status values (encoder position, speed, protection) using hybrid strategy
 * - Validates commands based on current state using command validation matrix
 *
 * Design: Strict separation of concerns with ServoXxd parent (configuration, status, helpers)
 */
class StepperEngine {
 public:
  /**
   * @brief Constructor
   *
   * @param parent Pointer to parent ServoXxd component (configuration, helpers)
   * @param transport Pointer to ITransport for Layer 3 integration
   * @param command_timeout_ms Timeout for commands (default: 1000ms)
   */
  StepperEngine(ServoXxd *parent, ITransport *transport = nullptr, uint32_t command_timeout_ms = 4000);

  ~StepperEngine();

  /**
   * @brief Initialize motor with configuration from parent
   *
   * Enqueues setup commands to CommandQueue:
   * - SET_SUBDIVISION (microstepping)
   * - SET_WORKING_CURRENT
   * - SET_WORK_MODE (position/speed)
   * - SET_ZERO (reset position to 0)
   *
   * Must be called after construction, before any movement commands.
   * Commands are executed asynchronously through CommandQueue.
   *
   * TODO: Add initial motor state query
   * - Query encoder position (0x36)
   * - Query motor status (0x3A)
   * - Query protection status (0x3E)
   * - Query current speed (0x32)
   */
  void setup_motor();

  /**
   * @brief Update method called cyclically from main loop
   *
   * Responsibilities:
   * - Process state machine transitions
   * - Execute CommandQueue update (timeouts, next command)
   * - Check for state-specific timeouts
   * - Process buffered commands
   *
   * Must be called regularly (e.g., every 10-50ms) for responsive operation.
   * Note: Hardware polling is triggered externally via poll_hardware().
   */
  void update();

  // ============================================================================
  // Movement Commands
  // ============================================================================

  /**
   * @brief Move to absolute target position
   *
   * @param target Target position (absolute, with unit)
   * @param speed Optional movement speed (overrides default), std::nullopt = use default
   * @param accel Optional acceleration (overrides default), std::nullopt = use default
   *
   * Validation:
   * - Only allowed in Idle state (Position Mode only)
   * - During Moving/Stopping: replaces target (override behavior)
   * - Rejected in other states (Error, Disabled, Running, Homing)
   *
   * State transition: Idle → Moving
   */
  void move_to(const Position &target, std::optional<Speed> speed = std::nullopt,
               std::optional<Acceleration> accel = std::nullopt);

  /**
   * @brief Stop motor with controlled deceleration
   *
   * @param decel Optional deceleration (overrides default), std::nullopt = use default
   *
   * Validation:
   * - Allowed in Moving, Running, Homing, Stopping states
   * - No-op in Idle (already stopped)
   * - Rejected in Disabled, Error states
   *
   * State transition: Moving/Running/Homing → Stopping → Idle (when speed = 0)
   */
  void stop(std::optional<Acceleration> decel = std::nullopt);

  /**
   * @brief Emergency stop - immediate halt without deceleration
   *
   * Allowed in all states. Clears command queue, disables motor, transitions to Error state.
   * Requires release_protection() or restart() to recover.
   *
   * State transition: * → Error
   */
  void emergency_stop();

  /**
   * @brief Start homing sequence
   *
   * Uses homing configuration from parent (mode, direction, speed, etc.)
   *
   * Validation:
   * - Only allowed in Idle state (Position Mode only)
   * - Rejected in other states
   *
   * State transition: Idle → Homing → Idle (when complete)
   */
  void home();

  /**
   * @brief Run motor continuously at specified speed
   *
   * @param speed Continuous rotation speed (positive = CCW, negative = CW), std::nullopt = use last/default
   * @param accel Acceleration for speed ramp, std::nullopt = use last/default
   *
   * Validation:
   * - Only allowed in Idle or Running states (Speed Mode only)
   * - Rejected in other states
   *
   * State transition: Idle → Running
   */
  void run_continuous(std::optional<Speed> speed = std::nullopt, std::optional<Acceleration> accel = std::nullopt);

  // ============================================================================
  // Configuration Commands
  // ============================================================================

  /**
   * @brief Enable motor
   *
   * Validation:
   * - Only allowed in Disabled state
   * - Rejected in other states
   *
   * State transition: Disabled → Idle
   */
  void enable();

  /**
   * @brief Disable motor
   *
   * Validation:
   * - Allowed in Idle, Error states
   * - During motion (Moving/Running/Homing/Stopping): buffered, executed after stop
   * - Rejected in Disabled state (already disabled)
   *
   * State transition: Idle → Disabled (or via Stopping → Idle → Disabled)
   */
  void disable();

  /**
   * @brief Release protection and clear error state
   *
   * Validation:
   * - Allowed in Error state
   * - Also allowed in Disabled, Idle (no-op if no error)
   * - Rejected during motion
   *
   * State transition: Error → Idle (if protection cleared successfully)
   */
  void release_protection();

  /**
   * @brief Restart motor (full reset)
   *
   * Validation:
   * - Allowed in Disabled, Idle, Error states
   * - Rejected during motion
   *
   * Clears all errors, resets state, re-initializes motor.
   */
  void restart();

  /**
   * @brief Calibrate encoder
   *
   * Starts encoder calibration sequence.
   */
  void calibrate();

  /**
   * @brief Lock physical keys on motor controller
   */
  void key_lock();

  /**
   * @brief Unlock physical keys on motor controller
   */
  void key_unlock();

  /**
   * @brief Set current position as zero reference
   *
   * Validation:
   * - Only allowed in Idle state
   * - Rejected in other states
   */
  void set_zero();

  // ============================================================================
  // Hardware Polling Methods (called from parent's set_interval)
  // ============================================================================

  /**
   * @brief Poll all hardware status values
   *
   * Queries encoder position, speed, motor status, and protection status.
   * Should be called periodically from parent component's set_interval().
   */
  void poll_hardware();

  /**
   * @brief Poll motor speed from hardware
   *
   * Sends Commandtype 0x32 to read real-time RPM.
   * Used for state transitions (e.g., Moving → Idle when speed reaches 0).
   */
  void poll_motor_speed();

  /**
   * @brief Poll motor status (enabled/disabled)
   *
   * Sends Commandtype 0x3A to read motor enable state.
   * Monitors sleep_when_done behavior.
   */
  void poll_motor_status();

  /**
   * @brief Poll protection status
   *
   * Sends Commandtype 0x3E to read locked-rotor protection.
   * Non-zero value triggers Error state transition.
   */
  void poll_protection_status();

  /**
   * @brief Query encoder position (Commandtype 0x30)
   *
   * Reads encoder carry + value, calculates absolute position.
   * @param callback Optional callback to receive position result
   */
  void poll_encoder_position(std::function<void(const Position &)> callback = nullptr);

  /**
   * @brief Get current state
   *
   * @return Current state machine state
   */
  State get_state() const { return state_; }

  /**
   * @brief Set state (for testing only)
   *
   * @param new_state State to set
   */
  void set_state(State new_state) { state_ = new_state; }

  /**
   * @brief Get state name (for logging/debugging)
   *
   * @param state State to convert
   * @return Human-readable state name
   */
  static const char *state_to_string(State state);

  /**
   * @brief Get current state as string
   *
   * @return Current state as string
   */
  std::string get_state_string() const { return state_to_string(state_); }

  /**
   * @brief Get transport instance
   * @return Pointer to transport (may be nullptr if using CommandQueue's internal transport)
   */
  ITransport *get_transport() const;

  /**
   * @brief Check if motor is currently moving
   *
   * @return True if state is Moving, Running, Homing, or Stopping
   */
  bool is_moving() const;

  // ============================================================================
  // Callbacks
  // ============================================================================

 private:
  // ============================================================================
  // Private Members
  // ============================================================================

  ServoXxd *parent_;     ///< Parent component (configuration, helpers)
  CommandQueue *queue_;  ///< Commandtype queue for serial Modbus execution
  State state_;          ///< Current state machine state
  bool emergency_flag_;  ///< Emergency stop flag (requires restart)

  // Status tracking
  Speed current_speed_;        ///< Last known motor speed (RPM)
  bool protection_triggered_;  ///< Protection triggered flag

  // State timing
  uint32_t state_enter_time_;                                    ///< State entry timestamp for timeout tracking
  uint32_t last_recovery_attempt_time_{0};                       ///< Last recovery attempt timestamp
  bool homed_{false};                                            ///< Set once a homing sequence completed successfully
  uint32_t last_poll_time_{0};                                   ///< Last hardware poll (per instance, multi-motor safe)
  static constexpr uint32_t ERROR_RECOVERY_DELAY_MS = 5000;      ///< Delay before first recovery attempt (5s)
  static constexpr uint32_t ERROR_RECOVERY_INTERVAL_MS = 10000;  ///< Interval between recovery attempts (10s)

  // Buffered commands (for commands that need to be deferred)
  bool disable_pending_;  ///< Disable command buffered (execute after stop)

  // ============================================================================
  // Private Methods - State Machine
  // ============================================================================

  /**
   * @brief Transition to new state
   *
   * Logs state change and performs state-specific initialization.
   *
   * @param new_state Target state
   */
  void transition_to(State new_state);

  /**
   * @brief Validate if command is allowed in current state
   *
   * @param command_name Commandtype name (for logging)
   * @param allowed_states List of allowed states
   * @return True if command is allowed
   */
  bool validate_command(const char *command_name, std::initializer_list<State> allowed_states);

  /**
   * @brief Check state-specific timeouts
   *
   * Monitors movement timeout, homing timeout, etc.
   * Transitions to Error state if timeout exceeded.
   */
  void check_state_timeouts();

  /**
   * @brief Attempt automatic recovery from Error state
   *
   * Queries motor status to check if error condition persists.
   * If motor reports OK, calls release_protection() to return to Idle.
   * Called automatically after ERROR_RECOVERY_DELAY_MS in Error state.
   */
  void attempt_error_recovery();

  // ============================================================================
  // Private Methods - Event Processing
  // ============================================================================

  /**
   * @brief Process encoder position update
   *
   * Updates current_position_, checks target reached, invokes callback.
   *
   * @param position The encoder position from hardware
   */
  void process_encoder_update(const Position &position);

  /**
   * @brief Process speed update
   *
   * Updates current_speed_, checks standstill, invokes callback.
   *
   * @param speed The current speed from hardware
   */
  void process_speed_update(const Speed &speed);

  /**
   * @brief Process motor status update
   *
   * Sets Motor Status
   *
   * @param status Motor status enum
   */
  void process_motor_status_update(CommandDecoder::MotorStatus status);

  /**
   * @brief Process protection status update
   *
   * Updates protection_triggered_, transitions to Error if protected.
   *
   * @param protected_status Protection status (0 = OK, 1 = protected)
   */
  void process_protection_update(uint8_t protected_status);

  /**
   * @brief Check if target position reached
   *
   * Compares current_position_ with target_position_ (threshold tolerance).
   * Triggers transition Moving → Idle if reached.
   *
   * @return True if target reached
   */
  bool is_target_reached();

  /**
   * @brief Handle error condition
   *
   * Transitions to Error state, logs error, invokes callbacks.
   *
   * @param error_message Error description
   */
  void handle_error(const char *error_message);
};

}  // namespace servoxxd
}  // namespace esphome
