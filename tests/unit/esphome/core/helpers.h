#pragma once

// Mock ESPHome helpers for unit testing

#include <cstdint>
#include <chrono>

namespace esphome {

// Mock millis() function for timing - uses test_millis_value
extern uint32_t test_millis_value;
inline uint32_t millis() { return test_millis_value; }

// Mock micros() function for precision timing
inline uint32_t micros() {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
}

}  // namespace esphome
