# Layer 1: Core Component (ServoXxd) - Detailed Specification

**Parent Document:** [02-cpp-interface.md](./02-cpp-interface.md)  
**Status:** 🔵 SPECIFICATION – Layer 1 implementation details

**Navigation:**
- [← Back to Overview](./02-cpp-interface.md#layer-1-core-component-transport-agnostic)
- [→ Next: Layer 2 (StepperEngine)](./02b-layer2-stepper-engine.md)

---

## Overview

**Class:** ServoXxd  
**Inherits:** 
- [`stepper::Stepper`](https://github.com/esphome/esphome/blob/dev/esphome/components/stepper/stepper.h) (ESPHome base stepper interface)
- [`modbus::ModbusDevice`](https://github.com/esphome/esphome/blob/dev/esphome/components/modbus/modbus_controller.h) (ESPHome Modbus client)
- [`Component`](https://github.com/esphome/esphome/blob/dev/esphome/core/component.h) (ESPHome component lifecycle)

**Design Pattern:** Facade - provides simple interface to complex subsystem

**Role in Architecture:**
- Entry point for all YAML actions and configurations
- Delegates movement logic to StepperEngine (Layer 2)
- Bridges ESPHome framework to our motor-specific implementation
- Manages component lifecycle and periodic polling

## Base Class Integration

The component follows ESPHome's standard architecture by inheriting from three base classes:

### 1. stepper::Stepper (ESPHome base stepper interface)

- Provides the standard stepper API that ESPHome automation expects (e.g., `set_target()`, `current_position`, `target_position`)
- Defines lifecycle hooks and state management common to all steppers
- Ensures compatibility with ESPHome's stepper actions and lambdas

> [!Important]
> The `stepper::Stepper` base class provides a **position-centric abstraction** (absolute target, current position). Our component extends this to support both **Position Mode** (using the base Stepper API) and **Speed Mode** (continuous rotation, bypassing position tracking). Internal state and action routing adapt to the configured operating mode.

### 2. modbus::ModbusDevice (ESPHome Modbus client)

- Handles RS485 communication via ESPHome's modbus component
- Provides `send()`, `on_modbus_data()`, `on_modbus_error()` for request/response flow
- Manages device address and parent modbus controller reference

### 3. Component (ESPHome component lifecycle)

- Provides `setup()`, `loop()`, `dump_config()` lifecycle methods
- Enables `set_interval()` and `set_timeout()` for periodic tasks

## Required Method Overrides (essentials)

- From `stepper::Stepper`:
  - Optional: `on_update_speed()` if runtime reactions to speed changes are needed.
  - Non-virtual base methods (cannot override, may overload): `set_target(int32_t)`, `report_position(int32_t)`, `set_max_speed(float)`.
  - Overloads provided by this component: `set_target(Position)`, `report_position(Position)`, `set_speed(Speed)`.
  - Synchronization: Keep base members `current_position`/`target_position` in sync with internal `Position` objects; monitor external changes in `loop()`.
- From `modbus::ModbusDevice`:
  - Implement `on_modbus_data(...)` and `on_modbus_error(...)` - these forward to active ITransport implementation (Layer 4)
  - These are ESPHome-specific callbacks; SerialTransport would use different integration mechanism
  - These are implementation details of the transport layer and should not be called directly

- From `Component`:
  - Implement `setup()`, `loop()`, `dump_config()`; other lifecycle hooks optional.

These overrides bridge ESPHome's standard interfaces to the motor-specific implementation.
## Responsibilities

- **Lifecycle Management:** setup(), dump_config(), loop()
- **Periodic Polling:** via set_interval("status_poll", ...): encoder, speed, motor status, protection status
- **Transport Bridge:** ESPHome protocol callbacks → forward to ITransport implementation (Layer 4) → CommandQueue
- **Public API:** Implementing all YAML actions (see [02-cpp-interface.md](./02-cpp-interface.md#public-c-api-binding))
- **Configuration Storage:** Holds all YAML configuration values
- **State Management:** Tracks runtime state, last-used parameters
- **Coordination:** Delegates to StepperEngine (Layer 2), manages helpers and sub-components
- **Coordination:** Delegates to StepperEngine (Layer 2), manages helpers and sub-components

## Key Configuration Fields

(backed by [01-yaml-api.md](./01-yaml-api.md))

### Motor Hardware Configuration (ConfigData)

All motor hardware settings are stored in a single `ConfigData` structure, which serves as the single source of truth. The implementation uses **type-safe enum classes** for all hardware settings (better than primitives).

**Architectural Decision:** ConfigData uses modern C++ features:
- **Enum classes** instead of uint8_t for type safety (ControlMode, EnPinActive, ScreenMode, etc.)
- **Type-safe wrappers** for units (Speed, Position instead of raw uint16_t)
- **Required parent pointer** for Speed/Position object construction
- **Deleted default constructor** to enforce proper initialization

```cpp
struct ConfigData {
  ServoXxd *parent;  ///< REQUIRED - no default (for Speed/Position construction)

  // Core motor settings (YAML-configurable with type-safe enums)
  ControlMode mode{ControlMode::SR_VFOC};                           // Control mode (default: SR_VFOC)
  HoldingCurrentPercent holding_current_percent{HoldingCurrentPercent::PERCENT_50};  // Holding current (default: 50%)
  uint16_t working_current_ma{2000};                                // Working current in mA (default: 2000 mA)
  uint8_t subdivision{16};                                          // Microstepping 1-256 (default: 16)
  EnPinActive en_pin_active{EnPinActive::EN_LOW};                   // EN pin active level (default: LOW)
  Direction direction{Direction::CW};                               // Motor shaft direction (default: CW)
  ScreenMode screen_mode{ScreenMode::AUTO_OFF};                     // Screen power mode (default: auto off)
  ProtectionMode protection{ProtectionMode::PROTECTION_OFF};        // Stall protection (default: disabled)
  InterpolationMode interpolation{InterpolationMode::INTERP_256X};  // Interpolation (default: 256x)
  KeypadLock keypad_lock{KeypadLock::UNLOCKED};                     // Keypad lock (default: unlocked)
  
  // Homing configuration (type-safe objects instead of primitives)
  EndstopTrigger homing_trigger{EndstopTrigger::TRIGGER_LOW};       // Endstop trigger level
  Direction homing_direction{Direction::CW};                        // Homing direction
  Speed homing_speed;                                               // Homing speed (Speed object, not uint16_t)
  EndstopLimit endstop_limit{EndstopLimit::LIMIT_OFF};              // Endstop limit checking
  Position nolimit_reverse_angle_ticks;                             // No-limit reverse (Position object, not uint32_t)
  HomingLimitMode homing_limit_mode{HomingLimitMode::WITH_LIMIT};   // Homing with/without limit
  uint16_t nolimit_current_ma{1000};                                // No-limit current threshold
  LimitPortMapping limit_port_mapping{LimitPortMapping::MAPPING_DEFAULT};  // Port mapping
  
  // Zero mode configuration (VIRTUAL homing)
  ZeroModeMode zero_mode{ZeroModeMode::MODE_DISABLED};              // Zero mode
  ZeroModeTask zero_task{ZeroModeTask::CLEAN};                      // Zero task
  ZeroingSpeed zero_speed{ZeroingSpeed::MEDIUM};                    // Zero speed
  Direction zero_direction{Direction::CW};                          // Zero direction

  // Constructor - parent is REQUIRED (no default value)
  explicit ConfigData(ServoXxd *parent_ptr);

  // Deleted default constructor to enforce parent requirement
  ConfigData() = delete;
};
```

### Additional Configuration (not in ConfigData)

```cpp
// Hardware constant (not in ConfigData, but fundamental for unit conversions)
static constexpr float BASE_STEPS_PER_REVOLUTION = 200.0f;  // 1.8° motor (hardware limitation)

// Homing configuration (HomingConfig struct)
HomingConfig homing_;                 // Complete homing configuration (mode, direction, speed, etc.)

// Default motion parameters
Speed default_speed_;                 // Default/max speed for movements (100 RPM)
Acceleration default_acceleration_;   // Default acceleration (1000 RPM/s)

// Sleep configuration
uint32_t sleep_when_done_ms_;         // UINT32_MAX=disabled, 0=immediate, 1+=delay (YAML: sleep_when_done)
```

**HomingConfig Structure:**
```cpp
struct HomingConfig {
  HomingMode mode{HomingMode::NO_HOMING};          // NO_HOMING, ENDSTOP, SENSORLESS, VIRTUAL
  bool at_startup{false};                          // Perform homing at startup
  HomingDirection direction{HomingDirection::CW};  // CW, CCW, NEAREST
  
  // Speed - union (mode determines which is active)
  union {
    Speed speed;         // For ENDSTOP/SENSORLESS modes (Speed object)
    ZeroingSpeed level;  // For VIRTUAL mode (enum: VERY_SLOW..VERY_FAST)
  };
  
  EndstopTrigger endstop_trigger{EndstopTrigger::TRIGGER_LOW};  // For ENDSTOP mode
  uint16_t current_ma{0};                                       // For SENSORLESS mode (0 = defaults)
  
  // Note: Requires proper union management (constructors/destructors)
};
```

## Runtime State

> **Architectural Decision:** Most runtime state is managed by **Layer 2 (StepperEngine)**, not Layer 1.
> ServoXxd (Layer 1) is a Facade - it delegates state management to the engine.
> Only position tracking is kept in Layer 1 for ESPHome base class compatibility.

```cpp
// Position tracking (encoder-split representation)
Position current_pos_;                // Current position (from encoder + offset)
Position target_pos_;                 // Target position for moves
Position position_offset_;            // Offset for report_position zeroing

// Stepper base class members (inherited from stepper::Stepper)
// IMPORTANT: These are PUBLIC members from the base class and CANNOT be overridden:
//   int32_t current_position;  // Must be kept in sync with current_pos_.steps()
//   int32_t target_position;   // Must be kept in sync with target_pos_.steps()
// These must be updated whenever current_pos_ or target_pos_ change to maintain
// ESPHome stepper API compatibility (used by automations, lambdas, and has_reached_target())

// Operating mode
OperatingMode operating_mode_;        // Current operating mode (POSITION or SPEED)

// Async setup state tracking
SetupState setup_state_;              // NOT_STARTED, IN_PROGRESS, COMPLETED, FAILED
uint32_t setup_start_time_;           // Time when setup_motor() started
```

**Motor Status (managed by Layer 2 - StepperEngine):**
- `State state_` - State machine state (Idle, Moving, Running, Homing, etc.)
- `Speed current_speed_` - Last known motor speed
- `bool protection_triggered_` - Protection status
- `bool emergency_flag_` - Emergency stop flag

**No "last_speed" / "last_accel" members needed:**
- Methods accept `std::optional<Speed>` / `std::optional<Acceleration>` parameters
- StepperEngine tracks defaults internally
- Cleaner API without persistent state in Layer 1

## Contracts

- **Non-blocking Operations:** All public methods must be non-blocking - they enqueue commands via CommandQueue (Layer 3) and return immediately
- **Input Validation:** Each user-facing action validates inputs and clamps to hardware-safe ranges before enqueueing
- **Unit Conversion:** Must centralize in helper functions to avoid duplication and drift
- **Position Synchronization:**
  - Whenever `current_pos_` or `target_pos_` are updated, the inherited public members `current_position` and `target_position` MUST be updated accordingly using `.steps()` (which uses stored parent_) to maintain ESPHome stepper API compatibility
  - In `loop()`, check if `target_position` changed externally (ESPHome action called base class `set_target(int32_t)`) and sync to `target_pos_` if changed
  - Both `set_target(Position)` and `set_target(int32_t)` overloads must update the same internal state consistently

## Integration with Other Layers

**Layer 2 (StepperEngine):**
- ServoXxd creates and holds StepperEngine instance
- Delegates all movement commands to engine
- Provides configuration access via parent pointer
- Calls engine_->update() in loop()

**Layer 3 (CommandQueue):**
- StepperEngine manages the queue (not ServoXxd directly)
- ServoXxd only bridges transport callbacks to engine
**Layer 4 (Transport + CommandDecoder):**
- ServoXxd inherits ModbusDevice (ESPHome framework requirement)
- ESPHome protocol callbacks are forwarded to active ITransport implementation
- Current implementation: ModbusTransport (via ModbusDevice callbacks)
- Future: SerialTransport would use different ESPHome integration (e.g., uart component callbacks)
- CommandDecoder (in Layer 4) provides encode/decode functions for all commands
- StepperEngine (Layer 2) uses codec to prepare command data and parse responses
- Transport layer is completely abstracted - upper layers only see ITransport and Command enumds
- StepperEngine (Layer 2) uses codec to prepare command data and parse responses

---

**Navigation:**
- [← Back to Overview](./02-cpp-interface.md#layer-1-core-component-transport-agnostic)
- [→ Next: Layer 2 (StepperEngine)](./02b-layer2-stepper-engine.md)
