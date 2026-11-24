"""Discover the firmware CI matrix using only the Python standard library."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def configuration_matrix(root: Path = ROOT) -> dict[str, list[str]]:
    files = sorted(
        str(path.relative_to(root))
        for folder in ("examples", "tests/esphome")
        for path in (root / folder).glob("*.yaml")
        if path.is_file() and path.name != "secrets.yaml"
    )
    if not files:
        raise ValueError("No example or integration YAML configurations found")
    return {"yaml-file": files}


if __name__ == "__main__":
    print(json.dumps(configuration_matrix()))
