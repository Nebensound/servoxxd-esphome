# YAML API Specification

**Status:** 🔵 **SPECIFICATION** – Defines the user-facing YAML configuration interface

**Audience:** This document is for **developers** implementing the component. For end-user documentation, see [README.md](../../README.md).

**Purpose:** This document specifies the complete YAML configuration API for the `servoxxd` component. It defines what users can configure, valid values, defaults, and validation rules. This serves as the authoritative reference for implementing the Python validation layer (`__init__.py`).

**Architecture Note:** The component uses a modular architecture:
- **`servoxxd`**: Main platform name (transport-agnostic)
- **Core component**: Facade, configuration, actions
- **Transport layer (Modbus RTU)**: Commands, callbacks
- **Movement logic (Stepper Engine)**: State machine and movement control

This separation allows future support for alternative transports (e.g., Serial, CAN) while maintaining the same YAML API.

## Type Definitions

This section defines reusable types used throughout the configuration and actions.

### `modbus_address` Type

**Description:** Modbus RTU slave address in hexadecimal format.

**Accepted Values:**

- Hexadecimal format: `0x01` to `0xF7` (1-247 decimal)
- Examples: `0x01`, `0x10`, `0xF7`

**Validation:**

```python
cv.All(
    cv.hex_uint8_t,           # ESPHome: Validates hex format (0x00-0xFF)
    cv.Range(min=1, max=247)  # Component: Validates Modbus RTU range
)
```

**C++ Type:** `uint8_t`

**References:**

- [`cv.hex_uint8_t`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py) from ESPHome core
- Range 1-247 per Modbus RTU specification (0, 248-255 reserved)

### `speed` Type

**Description:** Motor speed with multiple unit options. Converted to RPM for motor communication.

**Default Unit:** `STEPS_PER_SEC` (for ESPHome stepper compatibility)

**Accepted Values:**

- **Steps per second:** → `STEPS_PER_SEC`
  - Accepted strings: `steps/s`, `steps/sec`, `step/s`, `step/sec`
  - Example: `1000 steps/s`
- **Revolutions per minute:** → `RPM`
  - Accepted strings: `RPM`, `rpm`, `rev/min`, `revolutions/min`
  - Example: `60 RPM` or `60 rev/min`
- **Revolutions per second:** → `REV_PER_SEC`
  - Accepted strings: `rev/s`, `rev/sec`, `rps`, `revolutions/s`, `revolutions/sec`
  - Example: `1.5 rev/s`
- **Degrees per second:** → `DEGREES_PER_SEC`
  - Accepted strings: `deg/s`, `deg/sec`, `°/s`, `degrees/s`, `degrees/sec`
  - Example: `360 deg/s`
- **Radians per second:** → `RADIANS_PER_SEC`
  - Accepted strings: `rad/s`, `rad/sec`, `radians/s`, `radians/sec`
  - Example: `6.28 rad/s`
- **Degrees per minute:** → `DEGREES_PER_MIN`
  - Accepted strings: `deg/min`, `deg/m`, `°/min`, `degrees/minute`
  - Example: `360 deg/min`
- **Degrees per hour:** → `DEGREES_PER_HOUR`
  - Accepted strings: `deg/h`, `deg/hr`, `°/h`, `degrees/hour`
  - Example: `15 deg/h`
- **Metric prefixes:** `k` (kilo), `M` (mega), `m` (milli), `µ/u` (micro)

> [!NOTE]
> Multiple string aliases are accepted for user convenience (e.g., `rpm`, `RPM`, `rev/min` all map to `SpeedUnit::RPM`). Python validation normalizes these to the corresponding C++ enum. The `unit` field in dicts is **not templatable** and must be a compile-time constant.

> [!NOTE]
> Metric prefixes may only be used when the value is a plain number or string, not if its a lambda.

**Validation:**

- Use `cv.float_with_unit()` for metric prefix support
- Map unit string aliases to enum values (case-insensitive):
  - `steps/s`, `steps/sec`, `step/s`, `step/sec` → `STEPS_PER_SEC`
  - `RPM`, `rpm`, `rev/min`, `revolutions/min` → `RPM`
  - `rev/s`, `rev/sec`, `rps`, `revolutions/s` → `REV_PER_SEC`
  - `deg/s`, `deg/sec`, `°/s`, `degrees/s` → `DEGREES_PER_SEC`
  - `rad/s`, `rad/sec`, `radians/s` → `RADIANS_PER_SEC`
  - `deg/min`, `deg/m`, `°/min`, `degrees/minute` → `DEGREES_PER_MIN`
  - `deg/h`, `deg/hr`, `°/h`, `degrees/hour` → `DEGREES_PER_HOUR`
- Return dict: `{"value": float, "unit": enum_string}`

**Code Generation:**

Static values → Convert to RPM at build-time:

- `STEPS_PER_SEC`: `rpm = (value * 60.0) / (BASE_STEPS_PER_REVOLUTION * microsteps)`
- `RPM`: `rpm = value`
- `REV_PER_SEC`: `rpm = value * 60.0`
- `DEGREES_PER_SEC`: `rpm = (value * 60.0) / 360.0`
- `RADIANS_PER_SEC`: `rpm = (value * 60.0) / (2π)`
- `DEGREES_PER_MIN`: `rpm = value / 6.0`
- `DEGREES_PER_HOUR`: `rpm = value / 360.0`

Lambdas → Pass to C++ for runtime conversion:

- `value`: Lambda code (templatable)
- `unit`: Enum integer

> Implementation hint: The C++ layer maps these to an internal unit enum and integer RPM representation. Details are defined in the C++ spec.

**References:**

- Uses [`cv.float_with_unit()`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py) internally per unit

### `acceleration` Type

**Description:** Motor acceleration with multiple unit options. Converted to RPM/s for motor communication.

**Default Unit:** `STEPS_PER_SEC_SQ` (for ESPHome stepper compatibility)

**Accepted Values:**

- **Steps per second squared:** → `STEPS_PER_SEC_SQ`
  - Accepted strings: `steps/s²`, `steps/s/s`, `step/s²`, `step/s/s`, `steps/sec²`, `steps/sec/sec`
  - Example: `1000 steps/s²`
- **RPM per second:** → `RPM_PER_SEC`
  - Accepted strings: `RPM/s`, `RPM/sec`, `rpm/s`, `rpm/sec`, `rev/min/s`, `rev/min/sec`
  - Example: `100 RPM/s`
- **Revolutions per second squared:** → `REV_PER_SEC_SQ`
  - Accepted strings: `rev/s²`, `rev/s/s`, `rps/s`, `revolutions/s²`, `revolutions/s/s`, `rev/sec²`, `rev/sec/sec`
  - Example: `1.5 rev/s²`
- **Degrees per second squared:** → `DEGREES_PER_SEC_SQ`
  - Accepted strings: `deg/s²`, `deg/s/s`, `°/s²`, `degrees/s²`, `degrees/s/s`, `deg/sec²`, `deg/sec/sec`
  - Example: `360 deg/s²`
- **Radians per second squared:** → `RADIANS_PER_SEC_SQ`
  - Accepted strings: `rad/s²`, `rad/s/s`, `radians/s²`, `radians/s/s`, `rad/sec²`, `rad/sec/sec`
  - Example: `6.28 rad/s²`
- **Metric prefixes:** `k` (kilo), `M` (mega), `m` (milli), `µ/u` (micro)

> [!NOTE]
> Multiple string aliases are accepted for user convenience (e.g., `RPM/s`, `rpm/s`, `rev/min/s` all map to `AccelerationUnit::RPM_PER_SEC`). Python validation normalizes these to the corresponding C++ enum. The `unit` field in dicts is **not templatable** and must be a compile-time constant.

**Validation:**

- Use `cv.float_with_unit()` for metric prefix support
- Map unit string aliases to enum values (case-insensitive):
  - `steps/s²`, `steps/s/s`, `step/s²`, `steps/sec²` → `STEPS_PER_SEC_SQ`
  - `RPM/s`, `RPM/sec`, `rpm/s`, `rev/min/s` → `RPM_PER_SEC`
  - `rev/s²`, `rev/s/s`, `rps/s`, `revolutions/s²` → `REV_PER_SEC_SQ`
  - `deg/s²`, `deg/s/s`, `°/s²`, `degrees/s²` → `DEGREES_PER_SEC_SQ`
  - `rad/s²`, `rad/s/s`, `radians/s²` → `RADIANS_PER_SEC_SQ`
- Return dict: `{"value": float, "unit": enum_string}`

**Code Generation:**

Static values → Convert to RPM/s at build-time (clamp to 0-65535):

- `STEPS_PER_SEC_SQ`: `rpm_per_s = (value * 60.0) / (BASE_STEPS_PER_REVOLUTION * microsteps)`
- `RPM_PER_SEC`: `rpm_per_s = value`
- `REV_PER_SEC_SQ`: `rpm_per_s = value * 60.0`
- `DEGREES_PER_SEC_SQ`: `rpm_per_s = (value * 60.0) / 360.0`
- `RADIANS_PER_SEC_SQ`: `rpm_per_s = (value * 60.0) / (2π)`

Lambdas → Pass to C++ for runtime conversion:

- `value`: Lambda code (templatable)
- `unit`: Enum integer

> Implementation hint: The C++ layer uses a hardware-native 0–255 value; mapping is documented in the C++ spec.

**References:**

- [ESPHome `cv.float_with_unit`](https://esphome.io/components/sensor/index.html#config-validation)

### `position` Type

**Description:** Motor position with multiple unit options. Converted to steps for motor communication.

**Default Unit:** `STEPS` (for ESPHome stepper compatibility)

**Accepted Values:**

- **Steps:** → `STEPS`
  - Accepted strings: `steps`, `step`
  - Example: `3200 steps`
- **Revolutions:** → `REVOLUTIONS`
  - Accepted strings: `rev`, `revs`, `revolutions`, `revolution`
  - Example: `2.5 rev`
- **Degrees:** → `DEGREES`
  - Accepted strings: `deg`, `degrees`, `°`
  - Example: `720 deg`
- **Radians:** → `RADIANS`
  - Accepted strings: `rad`, `rads`, `radians`, `radian`
  - Example: `6.28 rad`
- **Arcminutes:** → `ARCMINUTES`
  - Accepted strings: `arcmin`, `arcminute`, `arcminutes`, `'`, `amin`
  - Example: `60 arcmin` (1 degree = 60 arcminutes)
- **Arcseconds:** → `ARCSECONDS`
  - Accepted strings: `arcsec`, `arcsecond`, `arcseconds`, `"`, `asec`
  - Example: `3600 arcsec` (1 degree = 3600 arcseconds)
- **Metric prefixes:** `k` (kilo), `M` (mega), `m` (milli), `µ/u` (micro)

> [!NOTE]
> Multiple string aliases are accepted for user convenience (e.g., `rev`, `revs`, `revolutions` all map to `PositionUnit::REVOLUTIONS`). Python validation normalizes these to the corresponding C++ enum. The `unit` field in dicts is **not templatable** and must be a compile-time constant.

> [!NOTE]
> Metric prefixes may only be used when the value is a plain number or string, not if its a lambda.

**Validation:**

- Use `cv.float_with_unit()` for metric prefix support
- Map unit string aliases to enum values (case-insensitive):
  - `steps`, `step` → `STEPS`
  - `rev`, `revs`, `revolutions`, `revolution` → `REVOLUTIONS`
  - `deg`, `degrees`, `°` → `DEGREES`
  - `rad`, `rads`, `radians`, `radian` → `RADIANS`
  - `arcmin`, `arcminute`, `arcminutes`, `'`, `amin` → `ARCMINUTES`
  - `arcsec`, `arcsecond`, `arcseconds`, `"`, `asec` → `ARCSECONDS`
- Return dict: `{"value": float, "unit": enum_string}`

**Code Generation:**

Static values → Convert to steps at build-time:

- `STEPS`: `steps = value`
- `REVOLUTIONS`: `steps = value * BASE_STEPS_PER_REVOLUTION * microsteps`
- `DEGREES`: `steps = (value / 360.0) * BASE_STEPS_PER_REVOLUTION * microsteps`
- `RADIANS`: `steps = (value / (2π)) * BASE_STEPS_PER_REVOLUTION * microsteps`
- `ARCMINUTES`: `steps = (value / 21600.0) * BASE_STEPS_PER_REVOLUTION * microsteps` (21600 arcmin = 360°)
- `ARCSECONDS`: `steps = (value / 1296000.0) * BASE_STEPS_PER_REVOLUTION * microsteps` (1296000 arcsec = 360°)

Lambdas → Pass to C++ for runtime conversion:

- `value`: Lambda code (templatable)
- `unit`: Enum integer

> Implementation hint: The C++ layer represents positions using encoder-aligned split format. See C++ spec for details.

**References:**

- Uses [`cv.float_with_unit()`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py) internally per unit

### `auto_sleep` Type

**Description:** Automatic motor power-down after idle time. Accepts boolean for enable/disable or time period for delayed shutdown.

**Accepted Values:**

- `false` or `inf` - Feature disabled, motor stays powered indefinitely
- `true` or `0s` - Disable motor immediately when idle
- Time period - Disable motor after specified idle time (e.g., `5s`, `30s`, `2min`)
  - Range: `1ms` to `4294967294ms` (~49.7 days)

**Validation:**

- Accept `false`/`inf` → return `UINT32_MAX` (disabled)
- Accept `true`/`0s` → return `0` (immediate)
- Accept time period → use `cv.positive_time_period_milliseconds`, clamp to `UINT32_MAX - 1`

**C++ Type:** `uint32_t` (milliseconds)

**C++ Semantics:**

- `UINT32_MAX` (4294967295) = Feature disabled (`false`/`inf`)
- `0` = Immediately disable (`true`/`0s`)
- `1` to `4294967294` = Delay in milliseconds

> Implementation hint: Auto-sleep maps to a 32-bit millisecond timeout; special values are defined in the C++ spec.

**YAML Examples:**

```yaml
auto_sleep: false      # Never disable (default)
auto_sleep: inf        # Never disable (alternative syntax)
auto_sleep: true       # Disable immediately (0ms)
auto_sleep: 0s         # Disable immediately (explicit)
auto_sleep: 30s        # Disable after 30 seconds
auto_sleep: 5min       # Disable after 5 minutes
```

**References:**

- [`cv.positive_time_period_milliseconds`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py#L854) from ESPHome core
- [`cv.Any`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py) for multi-type validation

### `current` Type

**Description:** Motor current with unit support. Converted to milliamperes (mA) for motor communication.

**Accepted Values:**

- **Amperes:** `A` - Current in amperes (e.g., `1.5A`, `2.5A`)
- **Milliamperes:** `mA` - Current in milliamperes (e.g., `1500mA`, `2500mA`)
- **Plain number:** Interpreted as milliamperes (e.g., `1500` = `1500mA`)
- **Metric prefixes:** Supported via ESPHome's current validator

**Validation:**

- Use ESPHome's `cv.current` (handles A/mA/plain numbers automatically)
- Convert to milliamperes: `current_ma = current_amps * 1000.0`
- Enforce servo_type limits (see defaults/maximums above)

**C++ Type:** `uint16_t` (milliamperes, 0-65535 mA)

**YAML Examples:**

```yaml
working_current: 1.5A      # Amperes
working_current: 1500mA    # Milliamperes
working_current: 1500      # Plain number (interpreted as mA)
working_current: 2.5A      # With decimal
```

**References:**

- [`cv.current`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py) from ESPHome core
- Handles amperes (A), milliamperes (mA), and plain numbers automatically

### `direction` Type

**Description:** Rotational direction used where a concrete rotation sense is required.

**Accepted Values:**

- `CW` – Clockwise (positive direction)
- `CCW` – Counter-clockwise (negative direction)

**Validation:**

Use `cv.one_of("CW", "CCW", lower=True)`

**C++ Type:** enum (component-internal)

### `homing_direction` Type

**Description:** Direction for homing operations. Extends [`direction`](#direction-type) with `NEAREST` for virtual homing.

**Accepted Values:**

- `NEAREST` – Shortest path to zero
- All values from [`direction`](#direction-type)

> [!IMPORTANT]
> `NEAREST` is only valid when `homing.mode` is `VIRTUAL`. Validation must check this context.

**Validation:**

Use `cv.one_of("CW", "CCW", "NEAREST", lower=True)`

Additional context check: If `NEAREST`, require `homing.mode == "VIRTUAL"`

**C++ Type:** enum (component-internal)

### `zeroing_speed` Type

**Description:** Speed levels for virtual homing (0_Mode/No_Limit return-to-zero). Only used when `homing.mode` is `VIRTUAL`.

**Accepted Values:**

- `VERY_SLOW` – Slowest speed level (0)
- `SLOW` – Slow speed level (1)
- `MEDIUM` – Medium speed level (2)
- `FAST` – Fast speed level (3)
- `VERY_FAST` – Fastest speed level (4)

> [!NOTE]
> The controller firmware quantizes these into five discrete speed levels (0–4). The actual RPM values are hardware-dependent and not user-configurable.

**Validation:**

Use `cv.one_of("VERY_SLOW", "SLOW", "MEDIUM", "FAST", "VERY_FAST", lower=True)`

**C++ Type:** enum (component-internal, maps to firmware speed levels 0–4)

**C++ Implementation Example:**

```cpp
enum class ZeroingSpeed : uint8_t {
    VERY_SLOW = 0,
    SLOW = 1,
    MEDIUM = 2,
    FAST = 3,
    VERY_FAST = 4
};
```

### `templatable_with_unit` Type

**Description:** Generic type for templatable values with optional unit specification. Used for actions where the value can be either a number or a lambda, but the unit is always a static enum (e.g., for position, speed, acceleration).

**Motivation:** ESPHome's standard `stepper` component calculates positions in steps, which requires users to think in motor steps rather than the actual physical units they care about (revolutions, degrees). This component internally uses angles and revolutions for calculations. The `templatable_with_unit` type allows users to specify values in their preferred unit while maintaining compatibility with ESPHome's stepper interface. When no unit is specified, the value falls back to the standard stepper unit (steps) to ensure compatibility with existing stepper configurations and automations.

**Accepted Formats:**

1. **Plain number**: Default unit is used
  
   ```yaml
   parameter: 1000
   ```
  
2. **String with unit**: Parsed at compile-time
  
   ```yaml
   parameter: "60 RPM"
   parameter: "5.5 revolutions"
   ```
  
3. **Dict with explicit value and unit**:
  
   ```yaml
   parameter:
     value: 60        # Number or lambda
     unit: RPM        # Unit parsed at compile-time
   ```
  
4. **Lambda without unit** (uses default unit):
  
   ```yaml
   parameter: !lambda "return id(sensor).state;"
   ```
  
5. **Dict with lambda and unit**:
  
   ```yaml
   parameter:
     value: !lambda "return id(sensor).state;"
     unit: RPM
   ```

**Validation:**

Handle 5 formats:

1. Plain number → `{"value": float, "unit": default_unit}`
2. String with unit → Parse and return `{"value": float, "unit": enum_string}`
3. Dict with value/unit → Validate with `cv.templatable(cv.float_)` and `cv.enum()`
4. Lambda without unit → `{"value": lambda, "unit": default_unit}`
5. Dict with lambda and unit → Same as 3

Returns: `{"value": templatable, "unit": enum_string}`

**Usage:**

- Actions for position, speed, acceleration, etc. (e.g., `set_speed`, `set_acceleration`, `move_to`)

**C++ API Pattern:**

```cpp
void action(float value, UnitEnum unit = UnitEnum::DEFAULT);
```

**References:**

- [`cv.templatable()`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py) – Validates static values, passes lambdas unchanged
- Conversion uses `BASE_STEPS_PER_REVOLUTION` (200.0f) and `microsteps` from component configuration

## Configuration

This section defines all configuration fields for the component, organized by scope. Possible configurations depend on the selected operating mode.

### Basic Configuration

Fields common to both Speed Mode and Position Mode.

#### `id`

Component instance identifier for referencing in actions and automations.

- **Type:** `ID`
- **Required:** ✅ Yes
- **Validation:** [`cv.declare_id(ServoXxd)`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py)

**Example:**

```yaml
stepper:
  - platform: servoxxd
    id: my_stepper
```

#### `modbus_id`

Reference to the Modbus controller this motor is connected to.

- **Type:** `ID`
- **Required:** ❌ Optional (only needed with multiple Modbus buses)
- **Default:** Auto-detected single bus
- **Validation:** [`cv.use_id(Modbus)`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py)

**Example:**

```yaml
modbus:
  - id: modbus1
    uart_id: uart_bus

stepper:
  - platform: servoxxd
    id: motor1
    modbus_id: modbus1
```

#### `address`

Modbus RTU slave address of the motor.

- **Type:** [`modbus_address`](#modbus_address-type)
- **Required:** ❌ Optional
- **Default:** `0x01`
- **Range:** `0x01` to `0xF7` (1-247 decimal)

**Example:**

```yaml
stepper:
  - platform: servoxxd
    address: 0x10  # Decimal 16
```

#### `microsteps`

Microstepping subdivision value. Combined with the hardware's base step angle (1.8°, 200 steps/revolution), this determines the effective resolution.

- **Type:** `uint16`
- **Required:** ❌ Optional
- **Default:** `1`
- **Range:** `1` to `256`
- **Validation:** `cv.int_range(min=1, max=256)`

> [!IMPORTANT]
> **Hardware Compatibility**
>
> ServoXXD motors **only support 1.8° step angle motors** (200 base steps per revolution).
> 0.9° motors (400 steps/rev) are **not supported** by the hardware.
>
> **Effective Resolution:**
>
> ```text
> effective_steps_per_revolution = 200 * microsteps
> ```
>
> **Examples:**
>
> - `microsteps: 1` → 200 steps/rev (full-step mode)
> - `microsteps: 16` → 3200 steps/rev (typical default)
> - `microsteps: 256` → 51200 steps/rev (maximum resolution)

> [!WARNING]
> **Speed Calibration Limitation**
>
> Motor speed is factory-calibrated only for microstepping values **16, 32, and 64**.
>
> For other values, actual speed may differ from commanded speed. The calibration factor is approximately:
>
> ```text
> actual_rpm ≈ commanded_rpm * (microsteps / 16.0)
> ```

> [!NOTE]
> **vFOC Control Mode Restriction**
>
> When using `control_mode: SR_VFOC`, only `microsteps: 1` is supported.
> Other microstepping values will cause validation errors.

**Example:**

```yaml
stepper:
  - platform: servoxxd
    microsteps: 16  # 200 * 16 = 3200 effective steps/rev
```

#### `microsteps`

Microstepping subdivision value.

- **Type:** `uint16`
- **Required:** ❌ Optional
- **Default:** `16`
- **Range:** `1` to `256`
- **Validation:** `cv.int_range(min=1, max=256)`

> [!WARNING]
> **Speed Calibration Limitation**
>
> Motor speed is factory-calibrated only for microstepping values **16, 32, and 64**.
>
> For other values, apply this correction in your code:
>
> ```python
> actual_rpm = commanded_rpm * (microsteps / 16.0)
> ```

**Example:**

```yaml
stepper:
  - platform: servoxxd
    microsteps: 16  # 200 * 16 = 3200 effective steps/rev
```

#### `speed` (alias: `max_speed`)

Target speed to drive the stepper at (ESPHome stepper compatibility).

- **Type:** [`speed`](#speed-type)
- **Required:** ❌ Optional
- **Default:** `1 RPM`
- **Alternative Name:** `max_speed` (for backward compatibility with older ESPHome stepper configurations)
- **Constraint:** Must not exceed hardware limits based on `control_mode`:
  - `SR_OPEN`: max `400 RPM`
  - `SR_CLOSE`: max `1500 RPM`
  - `SR_VFOC`: max `3000 RPM`

> [!NOTE]
> **Backward Compatibility**
>
> This field can be written as either `speed` or `max_speed` - they are exact aliases:
>
> - `speed`: Current ESPHome stepper field name (recommended)
> - `max_speed`: Legacy name (for backward compatibility)
>
> Validation must ensure that only one of these fields is specified. If both are present, a validation error must be raised.

**Examples:**

```yaml
stepper:
  - platform: servoxxd
    speed: 100 RPM          # Recommended
    
  - platform: servoxxd
    max_speed: 100 RPM      # Backward compatible (exact same meaning)
    
  - platform: servoxxd
    speed: 6000 steps/s     # Steps per second
    
  - platform: servoxxd
    speed: 360k steps/min   # With metric prefix
```

#### `acceleration`

Acceleration rate when the stepper starts and ends movement (ESPHome stepper compatibility).

- **Type:** [`acceleration`](#acceleration-type)
- **Required:** ❌ Optional
- **Default:** `inf` (infinite - instant acceleration)

> [!IMPORTANT]
> **Hardware Limitation**
>
> Unlike the [ESPHome Stepper Component](https://esphome.io/components/stepper/), this component does not support separate acceleration and deceleration values. The MKS ServoXXD motor controllers only support a single acceleration/deceleration rate.
>
> **Validation Requirement:**
>
> - If a `deceleration` field is specified in the configuration, a validation error must be generated
> - Error message: `"deceleration is not supported. Use acceleration instead. Hardware only supports a single acceleration/deceleration value."`
> - The `acceleration` value applies to both acceleration and deceleration

**Examples:**

```yaml
stepper:
  - platform: servoxxd
    acceleration: 100 RPM/s     # RPM per second
    
  - platform: servoxxd
    acceleration: 6k RPM/min    # With metric prefix
    
  - platform: servoxxd
    acceleration: 1000 steps/s² # Steps per second squared
```

#### `sleep_when_done`

Motor power-down behavior after movement completion.

- **Type:** [`auto_sleep`](#auto_sleep-type)
- **Required:** ❌ Optional
- **Default:** `false` or `inf` (motor stays powered)

**Examples:**

```yaml
stepper:
  - platform: servoxxd
    sleep_when_done: false      # Always hold position
    
  - platform: servoxxd
    sleep_when_done: true       # Power off immediately
    
  - platform: servoxxd
    sleep_when_done: 30s        # Power off after 30 seconds
```

**See:** [`auto_sleep` type definition](#auto_sleep-type) for detailed validation and C++ semantics.

#### `servo_type`

Physical motor model type.

- **Type:** `enum`
- **Required:** ✅ Yes
- **Values:**
  - `SERVO28D` - NEMA11 size (default: 0.6A, max: 3.0A)
  - `SERVO35D` - NEMA14 size (default: 0.8A, max: 3.0A)
  - `SERVO42D` - NEMA17 size (default: 1.6A, max: 3.0A)
  - `SERVO57D` - NEMA23 size (default: 3.2A, max: 5.2A)

> [!IMPORTANT]
> **Must match your physical motor model!**
>
> This setting cannot be auto-detected and determines:
>
> - Default `working_current` if not explicitly set
> - Maximum allowed `working_current` (validation enforced)
> - Sensorless homing current defaults
>
> Setting the wrong type may damage the motor due to incorrect current limits.

**Example:**

```yaml
stepper:
  - platform: servoxxd
    servo_type: SERVO42D     # Required!
    working_current: 1.6A    # Optional: uses default if omitted
```

#### `control_mode`

Motor control algorithm selection.

- **Type:** `enum`
- **Required:** ❌ Optional
- **Default:** `SR_VFOC`
- **Values:**
  - `SR_OPEN` - Open-loop mode, stepper behaves like a regular stepper motor. Working current is `working_current`, holding current is `holding_current_percent` of working current.
  - `SR_CLOSE` - Closed-loop mode, same as `SR_OPEN` but with position feedback from encoder to prevent missed steps.
  - `SR_VFOC` - FOC mode (recommended), same as `SR_CLOSE` but current may be adapted to the stepper's needs up to the max `working_current`. `holding_current_percent` is ignored in this mode.

> [!NOTE]
> Hardware speed limits depend on control mode:
>
> - `SR_OPEN`: 400 RPM
> - `SR_CLOSE`: 1500 RPM  
> - `SR_VFOC`: 3000 RPM
>
> These limits are automatically applied to `max_speed` if not explicitly set.

**Example:**

```yaml
stepper:
  - platform: servoxxd
    control_mode: SR_VFOC  # FOC mode (recommended)
```

#### `working_current`

Motor current during movement.

- **Type:** [`current`](#current-type)
- **Required:** ❌ Optional
- **Default:** Depends on `servo_type`:
  - `SERVO28D`: `600mA` (0.6A)
  - `SERVO35D`: `800mA` (0.8A)
  - `SERVO42D`: `1600mA` (1.6A)
  - `SERVO57D`: `3200mA` (3.2A)
- **Maximum:** Depends on `servo_type`:
  - `SERVO28D`: `3000mA` (3.0A)
  - `SERVO35D`: `3000mA` (3.0A)
  - `SERVO42D`: `3000mA` (3.0A)
  - `SERVO57D`: `5200mA` (5.2A)

> [!WARNING]
> Exceeding the maximum current for your servo type will be rejected during validation.

> [!NOTE]
> **Control Mode Behavior**
>
> - `SR_OPEN` / `SR_CLOSE`: Fixed current at this exact value during movement
> - `SR_VFOC`: Maximum allowed current - actual current may be lower, adapted automatically to motor needs

**Examples:**

```yaml
stepper:
  - platform: servoxxd
    servo_type: SERVO42D
    control_mode: SR_OPEN
    working_current: 2.5A       # Fixed 2.5A during movement
    
  - platform: servoxxd
    servo_type: SERVO57D
    control_mode: SR_VFOC
    working_current: 4500mA     # Max 4.5A, may use less
    
  - platform: servoxxd
    servo_type: SERVO42D
    # working_current omitted - uses default 1.6A
```

#### `holding_current_percent`

Current applied when motor is stationary (as percentage of `working_current`).

- **Type:** `percent`
- **Required:** ❌ Optional
- **Default:** `50%`
- **Range:** `0%` to `100%`
- **Validation:** [`cv.percentage`](https://github.com/esphome/esphome/blob/dev/esphome/config_validation.py)

> [!NOTE]
> **Control Mode Behavior**
>
> - `SR_OPEN` / `SR_CLOSE`: Holding current = `working_current` × `holding_current_percent`
> - `SR_VFOC`: This setting is **ignored** - FOC mode manages holding current automatically

**Examples:**

```yaml
stepper:
  - platform: servoxxd
    control_mode: SR_CLOSE
    working_current: 2.5A
    holding_current_percent: 30%  # 0.75A when stopped
    
  - platform: servoxxd
    control_mode: SR_VFOC
    working_current: 2.5A
    holding_current_percent: 30%  # Ignored in FOC mode
```

#### `en_pin_active`

Enable pin logic level polarity.

- **Type:** `enum`
- **Required:** ❌ Optional
- **Default:** `LOW`
- **Values:**
  - `LOW` - Motor enabled when pin is LOW (recommended for software enable/disable)
  - `HIGH` - Motor enabled when pin is HIGH
  - `ALWAYS` - Motor always enabled, ignores EN pin AND software enable/disable commands

> [!IMPORTANT]
> **For software enable/disable to work**, `en_pin_active` must be set to `LOW` or `HIGH`. 
> If set to `ALWAYS` (Hold mode), the motor will ignore `stepper.enable` and `stepper.disable` actions!
> 
> Default is `LOW` which allows software control via `stepper.enable` and `stepper.disable`.

**Example:**

```yaml
stepper:
  - platform: servoxxd
    en_pin_active: LOW     # Enable software control (default, recommended)
    
  - platform: servoxxd
    en_pin_active: HIGH    # Enable when pin is HIGH (alternative)
    
  - platform: servoxxd
    en_pin_active: ALWAYS  # Motor always on, ignores enable/disable commands
```

#### `auto_screen_off`

Automatically turn off motor's built-in display after timeout.

- **Type:** `bool`
- **Required:** ❌ Optional
- **Default:** `true`

**Example:**

```yaml
stepper:
  - platform: servoxxd
    auto_screen_off: true   # Display turns off after 15 seconds (default)
    
  - platform: servoxxd
    auto_screen_off: false  # Display stays on
```

#### `lock_keys_at_startup`

Lock physical buttons on the motor at power-up.

- **Type:** `bool`
- **Required:** ❌ Optional
- **Default:** `false`

**Example:**

```yaml
stepper:
  - platform: servoxxd
    lock_keys_at_startup: true  # Prevent manual control
```

#### `mode`

Operating mode determines available actions and behavior.

- **Type:** `enum`
- **Required:** ❌ Optional
- **Default:** `POSITION`
- **Values:**
  - `POSITION` - Position control mode (stepper behavior)
  - `SPEED` - Continuous rotation mode (velocity control)

> [!IMPORTANT]
> **This setting fundamentally changes how the motor operates!**
>
> **Component Behavior Changes:**
>
> - Motor control strategy (position tracking vs. continuous velocity)
> - Internal state management (target position vs. current speed/direction)
> - Communication protocol with motor controller
>
> **User-Facing Changes:**
>
> - **Available Configuration:** Mode-specific fields (e.g., `homing` only in position mode)
> - **Available Actions:** Different action sets per mode (see below)
> - **Motor Response:** Position tracking vs. continuous rotation

**Examples:**

```yaml
# Position Mode - for applications requiring precise positioning
stepper:
  - platform: servoxxd
    id: camera_slider
    mode: POSITION
    microsteps: 16  # 200 * 16 = 3200 effective steps/rev
    initial_speed: 500 steps/s
    initial_acceleration: 200 steps/s²
    homing:
      mode: ENDSTOP
      at_startup: true

# Speed Mode - for applications requiring continuous rotation
stepper:
  - platform: servoxxd
    id: conveyor_motor
    mode: SPEED
    microsteps: 16  # 200 * 16 = 3200 effective steps/rev
    initial_speed: 300 RPM
    initial_acceleration: 100 RPM/s
```

### Position Mode Configuration

Additional fields available only when [mode](#mode) is `POSITION`.

#### `homing`

Contains homing configuration. Optional but required for homing functionality.

- **Type:** `map`
- **Required:** ❌ Optional
- **Fields:** See below for sub-fields

#### `homing.mode`

Homing method selection. This setting unifies three firmware concepts into one simple option: (1) real homing with an endstop (ENDSTOP), (2) sensorless homing using stall detection (SENSORLESS), and (3) the controller's 0_Mode/No_Limit return-to-zero without an endstop (VIRTUAL). The following values map to the motor's internal behaviors.

- **Type:** `enum`
- **Required:** ✅ Yes
- **Values:**
  - `ENDSTOP` - Use physical endstop switch
  - `SENSORLESS` - Use steppers encoder to detect running into end (stallguard)
  - `VIRTUAL` - Software-defined home position (no physical detection)

Detailed behavior and guidance for each mode:

- ENDSTOP
  - Requirements: A physical endstop switch is wired and configured; set `homing.endstop_trigger` accordingly.
  - Behavior: Moves in `homing.direction` at `homing.speed` until the endstop triggers, then stops and sets the logical zero position. This is the most robust and repeatable option.

- SENSORLESS
  - Requirements: No switch needed. You must provide a suitable `homing.current` threshold that matches your mechanics and motor (defaults are provided per `servo_type`).
  - Behavior: Moves in `homing.direction` at `homing.speed` until the motor current exceeds `homing.current` (stall detected), then stops and sets zero. Sensitivity depends on friction/load and may need tuning.

- VIRTUAL
  - Requirements: No physical detection. Relies on the controller’s internal "return to zero" routine (0_Mode/No_Limit). Best used when a known zero was set previously and mechanics allow a safe return without hard stops.
  - Behavior: Moves to the controller’s stored zero without reading an endstop or stall. Intended for quick reference moves or after a known alignment. Does not detect or learn mechanical limits.

**Example:**

```yaml
stepper:
  - platform: servoxxd
    mode: POSITION
    homing:
      mode: ENDSTOP
```

#### `homing.at_startup`

Automatically perform homing sequence during component initialization.

- **Type:** `bool`
- **Required:** ❌ Optional
- **Default:** `false`

**Example:**

```yaml
stepper:
  - platform: servoxxd
    mode: POSITION
    homing:
      mode: ENDSTOP
      at_startup: true  # Home immediately on boot
```

#### `homing.direction`

Direction to move during homing sequence.

- **Type:** [`homing_direction`](#homing_direction-type)
- **Required:** ❌ Optional
- **Default:** `CW`

**Example:**

```yaml
stepper:
  - platform: servoxxd
    mode: POSITION
    homing:
      direction: CCW  # Move toward endstop in negative direction
```

#### `homing.speed`

Speed used during homing movement. Accepted type depends on `homing.mode`:

**When `homing.mode: VIRTUAL`:**

- **Type:** [`zeroing_speed`](#zeroing_speed-type)
- **Default:** `MEDIUM` (level 2)

> [!NOTE]
> The controller firmware only supports five discrete speed levels for virtual homing. These map to internal speed settings (0–4) and cannot be specified as RPM or steps/s.

**In all other cases (`ENDSTOP`, `SENSORLESS`):**

- **Type:** [`speed`](#speed-type)
- **Default:** `1 RPM`

**Examples:**

```yaml
# VIRTUAL mode - use discrete levels
stepper:
  - platform: servoxxd
    mode: POSITION
    homing:
      mode: VIRTUAL
      speed: FAST

# ENDSTOP/SENSORLESS mode - use standard speed units
stepper:
  - platform: servoxxd
    mode: POSITION
    homing:
      mode: ENDSTOP
      speed: 50 RPM  # Slow for accuracy
```

#### `homing.endstop_trigger`

Endstop switch trigger logic level.

- **Type:** `enum`
- **Required:** ⚠️ Required if `mode: ENDSTOP`
- **Values:**
  - `HIGH` - Endstop triggers when signal goes HIGH
  - `LOW` - Endstop triggers when signal goes LOW

**Example:**

```yaml
stepper:
  - platform: servoxxd
    mode: POSITION
    homing:
      mode: ENDSTOP
      endstop_trigger: LOW  # Normally-open switch
```

#### `homing.current`

Current threshold for sensorless homing (stallguard detection).

- **Type:** [`current`](#current-type)
- **Required:** ❌ Optional
- **Default:** Depends on `servo_type`:
  - `SERVO28D`: `200mA` (0.2A)
  - `SERVO35D`: `200mA` (0.2A)
  - `SERVO42D`: `800mA` (0.8A)
  - `SERVO57D`: `400mA` (0.4A)

> [!NOTE]
> **Only effective when `homing.mode: SENSORLESS`**
>
> Motor moves in `homing.direction` until current exceeds this threshold, indicating a physical obstruction (endstop/hard stop). 
>
> **Hardware Manual Recommendation:** Set to a smaller current as much as possible to avoid damaging the motor during homing collisions.
>
> Higher values = more force before detection, lower values = more sensitive but may trigger prematurely.
>
> This setting is ignored in `ENDSTOP` and `VIRTUAL` modes.

**Examples:**

```yaml
stepper:
  - platform: servoxxd
    servo_type: SERVO42D
    mode: POSITION
    homing:
      mode: SENSORLESS
      current: 1.5A          # Trigger at 1.5A
      direction: CCW
      
  - platform: servoxxd
    servo_type: SERVO57D
    mode: POSITION
    homing:
      mode: SENSORLESS
      current: 2500mA        # Trigger at 2.5A
      # current omitted - would use default 3.2A
      direction: CCW
```

### Speed Mode Configuration

Additional fields available only when [mode](#mode) is `SPEED`.

## Actions

Actions are organized by their availability in different operating modes. Some actions work in all modes, while others are specific to Position Mode or Speed Mode.

> [!NOTE]
> **Action Syntax:** All actions that only require an `id` parameter should support both short and long syntax:
>
> ```yaml
> # Short form (recommended for single-id actions)
> - stepper.enable: my_stepper
> - stepper.home: my_stepper
> 
> # Long form (use when adding optional parameters)
> - stepper.enable:
>     id: my_stepper
> - stepper.stop:
>     id: my_stepper
>     acceleration: 50 RPM/s  # Optional parameter
> ```
>
> **Applies to:** `enable`, `disable`, `emergency_stop`, `home`, `set_zero`, `calibrate`, `release_protection`, `restart`, `key_lock`, `key_unlock`
>
> **Implementation:** Uses `automation.maybe_conf(CONF_ID, cv.Schema({...}))` in Python validation.

### Basic Actions

Actions available in both operating modes.

#### `stepper.enable` / `stepper.disable`

Enable or disable the motor. Same action is used for the `sleep_when_done` configuration.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

> [!TIP]
> **Syntax Options:** Both short and long form are supported:
> ```yaml
> # Short form (recommended for single-id actions)
> - stepper.enable: my_stepper
> 
> # Long form (required when adding other parameters)
> - stepper.enable:
>     id: my_stepper
> ```

**C++ API:**

```cpp
void enable();
void disable();
```

**Examples:**

```yaml
on_...:
  - stepper.enable: my_stepper
  - stepper.disable: my_stepper
```

#### `stepper.emergency_stop`

Emergency stop - immediately halt motor with maximum deceleration.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void emergency_stop();
```

> [!WARNING]
> At speeds above about 1000 RPM, this can be mechanically harsh. Use regular [`stepper.stop`](#stepperstop) with controlled deceleration when possible.

> [!NOTE]
> Same as [`stepper.stop`](#stepperstop), but with acceleration set to `inf` (instant stop). The stepper will also be disabled after stopping, and acceleration cannot be changed. [`stepper.release_protection`](#stepperrelease_protection) may be called to re-enable normal operation after an emergency stop.

**Example:**

```yaml
on_...:
  - stepper.emergency_stop: my_stepper
```

#### `stepper.calibrate`

Start motor calibration sequence. Used to map measured magnetic field to encoder positions. Should be done at least once after motor installation.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void calibrate();
```

> [!NOTE]
> Motor will move during calibration. Make sure that the stepper moves freely and is not obstructed. Stepper will restart after calibration.

**Example:**

```yaml
on_...:
  - stepper.calibrate: my_stepper
```

#### `stepper.release_protection`

Release motor protection state after error condition. Is part of `stepper.home` action. Most of the cases that is the right action to recover from an error.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void release_protection();
```

**Example:**

```yaml
on_...:
  - stepper.release_protection: my_stepper
```

#### `stepper.restart`

Restart the motor controller. Part of initial setup, and is also called when `homing.mode: VIRTUAL` is used, `homing.at_startup: true` is set and `stepper.set_zero` was at least once called before.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void restart();
```

**Example:**

```yaml
on_...:
  - stepper.restart: my_stepper
```

#### `stepper.set_work_mode`

Change the motor control mode at runtime.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **mode** (**Required**, enum): Work mode, one of `SR_OPEN`, `SR_CLOSE`, `SR_VFOC`.

**C++ API:**

```cpp
void set_work_mode(WorkMode mode);
```

**Example:**

```yaml
on_...:
  - stepper.set_work_mode:
      id: my_stepper
      mode: SR_VFOC
```

#### `stepper.set_working_current`

Change the working current at runtime.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **current** (**Required**, [`current`](#current-type)): Current in mA. May be smaller than [`max` of `working_current`](#working_current).

**C++ API:**

```cpp
void set_working_current(float current_milliamps);
```

**Example:**

```yaml
on_...:
  - stepper.set_working_current:
      id: my_stepper
      current: 2000  # mA
```

#### `stepper.set_holding_current_percent`

Change the holding current percentage at runtime. Only works in `SR_OPEN` and `SR_CLOSE` modes.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **percent** (**Required**, [percent](https://esphome.io/guides/configuration-types.html#config-percentage)): Percentage of working current (10-90).

**C++ API:**

```cpp
void set_holding_current_percent(uint8_t percent);
```

**Example:**

```yaml
on_...:
  - stepper.set_holding_current_percent:
      id: my_stepper
      percent: 40  # 10-90%
```

#### `stepper.set_microstepping`

Change microstepping (step mode) at runtime. The effective steps per revolution (BASE_STEPS × microsteps) is automatically updated.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **subdivision** (**Required**, uint16): Microstepping / step mode (`1-256`), e.g., `1`=full, `2`=half, `4`=quarter.

**C++ API:**

```cpp
void set_microstepping(uint16_t subdivision);
```

**Example:**

```yaml
on_...:
  - stepper.set_microstepping:
      id: my_stepper
      subdivision: 32  # 1-256
```

#### `stepper.set_speed`

Set the maximum speed of the stepper at runtime.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **speed** (**Required**, [`templatable_with_unit`](#templatable_with_unit-type) as [`speed`](#speed-type)): The speed to drive the stepper at. Supports units like `steps/s`, `RPM`, `rev/s`, `deg/s`, `rad/s`. Value can be a number, string with unit, or lambda.

**C++ API:**

```cpp
void set_speed(Speed speed);
```

> [!NOTE]
> The `Speed` class has a constructor `Speed(float value, SpeedUnit unit, float steps_per_rev)` that ESPHome's code generator can use to construct instances directly from YAML parameters.

**Examples:**

```yaml
on_...:
  # Plain number with unit
  - stepper.set_speed:
      id: my_stepper
      speed: 250 steps/s
  
  # Lambda with unit
  - stepper.set_speed:
      id: my_stepper
      speed:
        value: !lambda "return id(speed_sensor).state;"
        unit: RPM
```

#### `stepper.set_acceleration`

Set the acceleration of the stepper at runtime (ESPHome stepper compatibility).

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **acceleration** (**Required**, [`templatable_with_unit`](#templatable_with_unit-type) as [`acceleration`](#acceleration-type)): The acceleration to use when starting to move. Supports units like `steps/s²`, `RPM/s`, `rev/s²`, `deg/s²`, `rad/s²`. Value can be a number, string with unit, or lambda.

**C++ API:**

```cpp
void set_acceleration(Acceleration acceleration);
```

> [!NOTE]
> The `Acceleration` class has a constructor `Acceleration(float value, AccelerationUnit unit, float steps_per_rev)` that ESPHome's code generator can use to construct instances directly from YAML parameters.

> [!IMPORTANT]
> **Hardware Limitation**
>
> Unlike the [ESPHome Stepper Component](https://esphome.io/components/stepper/), this component does not support separate acceleration and deceleration values. The MKS ServoXXD motor controllers only support a single acceleration/deceleration rate.
>
> **Validation Requirement:**
>
> - If a `stepper.set_deceleration` action is used, a validation error must be generated
> - Error message: `"set_deceleration is not supported. Use set_acceleration instead. Hardware only supports a single acceleration/deceleration value."`
> - Calling this action affects both acceleration and deceleration rates

**Examples:**

```yaml
on_...:
  # Plain number with unit
  - stepper.set_acceleration:
      id: my_stepper
      acceleration: 250 steps/s²
  
  # Lambda with unit
  - stepper.set_acceleration:
      id: my_stepper
      acceleration:
        value: !lambda "return id(accel_sensor).state;"
        unit: RPM/s
```

#### `stepper.stop`

Stop the current motor movement. Available in both Position and Speed modes.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **acceleration** (*Optional*, [`templatable_with_unit`](#templatable_with_unit-type) as [`acceleration`](#acceleration-type)): Deceleration to use when stopping the motor. Supports units like `steps/s²`, `RPM/s`, `rev/s²`, `deg/s²`, `rad/s²`. Value can be a number, string with unit, or lambda.

**C++ API:**

```cpp
void stop(optional<Acceleration> acceleration);
```

> [!NOTE]
> The `Acceleration` class has a constructor `Acceleration(float value, AccelerationUnit unit, float steps_per_rev)` that ESPHome's code generator can use to construct instances directly from YAML parameters.

> [!NOTE]
> The internal acceleration/deceleration value will be updated when calling this action with a `acceleration` parameter. If omitted, the current acceleration value is used. If a value was never set before, the component default (from `acceleration` config) is used.

> [!WARNING]
> At speeds above about 1000 RPM, avoid stopping too abruptly. Use some `acceleration` for smoother, safer stops to protect mechanics and couplings.

**Examples:**

```yaml
on_...:
  # Plain number with unit
  - stepper.stop:
      id: my_stepper
      acceleration: 500 steps/s²
  
  # Lambda with unit
  - stepper.stop:
      id: my_stepper
      acceleration:
        value: !lambda "return id(decel_sensor).state;"
        unit: RPM/s
```

#### `stepper.key_lock` / `stepper.key_unlock`

Lock or unlock the motor display buttons.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void key_lock();
void key_unlock();
```

**Examples:**

```yaml
on_...:
  - stepper.key_lock: my_stepper
  - stepper.key_unlock: my_stepper
```

### Position Mode Actions

Actions available only when [`mode`](#mode) is `POSITION`.

#### `stepper.set_target`

Set the target position of the motor. The stepper will move towards the target position and stop once reached.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **target** (**Required**, [`templatable_with_unit`](#templatable_with_unit-type) as [`position`](#position-type)): The target position. Supports units like `steps`, `revolutions`, `degrees`, `radians`. Value can be a number, string with unit, or lambda.

**C++ API:**

```cpp
void set_target(Position target);
```

> [!NOTE]
> The `Position` class has a constructor `Position(float value, PositionUnit unit, float steps_per_rev)` that ESPHome's code generator can use to construct instances directly from YAML parameters.

**Examples:**

```yaml
on_...:
  # Plain number (uses default unit: steps)
  - stepper.set_target:
      id: my_stepper
      target: 3200
  
  # String with unit
  - stepper.set_target:
      id: my_stepper
      target: 2.5 rev
  
  # Dict format with unit
  - stepper.set_target:
      id: my_stepper
      target:
        value: 720
        unit: deg
  
  # Lambda (uses default unit: steps)
  - stepper.set_target:
      id: my_stepper
      target: !lambda "return id(target_sensor).state;"
  
  # Lambda with explicit unit
  - stepper.set_target:
      id: my_stepper
      target:
        value: !lambda "return id(target_sensor).state;"
        unit: rev
```

#### `stepper.report_position`

Report the current position to a specific value. Sets an offset for future movements. To store a position for virtual homing, use [`stepper.set_zero`](#stepperset_zero) instead.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **position** (**Required**, [`templatable_with_unit`](#templatable_with_unit-type) as [`position`](#position-type)): The position to report. Supports units like `steps`, `revolutions`, `degrees`, `radians`. Value can be a number, string with unit, or lambda.

**C++ API:**

```cpp
void report_position(Position position);
```

> [!NOTE]
> The `Position` class has a constructor `Position(float value, PositionUnit unit, float steps_per_rev)` that ESPHome's code generator can use to construct instances directly from YAML parameters.

**Examples:**

```yaml
on_...:
  # Plain number (uses default unit: steps)
  - stepper.report_position:
      id: my_stepper
      position: 0
  
  # String with unit
  - stepper.report_position:
      id: my_stepper
      position: 1 rev
  
  # Dict format
  - stepper.report_position:
      id: my_stepper
      position:
        value: 90
        unit: deg
```

#### `stepper.home`

Execute homing sequence. Behavior depends on `homing.mode` configuration:

- `SENSORLESS`: Uses stall detection (sensorless homing)
- `ENDSTOP`: Uses endstop and GoHome command (real homing)
- `VIRTUAL`: Restarts motor to return to stored zero position

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void home(bool no_restart = false);
```

**Example:**

```yaml
on_...:
  - stepper.home: my_stepper
```

#### `stepper.set_zero`

Store the current position as persistent zero point for virtual homing. This must be called once before using virtual homing. The value is stored within the motor controller and remains after power-cycles. May only be used when `homing.mode` is `VIRTUAL`.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

**C++ API:**

```cpp
void set_zero();
```

**Example:**

```yaml
on_...:
  - stepper.set_zero: my_stepper
```

### Speed Mode Actions

Actions available only when [`mode`](#mode) is `SPEED`.

#### `stepper.run_continuous`

Run the motor continuously at specified speed. Used in speed mode and for continuous movements.

**Configuration:**

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **speed** (*Optional*, [`templatable_with_unit`](#templatable_with_unit-type) as [`speed`](#speed-type)): Target speed (signed; sign determines direction). Supports units like `steps/s`, `RPM`, `rev/s`, `deg/s`, `rad/s`. Value can be a number, string with unit, or lambda.
- **acceleration** (*Optional*, [`templatable_with_unit`](#templatable_with_unit-type) as [`acceleration`](#acceleration-type)): Acceleration for speed-mode change. Supports units like `steps/s²`, `RPM/s`, `rev/s²`, `deg/s²`, `rad/s²`. Value can be a number, string with unit, or lambda.

**C++ API:**

```cpp
void run_continuous(optional<Speed> speed,
                    optional<Acceleration> acceleration);
```

> [!NOTE]
> The `Speed` and `Acceleration` classes have constructors that take `(float value, Unit unit, float steps_per_rev)` parameters, allowing ESPHome's code generator to construct instances directly from YAML parameters.

> [!NOTE]
> At least one of `speed` or `acceleration` must be provided per call. Omitted values keep their last-used value. If a value was never set before, the component default (e.g., from `initial_speed` or `initial_acceleration`) is used. The sign of `speed` determines direction (positive => clockwise, negative => counter-clockwise).

**Examples:**

```yaml
on_...:
  # Set all parameters (clockwise)
  - stepper.run_continuous:
      id: my_stepper
      acceleration: 1000 steps/s²
      speed: 1000 steps/s
  
  # Change only speed (counter-clockwise)
  - stepper.run_continuous:
      id: my_stepper
    speed: -60 RPM
  
  # Use lambda for dynamic speed
  - stepper.run_continuous:
      id: my_stepper
      speed:
        value: !lambda "return id(speed_sensor).state;"
        unit: RPM
```
