#pragma once

// Mock ESPHome log header for unit testing

#include <cstdarg>
#include <cstdio>
#include <cstdint>

#ifdef SERVOXXD_TEST_LOGGING
#include <string>
#include <vector>
#endif

namespace esphome {

enum LogLevel {
  ESPHOME_LOG_LEVEL_NONE = 0,
  ESPHOME_LOG_LEVEL_ERROR = 1,
  ESPHOME_LOG_LEVEL_WARN = 2,
  ESPHOME_LOG_LEVEL_INFO = 3,
  ESPHOME_LOG_LEVEL_CONFIG = 4,
  ESPHOME_LOG_LEVEL_DEBUG = 5,
  ESPHOME_LOG_LEVEL_VERBOSE = 6,
  ESPHOME_LOG_LEVEL_VERY_VERBOSE = 7
};

#ifdef SERVOXXD_TEST_LOGGING
// Opt-in capture keeps normal tests quiet while checking every printf argument.
struct TestLogMessage {
  int level;
  std::string message;
};
inline std::vector<TestLogMessage> test_log_messages;

inline void esp_log_printf_(int level, const char *tag, int line, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

inline void esp_log_printf_(int level, const char *, int, const char *format, ...) {
  va_list args;
  va_start(args, format);
  va_list size_args;
  va_copy(size_args, args);
  int size = vsnprintf(nullptr, 0, format, size_args);
  va_end(size_args);
  if (size >= 0) {
    std::vector<char> buffer(static_cast<size_t>(size) + 1);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    test_log_messages.push_back({level, std::string(buffer.data(), static_cast<size_t>(size))});
  }
  va_end(args);
}
#else
inline void esp_log_printf_(int, const char *, int, const char *, ...) {}
#endif

}  // namespace esphome

// Mock logging macros
#ifdef SERVOXXD_TEST_LOGGING
#define ESP_LOGD(tag, ...) esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_DEBUG, tag, __LINE__, __VA_ARGS__)
#define ESP_LOGI(tag, ...) esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, __VA_ARGS__)
#define ESP_LOGW(tag, ...) esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_WARN, tag, __LINE__, __VA_ARGS__)
#define ESP_LOGE(tag, ...) esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, __VA_ARGS__)
#define ESP_LOGCONFIG(tag, ...) esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, __VA_ARGS__)
#define ESP_LOGV(tag, ...) esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_VERBOSE, tag, __LINE__, __VA_ARGS__)
#define ESP_LOGVV(tag, ...) \
  esphome::esp_log_printf_(esphome::ESPHOME_LOG_LEVEL_VERY_VERBOSE, tag, __LINE__, __VA_ARGS__)
#else
#define ESP_LOGD(tag, ...) ((void) 0)
#define ESP_LOGI(tag, ...) ((void) 0)
#define ESP_LOGW(tag, ...) ((void) 0)
#define ESP_LOGE(tag, format, ...) printf("[ERROR][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGCONFIG(tag, ...) ((void) 0)
#define ESP_LOGV(tag, ...) ((void) 0)
#define ESP_LOGVV(tag, ...) ((void) 0)
#endif

// LOG_STEPPER macro for stepper components
#define LOG_STEPPER(obj) ((void) 0)
