import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import tomllib
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / ".github/scripts/extract_ruff.py"
SPEC = importlib.util.spec_from_file_location("extract_ruff", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ExtractRuffTest(unittest.TestCase):
    def test_keeps_assignments_and_all_subsections(self):
        source = """
[project]
name = "not-ruff"
[tool.ruff]
target-version = "py311"
line-length = 88

[tool.ruff.lint]
select = [
    "E",
    "F",
]
[tool.ruff.lint.per-file-ignores]
"tests/*.py" = ["E501"]
[tool.other]
enabled = true
[tool.ruff.format] # later, non-contiguous subsection
quote-style = "double"
"""
        extracted = MODULE.extract_ruff(source)
        self.assertEqual(
            tomllib.loads(extracted),
            {"tool": {"ruff": tomllib.loads(source)["tool"]["ruff"]}},
        )
        self.assertIn('target-version = "py311"', extracted)
        self.assertIn('    "F",', extracted)
        self.assertNotIn("[tool.other]", extracted)

    def test_accepts_subsections_without_parent_header(self):
        source = '[tool.ruff.lint]\nselect = ["E"]\n'
        self.assertEqual(MODULE.extract_ruff(source), source)

    def test_rejects_invalid_missing_and_empty_configuration(self):
        for source in (
            "",
            "404: Not Found",
            "<html>error</html>",
            "[tool.ruff]\n",
            '[project]\nname = "no-ruff"',
            'tool = "not-a-table"',
            "[tool.ruff\n",
        ):
            with self.subTest(source=source), self.assertRaises(ValueError):
                MODULE.extract_ruff(source)

    def test_cli_does_not_overwrite_destination_on_invalid_download(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "download.toml"
            destination = Path(directory) / "pyproject.toml"
            destination.write_text("existing configuration")
            for content in ("", "<html>error</html>", "[tool.ruff]\n"):
                source.write_text(content)
                result = subprocess.run(
                    [sys.executable, str(SCRIPT), str(source), str(destination)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("extraction failed", result.stderr)
                self.assertEqual(destination.read_text(), "existing configuration")

    def test_cli_writes_valid_configuration_with_header(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "download.toml"
            destination = Path(directory) / "pyproject.toml"
            source.write_text("[tool.ruff]\nline-length = 88\n")
            subprocess.run(
                [sys.executable, str(SCRIPT), str(source), str(destination)],
                check=True,
            )
            self.assertTrue(
                destination.read_text().startswith("# Automatically synced")
            )
            self.assertEqual(
                tomllib.loads(destination.read_text())["tool"]["ruff"],
                {"line-length": 88},
            )


if __name__ == "__main__":
    unittest.main()
