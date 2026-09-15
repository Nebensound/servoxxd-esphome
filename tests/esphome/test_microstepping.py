"""Run with python3 tests/esphome/test_microstepping.py (requires esphome on PATH)."""

from pathlib import Path
import re
import subprocess
import tempfile
import unittest


class MicrosteppingCodegenTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="servoxxd-microstepping-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        fixture = Path(__file__).with_suffix(".yaml").resolve()
        self.yaml = fixture.read_text().replace(
            "../../components", str(fixture.parents[2] / "components")
        )
        self.yaml = self.yaml.replace(
            "  name: servoxxd-microstepping-test",
            f"  name: servoxxd-microstepping-test\n  build_path: {self.directory / 'build'}",
        )

    def run_esphome(self, yaml, *command):
        config = self.directory / "test.yaml"
        config.write_text(yaml)
        return subprocess.run(
            ["esphome", *command, str(config)],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_256_configuration_and_actions_codegen(self):
        result = self.run_esphome(self.yaml, "compile", "--only-generate")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        generated = (self.directory / "build/src/main.cpp").read_text()
        self.assertIn("test_stepper->set_microsteps(256);", generated)
        action_values = re.findall(
            r"->set_subdivision\((256|\[\]\(\) -> uint16_t \{\s*"
            r"(?:#line[^\n]*\n\s*)?return 256;\s*\})\);",
            generated,
        )
        self.assertEqual(len(action_values), 2)
        self.assertRegex(generated, r"set_subdivision\(\[\]\(\) -> uint16_t")
        self.assertIn("return 256;", generated)

    def test_configuration_boundaries(self):
        for value in (0, 257):
            with self.subTest(value=value):
                result = self.run_esphome(
                    self.yaml.replace("microsteps: 256", f"microsteps: {value}"),
                    "config",
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("microsteps", result.stdout + result.stderr)

    def test_action_boundaries(self):
        for value in (0, 257):
            with self.subTest(value=value):
                result = self.run_esphome(
                    self.yaml.replace("subdivision: 256", f"subdivision: {value}"),
                    "config",
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("subdivision", result.stdout + result.stderr)

    def test_vfoc_restriction(self):
        vfoc = self.yaml.replace("control_mode: SR_CLOSE", "control_mode: SR_VFOC")
        result = self.run_esphome(vfoc, "config")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("microsteps must be 1", result.stdout + result.stderr)
        result = self.run_esphome(vfoc.replace("microsteps: 256", "microsteps: 1"), "config")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
