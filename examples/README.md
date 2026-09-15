# ServoXXD RS485 Examples

This directory contains example configurations for the `servoxxd` ESPHome component.

All examples load `../components` from this checkout, so tests exercise the local
implementation. Run them from the repository, or replace `external_components`
when copying a configuration elsewhere:

```yaml
external_components:
  - source: github://Nebensound/servoxxd-esphome@develop
    components: [servoxxd]
```

Select an existing branch/tag appropriate to your deployment; no stable release is
implied by these examples. See the [known limitations](../CHANGELOG.md).

## Setup

### WiFi Credentials

1. Copy the secrets template:
   ```bash
   cp examples/secrets.yaml.template examples/secrets.yaml
   ```

2. Edit `examples/secrets.yaml` with your WiFi credentials:
   ```yaml
   wifi_ssid: "YourActualWiFiSSID"
   wifi_password: "YourActualPassword"
   api_encryption_key: "YOUR_PRIVATE_BASE64_KEY"
   ota_password: "YourOTAPassword"
   fallback_ap_password: "fallback123"
   ```

3. The `secrets.yaml` file is in `.gitignore` and will not be committed.

## Prerequisites

Before using these examples, ensure your MKS Servo42D/57D motor is properly configured:

1. **Enable MODBUS-RTU Mode** (Critical!)
   - Use the motor's built-in display and buttons
   - Set the `Mb_RTU` menu option to `Enable`
   - Without this, the motor won't respond to MODBUS commands!

2. **Set Communication Parameters**
   - Baud rate: match the YAML (`38400`, or `9600` in the comprehensive example)
   - Parity: `EVEN`
   - Stop bits: `1`
   - Motor address: `1` (or any unique value for multi-motor setups)

3. **Configure Work Mode**
   - The basic/speed examples use `SR_VFOC` with `microsteps: 1`
   - Positioning examples use `SR_CLOSE` with `microsteps: 16`
   - Do not combine `SR_VFOC` with larger microstep values

4. **Wire RS485 Connection**
   - ESP TX -> RS485 module DI (Data Input)
   - ESP RX -> RS485 module RO (Receiver Output)
   - RS485 A/B -> Motor A/B terminals
   - Connect GND between all devices

## Examples

### 1. `basic_stepper.yaml`
Minimal configuration to get started. Shows:
- Basic UART setup for RS485
- Single motor control
- Required configuration only

Tip: Set `servo_type` to match your motor (SERVO28D | SERVO35D | SERVO42D | SERVO57D). This controls the default working current and the maximum allowed current.

**Use this if:** You're setting up your first motor and want to verify communication.

### 2. `advanced_positioning.yaml`
Demonstrates advanced features including:
- Home Assistant integration
- Position control with number inputs
- Homing and calibration buttons
- Status monitoring sensors
- Emergency stop functionality

**Use this if:** You want full Home Assistant integration with UI controls.

Note on currents by model:
- Defaults (if `working_current` omitted): 28D=600mA, 35D=800mA, 42D=1600mA, 57D=3200mA
- Maximums: 28D/35D/42D = 3000mA, 57D = 5200mA

### 3. `multi_motor.yaml`
Shows how to control multiple motors on one RS485 bus:
- Three motors with different addresses
- Individual homing buttons with explicit ENDSTOP configuration

**Use this if:** You're building a multi-axis system (CNC, 3D printer, robot arm, etc.)

Each motor can have its own `servo_type` and `working_current`.

### 4. `comprehensive_example.yaml`
Shows position-mode configuration, movement, current/control-mode changes, and
display actions. It is not an exhaustive list of every specified feature.

### 5. `speed_mode.yaml`
Shows continuous rotation and stopping on a separate `mode: SPEED` motor. The
current validator rejects negative static speeds, so this example uses CW only.

### Homing requirements
Advanced, comprehensive, and multi-motor examples require physical endstops wired
to the motor controllers. Adjust `endstop_trigger` and direction for the actual
wiring. Homing is manual (`at_startup: false`); confirm successful completion before
commanding another move. Fixed delays do not prove completion. `VIRTUAL` and
`SENSORLESS` home actions are not implemented. `report_position` does not yet
provide a persistent motion offset.

## Testing Your Setup

1. Use an example in this checkout, or update its component source when copying it
2. Adjust GPIO pins to match your hardware
3. Create a `secrets.yaml` file for WiFi credentials (advanced examples only)
4. Compile with: `esphome compile your_config.yaml`
5. Upload to your ESP32: `esphome upload your_config.yaml`
6. Monitor logs: `esphome logs your_config.yaml`

To validate all examples without uploading, use the Python environment containing
ESPHome: `python -m unittest discover -s tests -p test_examples.py` from the repository
root. The test uses temporary copies and public placeholder secrets, never device
credentials. Configuration/compilation checks do not verify motor behavior.

## Troubleshooting

**Motor doesn't respond:**
- ✅ Check if the Mb_RTU menu option is enabled
- ✅ Verify baud rate matches between ESP and motor
- ✅ Check RS485 wiring (A to A, B to B)
- ✅ Ensure motor address matches configuration

**Communication errors:**
- ✅ Reduce baud rate to 38400
- ✅ Check for proper grounding
- ✅ Verify parity is set to EVEN
- ✅ Try shorter RS485 cable

**Position inaccuracies:**
- ✅ Perform motor calibration first
- ✅ Use SR_vFOC or SR_CLOSE mode
- ✅ Ensure proper current setting for your load

## More Information

See the main [README.md](../README.md) for complete API documentation and component details.
