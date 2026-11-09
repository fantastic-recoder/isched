#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Install checksum-pinned tools inside a disposable target build container only."""
from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

from package_identity import ROOT, load_targets, require


def run(args: list[str], **kwargs) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run(args, check=True, timeout=600, **kwargs)


def archive(url: str, expected: str, algorithm: str, destination: Path) -> None:
    """Verify bytes before extracting; tar's data filter rejects unsafe paths/links."""
    require(not destination.exists(), f"refusing to overwrite {destination}")
    with tempfile.TemporaryDirectory(prefix="isched-tools-") as directory:
        temporary = Path(directory)
        download = temporary / "archive.tar"
        digest = hashlib.new(algorithm)
        with urllib.request.urlopen(url, timeout=60) as response, download.open("wb") as out:
            while chunk := response.read(1024 * 1024):
                out.write(chunk)
                digest.update(chunk)
        require(digest.hexdigest() == expected, f"{algorithm} mismatch for {url}")
        expanded = temporary / "expanded"
        with tarfile.open(download) as source:
            source.extractall(expanded, filter="data")
        children = list(expanded.iterdir())
        require(len(children) == 1 and children[0].is_dir(), "unexpected tool archive layout")
        shutil.copytree(children[0], destination, symlinks=True)


def bootstrap(target: str, prefix: Path) -> None:
    versions = json.loads((ROOT / "packaging/tool-versions.json").read_text())
    require(target in {t["id"] for t in load_targets()}, f"unknown target: {target}")
    require(f"{sys.version_info.major}.{sys.version_info.minor}" ==
            versions["python_versions"][target], "target Python interpreter version mismatch")
    prefix.mkdir(parents=True, exist_ok=True)
    node = prefix / "node"
    pnpm = prefix / "pnpm"
    conan = prefix / "conan"
    require(not conan.exists(), f"refusing to reuse tool environment: {conan}")
    archive(versions["node"]["url"], versions["node"]["sha256"], "sha256", node)
    archive(versions["pnpm_archive"]["url"],
            base64.b64decode(versions["pnpm_archive"]["sha512_base64"]).hex(), "sha512", pnpm)
    (node / "bin/pnpm").symlink_to(pnpm / "bin/pnpm.cjs")
    run([sys.executable, "-m", "venv", str(conan)])
    python = str(conan / "bin/python")
    run([python, "-m", "pip", "install", "--no-cache-dir", "--require-hashes", "--only-binary=:all:",
         "-r", str(ROOT / "packaging/requirements-bootstrap.lock")])
    run([python, "-m", "pip", "install", "--no-cache-dir", "--require-hashes", "--no-build-isolation",
         "-r", str(ROOT / "packaging/requirements-conan.lock")])
    env = dict(os.environ, PATH=f"{node}/bin:{conan}/bin:" + os.environ.get("PATH", ""))
    for args, expected in [
        ([str(node / "bin/node"), "--version"], "v" + versions["node"]["version"]),
        ([str(node / "bin/pnpm"), "--version"], versions["pnpm"]),
        ([str(conan / "bin/conan"), "--version"], "Conan version " + versions["conan"]),
    ]:
        actual = subprocess.check_output(args, text=True, env=env, timeout=60).strip()
        require(actual == expected, f"tool version mismatch: {args[0]}: {actual}")
        print(actual, flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--prefix", type=Path, default=Path("/opt"))
    args = parser.parse_args()
    try:
        bootstrap(args.target, args.prefix.resolve())
        return 0
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as exc:
        print(f"[{args.target}] tool bootstrap failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
