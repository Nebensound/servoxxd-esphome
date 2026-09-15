"""Validate every example/integration YAML against the checked-out component."""

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import unquote

from esphome import yaml_util
from esphome.core import EsphomeError


ROOT = Path(__file__).resolve().parents[1]
CONFIGS = sorted([*ROOT.glob("examples/*.yaml"), *ROOT.glob("tests/esphome/*.yaml")])
CONFIGS = [path for path in CONFIGS if path.name != "secrets.yaml"]


def mappings(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from mappings(child)
    elif isinstance(value, list):
        for child in value:
            yield from mappings(child)


class ExampleTest(unittest.TestCase):
    def test_duplicate_configuration_keys_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "duplicate.yaml"
            for key in ("auto_screen_off", "lock_keys_at_startup"):
                with self.subTest(key=key):
                    path.write_text(f"{key}: first\n{key}: second\n")
                    with self.assertRaises(EsphomeError):
                        yaml_util.load_yaml(path)

    def test_readme_hardware_manual_links_exist(self):
        readme = (ROOT / "README.md").read_text()
        links = re.findall(r"\]\((docs/[^)]+\.pdf)\)", readme)
        self.assertEqual(len(links), 2)
        for link in links:
            self.assertTrue((ROOT / unquote(link)).is_file(), link)

    def test_ci_compiles_every_checked_in_configuration(self):
        workflow = yaml_util.load_yaml(ROOT / ".github/workflows/ci.yml")
        matrix = workflow["jobs"]["esphome-compile"]["strategy"]["matrix"]["yaml-file"]
        self.assertCountEqual(matrix, [str(path.relative_to(ROOT)) for path in CONFIGS])

    def test_all_configs_use_local_component_and_supported_actions(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout = Path(directory)
            (checkout / "components").symlink_to(
                ROOT / "components", target_is_directory=True
            )
            for folder in ("examples", "tests/esphome"):
                destination = checkout / folder
                destination.mkdir(parents=True)
                shutil.copyfile(
                    ROOT / folder / "secrets.yaml.template",
                    destination / "secrets.yaml",
                )
            for original in CONFIGS:
                with self.subTest(config=original.relative_to(ROOT)):
                    path = checkout / original.relative_to(ROOT)
                    shutil.copyfile(original, path)
                    config = yaml_util.load_yaml(path)
                    external = config["external_components"][0]
                    self.assertEqual(external["components"], ["servoxxd"])
                    self.assertEqual(external["source"]["type"], "local")
                    self.assertEqual(
                        (path.parent / external["source"]["path"]).resolve(),
                        ROOT / "components",
                    )
                    motors = {motor["id"]: motor for motor in config["stepper"]}
                    for motor in motors.values():
                        self.assertEqual(motor["platform"], "servoxxd")
                        if motor.get("control_mode", "SR_VFOC") == "SR_VFOC":
                            self.assertEqual(motor.get("microsteps", 1), 1)
                    for node in mappings(config):
                        for action, parameters in node.items():
                            if action == "stepper.run_continuous":
                                self.assertEqual(
                                    motors[parameters["id"]]["mode"], "SPEED"
                                )
                                self.assertNotIn("direction", parameters)
                            elif action == "stepper.home":
                                motor_id = (
                                    parameters
                                    if isinstance(parameters, str)
                                    else parameters["id"]
                                )
                                self.assertEqual(
                                    motors[motor_id]["homing"]["mode"], "ENDSTOP"
                                )
                            elif action == "stepper.set_microstepping":
                                motor = motors[parameters["id"]]
                                if motor.get("control_mode", "SR_VFOC") == "SR_VFOC":
                                    self.assertEqual(parameters["subdivision"], 1)
                    result = subprocess.run(
                        [sys.executable, "-m", "esphome", "config", str(path)],
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    self.assertEqual(
                        result.returncode, 0, result.stdout + result.stderr
                    )


if __name__ == "__main__":
    unittest.main()
