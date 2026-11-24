#pragma once

#include <cstdint>

// Mock millis() function for unit testing
// Will be overridden by test implementation
uint32_t millis();
