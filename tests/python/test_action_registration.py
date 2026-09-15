"""Regression coverage for ESPHome's synchronous action metadata."""

import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

from esphome import automation
from esphome import config_validation as cv
from esphome.components import modbus, stepper  # Load built-in registrations first.


class ActionRegistrationTest(unittest.TestCase):
    def test_all_actions_are_explicitly_synchronous(self):
        source = (
            Path(__file__).resolve().parents[2]
            / "components/servoxxd/stepper/__init__.py"
        )
        spec = importlib.util.spec_from_file_location("servoxxd_stepper_test", source)
        module = importlib.util.module_from_spec(spec)
        with patch.object(
            automation, "register_action", wraps=automation.register_action
        ) as register:
            spec.loader.exec_module(module)

        expected = {
            "set_target",
            "report_position",
            "home",
            "set_zero",
            "run_continuous",
            "stop",
            "emergency_stop",
            "enable",
            "disable",
            "calibrate",
            "release_protection",
            "restart",
            "set_control_mode",
            "set_working_current",
            "set_holding_current_percent",
            "set_microstepping",
            "set_speed",
            "set_acceleration",
            "key_lock",
            "key_unlock",
        }
        self.assertEqual(
            {call.args[0] for call in register.call_args_list},
            {f"stepper.{name}" for name in expected},
        )
        self.assertEqual(register.call_count, len(expected))
        for call in register.call_args_list:
            with self.subTest(action=call.args[0]):
                self.assertIs(call.kwargs.get("synchronous"), True)

    def test_minimum_esphome_version(self):
        source = (
            Path(__file__).resolve().parents[2]
            / "components/servoxxd/stepper/__init__.py"
        )
        spec = importlib.util.spec_from_file_location("servoxxd_version_test", source)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for version in ("2026.6.4", "2026.8.0", "2026.8.1"):
            with (
                self.subTest(version=version),
                patch.object(cv, "ESPHOME_VERSION", version),
            ):
                with self.assertRaisesRegex(
                    cv.Invalid, "at least ESPHome version 2026.8.2"
                ):
                    module.CONFIG_SCHEMA({})
        for version in ("2026.8.2", "2026.9.0"):
            with (
                self.subTest(version=version),
                patch.object(cv, "ESPHOME_VERSION", version),
            ):
                config = module.CONFIG_SCHEMA(
                    {"servo_type": "SERVO42D", "speed": "30 RPM"}
                )
                self.assertIn("speed", config)


if __name__ == "__main__":
    unittest.main()
