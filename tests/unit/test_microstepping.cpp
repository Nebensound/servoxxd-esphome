#include "servoxxd.h"
#include "servoxxd_commands.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <type_traits>

using namespace esphome::servoxxd;

static_assert(std::is_same_v<decltype(ConfigData::subdivision), uint16_t>);
static_assert(std::is_same_v<decltype(&ServoXxd::set_microsteps), void (ServoXxd::*)(uint16_t)>);
static_assert(std::is_same_v<decltype(ServoXxd().get_microstepping()), uint16_t>);

void test_configuration_and_units() {
  ServoXxd parent;
  parent.set_control_mode(ControlMode::SR_CLOSE);
  parent.set_microsteps(256);
  assert(parent.get_microstepping() == 256);
  assert(parent.get_effective_steps_per_revolution() == 51200.0f);
  assert(parent.get_steps_per_revolution() == 51200.0f);

  auto speed = Speed::from_steps_per_sec(51200.0f, &parent);
  assert(speed.rpm() == 60.0f);
  assert(speed.steps_per_sec() == 51200.0f);
  auto position = Position::from_steps(51200.0f, &parent);
  assert(position.get_ticks() == 16384);
  assert(position.revolutions() == 1.0f);
  assert(Position::from_revolutions(1.0f, &parent).get_steps() == 51200.0f);
  auto acceleration = Acceleration::from_rpm_per_sec(1000.0f, &parent);
  assert(std::abs(acceleration.get_steps_per_sec2() - 51200.0f * 1000.0f / 60.0f) < 0.1f);
  auto from_steps = Acceleration::from_steps_per_sec2(51200.0f * 1000.0f / 60.0f, &parent);
  assert(from_steps.acc_internal() == acceleration.acc_internal());

  for (uint16_t invalid : {0, 257, 65535}) {
    parent.set_microsteps(invalid);
    assert(parent.get_microstepping() == 256);
  }
  parent.set_microsteps(256);
  assert(parent.get_microstepping() == 256);
  parent.set_microsteps(1);
  assert(parent.get_effective_steps_per_revolution() == 200.0f);
  assert(speed.steps_per_sec() == 200.0f);
  assert(position.get_steps() == 200.0f);
  parent.set_microsteps(256);
  assert(speed.steps_per_sec() == 51200.0f);
  assert(position.get_steps() == 51200.0f);
}

void test_configuration_comparison() {
  ServoXxd parent;
  parent.set_microsteps(256);
  ConfigData desired(&parent);
  desired.subdivision = 256;

  ConfigData current = desired;
  assert(current.subdivision == 256);
  auto updates = current.get_update_command_types(desired);
  assert(std::find(updates.begin(), updates.end(), Commandtype::SET_SUBDIVISION) == updates.end());

  current.subdivision = 1;
  assert(current.subdivision == 1);
  updates = current.get_update_command_types(desired);
  assert(std::find(updates.begin(), updates.end(), Commandtype::SET_SUBDIVISION) != updates.end());
}

void test_speed_compensation_helper() {
  ServoXxd parent;

  for (uint16_t subdivision : {8, 16, 32, 64, 128, 256}) {
    parent.set_microsteps(subdivision);
    const int16_t expected = subdivision == 8 ? 2400 : subdivision == 128 ? 150 : subdivision == 256 ? 75 : 1200;
    for (float rpm : {1200.0f, -1200.0f}) {
      const auto speed = Speed::from_rpm(rpm, &parent);
      assert(speed.rpm_for_hardware() == (rpm < 0 ? -expected : expected));
    }
  }
}

int main() {
  test_configuration_and_units();
  test_configuration_comparison();
  test_speed_compensation_helper();
  std::cout << "Microstepping configuration, unit contexts and compensation helper tests passed\n";
}
