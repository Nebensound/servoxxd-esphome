# ServoXxd Component Tests

This directory contains both **ESPHome integration tests** and **C++ unit tests** for validating the component.

## Directory Structure

```
tests/
├── unit/                       # C++ unit tests for type classes
│   ├── test_speed.cpp         # Speed class with 7 units
│   ├── test_acceleration.cpp  # Acceleration class with 5 units
│   ├── test_position.cpp      # Position class with 6 units
│   ├── Makefile               # Build system for unit tests
│   └── README.md              # Unit test documentation
│
└── esphome/                   # ESPHome YAML integration tests
    ├── test_compile.yaml      # Full ESP32 compilation test
    ├── test_compile_host.yaml # Host platform test (no hardware)
    ├── test_hardware.yaml     # Hardware validation
    └── hw_setup_test.yaml     # Hardware setup sequence test
```

## Quick Start

### Run C++ Unit Tests

```bash
cd tests/unit
make test
```

### Run ESPHome Tests

```bash
# Python registration and minimum-version regressions (ESPHome must be installed)
python -m unittest discover -s tests/python -v

# Issue #16: ESP-IDF, all 20 actions, local components, no secrets or uploads
esphome compile tests/esphome/test_build_warnings.yaml

# Compile test (no upload)
esphome compile tests/esphome/test_compile.yaml

# Hardware test (requires ESP32 + motor)
esphome run tests/esphome/hw_setup_test.yaml
```

## Unit Tests (C++)

Fast, lightweight tests that verify unit conversion logic for all type classes.

**Advantages:**
- ⚡ Fast execution (<1 second)
- 🔧 No hardware required
- 📊 Comprehensive coverage (18 units total)
- 🐛 Easy debugging

**See:** [unit/README.md](unit/README.md) for details

## ESPHome Tests (YAML)

Integration tests that validate the component within ESPHome's build system.

**Setup (for hardware tests):**

1. Copy the secrets template:
   ```bash
   cp tests/secrets.yaml.template tests/secrets.yaml
   ```

2. Edit `tests/secrets.yaml` with your WiFi credentials:
   ```yaml
   wifi_ssid: "YourActualWiFiSSID"
   wifi_password: "YourActualPassword"
   fallback_ap_password: "test1234"
   ```

3. The `secrets.yaml` file is in `.gitignore` and will not be committed.

## Test Files

### `hw_setup_test.yaml`
**Purpose:** Hardware setup validation test (on real hardware)

- Tests actual motor setup sequence (7 steps)
- Validates UART/RS485 communication
- Auto-runs on boot and restarts after 15 seconds
- Requires actual MKS SERVO42D motor connected
- **Requires secrets.yaml** for WiFi credentials

**Run:** `esphome run tests/hw_setup_test.yaml`

**When to use:**
- ✅ Testing new hardware setup
- ✅ Validating RS485 communication
- ✅ Debugging motor initialization
- ✅ Verifying encoder and work mode settings

### `test_compile.yaml`
**Purpose:** Full ESP32 hardware compilation test

- Uses `esp32` platform with Arduino framework
- Tests hardware-specific features including UART/RS485
- Validates complete build chain
- Requires ESP32 toolchain

**Run:** `esphome compile tests/test_compile.yaml`

**When to use:**
- ✅ Before pushing to production
- ✅ Testing hardware-specific features
- ✅ Validating complete build process
- ✅ Before creating releases
- ✅ Primary compilation test for this component

### `test_compile_host.yaml`  
**Status:** Currently not functional

**Note:** The `host` platform doesn't support UART peripherals which are essential for this RS485/MODBUS component. We focus on ESP32 hardware testing instead.

For now, use `test_compile.yaml` for all compilation tests.

## Running Tests

### Primary Test (ESP32 Hardware)
```bash
esphome compile tests/test_compile.yaml
```

This is the main test for the component since it requires UART/RS485 functionality.

## CI/CD Integration

For automated testing in CI/CD pipelines, use the host test for speed:

```yaml
# Example GitHub Actions
- name: Test ESPHome Component
  run: |
    pip install esphome
    esphome compile tests/test_compile_host.yaml
```

## Expected Results

Both tests should compile without errors. Warnings are acceptable if they come from ESPHome core or Arduino framework.

**Success:** Component compiles cleanly  
**Failure:** Check error messages for syntax or dependency issues

### Build-warning regressions

Use ESPHome **2026.8.2 or newer**. `test_build_warnings.yaml` covers both motor modes,
ENDSTOP homing configuration, templated positions, and every ServoXXD action under
ESP-IDF. Its scripts are compile coverage only: none are invoked at boot.

Action registrations are synchronous in ESPHome's automation sense: they enqueue
motor operations and return without deferring the next automation action. This
does not mean a movement or homing operation has already finished.

The unit suite also checks Modbus FC04/FC06/FC10 encoding, PDU response handling,
queue rejection, and errors using a mock of the modern client API. Logging checks
compile all component sources with format checking enabled at every log level;
they also verify fractional positions and holding-current percentages in output.

Warnings from other external components, GPIO strapping-pin checks, and remote
build bundle/secrets checks are outside ServoXXD's scope and are not suppressed.
