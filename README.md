# ServoXXD Stepper Component

The `servoxxd` stepper platform allows you to control MKS ServoXXD closed-loop stepper motors with RS485 communication via Modbus RTU.

> [!NOTE]
> Support for direct Serial communication may be added in the future.

## Installation

**Requires ESPHome 2026.8.2 or newer.** Older versions are rejected during
configuration validation. The component uses ESPHome's current Modbus client API;
update ESPHome before updating this component.

Add this external component to your ESPHome configuration:

```yaml
external_components:
  - source: github://Nebensound/servoxxd-esphome
    components: [ servoxxd ]
```

> [!TIP]
> To use a specific branch or an existing tag, add it after `@`, for example `github://Nebensound/servoxxd-esphome@develop`. Public examples use this GitHub source and require no repository checkout. Tests and CI override only temporary copies to validate the code under review.

> [!WARNING]
> This component is under development, not a declared stable release. `VIRTUAL` and `SENSORLESS` homing actions are not implemented; use a wired `ENDSTOP` for homing. Position-offset reporting, negative static speed validation, microstepping at 256, and command-queue reliability have outstanding limitations. See [CHANGELOG.md](CHANGELOG.md). Validate wiring, current limits, and travel clearance before running motor actions.

## Configuration

```yaml
# Base setup shared by both profiles

stepper:
  - platform: servoxxd
    id: my_stepper
    address: 0x01
    microsteps: 1  # 200 base steps × 1 = 200 effective steps/rev
    control_mode: SR_VFOC
    servo_type: SERVO42D
    speed: 1000 steps/s          # or max_speed (alias)
    acceleration: 500 steps/s^2
    working_current: 1.6A
    holding_current_percent: 40%
    en_pin_active: ALWAYS
    auto_screen_off: false
    lock_keys_at_startup: UNLOCKED
    mode: ... # POSITION | SPEED
```

> [!NOTE]
> This component requires the Modbus component to be set up as well.

## Base Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): Specify the ID of the stepper so that you can control it.
- **modbus_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the Modbus controller. Only needed when you have multiple Modbus controllers.
- **address** (*Optional*, int): The Modbus device address. Defaults to `0x01`. Range: 1-247.
- **microsteps** (*Optional*, int): Microstepping subdivision (aka step mode). Typical values: `1`=full-step, `2`=half-step, `4`=quarter-step, then `8`, `16`, `32`, … Schema range `1-256`; the 256 setting has an outstanding implementation bug. Defaults to `1`.

> [!IMPORTANT]
> **Hardware Compatibility**: ServoXXD motors **only support 1.8° step angle motors** (200 base steps per revolution). 0.9° motors (400 steps/rev) are **not supported** by the hardware.
>
> The effective resolution is calculated as: `effective_steps_per_revolution = 200 × microsteps`
>
> Examples:
> - `microsteps: 1` → 200 steps/rev (full-step mode)
> - `microsteps: 16` → 3200 steps/rev (common in SR_CLOSE)
> - `microsteps: 256` → 51200 steps/rev (conversion only; hardware encoding remains unresolved)

> [!WARNING]
> **Speed Calibration**: Motor speed is factory-calibrated only for microstepping values **16, 32, and 64**. For other values, actual speed may differ from commanded speed.

> [!NOTE]
> **vFOC Restriction**: When using `control_mode: SR_VFOC`, only `microsteps: 1` is supported due to hardware limitations.

- **servo_type** (**Required**, enum): Motor model used. One of `SERVO28D`, `SERVO35D`, `SERVO42D`, `SERVO57D`. Must match your physical motor.
- **control_mode** (*Optional*, enum): Motor control mode. One of `SR_OPEN`, `SR_CLOSE`, `SR_VFOC`. Defaults to `SR_VFOC`.
  - `SR_OPEN`: Open-loop mode, stepper behaves like a regular stepper motor. Working current is `working_current`, holding current is `holding_current_percent` of working current.
  - `SR_CLOSE`: Closed-loop mode, same as `SR_OPEN` but with position feedback from encoder to prevent missed steps.
  - `SR_VFOC`: FOC mode (recommended), same as `SR_CLOSE` but current may be adaptet to the steppers needs up to the max `working_current`. `holding_current_percent` is ignored in this mode.

- **speed** (**Required**, or **max_speed** alias): Target speed in `steps/s` (ESPHome stepper compatibility). Hardware limits based on `control_mode`: `SR_OPEN`: 400 RPM, `SR_CLOSE`: 1500 RPM, `SR_VFOC`: 3000 RPM.
  - Supported forms:
    - `speed: 6000`                        # steps/s (default unit)
    - `speed: "2000 RPM"`                  # RPM (revolutions per minute)
    - `speed: "60 rev/min"`                # Alternative: rev/min = RPM
    - `speed: "1.5 rev/s"`                 # Revolutions per second
    - `speed: "360 deg/s"`                 # Degrees per second (1 rev/s)
    - `speed: "180 deg/min"`               # Degrees per minute (0.5 RPM)
    - `speed: "15 deg/h"`                  # Degrees per hour (astronomical tracking)
    - `speed: { value: 2000, unit: RPM }`
    - `max_speed: 2000 RPM`                # Alternative name (exact same meaning)
  
> [!NOTE]
> `speed` and `max_speed` are aliases - use one or the other, not both. Multiple unit string formats are accepted (e.g., `rpm`, `RPM`, `rev/min` all work).

> [!TIP]
> **Use Cases for Angular Velocity Units:**
> - **High-speed rotation:** `steps/s`, `RPM`, `rev/s`, `deg/s` - For fast movements and robotics
> - **Moderate rotation:** `deg/min` - For slow continuous rotation (display turntables, camera pans)
> - **Astronomical tracking:** `deg/h` - For telescope mounts following celestial objects (Earth rotates 15°/hour)

- **acceleration** (*Optional*): Acceleration in `steps/s²` (ESPHome stepper compatibility). Default: `inf` (instant).
  - Supported forms:
    - `acceleration: 500`                  # steps/s^2 (default unit)
    - `acceleration: "100 RPM/s"`          # RPM per second
    - `acceleration: "60 rev/min/s"`       # Alternative: rev/min/s = RPM/s
    - `acceleration: "1.5 rev/s²"`         # Revolutions per second squared
    - `acceleration: { value: 100, unit: RPM_PER_SEC }`
> [!IMPORTANT]
> **Hardware Limitation:** The motor controller does not support separate acceleration and deceleration values. Specifying a `deceleration` field will cause a **validation error**. Use `acceleration` only, which affects both acceleration and deceleration rates.

- **working_current** (*Optional*, [Current](https://esphome.io/guides/configuration-types.html#config-current)): Working current. Accepts units: `mA` or `A` (e.g., `1500`, `1500mA`, `1.5A`). Defaults and maximums depend on `servo_type`:
  - Defaults: `0.6A` (28D), `0.8A` (35D), `1.6A` (42D), `3.2A` (57D)
  - Max: up to `3.0A` (28D/35D/42D), up to `5.2A` (57D)

- **holding_current_percent** (*Optional*, [Percentage](https://esphome.io/guides/configuration-types.html#config-percentage)): Holding current as percentage of working current (10-90%). Use `40%` or `0.4`. Only effective in `SR_OPEN` and `SR_CLOSE` modes.

- **en_pin_active** (*Optional*, enum): EN pin behavior. One of `LOW`, `HIGH`, `ALWAYS`. The current schema defaults to `LOW`; set `ALWAYS` explicitly if required by your wiring.

- **auto_screen_off** (*Optional*, boolean): Automatically turn off motor display after 15 seconds. Defaults to `true`.

- **lock_keys_at_startup** (*Optional*, enum): Lock motor display buttons at startup. Use `LOCKED` or `UNLOCKED` (default).

- **mode** (*Optional*, enum): Operating mode of the stepper. One of `POSITION` or `SPEED`. Determines which actions and configurations are available. Defaults to `POSITION`.

## Speed Mode

```yaml
stepper:
  - platform: servoxxd
    id: my_stepper
    modbus_id: modbus1
    address: 0x01
    servo_type: SERVO42D
    mode: SPEED
    control_mode: SR_VFOC          # Hardware limit: 3000 RPM
    microsteps: 1                  # Required for SR_VFOC
    speed: 600 RPM                 # Target speed (validated against hardware limits)
```

### Configuration

- All other from [Base Configuration](#base-configuration).

In following actions are exclusively used in speed mode:

- [`stepper.run_continuous`](#stepperrun_continuous)

## Position Mode

```yaml
stepper:
  - platform: servoxxd
    id: my_stepper
    modbus_id: modbus1
    address: 0x01
    servo_type: SERVO42D
    mode: POSITION
    control_mode: SR_VFOC           # Hardware limit: 3000 RPM
    microsteps: 1                   # Required for SR_VFOC
    speed: 1000 steps/s             # Target speed (ESPHome compatibility)
    acceleration: 500 steps/s^2     # Optional: acceleration/deceleration rate

    # Physical endstop wired to the motor controller
    homing:
      mode: ENDSTOP
      at_startup: false          # run homing on boot
      speed: 10 RPM
      direction: CW              # CW | CCW
      endstop_trigger: HIGH      # match switch wiring
```

### Configuration

- **sleep_when_done** (*Optional*, [Time](https://esphome.io/guides/configuration-types.html#config-time) or boolean): Put the motor to sleep after reaching the target and waiting for the set amount of time. Defaults to `false` or `inf` which may deactivate this function. `true` or any other [Time](https://esphome.io/guides/configuration-types.html#config-time) value may deactivate the motor after that amount of [Time](https://esphome.io/guides/configuration-types.html#config-time). `true` may equal a delay of `0ms`.
- **homing** (*Optional*, object): Homing configuration. If omitted, `stepper.home()` action will log a warning and do nothing.
  - **mode** (**Required**, enum):
    - `ENDSTOP`: Hardware homing using a physical endstop (limit switch). Motor moves to endstop position.
    - `SENSORLESS`: Accepted by the schema, but the home action is not implemented.
    - `VIRTUAL`: Accepted by the schema, but the home action is not implemented.
  - **direction** (*Optional*, enum): `CW` clockwise, `CCW` counter-clockwise and `NEAREST`. Default: `CW`. `NEAREST` may only be used with `mode: VIRTUAL`.
  - **speed**: (*Optional*, string): Homing speed. Supports units: `RPM` or `steps/s`. Default: `1 RPM`.
  > [!NOTE]
  > In `mode: VIRTUAL`, the speed may only be provided in five discrete levels. Use `VERY_SLOW`, `SLOW`, `MEDIUM`, `FAST` or `VERY_FAST` in this mode to set the speed.
    Examples for ENDSTOP/SENSORLESS:
    - `speed: 1`                 # steps/s (default unit)
    - `speed: "50 RPM"`
    - `speed: { value: 50, unit: RPM }`
  - **endstop_trigger** (*Optional*, enum): Endstop may be `LOW` or `HIGH` to be recognized as triggered. May only be used for `mode: ENDSTOP`. Default: `HIGH`.
  - **current** (*Optional*, [Current](https://esphome.io/guides/configuration-types.html#config-current)): Current threshold for sensorless homing (stallguard detection). Accepts units: `mA` or `A` (e.g., `1500`, `1500mA`, `1.5A`). May only be used for `mode: SENSORLESS`. Default depends on `servo_type`: `0.2A` (28D), `0.2A` (35D), `0.8A` (42D), `0.4A` (57D). Hardware manual recommends setting to a smaller current to avoid motor damage during homing collisions.
  - **at_startup** (*Optional*, boolean): Run homing at startup. Default: `false`.

- All other from [Base Configuration](#base-configuration).

In following actions are exclusively used in position mode:

- [`stepper.set_target`](#stepperset_target)
- [`stepper.report_position`](#stepperreport_position)
- [`stepper.home`](#stepperhome)
- [`stepper.set_zero`](#stepperset_zero)

## `stepper.set_target`

Set the target position of the motor. The stepper will move towards the target position and stop once reached.

```yaml
on_...:
  # Syntax 1: Plain number (steps)
  - stepper.set_target:
      id: my_stepper
      target: 1000

  # Syntax 2: String with unit (parsed at compile-time)
  - stepper.set_target:
      id: my_stepper
      target: "5.5 revolutions"

  # Syntax 3: Dict with explicit unit
  - stepper.set_target:
      id: my_stepper
      target:
        value: 5.5
        unit: REVOLUTIONS

  # Syntax 4: Lambda with unit
  - stepper.set_target:
      id: my_stepper
      target:
        value: !lambda "return id(sensor).state;"
        unit: REVOLUTIONS
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **target** (**Required**, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): The target position. Accepts:
  - **Plain number** (int/float): Interpreted as steps (e.g., `1000`)
  - **String with unit**: Parsed at compile-time (e.g., `"5.5 revolutions"`, `"180 degrees"`, `"30 arcmin"`, `"1800 arcsec"`)
  - **Dict**: `{value: <number or lambda>, unit: <STEPS|REVOLUTIONS|DEGREES|RADIANS|ARCMINUTES|ARCSECONDS>}`
  
  Supported unit formats:
  - **Steps:** `steps`, `step`
  - **Revolutions:** `rev`, `revolutions`
  - **Degrees:** `deg`, `degrees`, `°`
  - **Radians:** `rad`, `radians`
  - **Arcminutes:** `arcmin`, `arcminute`, `'`, `amin` (1° = 60 arcminutes)
  - **Arcseconds:** `arcsec`, `arcsecond`, `"`, `asec` (1° = 3600 arcseconds)
  
> [!TIP]
> **Use Cases for Position Units:**
> - **Motor control:** `steps` - Direct motor steps for ESPHome compatibility
> - **Mechanical systems:** `revolutions` - Natural unit for rotating mechanisms
> - **Angular positioning:** `degrees`, `radians` - Standard engineering units
> - **High-precision optics:** `arcminutes`, `arcseconds` - Sub-degree positioning for microscopes, telescopes, laser alignment systems
    - `value` is templatable (can be lambda)
    - `unit` is static enum (not templatable)

## `stepper.report_position`

Set the locally reported current position. **Limitation:** the persistent/logical offset for subsequent movements is not implemented; this action logs a warning and only updates the local position. Do not use it to establish a reliable machine zero.

```yaml
on_...:
  # Syntax 1: Plain number (steps)
  - stepper.report_position:
      id: my_stepper
      position: 0

  # Syntax 2: String with unit
  - stepper.report_position:
      id: my_stepper
      position: "2 revolutions"

  # Syntax 3: Dict with explicit unit
  - stepper.report_position:
      id: my_stepper
      position:
        value: 2.5
        unit: REVOLUTIONS
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **position** (**Required**, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): The position to report. Accepts same formats as [`target` in set_target](#stepperset_target).

## `stepper.home`

```yaml
on_...:
  - stepper.home: my_stepper
```

Execute homing sequence. Behavior depends on `homing.mode` configuration:

- `ENDSTOP`: Starts hardware GoHome towards the physical endstop. The engine monitors completion asynchronously; the YAML action does not wait for the motor to finish.
- `SENSORLESS` and `VIRTUAL`: Log an unimplemented warning; no homing motion is started.
- If homing not configured: Logs warning and does nothing.

> [!IMPORTANT]
> Use a correctly wired physical endstop and confirm successful homing before issuing the next motion. A fixed delay alone is not evidence of completion.
## `stepper.set_zero`

Send the hardware zero-setting command. Only accepted at runtime when `homing.mode` is `VIRTUAL`; this does not implement the missing virtual home action and is not equivalent to the current local-only `report_position` implementation.

```yaml
on_...:
  - stepper.set_zero: my_stepper
```

> [!NOTE]
> Virtual homing remains incomplete. Use `ENDSTOP` for a functional homing workflow rather than relying on this action to make virtual homing work.

## `stepper.run_continuous`

Run the motor continuously at specified speed.

```yaml
on_...:
  # Full configuration
  - stepper.run_continuous:
      id: my_stepper
      acceleration: 1000 steps/s²
      speed: 1000 steps/s

  # With dict syntax for speed
  - stepper.run_continuous:
      id: my_stepper
      speed:
        value: 60
        unit: RPM
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **speed** (*Optional*, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): Target speed. The specified signed-speed behavior has a validation limitation noted below. Accepted positive forms:
  - **Plain number**: Interpreted as steps/s (e.g., `1000`)
  - **String with unit**: Parsed at compile-time (e.g., `"60 RPM"`, `"360 deg/s"`)
  - **Dict**: `{value: <number or lambda>, unit: <STEPS_PER_SEC|RPM|REV_PER_SEC|DEGREES_PER_SEC|RADIANS_PER_SEC>}`
- **acceleration** (*Optional*, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): Acceleration. Accepts:
  - **Plain number**: Interpreted as steps/s² (e.g., `500`)
  - **String with unit**: Parsed at compile-time (e.g., `"100 RPM/s"`)
  - **Dict**: `{value: <number or lambda>, unit: <STEPS_PER_SEC_SQ|RPM_PER_SEC|REV_PER_SEC_SQ>}`

> [!NOTE]
> At least one of `speed` or `acceleration` must be provided per call. Omitted values keep their last-used value. If a value was never set before, the component default is used. Signed speeds are specified for direction (positive=CW, negative=CCW), but the current Python validator rejects negative static values. The checked-in example uses positive speed only; there is no `direction` action parameter.

## `stepper.stop`

Stop the current motor movement.

```yaml
on_...:
  # Plain number
  - stepper.stop:
      id: my_stepper
      acceleration: 500 steps/s^2

  # With dict syntax
  - stepper.stop:
      id: my_stepper
      acceleration:
        value: 100
        unit: RPM_PER_SEC
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **acceleration** (*Optional*, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): Deceleration to use when stopping. Accepts same formats as acceleration in [`run_continuous`](#stepperrun_continuous).

> [!NOTE]
> `acceleration` keeps its last-used value. If a value was never set before, the component default is used.

> [!WARNING]
> At speeds above about 1000 RPM, avoid stopping too abruptly. Use a non-zero `acceleration` (deceleration) for smoother, safer stops to protect mechanics and couplings.

## `stepper.emergency_stop`

Emergency stop - immediately halt motor with maximum deceleration.

```yaml
on_...:
  - stepper.emergency_stop: my_stepper
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

> [!WARNING]
> At speeds above about 1000 RPM, avoid stopping too abruptly. Use a non-zero `acceleration` (deceleration) for smoother, safer stops to protect mechanics and couplings.

## `stepper.enable` / `stepper.disable`

Enable or disables the motor. Same action is used for the `sleep_when_done` configuration.

```yaml
on_...:
  - stepper.enable: my_stepper
  - stepper.disable: my_stepper
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

## `stepper.calibrate`

Start motor calibration sequence. Motor will move during calibration. Make sure that the stepper moves freely and is not obstructed.

```yaml
on_...:
  - stepper.calibrate: my_stepper
```

## `stepper.release_protection`

Release motor protection state after error condition. Is part of `stepper.home` action. Most of the cases that is th e right action to recover from an error.

```yaml
on_...:
  - stepper.release_protection: my_stepper
```

### `stepper.restart` Action

Restart the motor controller and reapply its configuration.

```yaml
on_...:
  - stepper.restart: my_stepper
```

## `stepper.set_control_mode`

Change the motor control mode at runtime.

```yaml
on_...:
  - stepper.set_control_mode:
      id: my_stepper
      control_mode: SR_CLOSE
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **control_mode** (**Required**, enum): Control mode, one of `SR_OPEN`, `SR_CLOSE`, `SR_VFOC`. Use `SR_VFOC` only with `microsteps: 1`.

## `stepper.set_working_current`

Change the working current at runtime.

```yaml
on_...:
  - stepper.set_working_current:
      id: my_stepper
      current: 2000  # mA
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **current** (**Required**, int): Current in mA (0-3000 for SERVO42D, 0-5200 for SERVO57D).

## `stepper.set_holding_current_percent`

Change the holding current percentage at runtime. Only works in `SR_OPEN` and `SR_CLOSE` modes.

```yaml
on_...:
  - stepper.set_holding_current_percent:
      id: my_stepper
      percent: 40%  # 10-90%
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **percent** (**Required**, percentage): Percentage of working current (`10%`-`90%`, or a ratio such as `0.4`).

## `stepper.set_microstepping`

Change microstepping (step mode) at runtime. The effective steps per revolution (200 × microsteps) is automatically updated. Position values in steps scale with microstepping changes.

**Example**: With `microsteps: 16`, position `1600 steps` represents a certain angle. After changing to `microsteps: 32`, the same angle would be `3200 steps`. However, if you command `stepper.set_target` to position `1600` after the change, it will move to half the previous angle.

```yaml
on_...:
  - stepper.set_microstepping:
      id: my_stepper
      subdivision: 32  # 1-256
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **subdivision** (**Required**, int): Microstepping / step mode (`1-256`), e.g., `1`=full, `2`=half, `4`=quarter.

> [!WARNING]
> Use subdivision `1` with `SR_VFOC`. The 256 setting has an outstanding implementation bug; the examples use lower subdivisions in `SR_CLOSE`.

## `stepper.set_speed`

Set the maximum speed of the stepper at runtime.

```yaml
on_...:
  # Plain number (steps/s)
  - stepper.set_speed:
      id: my_stepper
      speed: 250 steps/s

  # With dict syntax
  - stepper.set_speed:
      id: my_stepper
      speed:
        value: 60
        unit: RPM
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **speed** (**Required**, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): The speed to drive the stepper at. Accepts same formats as [`speed` in run_continuous](#stepperrun_continuous).

## `stepper.set_acceleration`

Set the acceleration of the stepper at runtime.

```yaml
on_...:
  # Plain number (steps/s²)
  - stepper.set_acceleration:
      id: my_stepper
      acceleration: 250 steps/s^2

  # With dict syntax
  - stepper.set_acceleration:
      id: my_stepper
      acceleration:
        value: 100
        unit: RPM_PER_SEC
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.
- **acceleration** (**Required**, [templatable](https://esphome.io/guides/configuration-types.html#config-templatable)): The acceleration to use when starting to move. Accepts same formats as acceleration in [`run_continuous`](#stepperrun_continuous).

> [!IMPORTANT]
> **Hardware Limitation:** The motor controller does not support separate acceleration and deceleration values. Calling this action affects both acceleration and deceleration rates. Using a `stepper.set_deceleration` action will cause a **validation error**.

## `stepper.key_lock` / `stepper.key_unlock`

Lock or unlock the motor display buttons.

```yaml
on_...:
  - stepper.key_lock: my_stepper
  - stepper.key_unlock: my_stepper
```

### Configuration

- **id** (**Required**, [ID](https://esphome.io/guides/configuration-types.html#config-id)): The ID of the stepper.

## Hardware Setup

### Supported Models

- MKS Servo28D
- MKS Servo35D
- MKS Servo42D
- MKS Servo57D

All motors must be D-series with RS485 communication. C-series motors and CAN-bus variants are not supported.

### Wiring

follow the [ESPHome Modbus Component](https://esphome.io/components/modbus.html) wiring instructions. Connect the motor's RS485 A/B lines to the corresponding Modbus transceiver A/B lines. Ensure proper power supply for the motor as per its specifications.

### Motor Configuration

> [!IMPORTANT]
> Before using this component, you must configure your motor for Modbus communication using the built-in display-menu and the three keys.

Required settings via motor menu:

- **Mb_RTU**: `Enable` (Enable MODBUS-RTU communication)
- **UartAddr**: Set device address 1-247 (must match ESPHome `address` config)
- **UartBaud**: Set baud rate (recommended: `9600`)

**Navigation:** Press `Menu` → Use `Next` to select → Press `Enter` to edit → Use `Next` to change → Press `Enter` to confirm

## Example Configurations

### Astronomical Telescope Mount

Use `deg/h` for sidereal tracking and `arcmin`/`arcsec` for precise positioning:

These snippets illustrate unit syntax, not guaranteed mechanical resolution. Hardware speed/position quantization applies; very small speeds or angular moves may round to zero. A position-mode speed setting alone does not start continuous tracking.

```yaml
stepper:
  - platform: servoxxd
    id: telescope_ra
    address: 0x01
    servo_type: SERVO42D
    microsteps: 16  # 200 × 16 = 3200 effective steps/rev
    control_mode: SR_CLOSE
    mode: POSITION
    
    # Sidereal tracking speed (Earth rotation: 15°/hour)
    speed: 15 deg/h
    acceleration: 10 deg/s²
    
    homing:
      mode: ENDSTOP
      speed: 30 deg/min
      direction: CW

# Slew to precise coordinates
script:
  - id: goto_target
    then:
      - stepper.set_speed:
          id: telescope_ra
          speed: 180 deg/min  # Fast slew
      - stepper.set_target:
          id: telescope_ra
          target: 3600 arcsec  # 1 degree = 3600 arcseconds
      - delay: 5s
      - stepper.set_speed:
          id: telescope_ra
          speed: 15 deg/h  # Speed for the next position move; not continuous tracking
```

### Display Turntable

Slow continuous rotation with `deg/min`:

```yaml
stepper:
  - platform: servoxxd
    id: turntable
    address: 0x02
    servo_type: SERVO35D
    microsteps: 1  # Required for SR_VFOC
    control_mode: SR_VFOC
    mode: SPEED
    
    # Moderate rotation for display
    speed: 30 deg/min  # 1 full rotation every 12 minutes
    acceleration: 50 deg/s²

# Control via automation
on_button:
  - stepper.run_continuous:
      id: turntable
      speed: 60 deg/min  # Double speed
```

### Precision Optical Alignment

Use `arcmin` for sub-degree positioning:

```yaml
stepper:
  - platform: servoxxd
    id: laser_gimbal
    address: 0x03
    servo_type: SERVO28D
    microsteps: 32  # 200 × 32 = 6400 effective steps/rev (high resolution)
    control_mode: SR_CLOSE
    mode: POSITION
    
    speed: 10 deg/s
    acceleration: 100 deg/s²

# Fine adjustment in arcminutes
script:
  - id: fine_adjust
    then:
      - stepper.set_target:
          id: laser_gimbal
          target: 30 arcmin  # 0.5 degrees
      - delay: 1s
      - stepper.set_target:
          id: laser_gimbal
          target:
            value: !lambda "return id(sensor).state;"  # Dynamic positioning
            unit: ARCMINUTES
```

<!-- Examples moved into Speed Mode and Position Mode sections above -->
## See Also

- [ESPHome Modbus Component](https://esphome.io/components/modbus.html)
- [ESPHome Stepper Component](https://esphome.io/components/stepper/)
- [MKS Servo42&57D RS485 User Manual V1.0.5](docs/servo_hardware_doc/MKS%20SERVO42%2657D_RS485%20User%20Manual%20V1.0.5.pdf)
- [MKS Servo42&57D RS485 User Manual V1.0.6](docs/servo_hardware_doc/MKS%20SERVO42%2657D_RS485%20User%20Manual%20V1.0.6.pdf)
