/**
 * @file hal.cpp
 * @brief Mock HAL implementation for unit tests
 */

#include "hal.h"
#include <chrono>

// Mock millis() implementation for unit tests
// Define test_millis_value in esphome namespace so it's shared across all compilation units
namespace esphome {
uint32_t test_millis_value = 0;
}

uint32_t millis() { return esphome::test_millis_value; }
