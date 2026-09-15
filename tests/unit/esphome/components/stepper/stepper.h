#pragma once

// Mock ESPHome Stepper header for unit testing

#include "esphome/core/component.h"
#include <cstdint>

namespace esphome {
namespace stepper {

class Stepper : virtual public Component {
 public:
  virtual ~Stepper() = default;

  // Mock stepper interface for unit testing
  virtual void set_target(int32_t steps) { target_position = steps; }
  virtual void set_acceleration(float acceleration) { acceleration_ = acceleration; }
  virtual void set_deceleration(float deceleration) { deceleration_ = deceleration; }
  virtual void set_max_speed(float speed) { max_speed_ = speed; }

  // Public members to match real ESPHome Stepper API
  int32_t current_position{0};
  int32_t target_position{0};

 protected:
  float acceleration_{1000.0f};
  float deceleration_{1000.0f};
  float max_speed_{1000.0f};
  float current_speed_{0.0f};
};

}  // namespace stepper
}  // namespace esphome
