# ServoXxd Unit Tests

Comprehensive C++ unit tests for the ServoXxd ESPHome component core classes.

## Status: ✅ All Tests Passing (6/6)

```
test_speed            ✅ PASSING (53 assertions)
test_acceleration     ✅ PASSING (49 assertions)
test_position         ✅ PASSING (79 assertions)
test_command_queue    ✅ PASSING (14 test scenarios)
test_stepper_engine   ✅ PASSING (8 scenarios, 23 assertions)
test_command_codec    ✅ PASSING (81 assertions)
```

**Total:** 160+ assertions across all tests

## Requirements

- C++17 compiler (g++ or clang++)
- Make

## Running Tests

### Build and run all tests:
```bash
make test
```

### Run individual tests:
```bash
make test_speed
./test_speed

make test_acceleration
./test_acceleration

make test_position
./test_position

make test_command_queue
./test_command_queue

make test_stepper_engine
./test_stepper_engine

make test_command_codec
./test_command_codec
```

### Clean build artifacts:
```bash
make clean
```

## Test Coverage

### 1. Speed Class (`test_speed.cpp`)
Tests the `Speed` class with all 7 supported units and microstepping conversions.

**Coverage:**
- ✅ All 7 speed units (RPM, DEG/s, RAD/s, STEPS/s, REV/s, REV/min, HZ)
- ✅ Unit conversions (to_rpm(), to_deg_per_sec(), etc.)
- ✅ Microstepping adjustments (1-256 microsteps)
- ✅ Hardware encoding (to_hardware_rpm())
- ✅ Edge cases (zero speed, max values)

### 2. Acceleration Class (`test_acceleration.cpp`)
Tests the `Acceleration` class with all 5 supported units and inverse time mapping.

**Coverage:**
- ✅ All 5 acceleration units (RPM/s, DEG/s², RAD/s², STEPS/s², REV/s²)
- ✅ Unit conversions (to_rpm_per_sec(), etc.)
- ✅ Inverse time mapping to hardware units (1-255)
- ✅ Boundary values (acc=0 → 255, acc=max → 1)
- ✅ Edge cases

### 3. Position Class (`test_position.cpp`)
Tests the `Position` class with all 6 supported units and split encoder format.

**Coverage:**
- ✅ All 6 position units (STEPS, REVOLUTIONS, DEGREES, RADIANS, ARCMINUTES, ARCSECONDS)
- ✅ Unit conversions (to_steps(), to_revolutions(), etc.)
- ✅ Split format (encoder_carry + encoder_addition for 32-bit position)
- ✅ Arithmetic operations (+, -, *, /)
- ✅ Negative positions
- ✅ Edge cases

### 4. CommandQueue (`test_command_queue.cpp`)
Tests the command queue with async execution, deduplication, and timeout handling.

**Coverage (14 tests):**
- ✅ Single-flight execution (only one command executing at a time)
- ✅ Command deduplication (multiple identical reads → single command)
- ✅ Callback merging for deduplicated commands
- ✅ Command timeout handling (1000ms default)
- ✅ Priority commands (emergency_stop clears queue)
- ✅ Success/failure callback propagation
- ✅ Queue clearing
- ✅ Empty queue updates (no crash)
- ✅ Late response rejection (after timeout)
- ✅ Mixed read/write command ordering

**Critical Bug Fixed:** ✅ CommandQueue now registers Transport callbacks in constructor!

### 5. StepperEngine (`test_stepper_engine.cpp`)
Tests the StepperEngine core state machine with realistic mock transport.

**Coverage (8 tests, 23 assertions):**
- ✅ State transitions (Disabled→Idle→Moving→Error)
- ✅ Enable/disable commands
- ✅ Basic move_to() command
- ✅ Optional parameters (move_to with speed/accel)
- ✅ stop() command (with optional deceleration)
- ✅ emergency_stop() command
- ✅ Transport callback propagation
- ✅ Error recovery (release_protection)
- ✅ Command validation matrix (state-dependent rejection)

## Critical Bug Fixed

**CommandQueue Callback Registration:**
- **Issue:** CommandQueue constructor never registered callbacks with ITransport
- **Impact:** Layer 2→3→4 callback chain was broken
- **Fix:** Added `set_response_callback()` and `set_error_callback()` in constructor
- **Verification:** All tests passing, callback propagation verified

## Directory Structure

```
tests/unit/
├── README.md                    # This file
├── Makefile                     # Build configuration (C++17)
├── test_speed.cpp              # Speed class tests
├── test_acceleration.cpp       # Acceleration class tests
├── test_position.cpp           # Position class tests
├── test_command_queue.cpp      # CommandQueue Layer 3 tests
├── test_stepper_engine.cpp     # StepperEngine state machine tests
├── test_command_codec.cpp      # CommandCodec encoding/decoding tests
├── test_debug.cpp              # Debug helper
├── test_acc_debug.cpp          # Acceleration debug helper
├── test_epsilon.cpp            # Floating-point epsilon helper
└── esphome/                    # Mock ESPHome headers
    ├── core/
    │   ├── log.h               # Mock logging
    │   ├── hal.h               # Mock HAL
    │   └── component.h         # Mock Component
    └── components/
        ├── stepper/stepper.h   # Mock Stepper
        └── modbus/modbus.h     # Mock Modbus
```

## Notes

- Uses C++17 features (required: `-std=c++17`)
- Float comparisons use EPSILON tolerance for accuracy
- Mock transport provides realistic async response simulation
- All tests include verbose output with ✓/✗ indicators
- Exit code 0 = all tests passed, 1 = test failure
