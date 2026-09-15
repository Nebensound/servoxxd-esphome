# Changelog

## Unreleased

This describes the current development branch; it is not a dated or tagged stable
release.

### Available implementation

- MKS Servo28D/35D/42D/57D integration over RS485/Modbus RTU, with typed speed,
  acceleration, and position values.
- Position and speed operating modes, movement/stop actions, physical ENDSTOP
  homing, and motor configuration actions including `stepper.set_control_mode`.
- Layered core, stepper engine, command queue, and transport implementation.
- Public Layer 1 headers no longer expose Layer 4 headers; Modbus FC06 writes
  use the current ESPHome validation path.

### Documentation and validation cleanup

- Examples use `servoxxd` from GitHub's `develop` branch, current nested homing
  configuration, valid operating modes, and vFOC-compatible microsteps.
- Home Assistant number controls now send target/speed actions; position sensors
  read the component's reported position.
- Separate speed-mode example; obsolete/unsupported action and configuration
  fields removed from functional examples.
- Hardware-manual links point to the included V1.0.5 and V1.0.6 PDFs.
- CI compiles examples and integration YAMLs individually using temporary copies
  with local-component overrides. Public examples remain usable without a checkout.
  Regression checks cover both sources and complete Ruff TOML extraction.
- Format-config downloads fail explicitly on HTTP errors; empty/invalid Ruff
  documents are rejected before replacing the local configuration.

### Known limitations

- `VIRTUAL` and `SENSORLESS` homing are accepted by the schema, but their home
  actions are not implemented. Use physical ENDSTOP homing for a functional
  workflow. Motor actions and fixed delays do not guarantee successful completion.
- `stepper.report_position` currently updates local position only; logical offset
  behavior for subsequent motion is not implemented.
- Negative static speeds are rejected by the Python validator despite the
  specified signed-speed direction API. The speed example uses positive speed.
- The `microsteps: 256` in-memory width/conversion path and command-queue
  timeout/cancellation/deduplication behavior have bugs being addressed separately;
  this cleanup does not claim to fix them. The hardware encoding of 256 in bulk
  configuration remains unresolved even after an in-memory width fix.
- The unused `write_all_config` factory still has a 40-byte layout rather than
  the required 38 bytes. Motion encoders use internal RPM rather than the existing
  hardware-speed compensation helper. These release blockers are not fixed here.
- `SR_VFOC` requires `microsteps: 1`. The hardware uses 200 base steps/revolution,
  one acceleration/deceleration setting, and quantized speeds/positions.
- Direct Serial/CAN transports are not implemented. Passing configuration,
  compilation, or unit tests is not hardware certification or a stability claim.
