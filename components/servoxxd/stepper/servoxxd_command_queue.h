#pragma once

#include "esphome/core/log.h"
#include "servoxxd_transport.h"
#include "servoxxd_commands.h"
#include <vector>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace esphome {
namespace servoxxd {

/**
 * @brief Commandtype priority levels
 *
 * Time-penalty based scheduling with automatic age-promotion:
 * - CRITICAL: Emergency stop (always first among pending commands)
 * - SETUP: Motor initialization commands (after CRITICAL, before all other priorities)
 * - NORMAL: Movements, configuration (effective_time = enqueued_time, FIFO)
 * - BACKGROUND: Important status reads like position (effective_time = enqueued_time + penalty)
 * - IDLE: Debug/UI data like temperature (effective_time = enqueued_time + penalty)
 */
enum class Priority : uint8_t {
  CRITICAL = 0,    // Emergency - always first (no penalty)
  SETUP = 1,       // Setup commands - before NORMAL, ensures setup completes first
  NORMAL = 2,      // Standard - FIFO (no penalty)
  BACKGROUND = 3,  // Important reads - penalty configured
  IDLE = 4         // Debug/UI reads - penalty configured
};

/**
 * @brief Commandtype state for state machine
 *
 * From spec: "Each command follows a lifecycle state machine"
 */
enum class CommandState : uint8_t {
  PENDING = 0,  // Commandtype in queue, not yet sent
  EXECUTING,    // Commandtype sent to transport, waiting for response
  COMPLETED,    // Response received and processed successfully
  FAILED,       // Transport error received
  TIMEOUT       // No response within timeout period
};

/**
 * @brief Commandtype queue for serialized transport execution
 *
 * From spec (02c-layer3-command-queue.md):
 * - Single-flight execution: Only one command in EXECUTING state at any time
 * - Timeout handling: Abort commands on timeout, advance to next
 * - Deduplication: Coalesce multiple identical read commands
 * - Priority: Emergency commands (EMERGENCY_STOP) clear pending queue
 * - Callback support: Notify when command completes (success or failure)
 *
 * **Single-Flight Guarantee:**
 * - An EXECUTING front command prevents another send
 * - Callback dispatch also blocks sends, including reentrant enqueue/update calls
 * - Completion removes the command before notifying all its requesters
 *
 * **Integration with Layer 4:**
 * - Uses ITransport interface for protocol-agnostic communication
 * - Commands identified by Commandtype enum, not raw function codes
 * - Transport callbacks forwarded to StepperEngine (Layer 2)
 *
 * @see docs/specification/02c-layer3-command-queue.md
 */
class CommandQueue {
 public:
  /**
   * @brief Commandtype callback signature
   *
   * @param success True if command succeeded, false on error/timeout
   * @param cmd Command object with populated response field (for read commands)
   */
  using CommandCallback = std::function<void(bool success, const Command &cmd)>;

  /**
   * @brief Construct a new Commandtype Queue object
   *
   * @param transport Transport layer interface (Layer 4)
   * @param timeout_ms Default timeout for commands in milliseconds (default: 1000ms)
   */
  CommandQueue(ITransport *transport, uint32_t timeout_ms = 1000);

  /**
   * @brief Update the command queue (called from ServoXxd::loop())
   *
   * From spec: "Called each loop iteration to detect stuck commands"
   * - Check timeout on current executing command
   * - Opportunistic execution: start next command if queue idle
   */
  void update();

  /**
   * @brief Enqueue a command
   *
   * @param cmd Command object with type and payload
   * @param callback Notified once on completion or cancellation, including for deduplicated requests
   * @param priority Command priority (CRITICAL/SETUP/NORMAL/BACKGROUND/IDLE, default: NORMAL)
   * @param delay_before_next_ms Delay in milliseconds before executing next command (default: 0)
   *                             Used for commands that need recovery time (e.g. RESTART needs 4000ms)
   *                             Success callbacks wait for this delay; errors notify immediately
   *                             but delay the next send. Queue timeouts advance immediately.
   * @param deduplicate Optional: true=force dedup, false=force no dedup, nullopt=auto (default: nullopt)
   *                    Auto mode: BACKGROUND/IDLE commands are deduplicated, others are not
   */
  void enqueue(const Command &cmd, CommandCallback callback, Priority priority = Priority::NORMAL,
               uint32_t delay_before_next_ms = 0, std::optional<bool> deduplicate = std::nullopt);

  /**
   * @brief Handle command response (from ITransport callback)
   *
   * From spec: "Completes command, clears guard, calls execute_next()"
   *
   * @param cmd Command that completed (contains type and populated response field)
   */
  void on_response(const Command &cmd);

  /**
   * @brief Handle command error (from ITransport callback)
   *
   * From spec: "Fails command, clears guard, continues"
   *
   * @param cmd Command that failed (contains type and original payload)
   * @param error Error code from transport
   */
  void on_error(const Command &cmd, ErrorCode error);

  /**
   * @brief Clear all pending commands
   *
   * From spec: "emergency_stop shall clear all pending (non-executing) commands"
   * - Invoke all callbacks with success=false
   * - Clear queue (except currently executing command)
   * - Does NOT clear executing command (that must complete/timeout naturally)
   */
  void clear();

  /**
   * @brief Check if queue is empty
   */
  bool is_empty() const { return queue_.empty(); }

  /**
   * @brief Get queue size (includes executing command if present)
   */
  size_t size() const { return queue_.size(); }

  /**
   * @brief Get transport instance
   * @return Pointer to transport layer
   */
  ITransport *get_transport() const { return transport_; }

 private:
  /**
   * @brief Command structure with state machine
   */
  struct QueuedCommand {
    Command command;  // Command object (contains type and payload)
    std::vector<CommandCallback> callbacks;
    CommandState state;             // State machine state
    Priority priority;              // Command priority (for time-penalty scheduling)
    uint32_t enqueued_time;         // millis() when enqueued (for age-based scheduling)
    uint32_t sent_time;             // millis() when sent (for timeout)
    uint32_t delay_before_next_ms;  // Delay before next command (e.g. motor restart)

    QueuedCommand(const Command &cmd, CommandCallback cb, Priority prio, uint32_t delay, uint32_t enqueued)
        : command(cmd),
          state(CommandState::PENDING),
          priority(prio),
          enqueued_time(enqueued),
          sent_time(0),
          delay_before_next_ms(delay) {
      if (cb) {
        callbacks.push_back(std::move(cb));
      }
    }
  };

  // Queue and execution state
  std::deque<QueuedCommand> queue_;  // FIFO queue (deque for efficient reordering)
  ITransport *transport_{nullptr};   // Transport layer interface
  uint32_t delay_started_ms_{0};
  uint32_t delay_duration_ms_{0};
  bool dispatching_callbacks_{false};
  std::function<void()> pending_callback_{nullptr};  // Callback to invoke after delay

  // Configuration
  uint32_t timeout_ms_{1000};                               // Default timeout (1 second)
  static constexpr uint32_t BACKGROUND_PENALTY_MS = 10000;  // BACKGROUND commands delayed by 10s
  static constexpr uint32_t IDLE_PENALTY_MS = 60000;        // IDLE commands delayed by 60s

  /**
   * @brief Execute next pending command (if not already executing)
   *
   * From spec: "checks execution guard, sends next command if idle"
   * - If executing or dispatching callbacks: return immediately
   * - Deliver delayed callbacks even if no commands remain
   * - Get next PENDING command, transition to EXECUTING, send via transport
   */
  void execute_next();

  /**
   * @brief Check for timeout on executing command
   *
   * From spec: "Called periodically in update() to detect stuck commands"
   * - Check if current command exceeded timeout_ms_
   * - If timeout: transition to TIMEOUT state, invoke callbacks, clear guard
   */
  void check_timeout();

  /**
   * @brief Find duplicate command for deduplication
   *
   * Requirements:
   * - Same Command (type AND payload)
   * - PENDING state (not EXECUTING or completed)
   * - Skips EXECUTING command at front of queue
   *
   * Deduplication retains all callbacks, the highest priority, and the newest delay.
   *
   * @param cmd Full Command object (type + payload) to search for
   * @return Iterator to existing command if found, queue_.end() otherwise
   */
  std::deque<QueuedCommand>::iterator find_duplicate(const Command &cmd);

  /**
   * @brief Prepare next command for execution with time-penalty scheduling
   *
   * Implements age-based fairness policy:
   * - CRITICAL then SETUP: strict precedence, FIFO within each priority
   * - NORMAL/BACKGROUND/IDLE: lowest penalty minus elapsed age first
   * - Equivalent effective times retain insertion order
   *
   * Automatic age-promotion: Old low-priority commands eventually overtake newer high-priority ones
   * Example: Old BACKGROUND commands can execute before newer NORMAL commands
   *
   * Side-effect: Moves command with lowest effective_time to front of queue
   */
  void prepare_next_command();

  /**
   * @brief Calculate effective execution time for a command
   *
   * @param cmd Commandtype to calculate for
   * @param now Current time (millis())
   * Uses elapsed unsigned time to handle millis() rollover for waits under one full clock cycle.
   * @return Effective time relative to now (lower values execute first)
   */
  int64_t calculate_effective_time(const QueuedCommand &cmd, uint32_t now);

  void notify_callbacks(std::vector<CommandCallback> callbacks, bool success, const Command &cmd);
};

}  // namespace servoxxd
}  // namespace esphome
