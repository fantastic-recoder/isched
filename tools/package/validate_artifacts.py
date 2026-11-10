#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Validate Ubuntu and Fedora package outputs using the shared validator."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "packaging/scripts/validate_packages.py"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ubuntu", type=Path, required=True)
    parser.add_argument("--fedora", type=Path, required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args(argv)
    if re.fullmatch(r"[0-9a-f]{40}", args.revision) is None:
        parser.error("--revision must be a full lowercase 40-digit SHA")

    failures = 0
    for target, path in (("ubuntu-26.04", args.ubuntu), ("fedora-45", args.fedora)):
        directory = path.expanduser().resolve()
        if not directory.is_dir():
            print(f"target={target} artifact validation failed: directory does not exist: {directory}",
                  file=sys.stderr)
            failures += 1
            continue
        command = [sys.executable, str(VALIDATOR), "--target", target,
                   "--revision", args.revision, "--directory", str(directory)]
        print(f"target={target} $ {' '.join(command)}", flush=True)
        try:
            result = subprocess.run(command, cwd=ROOT, timeout=600, check=False)
        except (OSError, subprocess.SubprocessError) as error:
            print(f"target={target} artifact validation could not run: {error}", file=sys.stderr)
            failures += 1
            continue
        if result.returncode:
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
