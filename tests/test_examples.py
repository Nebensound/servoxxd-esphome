"""Validate every example/integration YAML against the checked-out component."""

from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from urllib.parse import unquote

from esphome import yaml_util
from esphome.core import EsphomeError

from local_config import CONFIGS, ROOT, main, prepared_config
from configurations import configuration_matrix


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
        compile_job = workflow["jobs"]["esphome-compile"]
        self.assertEqual(compile_job["needs"], "configurations")
        self.assertEqual(
            compile_job["strategy"]["matrix"],
            "${{ fromJSON(needs.configurations.outputs.matrix) }}",
        )
        discovery = workflow["jobs"]["configurations"]
        self.assertEqual(
            discovery["outputs"]["matrix"], "${{ steps.discover.outputs.matrix }}"
        )
        step = next(step for step in discovery["steps"] if step.get("id") == "discover")
        self.assertEqual(
            step["run"],
            'echo "matrix=$(python tests/configurations.py)" >> "$GITHUB_OUTPUT"',
        )
        result = subprocess.run(
            [sys.executable, str(ROOT / "tests/configurations.py")],
            capture_output=True,
            text=True,
            check=True,
        )
        emitted = json.loads(result.stdout)
        self.assertEqual(set(emitted), {"yaml-file"})
        self.assertIsInstance(emitted["yaml-file"], list)
        self.assertTrue(emitted["yaml-file"])
        expected = [
            str(path.relative_to(ROOT))
            for folder in ("examples", "tests/esphome")
            for path in (ROOT / folder).glob("*.yaml")
            if path.is_file() and path.name != "secrets.yaml"
        ]
        self.assertCountEqual(emitted["yaml-file"], expected)
        self.assertCountEqual(
            emitted["yaml-file"], [str(path.relative_to(ROOT)) for path in CONFIGS]
        )
        compile_step = next(
            step
            for step in workflow["jobs"]["esphome-compile"]["steps"]
            if step.get("name", "").startswith("Compile ")
        )
        self.assertEqual(
            compile_step["run"].strip(),
            'python tests/local_config.py compile "${{ matrix.yaml-file }}"',
        )
        warning_fixture = "tests/esphome/test_build_warnings.yaml"
        self.assertIn(warning_fixture, emitted["yaml-file"])
        self.assertEqual(compile_step["if"], f"matrix.yaml-file != '{warning_fixture}'")
        warning_step = next(
            step
            for step in compile_job["steps"]
            if step.get("name") == "Check ServoXXD build warnings"
        )
        self.assertEqual(warning_step["if"], f"matrix.yaml-file == '{warning_fixture}'")
        self.assertEqual(warning_step["shell"], "bash")
        self.assertIn(
            'python tests/local_config.py compile "${{ matrix.yaml-file }}" 2>&1 | tee',
            warning_step["run"],
        )

    def test_ci_runs_all_python_suites_with_supported_esphome(self):
        workflow = yaml_util.load_yaml(ROOT / ".github/workflows/ci.yml")
        steps = workflow["jobs"]["configuration-tests"]["steps"]
        python_step = next(
            step for step in steps if step.get("uses") == "actions/setup-python@v5"
        )
        self.assertEqual(python_step["with"]["python-version"], "3.13")
        commands = [step["run"] for step in steps if "run" in step]
        self.assertIn("pip install esphome==2026.8.2", commands)
        for directory in ("tests", "tests/esphome", "tests/python"):
            self.assertIn(
                f"python -m unittest discover -s {directory} -p 'test_*.py'",
                commands,
            )

    def test_new_configurations_are_discovered_without_matrix_edits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for folder in ("examples", "tests/esphome"):
                (root / folder).mkdir(parents=True)
                (root / folder / "secrets.yaml").write_text("wifi_ssid: placeholder\n")
            with self.assertRaisesRegex(ValueError, "No example or integration"):
                configuration_matrix(root)
            (root / "examples/basic.yaml").touch()
            self.assertEqual(
                configuration_matrix(root), {"yaml-file": ["examples/basic.yaml"]}
            )
            (root / "tests/esphome/new_fixture.yaml").touch()
            self.assertEqual(
                configuration_matrix(root),
                {
                    "yaml-file": [
                        "examples/basic.yaml",
                        "tests/esphome/new_fixture.yaml",
                    ],
                },
            )

    def test_public_examples_default_to_github_without_checkout(self):
        for original in CONFIGS:
            if original.parent != ROOT / "examples":
                continue
            with (
                self.subTest(config=original.name),
                prepared_config(original, local=False) as path,
            ):
                config = yaml_util.load_yaml(path)
                self.assertEqual(
                    config["external_components"],
                    [
                        {
                            "source": "github://Nebensound/servoxxd-esphome@develop",
                            "components": ["servoxxd"],
                        }
                    ],
                )

    def test_local_override_preserves_configuration_and_placeholder_secrets(self):
        for original in CONFIGS:
            with (
                self.subTest(config=original.name),
                prepared_config(original, local=False) as public_path,
                prepared_config(original) as local_path,
            ):
                public = yaml_util.load_yaml(public_path)
                local = yaml_util.load_yaml(local_path)
                public.pop("external_components")
                local.pop("external_components")
                self.assertEqual(yaml_util.dump(public), yaml_util.dump(local))
                self.assertEqual(
                    (local_path.parent / "secrets.yaml").read_bytes(),
                    (original.parent / "secrets.yaml.template").read_bytes(),
                )

    def test_cli_propagates_failure_and_cleans_up_temporary_configuration(self):
        seen_paths = []

        def failed_compile(command, *, check):
            self.assertFalse(check)
            self.assertEqual(command[:4], [sys.executable, "-m", "esphome", "compile"])
            path = Path(command[4])
            seen_paths.append(path)
            source = yaml_util.load_yaml(path)["external_components"][0]["source"]
            self.assertEqual(
                source, {"type": "local", "path": str(ROOT / "components")}
            )
            return subprocess.CompletedProcess(command, 42)

        with (
            patch("sys.argv", ["local_config.py", "compile", str(CONFIGS[0])]),
            patch("local_config.subprocess.run", side_effect=failed_compile),
        ):
            self.assertEqual(main(), 42)
        self.assertEqual(len(seen_paths), 1)
        self.assertFalse(seen_paths[0].parent.exists())

    def test_all_configs_use_local_component_and_supported_actions(self):
        for original in CONFIGS:
            before = original.read_bytes()
            with (
                self.subTest(config=original.relative_to(ROOT)),
                prepared_config(original) as path,
            ):
                config = yaml_util.load_yaml(path)
                external = config["external_components"][0]
                self.assertEqual(external["components"], ["servoxxd"])
                self.assertEqual(external["source"]["type"], "local")
                self.assertTrue(Path(external["source"]["path"]).is_absolute())
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
                            self.assertEqual(motors[parameters["id"]]["mode"], "SPEED")
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
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(original.read_bytes(), before)
            self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
