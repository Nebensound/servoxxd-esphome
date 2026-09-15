# ServoXXD Component Tests

## C++ unit tests

```bash
cd tests/unit
make test
```

These cover physical-unit conversions, commands, transport encoding/decoding,
command queues, the stepper engine, and the public-header boundary.
See [unit/README.md](unit/README.md).

## Python configuration regression tests

Use Python 3.11 or newer from an environment with ESPHome installed:

```bash
python -m unittest discover -s tests -p 'test_*.py'
```

`test_extract_ruff.py` uses only the standard library. It checks that the format
sync workflow's extractor retains Ruff assignments and nested tables and rejects
empty/invalid downloads without overwriting the existing configuration.

`test_examples.py` validates every YAML in `examples/` and `tests/esphome/` with
ESPHome, using temporary copies and public placeholder secrets. It also checks
local component loading, vFOC microstep constraints, action operating modes, and
ENDSTOP configuration for homing buttons. ESPHome's YAML loader rejects duplicate
mapping keys. No device credentials or hardware are required.

## ESPHome compilation

All checked-in configurations load the local `components/servoxxd` implementation,
not a remote copy. Prepare ignored placeholder secrets for compilation:

```bash
cp tests/esphome/secrets.yaml.template tests/esphome/secrets.yaml
cp examples/secrets.yaml.template examples/secrets.yaml
esphome compile tests/esphome/test_compile.yaml
```

Do not overwrite existing device secrets with these templates. CI compiles each
configuration separately; shell wildcard expansion is not an ESPHome test matrix.

| Configuration | Coverage |
| --- | --- |
| `esphome/test_compile.yaml` | Arduino ESP32; position and separate speed-mode motors/actions |
| `esphome/test_hardware.yaml` | Arduino ESP32; boot-time motor exercise with ENDSTOP homing |
| `esphome/test_valve_endstop.yaml` | ESP-IDF; valve positioning, ENDSTOP homing, templated actions |
| `../examples/*.yaml` | All user examples, including Home Assistant controls and speed mode |

The old host/setup-test YAML files are not part of this repository. Compilation
validates generated C++ and framework integration, not physical motor behavior.
Hardware tests contain diagnostic logging, not an automated proof that motion
succeeded.

## Hardware exercise

**Uploading the hardware test causes motor movement on boot.** Check current
limits, free travel, endstop wiring/polarity, UART pins, and Modbus address first.
Replace placeholder WiFi credentials and any public example API key before use.

```bash
esphome compile tests/esphome/test_hardware.yaml
esphome upload tests/esphome/test_hardware.yaml
timeout 30s esphome logs tests/esphome/test_hardware.yaml
```

Use separate compile/upload/log commands, not `esphome run`. Inspect the logs and
actual motor response; a timed wait expiring does not mean an operation succeeded.
