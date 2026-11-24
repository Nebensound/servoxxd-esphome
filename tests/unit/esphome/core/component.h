#pragma once

// Mock ESPHome component header for unit testing

#include <functional>
#include <vector>
#include <cstdint>

// Mock LOG_STR macro for unit testing (ESPHome provides this in esphome/core/log.h)
#ifndef LOG_STR
#define LOG_STR(s) s
#endif

namespace esphome {

class Component {
 public:
  virtual ~Component() = default;

  // Virtual methods that can be overridden
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}

  // Mock methods for testing
  void mark_failed() { failed_ = true; }
  bool is_failed() const { return failed_; }
  void status_set_error(const char * /*message*/ = nullptr) { mark_failed(); }  // Mock ESPHome error status

  // Mock interval functionality
  template<typename F> void set_interval(const char *name, uint32_t interval, F &&f) {
    // Store callback for testing
    interval_callbacks_.push_back(std::forward<F>(f));
  }

  // Mock timeout functionality
  template<typename F> void set_timeout(const char * /*name*/, uint32_t /*timeout*/, F && /*f*/) {
    // Mock: In real ESPHome, this schedules a one-time callback
  }

 protected:
  bool failed_ = false;
  std::vector<std::function<void()>> interval_callbacks_;
};

}  // namespace esphome
