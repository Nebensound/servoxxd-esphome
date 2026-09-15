# ServoXXD ESPHome Component - AI Coding Agent Guide

## Core Development Principles

**CRITICAL - READ FIRST:**

### 1. Specification is Source of Truth
- **All implementation must match the specifications exactly**
- Specifications are located in [docs/specification/](../docs/specification/)
- **NEVER change a specification without explicit user approval**
- When implementation doesn't match spec: Fix the implementation, or get user approval to change spec
- When spec is unclear or wrong: **STOP and ask the user for clarification**

### 2. Specification Change Workflow
```
User identifies need → Discussion with user → User approves spec change → Update spec → Update implementation → Update tests
```
**You MUST NOT skip any step, especially user approval before changing specs**

### 3. Test Maintenance
- **Every code change MUST include corresponding test updates**
- Tests validate that implementation matches specification
- Run tests before committing changes: `cd tests/unit && make test`
- If tests fail after your changes: Fix your implementation, not the tests (unless tests are wrong)
- New features require new tests

### 4. Development Cycle
1. Read relevant specification
2. Understand current implementation
3. Make changes to code
4. Update or add tests
5. Run tests to verify
6. **Only then**: Consider the change complete

## Project Overview

ESPHome external component for controlling MKS ServoXXD (28D/35D/42D/57D) closed-loop stepper motors via RS485/Modbus RTU. Provides position and speed control through YAML configuration and Home Assistant integration.

## ESPHome YAML Configuration

All specifications for YAML configuration are detailed in the [ESPHome Component Specification](../docs/specification/01-yaml-api.md). Documentation for the end user is available at [ESPHome ServoXXD Component Docs](../README.md).

## Architecture: 4-Layer Design

**Layered architecture with strict boundaries** ([detailed specification](../docs/specification/02-cpp-interface.md#high-level-architecture)):

- **Layer 1** (ServoXxd): ESPHome integration, YAML API facade → [Details](../docs/specification/02a-layer1-core.md)
- **Layer 2** (StepperEngine): State machine, movement coordination → [Details](../docs/specification/02b-layer2-stepper-engine.md)
- **Layer 3** (CommandQueue): Single-flight execution, deduplication → [Details](../docs/specification/02c-layer3-command-queue.md)
- **Layer 4** (Transport): Modbus/Serial abstraction, command encoding → [Details](../docs/specification/02d-layer4-transport.md)

**Key rule:** Each layer communicates only with adjacent layers. Commands flow down, callbacks flow up.

## Type-Safe Unit System

**Design Principle:** All physical quantities use strongly-typed wrapper classes for compile-time safety and runtime flexibility. ([detailed specification](../docs/specification/02-cpp-interface.md#type-safe-unit-system))

**Current implementations:**
- **Speed** - 7 units: RPM, steps/s, rev/s, deg/s, rad/s, deg/min, deg/h
- **Acceleration** - 5 units: RPM/s, steps/s², rev/s², deg/s², rad/s²
- **Position** - 6 units: steps, revolutions, degrees, radians, arcminutes, arcseconds

**Pattern:** Use `Speed::from_rpm(60.0f, steps_per_rev)`, never raw floats in public APIs.

**Critical:** When adding new physical quantities (Torque, Current, etc.), follow the same pattern documented in the specification.

## Building & Testing

**Unit tests** (preferred):
```bash
cd tests/unit && make test              # Run all tests
cd tests/unit && make test_speed        # Run specific test
```

**ESPHome compilation:**
```bash
esphome compile tests/esphome/test_compile.yaml
```

**Hardware test:**
```bash
esphome upload tests/esphome/test_hardware.yaml  # Do not use 'run'!
timeout 30s esphome logs tests/esphome/test_hardware.yaml
```

## Adding New Commands

**Determine layer first:**

**Internal commands** (Layer 4, no YAML):
1. Add enum to `Command` in servoxxd_transport.h
2. Implement encoder/decoder in servoxxd_command_decoder.cpp
3. Write unit test in tests/unit/test_command_decoder.cpp

**User-facing commands** (exposed in YAML):
1. **Update specification first** - Get user approval
2. Follow Layer 4 steps above
3. Add public API to ServoXxd (Layer 1)
4. Implement Python validation in components/servoxxd/__init__.py
5. Add example to examples/

## Common Pitfalls

1. **Don't bypass the type system** - Use `Speed::from_rpm()`, not raw floats
2. **Layer violations** - Layer 1 never calls Transport directly
3. **Missing microstep conversion** - Speed/Position conversions require `steps_per_revolution`
4. **Arduino macro conflicts** - Always `#undef degrees` and `#undef radians` in headers
5. **Ignoring specifications** - Read specs before changing public APIs

