"""
ServoXXD Stepper Platform for ESPHome
Implements YAML validation and code generation for MKS ServoXXD motors
Transport layer: Modbus RTU (via *_modbus.cpp/h files)
"""

import math

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import modbus, stepper
from esphome.const import (
    CONF_ACCELERATION,
    CONF_ID,
    CONF_MAX_SPEED,
    CONF_POSITION,
    CONF_SPEED,
    CONF_TARGET,
)

# Create namespace (servoxxd - generic stepper logic)
# Modbus transport is in ServoXxd class (*_modbus files)
servoxxd_ns = cg.esphome_ns.namespace("servoxxd")

# Main component class (Modbus transport implementation)
ServoXxd = servoxxd_ns.class_(
    "ServoXxd", stepper.Stepper, modbus.ModbusDevice, cg.Component
)

# Action classes - all 20 actions declared in servoxxd namespace
# Movement actions
SetTargetAction = servoxxd_ns.class_("SetTargetAction", automation.Action)
RunContinuousAction = servoxxd_ns.class_("RunContinuousAction", automation.Action)
StopAction = servoxxd_ns.class_("StopAction", automation.Action)
EmergencyStopAction = servoxxd_ns.class_("EmergencyStopAction", automation.Action)
HomeAction = servoxxd_ns.class_("HomeAction", automation.Action)

# Position actions
ReportPositionAction = servoxxd_ns.class_("ReportPositionAction", automation.Action)
SetZeroAction = servoxxd_ns.class_("SetZeroAction", automation.Action)

# Motor control actions
EnableAction = servoxxd_ns.class_("EnableAction", automation.Action)
DisableAction = servoxxd_ns.class_("DisableAction", automation.Action)

# Configuration actions
SetSpeedAction = servoxxd_ns.class_("SetSpeedAction", automation.Action)
SetAccelerationAction = servoxxd_ns.class_("SetAccelerationAction", automation.Action)
SetControlModeAction = servoxxd_ns.class_("SetControlModeAction", automation.Action)
SetWorkingCurrentAction = servoxxd_ns.class_(
    "SetWorkingCurrentAction", automation.Action
)
SetHoldingCurrentPercentAction = servoxxd_ns.class_(
    "SetHoldingCurrentPercentAction", automation.Action
)
SetMicrosteppingAction = servoxxd_ns.class_("SetMicrosteppingAction", automation.Action)

# System actions
CalibrateAction = servoxxd_ns.class_("CalibrateAction", automation.Action)
ReleaseProtectionAction = servoxxd_ns.class_(
    "ReleaseProtectionAction", automation.Action
)
RestartAction = servoxxd_ns.class_("RestartAction", automation.Action)

# Key lock actions
KeyLockAction = servoxxd_ns.class_("KeyLockAction", automation.Action)
KeyUnlockAction = servoxxd_ns.class_("KeyUnlockAction", automation.Action)

# Enums for C++ (matching specification)
SpeedUnit = servoxxd_ns.enum("SpeedUnit", is_class=True)
SPEED_UNITS = {
    "STEPS_PER_SEC": SpeedUnit.STEPS_PER_SEC,
    "RPM": SpeedUnit.RPM,
    "REV_PER_SEC": SpeedUnit.REV_PER_SEC,
    "DEGREES_PER_SEC": SpeedUnit.DEGREES_PER_SEC,
    "RADIANS_PER_SEC": SpeedUnit.RADIANS_PER_SEC,
    "DEGREES_PER_MIN": SpeedUnit.DEGREES_PER_MIN,
    "DEGREES_PER_HOUR": SpeedUnit.DEGREES_PER_HOUR,
}

AccelerationUnit = servoxxd_ns.enum("AccelerationUnit", is_class=True)
ACCELERATION_UNITS = {
    "STEPS_PER_SEC_SQ": AccelerationUnit.STEPS_PER_SEC_SQ,
    "RPM_PER_SEC": AccelerationUnit.RPM_PER_SEC,
    "REV_PER_SEC_SQ": AccelerationUnit.REV_PER_SEC_SQ,
    "DEGREES_PER_SEC_SQ": AccelerationUnit.DEGREES_PER_SEC_SQ,
    "RADIANS_PER_SEC_SQ": AccelerationUnit.RADIANS_PER_SEC_SQ,
}

PositionUnit = servoxxd_ns.enum("PositionUnit", is_class=True)
POSITION_UNITS = {
    "STEPS": PositionUnit.STEPS,
    "REVOLUTIONS": PositionUnit.REVOLUTIONS,
    "DEGREES": PositionUnit.DEGREES,
    "RADIANS": PositionUnit.RADIANS,
    "ARCMINUTES": PositionUnit.ARCMINUTES,
    "ARCSECONDS": PositionUnit.ARCSECONDS,
}

ScreenMode = servoxxd_ns.enum("ScreenMode", is_class=True)
SCREEN_MODES = {
    "ALWAYS_ON": ScreenMode.ALWAYS_ON,
    "AUTO_OFF": ScreenMode.AUTO_OFF,
}

KeypadLock = servoxxd_ns.enum("KeypadLock", is_class=True)
KEYPAD_LOCK_VALUES = {
    "UNLOCKED": KeypadLock.UNLOCKED,
    "LOCKED": KeypadLock.LOCKED,
}

ZeroingSpeed = servoxxd_ns.enum("ZeroingSpeed", is_class=True)
ZEROING_SPEEDS = {
    "VERY_SLOW": ZeroingSpeed.VERY_SLOW,
    "SLOW": ZeroingSpeed.SLOW,
    "MEDIUM": ZeroingSpeed.MEDIUM,
    "FAST": ZeroingSpeed.FAST,
    "VERY_FAST": ZeroingSpeed.VERY_FAST,
}

Direction = servoxxd_ns.enum("Direction", is_class=True)
DIRECTIONS = {
    "CW": Direction.CW,
    "CCW": Direction.CCW,
}

HomingDirection = servoxxd_ns.enum("HomingDirection", is_class=True)
HOMING_DIRECTIONS = {
    "CW": HomingDirection.CW,
    "CCW": HomingDirection.CCW,
    "NEAREST": HomingDirection.NEAREST,
}

HomingMode = servoxxd_ns.enum("HomingMode", is_class=True)
HOMING_MODES = {
    "SENSORLESS": HomingMode.SENSORLESS,
    "ENDSTOP": HomingMode.ENDSTOP,
    "VIRTUAL": HomingMode.VIRTUAL,
}

EndstopTrigger = servoxxd_ns.enum("EndstopTrigger", is_class=True)
ENDSTOP_TRIGGERS = {
    "LOW": EndstopTrigger.TRIGGER_LOW,
    "HIGH": EndstopTrigger.TRIGGER_HIGH,
}

ServoType = servoxxd_ns.enum("ServoType", is_class=True)
SERVO_TYPES = {
    "SERVO28D": ServoType.SERVO28D,
    "SERVO35D": ServoType.SERVO35D,
    "SERVO42D": ServoType.SERVO42D,
    "SERVO57D": ServoType.SERVO57D,
}

ControlMode = servoxxd_ns.enum("ControlMode", is_class=True)
CONTROL_MODES = {
    "CR_OPEN": ControlMode.CR_OPEN,
    "CR_CLOSE": ControlMode.CR_CLOSE,
    "CR_VFOC": ControlMode.CR_VFOC,
    "SR_OPEN": ControlMode.SR_OPEN,
    "SR_CLOSE": ControlMode.SR_CLOSE,
    "SR_VFOC": ControlMode.SR_VFOC,
}

EnPinActive = servoxxd_ns.enum("EnPinActive", is_class=True)
EN_PIN_ACTIVE_VALUES = {
    "LOW": EnPinActive.EN_LOW,
    "HIGH": EnPinActive.EN_HIGH,
    "ALWAYS": EnPinActive.EN_ALWAYS,
}

HoldingCurrentPercent = servoxxd_ns.enum("HoldingCurrentPercent", is_class=True)
HOLDING_CURRENT_PERCENT_VALUES = {
    10: HoldingCurrentPercent.PERCENT_10,
    20: HoldingCurrentPercent.PERCENT_20,
    30: HoldingCurrentPercent.PERCENT_30,
    40: HoldingCurrentPercent.PERCENT_40,
    50: HoldingCurrentPercent.PERCENT_50,
    60: HoldingCurrentPercent.PERCENT_60,
    70: HoldingCurrentPercent.PERCENT_70,
    80: HoldingCurrentPercent.PERCENT_80,
    90: HoldingCurrentPercent.PERCENT_90,
}

OperatingMode = servoxxd_ns.enum("OperatingMode", is_class=True)
OPERATING_MODES = {
    "POSITION": OperatingMode.POSITION,
    "SPEED": OperatingMode.SPEED,
}

# Configuration constants
CONF_MODBUS_ID = "modbus_id"
CONF_ADDRESS = "address"
CONF_MICROSTEPS = "microsteps"
CONF_SERVO_TYPE = "servo_type"
CONF_CONTROL_MODE = "control_mode"
CONF_WORKING_CURRENT = "working_current"
CONF_HOLDING_CURRENT_PERCENT = "holding_current_percent"
CONF_EN_PIN_ACTIVE = "en_pin_active"
CONF_AUTO_SCREEN_OFF = "auto_screen_off"
CONF_LOCK_KEYS_AT_STARTUP = "lock_keys_at_startup"
CONF_MODE = "mode"
CONF_SLEEP_WHEN_DONE = "sleep_when_done"
CONF_HOMING = "homing"
CONF_HOMING_MODE = "mode"
CONF_HOMING_DIRECTION = "direction"
CONF_HOMING_SPEED = "speed"
CONF_ENDSTOP_TRIGGER = "endstop_trigger"
CONF_HOMING_CURRENT = "current"
CONF_AT_STARTUP = "at_startup"

# ============================================================================
# TYPE VALIDATORS
# ============================================================================


def validate_modbus_address(value):
    """Validate Modbus RTU address (1-247 decimal, typically written as hex)."""
    value = cv.hex_uint8_t(value)
    if value < 1 or value > 247:
        raise cv.Invalid(
            f"Modbus address must be between 0x01 and 0xF7 (1-247), got {hex(value)}"
        )
    return value


def validate_microsteps(value):
    """Validate microstepping value (1-256)."""
    value = cv.int_range(min=1, max=256)(value)
    return value


def validate_speed_with_unit(value, allow_inf=True):
    """
    Validate speed with unit support.
    Returns dict: {"value": float/templatable, "unit": "STEPS_PER_SEC"|"RPM"|...}

    Supports:
    - Plain number: Interpreted as steps/s (default unit)
    - String with unit: e.g., "1000 steps/s", "60 RPM", "360 deg/s"
    - Lambda: Must be in dict form with unit specified
    - Dict: {value: <number or lambda>, unit: <enum>}
    """
    # Handle lambda (must be dict with unit)
    if isinstance(value, cv.Lambda):
        raise cv.Invalid(
            "Lambda for speed must be specified as dict with unit: "
            "{value: !lambda ..., unit: STEPS_PER_SEC}"
        )

    # Handle dict format
    if isinstance(value, dict):
        if "value" not in value or "unit" not in value:
            raise cv.Invalid("Speed dict must have 'value' and 'unit' keys")

        unit_str = cv.enum(SPEED_UNITS, upper=True)(value["unit"])
        val = value["value"]

        # Value can be templatable
        if isinstance(val, cv.Lambda):
            return {"value": val, "unit": unit_str}

        val = cv.float_(val)
        if val <= 0 and not (allow_inf and val == float("inf")):
            raise cv.Invalid("Speed must be positive")

        return {"value": val, "unit": unit_str}

    # Handle string with unit
    if isinstance(value, str):
        value_str = value.strip()

        # Handle infinity
        if allow_inf and value_str.lower() in ("inf", "infinity"):
            return {"value": 1e6, "unit": "STEPS_PER_SEC"}

        # Parse unit suffixes
        unit_map = {
            ("steps/s", "steps/sec", "step/s", "step/sec"): "STEPS_PER_SEC",
            ("rpm", "RPM", "rev/min", "revolutions/min"): "RPM",
            (
                "rev/s",
                "rev/sec",
                "rps",
                "revolutions/s",
                "revolutions/sec",
            ): "REV_PER_SEC",
            (
                "deg/s",
                "deg/sec",
                "degrees/s",
                "degrees/sec",
                "°/s",
                "°/sec",
            ): "DEGREES_PER_SEC",
            ("rad/s", "rad/sec", "radians/s", "radians/sec"): "RADIANS_PER_SEC",
            ("deg/min", "deg/m", "°/min", "degrees/minute"): "DEGREES_PER_MIN",
            ("deg/h", "deg/hr", "°/h", "degrees/hour"): "DEGREES_PER_HOUR",
        }

        for suffixes, unit_type in unit_map.items():
            for suffix in suffixes:
                if value_str.lower().endswith(suffix.lower()):
                    value_part = value_str[: -len(suffix)].strip()
                    try:
                        val = float(value_part)
                        if val <= 0:
                            raise cv.Invalid(f"Speed must be positive, got {val}")
                        return {"value": val, "unit": unit_type}
                    except ValueError as e:
                        raise cv.Invalid(f"Invalid speed value '{value_part}': {e}")

        # No unit found, try parsing as plain number (default to steps/s)
        try:
            val = float(value_str)
            if val <= 0 and not (allow_inf and val == float("inf")):
                raise cv.Invalid("Speed must be positive")
            return {"value": val, "unit": "STEPS_PER_SEC"}
        except ValueError:
            raise cv.Invalid(f"Could not parse speed: {value}")

    # Handle plain number
    val = cv.float_(value)
    if val <= 0 and not (allow_inf and val == float("inf")):
        raise cv.Invalid("Speed must be positive")

    return {"value": val, "unit": "STEPS_PER_SEC"}


def validate_acceleration_with_unit(value, allow_inf=True):
    """
    Validate acceleration with unit support.
    Returns dict: {"value": float/templatable, "unit": "STEPS_PER_SEC_SQ"|"RPM_PER_SEC"|...}

    Similar to validate_speed_with_unit but for acceleration units.
    """
    # Handle lambda (must be dict with unit)
    if isinstance(value, cv.Lambda):
        raise cv.Invalid(
            "Lambda for acceleration must be specified as dict with unit: "
            "{value: !lambda ..., unit: STEPS_PER_SEC_SQ}"
        )

    # Handle dict format
    if isinstance(value, dict):
        if "value" not in value or "unit" not in value:
            raise cv.Invalid("Acceleration dict must have 'value' and 'unit' keys")

        unit_str = cv.enum(ACCELERATION_UNITS, upper=True)(value["unit"])
        val = value["value"]

        # Value can be templatable
        if isinstance(val, cv.Lambda):
            return {"value": val, "unit": unit_str}

        val = cv.float_(val)
        if val <= 0 and not (allow_inf and val == float("inf")):
            raise cv.Invalid("Acceleration must be positive")

        return {"value": val, "unit": unit_str}

    # Handle string with unit
    if isinstance(value, str):
        value_str = value.strip()

        # Handle infinity
        if allow_inf and value_str.lower() in ("inf", "infinity"):
            return {"value": 1e6, "unit": "STEPS_PER_SEC_SQ"}

        # Parse unit suffixes (more complex for acceleration)
        unit_map = {
            (
                "steps/s²",
                "steps/s^2",
                "steps/s/s",
                "steps/ss",
                "step/s²",
                "step/s^2",
            ): "STEPS_PER_SEC_SQ",
            (
                "rpm/s",
                "rpm/sec",
                "RPM/s",
                "RPM/sec",
                "rev/min/s",
                "rev/min/sec",
            ): "RPM_PER_SEC",
            (
                "rev/s²",
                "rev/s^2",
                "rev/s/s",
                "revolutions/s²",
                "revolutions/s^2",
            ): "REV_PER_SEC_SQ",
            (
                "deg/s²",
                "deg/s^2",
                "deg/s/s",
                "degrees/s²",
                "°/s²",
                "°/s^2",
            ): "DEGREES_PER_SEC_SQ",
            (
                "rad/s²",
                "rad/s^2",
                "rad/s/s",
                "radians/s²",
                "radians/s^2",
            ): "RADIANS_PER_SEC_SQ",
        }

        for suffixes, unit_type in unit_map.items():
            for suffix in suffixes:
                if value_str.lower().endswith(suffix.lower()):
                    value_part = value_str[: -len(suffix)].strip()
                    try:
                        val = float(value_part)
                        if val <= 0:
                            raise cv.Invalid(
                                f"Acceleration must be positive, got {val}"
                            )
                        return {"value": val, "unit": unit_type}
                    except ValueError as e:
                        raise cv.Invalid(
                            f"Invalid acceleration value '{value_part}': {e}"
                        )

        # No unit found, try parsing as plain number (default to steps/s²)
        try:
            val = float(value_str)
            if val <= 0 and not (allow_inf and val == float("inf")):
                raise cv.Invalid("Acceleration must be positive")
            return {"value": val, "unit": "STEPS_PER_SEC_SQ"}
        except ValueError:
            raise cv.Invalid(f"Could not parse acceleration: {value}")

    # Handle plain number
    val = cv.float_(value)
    if val <= 0 and not (allow_inf and val == float("inf")):
        raise cv.Invalid("Acceleration must be positive")

    return {"value": val, "unit": "STEPS_PER_SEC_SQ"}


def validate_position_with_unit(value):
    """
    Validate position with unit support.
    Returns dict: {"value": float/templatable, "unit": "STEPS"|"REVOLUTIONS"|...}

    Similar to validate_speed_with_unit but for position units.
    """
    # Handle lambda (must be dict with unit)
    if isinstance(value, cv.Lambda):
        raise cv.Invalid(
            "Lambda for position must be specified as dict with unit: "
            "{value: !lambda ..., unit: STEPS}"
        )

    # Handle dict format
    if isinstance(value, dict):
        if "value" not in value or "unit" not in value:
            raise cv.Invalid("Position dict must have 'value' and 'unit' keys")

        unit_str = cv.enum(POSITION_UNITS, upper=True)(value["unit"])
        val = value["value"]

        # Value can be templatable
        if isinstance(val, cv.Lambda):
            return {"value": val, "unit": unit_str}

        val = cv.float_(val)

        return {"value": val, "unit": unit_str}

    # Handle string with unit
    if isinstance(value, str):
        value_str = value.strip()

        # Parse unit suffixes
        unit_map = {
            ("steps", "step"): "STEPS",
            ("rev", "revolutions", "revolution"): "REVOLUTIONS",
            ("deg", "degrees", "degree", "°"): "DEGREES",
            ("rad", "radians", "radian"): "RADIANS",
            ("arcmin", "arcminute", "arcminutes", "'", "amin"): "ARCMINUTES",
            ("arcsec", "arcsecond", "arcseconds", '"', "asec"): "ARCSECONDS",
        }

        for suffixes, unit_type in unit_map.items():
            for suffix in suffixes:
                if value_str.lower().endswith(suffix.lower()):
                    value_part = value_str[: -len(suffix)].strip()
                    try:
                        val = float(value_part)
                        return {"value": val, "unit": unit_type}
                    except ValueError as e:
                        raise cv.Invalid(f"Invalid position value '{value_part}': {e}")

        # No unit found, try parsing as plain number (default to steps)
        try:
            val = float(value_str)
            return {"value": val, "unit": "STEPS"}
        except ValueError:
            raise cv.Invalid(f"Could not parse position: {value}")

    # Handle plain number
    val = cv.float_(value)
    return {"value": val, "unit": "STEPS"}


def validate_auto_sleep(value):
    """
    Validate auto_sleep configuration.
    Returns: uint32 milliseconds or special values:
    - UINT32_MAX (4294967295) = disabled (false/inf)
    - 0 = immediate (true/0s)
    - 1-4294967294 = delay in milliseconds
    """
    # Handle boolean
    if isinstance(value, bool):
        if value:
            return 0  # true = immediate
        else:
            return 0xFFFFFFFF  # false = disabled (UINT32_MAX)

    # Handle string "inf" or "infinity"
    if isinstance(value, str) and value.lower() in ("inf", "infinity"):
        return 0xFFFFFFFF  # disabled

    # Handle time period (ESPHome might pass as dict or need conversion)
    try:
        # Try cv.positive_time_period_milliseconds which handles strings like "30s"
        ms = cv.positive_time_period_milliseconds(value)
        if ms >= 0xFFFFFFFF:
            return 0xFFFFFFFE  # clamp to max-1
        return ms
    except Exception:
        pass

    raise cv.Invalid(
        "auto_sleep must be false/true/inf or a time period (e.g., '30s', '5min')"
    )


def validate_current(value):
    """
    Validate current with unit support (A or mA).
    Returns: int in milliamperes

    Accepts:
    - Plain number: interpreted as milliamperes (e.g., 1600 = 1600mA)
    - String with unit: "1.6A" or "1600mA"
    """
    # Handle string with unit
    if isinstance(value, str):
        value_str = value.strip().lower()

        # Check for mA unit
        if value_str.endswith("ma"):
            try:
                val = float(value_str[:-2].strip())
                return int(val)
            except ValueError as e:
                raise cv.Invalid(f"Invalid current value '{value_str}': {e}")

        # Check for A unit
        if value_str.endswith("a") and not value_str.endswith("ma"):
            try:
                val = float(value_str[:-1].strip())
                return int(val * 1000.0)  # Convert A to mA
            except ValueError as e:
                raise cv.Invalid(f"Invalid current value '{value_str}': {e}")

        # No unit - try parsing as plain number (interpret as mA)
        try:
            val = float(value_str)
            return int(val)
        except ValueError:
            raise cv.Invalid(f"Could not parse current: {value}")

    # Handle plain number (interpret as mA)
    val = cv.float_(value)
    return int(val)


def validate_homing_speed(value, homing_mode):
    """
    Validate homing speed - can be either:
    - Regular speed dict (for ENDSTOP/SENSORLESS modes)
    - ZeroingSpeed enum (for VIRTUAL mode)

    Returns:
    - For VIRTUAL: {"level": "SLOW", "unit": "ZEROING_SPEED"}
    - For others: {"value": float, "unit": "STEPS_PER_SEC"} etc.
    """
    # Try zeroing speed (for VIRTUAL mode)
    if isinstance(value, str):
        value_upper = value.upper()
        if value_upper in ZEROING_SPEEDS:
            if homing_mode != "VIRTUAL":
                raise cv.Invalid(
                    f"Zeroing speed levels (VERY_SLOW/SLOW/MEDIUM/FAST/VERY_FAST) "
                    f"can only be used with homing.mode: VIRTUAL, "
                    f"but mode is {homing_mode}"
                )
            return {"level": value_upper, "unit": "ZEROING_SPEED"}

    # Otherwise validate as regular speed
    speed_dict = validate_speed_with_unit(value, allow_inf=False)

    # For VIRTUAL mode, only zeroing speeds allowed
    if homing_mode == "VIRTUAL":
        raise cv.Invalid(
            "homing.speed for VIRTUAL mode must be a zeroing speed level "
            "(VERY_SLOW, SLOW, MEDIUM, FAST, or VERY_FAST), not a numeric speed"
        )

    return speed_dict


def validate_homing_direction(value, homing_mode):
    """
    Validate homing direction.
    NEAREST is only allowed for VIRTUAL mode.
    """
    direction = cv.enum(HOMING_DIRECTIONS, upper=True)(value)

    if direction == "NEAREST" and homing_mode != "VIRTUAL":
        raise cv.Invalid(
            f"homing.direction: NEAREST can only be used with homing.mode: VIRTUAL, "
            f"but mode is {homing_mode}"
        )

    return direction


# ============================================================================
# CONFIGURATION SCHEMA
# ============================================================================

# Homing configuration schema
HOMING_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HOMING_MODE): cv.enum(HOMING_MODES, upper=True),
        cv.Optional(
            CONF_HOMING_DIRECTION, default="CW"
        ): cv.string,  # Validated later with context
        cv.Optional(CONF_HOMING_SPEED, default="1 RPM"): cv.Any(
            cv.string, dict
        ),  # Validated later with context
        cv.Optional(CONF_ENDSTOP_TRIGGER, default="HIGH"): cv.enum(
            ENDSTOP_TRIGGERS, upper=True
        ),
        cv.Optional(CONF_HOMING_CURRENT): validate_current,  # Only for SENSORLESS
        cv.Optional(CONF_AT_STARTUP, default=True): cv.boolean,
    }
)


def validate_homing_config(config):
    """
    Validate homing configuration with cross-field dependencies.
    """
    homing_mode = config[CONF_HOMING_MODE]

    # Validate direction with mode context
    if CONF_HOMING_DIRECTION in config:
        config[CONF_HOMING_DIRECTION] = validate_homing_direction(
            config[CONF_HOMING_DIRECTION], homing_mode
        )

    # Validate speed with mode context
    if CONF_HOMING_SPEED in config:
        config[CONF_HOMING_SPEED] = validate_homing_speed(
            config[CONF_HOMING_SPEED], homing_mode
        )

    # Validate mode-specific fields
    if homing_mode == "ENDSTOP":
        # endstop_trigger is valid
        pass
    else:
        # endstop_trigger only for ENDSTOP mode
        if CONF_ENDSTOP_TRIGGER in config and config[CONF_ENDSTOP_TRIGGER] != "HIGH":
            raise cv.Invalid(
                f"homing.endstop_trigger can only be used with homing.mode: ENDSTOP, "
                f"but mode is {homing_mode}"
            )

    if homing_mode == "SENSORLESS":
        # current is valid
        pass
    else:
        # current only for SENSORLESS mode
        if CONF_HOMING_CURRENT in config:
            raise cv.Invalid(
                f"homing.current can only be used with homing.mode: SENSORLESS, "
                f"but mode is {homing_mode}"
            )

    return config


# Main component configuration schema
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ServoXxd),
            cv.GenerateID(CONF_MODBUS_ID): cv.use_id(modbus.Modbus),
            # Basic configuration
            cv.Optional(CONF_ADDRESS, default=0x01): validate_modbus_address,
            cv.Optional(CONF_MICROSTEPS, default=1): validate_microsteps,
            cv.Required(CONF_SERVO_TYPE): cv.enum(SERVO_TYPES, upper=True),
            cv.Optional(CONF_CONTROL_MODE, default="SR_VFOC"): cv.enum(
                CONTROL_MODES, upper=True
            ),
            # Speed and acceleration (ESPHome stepper compatibility)
            cv.Optional(CONF_SPEED): validate_speed_with_unit,
            cv.Optional(CONF_MAX_SPEED): validate_speed_with_unit,  # Alias for speed
            cv.Optional(
                CONF_ACCELERATION, default="inf"
            ): validate_acceleration_with_unit,
            # Motor configuration
            cv.Optional(CONF_WORKING_CURRENT): validate_current,
            cv.Optional(CONF_HOLDING_CURRENT_PERCENT, default=0.50): cv.percentage,
            cv.Optional(CONF_EN_PIN_ACTIVE, default="LOW"): cv.enum(
                EN_PIN_ACTIVE_VALUES, upper=True
            ),
            cv.Optional(CONF_AUTO_SCREEN_OFF, default="AUTO_OFF"): cv.All(
                cv.Any(cv.boolean, cv.enum(SCREEN_MODES, upper=True)),
                lambda value: "AUTO_OFF" if value is True else ("ALWAYS_ON" if value is False else value)
            ),
            cv.Optional(CONF_LOCK_KEYS_AT_STARTUP, default="UNLOCKED"): cv.enum(KEYPAD_LOCK_VALUES, upper=True),
            # Operating mode
            cv.Optional(CONF_MODE, default="POSITION"): cv.enum(
                OPERATING_MODES, upper=True
            ),
            # Position mode specific
            cv.Optional(CONF_SLEEP_WHEN_DONE): cv.Any(
                cv.boolean,
                cv.All(cv.string_strict, cv.one_of("inf", "infinity", lower=True)),
                cv.positive_time_period_milliseconds,
            ),
            cv.Optional(CONF_HOMING): HOMING_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(modbus.modbus_device_schema(0x01)),
    # Custom validation
    cv.has_at_least_one_key(CONF_SPEED, CONF_MAX_SPEED),  # At least one required
)


def validate_config_cross_fields(config):
    """
    Validate cross-field dependencies and constraints.
    """
    # Validate microsteps configuration in vFOC modes
    control_mode = config.get(CONF_CONTROL_MODE, "SR_VFOC")
    if control_mode in ("CR_VFOC", "SR_VFOC"):
        # vFOC mode only supports microsteps: 1 (no microstepping)
        if CONF_MICROSTEPS in config and config[CONF_MICROSTEPS] != 1:
            raise cv.Invalid(
                f"microsteps must be 1 in {control_mode} mode (hardware limitation). "
                "vFOC mode does not support microstepping. "
                "Either set 'microsteps: 1' or switch to OPEN/CLOSE control mode."
            )
    
    # Check for unsupported deceleration field (hardware limitation)
    if "deceleration" in config:
        raise cv.Invalid(
            "deceleration is not supported. Use acceleration instead. "
            "Hardware only supports a single acceleration/deceleration value."
        )

    # Ensure only one of speed/max_speed is set
    if CONF_SPEED in config and CONF_MAX_SPEED in config:
        raise cv.Invalid(
            "Cannot specify both 'speed' and 'max_speed'. "
            "They are aliases - use one or the other."
        )

    # Copy speed to max_speed if only speed is set (for internal consistency)
    if CONF_SPEED in config and CONF_MAX_SPEED not in config:
        config[CONF_MAX_SPEED] = config[CONF_SPEED]
    elif CONF_MAX_SPEED in config and CONF_SPEED not in config:
        config[CONF_SPEED] = config[CONF_MAX_SPEED]

    # Validate working current defaults based on servo_type
    servo_type = config[CONF_SERVO_TYPE]
    if CONF_WORKING_CURRENT not in config:
        # Set defaults based on servo type (in mA)
        defaults = {
            "SERVO28D": 600,
            "SERVO35D": 800,
            "SERVO42D": 1600,
            "SERVO57D": 3200,
        }
        config[CONF_WORKING_CURRENT] = defaults[servo_type]

    # Validate working current maximums
    working_current = config[CONF_WORKING_CURRENT]
    max_currents = {
        "SERVO28D": 3000,
        "SERVO35D": 3000,
        "SERVO42D": 3000,
        "SERVO57D": 5200,
    }
    if working_current > max_currents[servo_type]:
        raise cv.Invalid(
            f"working_current for {servo_type} must not exceed "
            f"{max_currents[servo_type]}mA, got {working_current}mA"
        )

    # Handle sleep_when_done validation (only for POSITION mode)
    operating_mode = config.get(CONF_MODE, "POSITION")

    if operating_mode == "POSITION" and CONF_SLEEP_WHEN_DONE in config:
        sleep_val = config[CONF_SLEEP_WHEN_DONE]
        if isinstance(sleep_val, bool):
            config[CONF_SLEEP_WHEN_DONE] = 0 if sleep_val else 0xFFFFFFFF
        elif isinstance(sleep_val, str) and sleep_val.lower() in ("inf", "infinity"):
            config[CONF_SLEEP_WHEN_DONE] = 0xFFFFFFFF
        elif isinstance(sleep_val, cv.TimePeriodMilliseconds):
            ms = sleep_val.total_milliseconds
            config[CONF_SLEEP_WHEN_DONE] = min(ms, 0xFFFFFFFE)
        elif isinstance(sleep_val, int):
            # Already milliseconds
            config[CONF_SLEEP_WHEN_DONE] = min(sleep_val, 0xFFFFFFFE)
    elif operating_mode == "POSITION":
        config[CONF_SLEEP_WHEN_DONE] = 0xFFFFFFFF  # Default: disabled

    # Validate homing current defaults if in SENSORLESS mode
    if CONF_HOMING in config:
        homing = config[CONF_HOMING]
        if homing[CONF_HOMING_MODE] == "SENSORLESS":
            if CONF_HOMING_CURRENT not in homing:
                # Set defaults based on servo type (in mA)
                defaults = {
                    "SERVO28D": 200,
                    "SERVO35D": 200,
                    "SERVO42D": 800,
                    "SERVO57D": 400,
                }
                homing[CONF_HOMING_CURRENT] = defaults[servo_type]

        # Validate homing configuration
        config[CONF_HOMING] = validate_homing_config(homing)

    # Mode-specific validation
    if operating_mode == "SPEED":
        # Speed mode doesn't support position-specific features
        if CONF_HOMING in config:
            raise cv.Invalid("homing is only available in POSITION mode")

    return config


# Apply cross-field validation
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, validate_config_cross_fields)


# ============================================================================
# CODE GENERATION
# ============================================================================


async def to_code(config):
    """
    Generate C++ code for the component configuration.
    """
    # Add required includes

    # Create component instance
    var = cg.new_Pvariable(config[CONF_ID])

    # Register as component and modbus device
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)

    # Set basic configuration
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_microsteps(config[CONF_MICROSTEPS]))
    cg.add(var.set_servo_type(config[CONF_SERVO_TYPE]))
    cg.add(var.set_control_mode(config[CONF_CONTROL_MODE]))

    # Set speed/acceleration (use max_speed as primary)
    speed_dict = config[CONF_MAX_SPEED]
    await set_speed_from_dict(var, speed_dict, config[CONF_MICROSTEPS])

    accel_dict = config[CONF_ACCELERATION]
    await set_acceleration_from_dict(var, accel_dict, config[CONF_MICROSTEPS])

    # Set motor configuration
    cg.add(var.set_working_current(config[CONF_WORKING_CURRENT]))
    # cv.percentage returns float 0.0-1.0, map to enum (10%-90% in 10% steps)
    holding_percent_float = config[CONF_HOLDING_CURRENT_PERCENT]
    holding_percent_int = int(round(holding_percent_float * 100))
    # Round to nearest 10% and clamp to 10-90 range
    holding_percent_int = max(10, min(90, (holding_percent_int + 5) // 10 * 10))
    holding_enum = HOLDING_CURRENT_PERCENT_VALUES[holding_percent_int]
    cg.add(var.set_holding_current_percent(holding_enum))
    cg.add(var.set_en_pin_active(config[CONF_EN_PIN_ACTIVE]))
    cg.add(var.set_auto_screen_off(SCREEN_MODES[config[CONF_AUTO_SCREEN_OFF]]))
    cg.add(var.set_lock_keys_at_startup(config[CONF_LOCK_KEYS_AT_STARTUP]))

    # Set operating mode
    cg.add(var.set_mode(config[CONF_MODE]))

    # Position mode specific
    if config[CONF_MODE] == "POSITION":
        cg.add(var.set_sleep_when_done(config[CONF_SLEEP_WHEN_DONE]))

        # Homing configuration
        if CONF_HOMING in config:
            homing = config[CONF_HOMING]
            cg.add(var.set_homing_mode(homing[CONF_HOMING_MODE]))
            cg.add(var.set_homing_direction(homing[CONF_HOMING_DIRECTION]))

            # Homing speed - depends on mode
            homing_speed = homing[CONF_HOMING_SPEED]
            if "level" in homing_speed:
                # ZeroingSpeed level for VIRTUAL mode - calls set_homing_speed_level(ZeroingSpeed)
                cg.add(
                    var.set_homing_speed_level(ZEROING_SPEEDS[homing_speed["level"]])
                )
            else:
                # Regular Speed object for ENDSTOP/SENSORLESS - reuse set_speed_from_dict
                await set_speed_from_dict(
                    var,
                    homing_speed,
                    config[CONF_MICROSTEPS],
                    "set_homing_speed",
                )

            if homing[CONF_HOMING_MODE] == "ENDSTOP":
                cg.add(var.set_homing_endstop_trigger(homing[CONF_ENDSTOP_TRIGGER]))

            if homing[CONF_HOMING_MODE] == "SENSORLESS":
                cg.add(var.set_homing_current(homing[CONF_HOMING_CURRENT]))

            cg.add(var.set_homing_at_startup(homing[CONF_AT_STARTUP]))


async def set_speed_from_dict(var, speed_dict, microsteps, method_name="set_speed"):
    """
    Create Speed object from dict and pass to C++ method.

    Args:
        var: Component variable
        speed_dict: {"value": float, "unit": "RPM"} dictionary
        microsteps: Microstepping subdivisions (for documentation only, C++ handles conversion)
        method_name: Name of method to call (set_speed or set_homing_speed)
    """
    value = speed_dict["value"]
    unit = speed_dict["unit"]

    # Create Speed object in C++
    if isinstance(value, cv.Lambda):
        template = await cg.templatable(value, [], cg.float_)
    else:
        template = value

    # Create Speed object and pass to specified method
    speed_obj = cg.RawExpression(
        f"esphome::servoxxd::Speed({template}, esphome::servoxxd::SpeedUnit::{unit}, {var})"
    )
    cg.add(getattr(var, method_name)(speed_obj))


async def set_acceleration_from_dict(var, accel_dict, microsteps):
    """
    Pass acceleration value and unit to C++ for runtime conversion.
    C++ will handle the conversion based on BASE_STEPS_PER_REVOLUTION and microsteps.
    
    Args:
        var: Component variable
        accel_dict: {"value": float, "unit": "RPM_PER_S"} dictionary
        microsteps: Microstepping subdivisions (for documentation only, C++ handles conversion)
    """
    value = accel_dict["value"]
    unit = accel_dict["unit"]

    # Always pass both value and unit to C++ - conversion happens there
    if isinstance(value, cv.Lambda):
        template = await cg.templatable(value, [], cg.float_)
    else:
        template = value

    cg.add(var.set_acceleration(template, ACCELERATION_UNITS[unit]))


# ============================================================================
# ACTIONS IMPLEMENTATION
# ============================================================================

# Constants for action parameters
CONF_CURRENT = "current"
CONF_PERCENT = "percent"
CONF_SUBDIVISION = "subdivision"
CONF_WORK_MODE = "work_mode"
CONF_DECELERATION = "deceleration"

# ============================================================================
# Position Mode Actions
# ============================================================================


@automation.register_action(
    "stepper.set_target",
    SetTargetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_TARGET): cv.templatable(validate_position_with_unit),
        }
    ),
)
async def stepper_set_target_to_code(config, action_id, template_arg, args):
    """Set target position - position mode only."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Handle position with unit
    pos_config = config[CONF_TARGET]
    if isinstance(pos_config, dict):
        # Value with unit - pass both to C++ for runtime conversion
        template_ = await cg.templatable(pos_config["value"], args, cg.float_)
        cg.add(var.set_value(template_))
        cg.add(var.set_unit(POSITION_UNITS[pos_config["unit"]]))
    else:
        # Top-level lambda: value in steps
        template_ = await cg.templatable(pos_config, args, cg.float_)
        cg.add(var.set_value(template_))
        cg.add(var.set_unit(POSITION_UNITS["STEPS"]))

    return var


@automation.register_action(
    "stepper.report_position",
    ReportPositionAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_POSITION): cv.templatable(validate_position_with_unit),
        }
    ),
)
async def stepper_report_position_to_code(config, action_id, template_arg, args):
    """Report current position - position mode only."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Handle position with unit
    pos_config = config[CONF_POSITION]
    if isinstance(pos_config, dict):
        # Value with unit - pass both to C++ for runtime conversion
        template_ = await cg.templatable(pos_config["value"], args, cg.float_)
        cg.add(var.set_value(template_))
        cg.add(var.set_unit(POSITION_UNITS[pos_config["unit"]]))
    else:
        # Top-level lambda: value in steps
        template_ = await cg.templatable(pos_config, args, cg.float_)
        cg.add(var.set_value(template_))
        cg.add(var.set_unit(POSITION_UNITS["STEPS"]))

    return var


@automation.register_action(
    "stepper.home",
    HomeAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_home_to_code(config, action_id, template_arg, args):
    """Execute homing sequence - position mode only."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "stepper.set_zero",
    SetZeroAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_set_zero_to_code(config, action_id, template_arg, args):
    """Store current position as zero for virtual homing - position mode only."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


# ============================================================================
# Speed Mode Actions
# ============================================================================


@automation.register_action(
    "stepper.run_continuous",
    RunContinuousAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(ServoXxd),
                cv.Optional(CONF_SPEED): cv.templatable(validate_speed_with_unit),
                cv.Optional(CONF_ACCELERATION): cv.templatable(
                    validate_acceleration_with_unit
                ),
            }
        ),
        cv.has_at_least_one_key(CONF_SPEED, CONF_ACCELERATION),
    ),
)
async def stepper_run_continuous_to_code(config, action_id, template_arg, args):
    """Run motor continuously - speed mode only."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Handle speed if provided
    if CONF_SPEED in config:
        speed_config = config[CONF_SPEED]
        if isinstance(speed_config, dict):
            # Value with unit - pass both to C++
            template_ = await cg.templatable(speed_config["value"], args, cg.float_)
            cg.add(var.set_speed(template_))
            cg.add(var.set_speed_unit(SPEED_UNITS[speed_config["unit"]]))
        else:
            # Top-level lambda: value in steps/s
            template_ = await cg.templatable(speed_config, args, cg.float_)
            cg.add(var.set_speed(template_))
            cg.add(var.set_speed_unit(SPEED_UNITS["STEPS_PER_SEC"]))

    # Handle acceleration if provided
    if CONF_ACCELERATION in config:
        accel_config = config[CONF_ACCELERATION]
        if isinstance(accel_config, dict):
            # Value with unit - pass both to C++
            template_ = await cg.templatable(accel_config["value"], args, cg.float_)
            cg.add(var.set_acceleration(template_))
            cg.add(var.set_acceleration_unit(ACCELERATION_UNITS[accel_config["unit"]]))
        else:
            # Top-level lambda: value in steps/s^2
            template_ = await cg.templatable(accel_config, args, cg.float_)
            cg.add(var.set_acceleration(template_))
            cg.add(var.set_acceleration_unit(ACCELERATION_UNITS["STEPS_PER_SEC_SQ"]))

    return var


@automation.register_action(
    "stepper.stop",
    StopAction,
    automation.maybe_conf(
        CONF_ID,
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(ServoXxd),
                cv.Optional(CONF_ACCELERATION): cv.templatable(
                    validate_acceleration_with_unit
                ),
            }
        ),
    ),
)
async def stepper_stop_to_code(config, action_id, template_arg, args):
    """Stop motor with deceleration."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Handle acceleration if provided
    if CONF_ACCELERATION in config:
        accel_config = config[CONF_ACCELERATION]
        if isinstance(accel_config, dict):
            # Value with unit - pass both to C++
            template_ = await cg.templatable(accel_config["value"], args, cg.float_)
            cg.add(var.set_acceleration(template_))
            cg.add(var.set_acceleration_unit(ACCELERATION_UNITS[accel_config["unit"]]))
        else:
            # Top-level lambda: value in steps/s^2
            template_ = await cg.templatable(accel_config, args, cg.float_)
            cg.add(var.set_acceleration(template_))
            cg.add(var.set_acceleration_unit(ACCELERATION_UNITS["STEPS_PER_SEC_SQ"]))

    return var


@automation.register_action(
    "stepper.emergency_stop",
    EmergencyStopAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_emergency_stop_to_code(config, action_id, template_arg, args):
    """Emergency stop - immediate halt with maximum deceleration."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


# ============================================================================
# Common Actions (Both Modes)
# ============================================================================


@automation.register_action(
    "stepper.enable",
    EnableAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_enable_to_code(config, action_id, template_arg, args):
    """Enable motor power."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "stepper.disable",
    DisableAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_disable_to_code(config, action_id, template_arg, args):
    """Disable motor power."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "stepper.calibrate",
    CalibrateAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_calibrate_to_code(config, action_id, template_arg, args):
    """Start motor calibration sequence."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "stepper.release_protection",
    ReleaseProtectionAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_release_protection_to_code(config, action_id, template_arg, args):
    """Release motor protection state after error."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "stepper.restart",
    RestartAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_restart_to_code(config, action_id, template_arg, args):
    """Restart motor controller."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


# ============================================================================
# Configuration Actions
# ============================================================================


@automation.register_action(
    "stepper.set_control_mode",
    SetControlModeAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_CONTROL_MODE): cv.enum(CONTROL_MODES, upper=True),
        }
    ),
)
async def stepper_set_control_mode_to_code(config, action_id, template_arg, args):
    """Change control mode (SR_OPEN/SR_CLOSE/SR_VFOC) at runtime."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_control_mode(config[CONF_CONTROL_MODE]))
    return var


@automation.register_action(
    "stepper.set_working_current",
    SetWorkingCurrentAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_CURRENT): cv.templatable(validate_current),
        }
    ),
)
async def stepper_set_working_current_to_code(config, action_id, template_arg, args):
    """Change working current at runtime."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_CURRENT], args, cg.uint16)
    cg.add(var.set_current(template_))
    return var


@automation.register_action(
    "stepper.set_holding_current_percent",
    SetHoldingCurrentPercentAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_PERCENT): cv.templatable(cv.percentage),
        }
    ),
)
async def stepper_set_holding_current_percent_to_code(
    config, action_id, template_arg, args
):
    """Change holding current percentage at runtime."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    # Percentage (0.0-1.0) is mapped to the 10-90 % enum (10 % steps) at runtime in C++
    template_ = await cg.templatable(config[CONF_PERCENT], args, cg.float_)
    cg.add(var.set_percent(template_))
    return var


@automation.register_action(
    "stepper.set_microstepping",
    SetMicrosteppingAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_SUBDIVISION): cv.templatable(validate_microsteps),
        }
    ),
)
async def stepper_set_microstepping_to_code(config, action_id, template_arg, args):
    """Change microstepping at runtime."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_SUBDIVISION], args, cg.uint16)
    cg.add(var.set_subdivision(template_))
    return var


@automation.register_action(
    "stepper.set_speed",
    SetSpeedAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_SPEED): cv.templatable(validate_speed_with_unit),
        }
    ),
)
async def stepper_set_speed_to_code(config, action_id, template_arg, args):
    """Set maximum speed at runtime."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Handle speed with unit
    speed_config = config[CONF_SPEED]
    if isinstance(speed_config, dict):
        # Value with unit - pass both to C++ for runtime conversion
        template_ = await cg.templatable(speed_config["value"], args, cg.float_)
        cg.add(var.set_value(template_))
        cg.add(var.set_unit(SPEED_UNITS[speed_config["unit"]]))
    else:
        # Plain value
        template_ = await cg.templatable(speed_config, args, cg.float_)
        cg.add(var.set_value(template_))

    return var


@automation.register_action(
    "stepper.set_acceleration",
    SetAccelerationAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
            cv.Required(CONF_ACCELERATION): cv.templatable(
                validate_acceleration_with_unit
            ),
        }
    ),
)
async def stepper_set_acceleration_to_code(config, action_id, template_arg, args):
    """Set acceleration at runtime."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Handle acceleration with unit
    accel_config = config[CONF_ACCELERATION]
    if isinstance(accel_config, dict):
        # Value with unit - pass both to C++ for runtime conversion
        template_ = await cg.templatable(accel_config["value"], args, cg.float_)
        cg.add(var.set_value(template_))
        cg.add(var.set_unit(ACCELERATION_UNITS[accel_config["unit"]]))
    else:
        # Plain value
        template_ = await cg.templatable(accel_config, args, cg.float_)
        cg.add(var.set_value(template_))

    return var


# ============================================================================
# Key Lock Actions
# ============================================================================


@automation.register_action(
    "stepper.key_lock",
    KeyLockAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_key_lock_to_code(config, action_id, template_arg, args):
    """Lock motor display buttons."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


@automation.register_action(
    "stepper.key_unlock",
    KeyUnlockAction,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(ServoXxd),
        }
    ),
)
async def stepper_key_unlock_to_code(config, action_id, template_arg, args):
    """Unlock motor display buttons."""
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var
