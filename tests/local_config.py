"""Run ESPHome on temporary configurations using this checkout, never remote code."""

import argparse
from contextlib import contextmanager
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

from esphome import yaml_util

from configurations import ROOT, configuration_matrix


CONFIGS = [ROOT / path for path in configuration_matrix()["yaml-file"]]


@contextmanager
def prepared_config(original: Path, *, local: bool = True):
    """Isolate placeholder secrets and source overrides from committed examples."""
    with tempfile.TemporaryDirectory(prefix="servoxxd-config-") as directory:
        path = Path(directory) / original.name
        shutil.copyfile(
            original.parent / "secrets.yaml.template", path.parent / "secrets.yaml"
        )
        shutil.copyfile(original, path)
        if local:
            config = yaml_util.load_yaml(path)
            config["external_components"] = [
                {
                    "source": {"type": "local", "path": str(ROOT / "components")},
                    "components": ["servoxxd"],
                }
            ]
            path.write_text(yaml_util.dump(config), encoding="utf-8")
        yield path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("config", "compile"))
    parser.add_argument("configuration", type=Path)
    args = parser.parse_args()
    original = args.configuration.resolve()
    if original not in CONFIGS:
        parser.error(
            "configuration must be a checked-in example or integration fixture"
        )
    with prepared_config(original) as path:
        return subprocess.run(
            [sys.executable, "-m", "esphome", args.command, str(path)], check=False
        ).returncode


if __name__ == "__main__":
    sys.exit(main())
