"""Extract complete Ruff tables without reformatting their TOML values."""

import argparse
from pathlib import Path
import re
import tomllib


def extract_ruff(source: str) -> str:
    document = tomllib.loads(source)
    tool = document.get("tool")
    ruff = tool.get("ruff") if isinstance(tool, dict) else None
    if not isinstance(ruff, dict) or not ruff:
        raise ValueError("Downloaded TOML has no non-empty tool.ruff configuration")

    lines = []
    in_ruff = False
    for line in source.splitlines(keepends=True):
        if re.match(r"^\s*\[\[?.+?\]\]?\s*(?:#.*)?$", line):
            in_ruff = bool(re.match(r"^\s*\[\s*tool\.ruff(?:\.|\s*\])", line))
        if in_ruff:
            lines.append(line)

    result = "".join(lines)
    # Fail closed if a multiline value looked like a header or upstream starts
    # using an unsupported header spelling.
    if tomllib.loads(result) != {"tool": {"ruff": ruff}}:
        raise ValueError(
            "Could not extract tool.ruff tables without changing their values"
        )
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        result = extract_ruff(args.source.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        parser.exit(1, f"Ruff configuration extraction failed: {error}\n")
    args.destination.write_text(
        "# Automatically synced from: https://github.com/esphome/esphome/blob/dev/pyproject.toml\n"
        "# DO NOT EDIT - Changes will be overwritten by update-format-configs workflow\n\n"
        + result,
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
