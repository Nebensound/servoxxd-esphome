# Homing Modes Specification

## Four Cases

### A) No Homing

```yaml
# homing: omitted
```

- `.home()`: Runtime warning, no-op
- Setup: 0x9A mode=0 (disable 0_Mode)

### B) ENDSTOP Mode

```yaml
homing:
  mode: ENDSTOP
  direction: CW|CCW          # not NEAREST
  speed: 600 rpm
  endstop_trigger: HIGH|LOW  # required
  at_startup: false
```

- `.home()`: Command 0x91 → moves to endstop
- Setup: 0x90 + 0x94(mode=0) + 0x9A(disable)
- If `at_startup=true`: Execute `.home()` after setup

### C) SENSORLESS Mode

```yaml
homing:
  mode: SENSORLESS
  direction: CW|CCW          # not NEAREST
  speed: 600 rpm
  current: 800 mA            # required
  at_startup: false
```

- `.home()`: Command 0x91 → stall detection
- Setup: 0x90 + 0x94(mode=1) + 0x9A(disable)
- If `at_startup=true`: Execute `.home()` after setup

### D) VIRTUAL Mode

```yaml
homing:
  mode: VIRTUAL
  direction: CW|CCW|NEAREST  # NEAREST allowed
  speed: MEDIUM              # discrete enum only
  at_startup: true
```

- `.home()`: Move to position 0 (blocking), round offset to no ticks
- Setup: 0x9A(enable if at_startup, else disable)
- `set_zero()`: Command 0x9A (enable: 1) + reset offset + Command 0x92 (store zero point)

---

## Key Behaviors

| Mode | `.home()` | at_startup | direction=NEAREST |
|------|-----------|------------|-------------------|
| None | Warning | - | - |
| ENDSTOP | Cmd 0x91 (hardware) | Runs `.home()` | ❌ |
| SENSORLESS | Cmd 0x91 (hardware) | Runs `.home()` | ❌ |
| VIRTUAL | Move to pos 0 (software) | Enables 0_Mode | ✅ |

---

## VIRTUAL Mode Details

### `.home()` = Move to position 0

- Not a hardware restart
- Direction: CW (clockwise to 0), CCW (counter-clockwise to 0), NEAREST (shortest path)
- Offset rounded to 0 ticks after arrival

### 0_Mode (auto-return on power-on)

- Only active when `at_startup=true`
- Command 0x9A with enable=1
- Separate from `.home()` action

### `.set_zero()` Action

- Equivalent to `report_position(0)` but:
- Command 0x92 (Set Axis to Zero)
- Resets offset to 0
- Stores zero point for 0_Mode (persistent)
