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

Use Python 3.12 or newer with ESPHome 2026.8.2 or newer installed. CI uses
Python 3.13 and ESPHome 2026.8.2:

```bash
python -m unittest discover -s tests -p 'test_*.py'
python -m unittest discover -s tests/esphome -p 'test_*.py'
python -m unittest discover -s tests/python -p 'test_*.py'
```

`test_extract_ruff.py` uses only the standard library. It checks that the format
sync workflow's extractor retains Ruff assignments and nested tables and rejects
empty/invalid downloads without overwriting the existing configuration.

`test_examples.py` validates every YAML in `examples/` and `tests/esphome/` with
ESPHome, using temporary copies and public placeholder secrets. It also checks
the public examples' default GitHub source, temporary local component loading,
vFOC microstep constraints, action operating modes, and
ENDSTOP configuration for homing buttons. ESPHome's YAML loader rejects duplicate
mapping keys. No device credentials or hardware are required.

## ESPHome compilation

Public `examples/*.yaml` load `github://Nebensound/servoxxd-esphome@develop` and
require no checkout. Internal `tests/esphome/*.yaml` fixtures use local sources.
To compile either against the code under review, use the same helper as CI:

```bash
python tests/local_config.py compile examples/basic_stepper.yaml
python tests/local_config.py compile tests/esphome/test_compile.yaml
```

The helper creates a temporary copy with public placeholder secrets and overrides
its component source with the absolute path to this checkout's `components`.
Committed examples and device secrets are not modified. Temporary files and build
output are removed when the command exits. Use `config` instead of `compile` for
configuration-only validation. CI compiles each configuration separately; shell
wildcard expansion is not an ESPHome test matrix. A dependency-free discovery job
runs `python tests/configurations.py` on the CI checkout and emits the exact
`{"yaml-file": [...]}` matrix. New example/integration YAML files are automatically
included; `secrets.yaml` is excluded and an empty matrix is an error.

| Configuration | Coverage |
| --- | --- |
| `esphome/test_compile.yaml` | Arduino ESP32; position and separate speed-mode motors/actions |
| `esphome/test_hardware.yaml` | Arduino ESP32; boot-time motor exercise with ENDSTOP homing |
| `esphome/test_valve_endstop.yaml` | ESP-IDF; valve positioning, ENDSTOP homing, templated actions |
| `esphome/test_microstepping.yaml` | ESP-IDF; 256 microsteps with literal and templated actions |
| `esphome/test_build_warnings.yaml` | ESP-IDF; all 20 actions and ServoXXD build-warning checks |
| `../examples/*.yaml` | All user examples, including Home Assistant controls and speed mode |

The old host/setup-test YAML files are not part of this repository. Compilation
validates generated C++ and framework integration, not physical motor behavior.
Hardware tests contain diagnostic logging, not an automated proof that motion
succeeded.

## Hardware exercise

**Uploading the hardware test causes motor movement on boot.** Check current
limits, free travel, endstop wiring/polarity, UART pins, and Modbus address first.
Replace placeholder WiFi credentials and any public example API key before use.
Create `tests/esphome/secrets.yaml` from its adjacent template if it does not
already exist; never overwrite existing device credentials.

```bash
esphome compile tests/esphome/test_hardware.yaml
esphome upload tests/esphome/test_hardware.yaml
timeout 30s esphome logs tests/esphome/test_hardware.yaml
```

Use separate compile/upload/log commands, not `esphome run`. Inspect the logs and
actual motor response; a timed wait expiring does not mean an operation succeeded.

## Build-warning regressions

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
