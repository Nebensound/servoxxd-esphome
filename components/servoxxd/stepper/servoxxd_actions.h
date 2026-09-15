
#pragma once

#include "servoxxd.h"
#include "esphome/core/automation.h"
#include "servoxxd_modbus.h"
#include "servoxxd_speed.h"
#include "servoxxd_acceleration.h"
#include "servoxxd_position.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace esphome {
namespace servoxxd {

/**
 * @brief Action templates for ServoXxd stepper motor control
 *
 * This file defines all 18 action classes that can be used in YAML automations.
 * Each action corresponds to a public method on ServoXxd.
 *
 * **Action Categories:**
 *
 * 1. **Movement Actions (5):**
 *    - SetTargetAction: Move to absolute position (Position Mode)
 *    - RunContinuousAction: Run at constant speed (Speed Mode)
 *    - StopAction: Stop with deceleration
 *    - EmergencyStopAction: Immediate halt
 *    - HomeAction: Run homing sequence
 *
 * 2. **Position Actions (2):**
 *    - ReportPositionAction: Set current position offset
 *    - SetZeroAction: Store current position as zero for virtual homing
 *
 * 3. **Control Actions (2):**
 *    - EnableAction: Enable motor
 *    - DisableAction: Disable motor
 *
 * 4. **Speed/Acceleration Actions (2):**
 *    - SetSpeedAction: Set speed for next movement
 *    - SetAccelerationAction: Set acceleration/deceleration
 *
 * 5. **Configuration Actions (4):**
 *    - SetWorkModeAction: Change work mode (CR_OPEN_LOOP, SR_VFOC, etc.)
 *    - SetWorkingCurrentAction: Set working current (mA)
 *    - SetHoldingCurrentPercentAction: Set holding current (0-100%)
 *    - SetMicrosteppingAction: Set microstepping (8, 16, 32, 64, 128, 256)
 *
 * 6. **System Actions (3):**
 *    - ReleaseProtectionAction: Clear error/protection state
 *    - RestartAction: Restart motor controller
 *    - CalibrateAction: Run motor calibration
 *
 * **Implementation Pattern:**
 * Each action class inherits from Action<> and implements:
 * - play() method that calls the corresponding ServoXxd method
 * - Template setters for unit-based values (position, speed, acceleration)
 * - ESPHome automation system integration
 *
 * TODO: Implementation required for all 18 actions
 * - [ ] SetTargetAction with Position support
 * - [ ] RunContinuousAction with Speed support
 * - [ ] StopAction
 * - [ ] EmergencyStopAction
 * - [ ] HomeAction
 * - [ ] ReportPositionAction with Position support
 * - [ ] SetZeroAction
 * - [ ] EnableAction
 * - [ ] DisableAction
 * - [ ] SetSpeedAction with Speed support
 * - [ ] SetAccelerationAction with Acceleration support
 * - [ ] SetWorkModeAction
 * - [ ] SetWorkingCurrentAction
 * - [ ] SetHoldingCurrentPercentAction
 * - [ ] SetMicrosteppingAction
 * - [ ] ReleaseProtectionAction
 * - [ ] RestartAction
 * - [ ] CalibrateAction
 *
 * @see ServoXxd for component API
 * @see README.md for user-facing documentation
 * @see 01-yaml-api.md for YAML configuration details
 */

// ============================================================================
// 1. Movement Actions
// ============================================================================

/**
 * @brief Action: Move to absolute position (Position Mode only)
 *
 * YAML: `stepper.set_target`
 *
 * TODO: Implement play() method
 * - Call parent_->move_to(position_)
 * - Handle templatable position values
 */
template<typename... Ts> class SetTargetAction : public Action<Ts...> {
 public:
  explicit SetTargetAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, value)

  void set_unit(PositionUnit unit) { unit_ = unit; }

  void play(const Ts &...x) override {
    if (parent_->get_operating_mode() != OperatingMode::POSITION) {
      ESP_LOGE("servoxxd.action", "set_target requires mode: POSITION");
      return;
    }
    float value = this->value_.value(x...);
    Position pos(value, unit_, parent_);
    parent_->move_to(pos);
  }

 protected:
  ServoXxd *parent_;
  PositionUnit unit_{PositionUnit::STEPS};
};

/**
 * @brief Action: Run continuously at constant speed (Speed Mode only)
 *
 * YAML: `stepper.run_continuous`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class RunContinuousAction : public Action<Ts...> {
 public:
  explicit RunContinuousAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, speed)
  TEMPLATABLE_VALUE(float, acceleration)

  // Unit setters also mark the corresponding value as configured (codegen always calls them)
  void set_speed_unit(SpeedUnit unit) {
    speed_unit_ = unit;
    has_speed_ = true;
  }
  void set_acceleration_unit(AccelerationUnit unit) {
    acceleration_unit_ = unit;
    has_acceleration_ = true;
  }

  void play(const Ts &...x) override {
    if (parent_->get_operating_mode() != OperatingMode::SPEED) {
      ESP_LOGE("servoxxd.action", "run_continuous requires mode: SPEED");
      return;
    }
    std::optional<Speed> speed;
    if (has_speed_) {
      speed = Speed(this->speed_.value(x...), speed_unit_, parent_);
    }
    std::optional<Acceleration> accel;
    if (has_acceleration_) {
      accel = Acceleration(this->acceleration_.value(x...), acceleration_unit_, parent_);
    }
    parent_->run_continuous(speed, accel);
  }

 protected:
  ServoXxd *parent_;
  SpeedUnit speed_unit_{SpeedUnit::STEPS_PER_SEC};
  AccelerationUnit acceleration_unit_{AccelerationUnit::STEPS_PER_SEC_SQ};
  bool has_speed_{false};
  bool has_acceleration_{false};
};

/**
 * @brief Action: Stop motor with deceleration
 *
 * YAML: `stepper.stop`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class StopAction : public Action<Ts...> {
 public:
  explicit StopAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, acceleration)

  // Unit setter also marks the deceleration as configured (codegen always calls it)
  void set_acceleration_unit(AccelerationUnit unit) {
    acceleration_unit_ = unit;
    has_acceleration_ = true;
  }

  void play(const Ts &...x) override {
    std::optional<Acceleration> decel;
    if (has_acceleration_) {
      decel = Acceleration(this->acceleration_.value(x...), acceleration_unit_, parent_);
    }
    parent_->stop(decel);
  }

 protected:
  ServoXxd *parent_;
  AccelerationUnit acceleration_unit_{AccelerationUnit::STEPS_PER_SEC_SQ};
  bool has_acceleration_{false};
};

/**
 * @brief Action: Emergency stop (immediate halt)
 *
 * YAML: `stepper.emergency_stop`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class EmergencyStopAction : public Action<Ts...> {
 public:
  explicit EmergencyStopAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->emergency_stop(); }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Run homing sequence
 *
 * YAML: `stepper.home`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class HomeAction : public Action<Ts...> {
 public:
  explicit HomeAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override {
    if (parent_->get_operating_mode() != OperatingMode::POSITION) {
      ESP_LOGE("servoxxd.action", "home requires mode: POSITION");
      return;
    }
    parent_->home();
  }

 protected:
  ServoXxd *parent_;
};

// ============================================================================
// 2. Position Actions
// ============================================================================

/**
 * @brief Action: Set current position offset
 *
 * YAML: `stepper.report_position`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class ReportPositionAction : public Action<Ts...> {
 public:
  explicit ReportPositionAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, value)

  void set_unit(PositionUnit unit) { unit_ = unit; }

  void play(const Ts &...x) override {
    if (parent_->get_operating_mode() != OperatingMode::POSITION) {
      ESP_LOGE("servoxxd.action", "report_position requires mode: POSITION");
      return;
    }
    float value = this->value_.value(x...);
    Position pos(value, unit_, parent_);
    parent_->report_position(pos);
  }

 protected:
  ServoXxd *parent_;
  PositionUnit unit_{PositionUnit::STEPS};
};

/**
 * @brief Action: Store current position as zero for virtual homing
 *
 * YAML: `stepper.set_zero`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class SetZeroAction : public Action<Ts...> {
 public:
  explicit SetZeroAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override {
    if (parent_->get_operating_mode() != OperatingMode::POSITION) {
      ESP_LOGE("servoxxd.action", "set_zero requires mode: POSITION");
      return;
    }
    parent_->set_zero();
  }

 protected:
  ServoXxd *parent_;
};

// ============================================================================
// 3. Control Actions
// ============================================================================

/**
 * @brief Action: Enable motor
 *
 * YAML: `stepper.enable`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class EnableAction : public Action<Ts...> {
 public:
  explicit EnableAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->enable(); }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Disable motor
 *
 * YAML: `stepper.disable`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class DisableAction : public Action<Ts...> {
 public:
  explicit DisableAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->disable(); }

 protected:
  ServoXxd *parent_;
};

// ============================================================================
// 4. Speed/Acceleration Actions
// ============================================================================

/**
 * @brief Action: Set speed for next movement
 *
 * YAML: `stepper.set_speed`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class SetSpeedAction : public Action<Ts...> {
 public:
  explicit SetSpeedAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, value)

  void set_unit(SpeedUnit unit) { unit_ = unit; }

  void play(const Ts &...x) override {
    float value = this->value_.value(x...);
    Speed speed(value, unit_, parent_);
    parent_->set_speed(speed);
  }

 protected:
  ServoXxd *parent_;
  SpeedUnit unit_{SpeedUnit::STEPS_PER_SEC};
};

/**
 * @brief Action: Set acceleration/deceleration
 *
 * YAML: `stepper.set_acceleration`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class SetAccelerationAction : public Action<Ts...> {
 public:
  explicit SetAccelerationAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, value)

  void set_unit(AccelerationUnit unit) { unit_ = unit; }

  void play(const Ts &...x) override {
    float value = this->value_.value(x...);
    Acceleration acc(value, unit_, parent_);
    parent_->set_acceleration(acc);
  }

 protected:
  ServoXxd *parent_;
  AccelerationUnit unit_{AccelerationUnit::STEPS_PER_SEC_SQ};
};

// ============================================================================
// 5. Configuration Actions
// ============================================================================

/**
 * @brief Action: Change control mode (SR_OPEN/SR_CLOSE/SR_VFOC)
 *
 * YAML: `stepper.set_control_mode`
 *
 * Changes the hardware control loop type at runtime.
 */
template<typename... Ts> class SetControlModeAction : public Action<Ts...> {
 public:
  explicit SetControlModeAction(ServoXxd *parent) : parent_(parent) {}

  void set_control_mode(ControlMode mode) { mode_ = mode; }

  void play(const Ts &...x) override { parent_->set_control_mode(mode_); }

 protected:
  ServoXxd *parent_;
  ControlMode mode_{ControlMode::SR_VFOC};
};

/**
 * @brief Action: Set working current (mA)
 *
 * YAML: `stepper.set_working_current`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class SetWorkingCurrentAction : public Action<Ts...> {
 public:
  explicit SetWorkingCurrentAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint16_t, current)

  void play(const Ts &...x) override {
    uint16_t current = this->current_.value(x...);
    parent_->set_working_current(current);
  }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Set holding current percent (0-100%)
 *
 * YAML: `stepper.set_holding_current_percent`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class SetHoldingCurrentPercentAction : public Action<Ts...> {
 public:
  explicit SetHoldingCurrentPercentAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(float, percent)  // 0.0 - 1.0

  void play(const Ts &...x) override {
    // Round to nearest 10 % and clamp to the supported 10-90 % range
    int pct = static_cast<int>(std::lround(this->percent_.value(x...) * 100.0f));
    pct = std::max(10, std::min(90, (pct + 5) / 10 * 10));
    parent_->set_holding_current_percent(static_cast<HoldingCurrentPercent>((pct - 10) / 10));
  }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Set microstepping
 *
 * YAML: `stepper.set_microstepping`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class SetMicrosteppingAction : public Action<Ts...> {
 public:
  explicit SetMicrosteppingAction(ServoXxd *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint16_t, subdivision)

  void play(const Ts &...x) override { parent_->set_microsteps(this->subdivision_.value(x...)); }

 protected:
  ServoXxd *parent_;
};

// ============================================================================
// 6. System Actions
// ============================================================================

/**
 * @brief Action: Clear error/protection state
 *
 * YAML: `stepper.release_protection`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class ReleaseProtectionAction : public Action<Ts...> {
 public:
  explicit ReleaseProtectionAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->release_protection(); }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Restart motor controller
 *
 * YAML: `stepper.restart`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class RestartAction : public Action<Ts...> {
 public:
  explicit RestartAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->restart(); }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Run motor calibration
 *
 * YAML: `stepper.calibrate`
 *
 * TODO: Implement play() method
 */
template<typename... Ts> class CalibrateAction : public Action<Ts...> {
 public:
  explicit CalibrateAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->calibrate(); }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Lock motor display buttons
 *
 * YAML: `stepper.key_lock`
 */
template<typename... Ts> class KeyLockAction : public Action<Ts...> {
 public:
  explicit KeyLockAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->key_lock(); }

 protected:
  ServoXxd *parent_;
};

/**
 * @brief Action: Unlock motor display buttons
 *
 * YAML: `stepper.key_unlock`
 */
template<typename... Ts> class KeyUnlockAction : public Action<Ts...> {
 public:
  explicit KeyUnlockAction(ServoXxd *parent) : parent_(parent) {}

  void play(const Ts &...x) override { parent_->key_unlock(); }

 protected:
  ServoXxd *parent_;
};

}  // namespace servoxxd
}  // namespace esphome
