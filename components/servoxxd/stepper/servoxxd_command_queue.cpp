#include "servoxxd_command_queue.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace servoxxd {

static const char *const TAG = "servoxxd.queue";

CommandQueue::CommandQueue(ITransport *transport, uint32_t timeout_ms)
    : transport_(transport), timeout_ms_(timeout_ms) {
  ESP_LOGCONFIG(TAG, "CommandQueue initialized: timeout=%" PRIu32 "ms", timeout_ms_);

  // Register callbacks with transport layer
  if (transport_) {
    transport_->set_response_callback([this](const Command &cmd) { this->on_response(cmd); });

    transport_->set_error_callback([this](const Command &cmd, ErrorCode error) { this->on_error(cmd, error); });
  }
}

void CommandQueue::update() {
  // CRITICAL: Update transport layer first to check for timeouts!
  // Without this, transport stays stuck in WAITING_READ/WAITING_WRITE forever
  if (transport_) {
    transport_->update();
  }

  // From spec: "Called each loop iteration to detect stuck commands"
  check_timeout();

  // Always call execute_next() - it handles:
  // 1. Pending callbacks waiting for the recovery delay
  // 2. Starting next command when idle
  // 3. Early exit if command is executing
  execute_next();
}

void CommandQueue::enqueue(const Command &cmd, CommandCallback callback, Priority priority,
                           uint32_t delay_before_next_ms, std::optional<bool> deduplicate) {
  // Determine deduplication: Auto-decision based on priority if not specified
  bool should_deduplicate = deduplicate.value_or(priority == Priority::BACKGROUND || priority == Priority::IDLE);

  // Deduplication: Check if identical command exists (only if enabled)
  if (should_deduplicate) {
    auto existing = find_duplicate(cmd);
    if (existing != queue_.end()) {
      // Update existing command with new data
      existing->command.payload = cmd.payload;
      existing->priority = std::min(priority, existing->priority);
      existing->delay_before_next_ms = delay_before_next_ms;
      if (callback) {
        existing->callbacks.push_back(std::move(callback));
      }
      return;
    }
  }

  queue_.emplace_back(cmd, std::move(callback), priority, delay_before_next_ms, millis());

  // Try to execute immediately if idle
  execute_next();
}

void CommandQueue::on_response(const Command &response_cmd) {
  if (queue_.empty() || queue_.front().state != CommandState::EXECUTING) {
    ESP_LOGW(TAG, "Unexpected response for command 0x%04X (no executing command)", response_cmd.register_address());
    return;
  }

  auto &current_cmd = queue_.front();
  if (current_cmd.command.command_type != response_cmd.command_type) {
    ESP_LOGW(TAG, "Response mismatch: expected 0x%04X, got 0x%04X", current_cmd.command.register_address(),
             response_cmd.register_address());
    return;
  }

  Command response = response_cmd;
  auto callbacks = std::move(current_cmd.callbacks);
  delay_started_ms_ = millis();
  delay_duration_ms_ = current_cmd.delay_before_next_ms;
  queue_.pop_front();

  if (delay_duration_ms_ > 0) {
    pending_callback_ = [this, callbacks = std::move(callbacks), response]() mutable {
      notify_callbacks(std::move(callbacks), true, response);
    };
  } else {
    notify_callbacks(std::move(callbacks), true, response);
  }

  execute_next();
}

void CommandQueue::on_error(const Command &error_cmd, ErrorCode error) {
  if (queue_.empty() || queue_.front().state != CommandState::EXECUTING) {
    ESP_LOGW(TAG, "Unexpected error for command 0x%04X (no executing command)", error_cmd.register_address());
    return;
  }

  auto &current_cmd = queue_.front();
  if (current_cmd.command.command_type != error_cmd.command_type) {
    ESP_LOGW(TAG, "Error mismatch: expected 0x%04X, got 0x%04X", current_cmd.command.register_address(),
             error_cmd.register_address());
    return;
  }

  ESP_LOGE(TAG, "Command 0x%04X failed: error %d", error_cmd.register_address(), static_cast<int>(error));

  Command empty_cmd(error_cmd.command_type);
  auto callbacks = std::move(current_cmd.callbacks);
  // Recovery delays (e.g. RESTART) also apply on transport failure.
  delay_started_ms_ = millis();
  delay_duration_ms_ = current_cmd.delay_before_next_ms;
  queue_.pop_front();

  notify_callbacks(std::move(callbacks), false, empty_cmd);
  execute_next();
}

void CommandQueue::clear() {
  // From spec: "Clear all pending (non-executing) commands"
  if (queue_.empty()) {
    return;
  }

  // If executing, skip first command (it must complete/timeout naturally)
  size_t start_index = (!queue_.empty() && queue_.front().state == CommandState::EXECUTING) ? 1 : 0;

  ESP_LOGW(TAG, "Clearing %zu pending commands", queue_.size() - start_index);

  // Detach cancellations before invoking user code, which may clear or enqueue again.
  std::deque<QueuedCommand> cancelled;
  if (start_index > 0) {
    cancelled.push_back(std::move(queue_.front()));
    queue_.pop_front();
  }
  cancelled.swap(queue_);

  bool was_dispatching = dispatching_callbacks_;
  dispatching_callbacks_ = true;
  for (auto &cmd : cancelled) {
    Command empty_cmd(cmd.command.command_type);
    notify_callbacks(std::move(cmd.callbacks), false, empty_cmd);
  }
  dispatching_callbacks_ = was_dispatching;
  execute_next();
}

void CommandQueue::notify_callbacks(std::vector<CommandCallback> callbacks, bool success, const Command &cmd) {
  bool was_dispatching = dispatching_callbacks_;
  dispatching_callbacks_ = true;
  for (auto &callback : callbacks) {
    callback(success, cmd);
  }
  dispatching_callbacks_ = was_dispatching;
}

void CommandQueue::execute_next() {
  if (dispatching_callbacks_ || (!queue_.empty() && queue_.front().state == CommandState::EXECUTING)) {
    return;
  }

  if (static_cast<uint32_t>(millis() - delay_started_ms_) < delay_duration_ms_) {
    return;
  }
  delay_duration_ms_ = 0;

  if (pending_callback_) {
    auto callback = std::move(pending_callback_);
    pending_callback_ = nullptr;
    callback();
  }

  if (queue_.empty()) {
    return;
  }

  // Prepare next command (sorts by effective time with age-based penalties)
  prepare_next_command();

  auto &cmd = queue_.front();

  // Transition to EXECUTING (single-flight guarantee)
  cmd.state = CommandState::EXECUTING;
  cmd.sent_time = millis();

  // Send via transport - execute_command handles both read (0x04) and write (0x06/0x10)
  // A synchronous transport callback may remove the queued command.
  Command command = cmd.command;
  transport_->execute_command(command);
}

void CommandQueue::prepare_next_command() {
  // Time-penalty based scheduling: Find command with lowest effective_time
  if (queue_.empty())
    return;

  uint32_t now = millis();
  auto best = queue_.begin();
  int64_t best_effective_time = calculate_effective_time(*best, now);

  // Find command with smallest effective time (executes first)
  for (auto it = queue_.begin() + 1; it != queue_.end(); ++it) {
    int64_t effective_time = calculate_effective_time(*it, now);
    bool strict_priority = it->priority < Priority::NORMAL || best->priority < Priority::NORMAL;
    if (strict_priority ? it->priority < best->priority : effective_time < best_effective_time) {
      best = it;
      best_effective_time = effective_time;
    }
  }

  // Move best command to front if not already there
  if (best != queue_.begin()) {
    // Can't use erase() with const members - rebuild queue with best element first
    auto index = std::distance(queue_.begin(), best);
    std::deque<QueuedCommand> new_queue;

    new_queue.push_back(std::move(*best));  // Best command first
    for (size_t i = 0; i < queue_.size(); ++i) {
      if (i != static_cast<size_t>(index)) {
        new_queue.push_back(std::move(queue_[i]));
      }
    }
    queue_ = std::move(new_queue);

    // Log if non-FIFO reordering happened
    if (queue_[0].priority == Priority::CRITICAL) {
      ESP_LOGD(TAG, "Moving CRITICAL command 0x%04X to front", queue_[0].command.register_address());
    }
  }
}

int64_t CommandQueue::calculate_effective_time(const QueuedCommand &cmd, uint32_t now) {
  int64_t age = static_cast<uint32_t>(now - cmd.enqueued_time);
  switch (cmd.priority) {
    case Priority::CRITICAL:
    case Priority::SETUP:
    case Priority::NORMAL:
      return -age;

    case Priority::BACKGROUND:
      return BACKGROUND_PENALTY_MS - age;

    case Priority::IDLE:
    default:
      return IDLE_PENALTY_MS - age;
  }
}

void CommandQueue::check_timeout() {
  if (queue_.empty() || queue_.front().state != CommandState::EXECUTING) {
    return;  // No executing command
  }

  auto &current_cmd = queue_.front();
  uint32_t elapsed = millis() - current_cmd.sent_time;

  if (elapsed > timeout_ms_) {
    ESP_LOGW(TAG, "Command 0x%04X timed out after %" PRIu32 "ms (timeout=%" PRIu32 "ms)",
             current_cmd.command.register_address(), elapsed, timeout_ms_);

    Command empty_cmd(current_cmd.command.command_type);
    auto callbacks = std::move(current_cmd.callbacks);
    queue_.pop_front();

    notify_callbacks(std::move(callbacks), false, empty_cmd);
    execute_next();
  }
}

std::deque<CommandQueue::QueuedCommand>::iterator CommandQueue::find_duplicate(const Command &cmd) {
  // Find duplicate command for deduplication
  // Requirements:
  // 1. Same Command (type AND payload)
  // 2. PENDING state (not EXECUTING or completed)
  // 3. Skip first command if it's EXECUTING

  if (queue_.empty()) {
    return queue_.end();
  }

  size_t start_index = (queue_.front().state == CommandState::EXECUTING) ? 1 : 0;

  for (size_t i = start_index; i < queue_.size(); ++i) {
    auto &queued = queue_[i];
    // Match both command type and payload for true duplicate detection
    if (queued.command.command_type == cmd.command_type && queued.command.payload == cmd.payload &&
        queued.state == CommandState::PENDING) {
      return queue_.begin() + i;
    }
  }

  return queue_.end();
}

}  // namespace servoxxd
}  // namespace esphome
