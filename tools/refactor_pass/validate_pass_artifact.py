#!/usr/bin/env python3
"""Validate refactor pass artifact JSON against the feature schema."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate a refactor pass artifact JSON file")
    parser.add_argument("artifact", type=pathlib.Path, help="Path to refactor-pass-artifact.json")
    parser.add_argument(
        "--schema",
        type=pathlib.Path,
        default=pathlib.Path("specs/008-dod-mech-refactor/contracts/refactor-pass-artifact.schema.json"),
        help="Path to JSON schema file",
    )
    return parser.parse_args()


def load_json(path: pathlib.Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def main() -> int:
    args = parse_args()
    if not args.artifact.exists():
        print(f"Artifact not found: {args.artifact}", file=sys.stderr)
        return 2
    if not args.schema.exists():
        print(f"Schema not found: {args.schema}", file=sys.stderr)
        return 2

    artifact = load_json(args.artifact)
    schema = load_json(args.schema)

    try:
        import jsonschema
    except ModuleNotFoundError:
        print("jsonschema package is required. Install with: pip install jsonschema", file=sys.stderr)
        return 3

    validator = jsonschema.Draft202012Validator(schema)
    errors = sorted(validator.iter_errors(artifact), key=lambda e: list(e.path))
    if errors:
        print(f"FAILED: {len(errors)} validation error(s)")
        for error in errors:
            path = ".".join(str(p) for p in error.absolute_path) or "<root>"
            print(f" - {path}: {error.message}")
        return 1

    print("PASS: artifact JSON conforms to schema")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

