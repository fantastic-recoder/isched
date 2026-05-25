#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Run the frontend unit test suite from the repository root.

This is a tiny convenience wrapper around the UI project's `pnpm` test scripts.
By default it runs `pnpm run test:ci` in `src/ui/`, but you can choose any
script from `src/ui/package.json` and forward extra arguments after `--`.

Examples
--------
    # Run the full frontend unit suite:
    python3 tools/run_frontend_tests.py

    # Run a focused UI script:
    python3 tools/run_frontend_tests.py --script test:shell

    # Forward extra arguments to the package script:
    python3 tools/run_frontend_tests.py -- --watch=false
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
UI_DIR = REPO_ROOT / "src" / "ui"
DEFAULT_SCRIPT = "test:ci"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run isched frontend unit tests via the UI project's pnpm scripts.",
        epilog="Extra arguments after '--' are forwarded to the selected package script.",
    )
    parser.add_argument(
        "--script",
        default=DEFAULT_SCRIPT,
        help=(
            "Package script to run from src/ui/package.json "
            f"(default: {DEFAULT_SCRIPT})."
        ),
    )

    if "--" in argv:
        split = argv.index("--")
        own_argv, forwarded_argv = argv[:split], argv[split + 1 :]
    else:
        own_argv, forwarded_argv = argv, []

    ns = parser.parse_args(own_argv)
    ns.forwarded_argv = forwarded_argv
    return ns


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)

    if not UI_DIR.exists():
        print(f"[run_frontend_tests] ERROR: UI directory not found: {UI_DIR}", file=sys.stderr)
        return 1

    package_json = UI_DIR / "package.json"
    if not package_json.exists():
        print(f"[run_frontend_tests] ERROR: package.json not found: {package_json}", file=sys.stderr)
        return 1

    cmd = ["pnpm", "run", args.script, *args.forwarded_argv]
    print(f"[run_frontend_tests] Running : {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=str(UI_DIR))
    print(f"[run_frontend_tests] Done (exit {result.returncode}).")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())

