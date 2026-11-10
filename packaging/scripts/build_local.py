#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Build and validate one pinned target through Docker BuildKit."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_identity import ROOT, checkout_version, git_output, identity, load_targets, require


def _failure_stage(error: subprocess.CalledProcessError) -> str:
    output = "\n".join(str(value or "") for value in (error.stdout, error.stderr))
    match = re.findall(r"stage=([A-Za-z0-9._-]+)", output)
    return match[-1] if match else "docker-build"


def _run(command: list[str], target: str, stage: str, *, timeout: int | None = None) -> None:
    print(f"[{target}] stage={stage} $ {' '.join(command)}", flush=True)
    try:
        subprocess.run(command, cwd=ROOT, check=True, timeout=timeout)
    except (OSError, subprocess.SubprocessError) as error:
        if isinstance(error, subprocess.CalledProcessError):
            stage = _failure_stage(error)
            output = "\n".join(str(value or "") for value in (error.stdout, error.stderr)).strip()
        else:
            output = str(error)
        raise RuntimeError(f"target={target} stage={stage}: {output or error}") from error


def _validate_export(target: dict, revision: str, output: Path, version: str) -> None:
    records = [identity(target, role, revision, version) for role in ("runtime", "developer")]
    expected = {record["package_filename"] for record in records}
    actual = {path.name for path in output.iterdir() if path.is_file() and path.suffix in {".deb", ".rpm"}}
    require(actual == expected,
            f"target={target['id']} exported package set mismatch; expected={sorted(expected)}, actual={sorted(actual)}")
    required_evidence = {"build-evidence.json", "runtime-verification.json", "sdk-verification.json"}
    missing = required_evidence - {path.name for path in output.iterdir() if path.is_file()}
    require(not missing, f"target={target['id']} BuildKit export lacks verification evidence: {sorted(missing)}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--targets", type=Path, default=ROOT / "packaging/targets.json",
                        help="validated target-set JSON (defaults to the production set)")
    parser.add_argument("--revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    target_id = args.target
    try:
        revision = args.revision
        targets_path = args.targets.expanduser().resolve()
        require(targets_path.is_relative_to(ROOT), "--targets must be inside the Docker build context")
        targets = load_targets(targets_path)
        target = next((record for record in targets if record["id"] == target_id), None)
        require(target is not None, f"unknown target: {target_id}")
        version = checkout_version(ROOT, revision)
        status = git_output(ROOT, "status", "--porcelain", "--untracked-files=all")
        require(not status,
                "refusing to build an identity-bearing package from a dirty checkout; "
                "commit or otherwise provide a clean source revision")
        output = args.output.expanduser().absolute()
        require(not output.exists(), f"refusing to overwrite existing output directory: {output}")
        require(shutil.which("docker") is not None, "Docker CLI is required")
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="isched-build-", dir=output.parent) as temp:
            exported = Path(temp) / "export"
            os_packages = " ".join(target["os_packages"])
            command = [
                "docker", "buildx", "build", "--pull", "--no-cache", "--progress=plain",
                "--platform", target["platform"], "--file", str(ROOT / target["dockerfile"]),
                "--build-arg", f"ISCHED_SOURCE_REVISION={revision}",
                "--build-arg", f"ISCHED_PACKAGE_VERSION={version}",
                "--build-arg", f"ISCHED_OS_PACKAGES={os_packages}",
                "--build-arg", f"ISCHED_TARGET={target_id}",
                "--build-arg", f"ISCHED_BASE_IMAGE={target['image']}",
                "--build-arg", f"ISCHED_TARGETS_FILE={targets_path.relative_to(ROOT).as_posix()}",
                "--output", f"type=local,dest={exported}", str(ROOT),
            ]
            _run(command, target_id, "image-startup", timeout=14400)
            require(exported.is_dir(), f"target={target_id} BuildKit produced no local export")
            _validate_export(target, revision, exported, version)
            _run([
                sys.executable, str(ROOT / "packaging/scripts/validate_packages.py"),
                "--target", target_id, "--revision", revision, "--directory", str(exported),
                "--targets", str(targets_path),
            ], target_id, "package-validation", timeout=600)
            os.replace(exported, output)
        print(f"[{target_id}] validated output: {output}", flush=True)
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f"[{target_id}] build_local failed: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
