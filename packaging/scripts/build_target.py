#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Build and validate both package roles inside one pinned target container."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_identity import ROOT, conan_profile, identity, load_targets, require
from logged_process import run_logged


def run(stage: str, command: list[str], *, cwd: Path = ROOT,
        env: dict[str, str] | None = None, timeout: int = 3600) -> str:
    """Run one pipeline stage and preserve a target-qualified diagnostic."""
    target = os.environ.get("ISCHED_BUILD_TARGET", "unknown")
    print(f"[{target}] stage={stage} $ {' '.join(command)}", flush=True)
    try:
        completed = run_logged(
            command, cwd=cwd, env=env,
            check=True, timeout=timeout,
        )
    except (OSError, subprocess.SubprocessError) as error:
        detail = ""
        if isinstance(error, subprocess.CalledProcessError):
            detail = f"\nstdout:\n{error.stdout or ''}\nstderr:\n{error.stderr or ''}"
        raise RuntimeError(f"target={target} stage={stage} failed: {error}{detail}") from error
    return completed.stdout


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def frontend_outputs(root: Path) -> tuple[Path, Path, Path]:
    index = root / "src/ui/dist/isched-ui/browser/index.html"
    notices = root / "src/ui/dist/isched-ui/3rdpartylicenses.txt"
    header = root / "src/main/cpp/isched/backend/isched_ui_assets.hpp"
    for path in (index, notices, header):
        require(path.is_file() and path.stat().st_size > 0,
                f"frontend generation failed to produce non-empty {path}")
    return index, notices, header


def frontend_test_command(pnpm: str) -> list[str]:
    """Use Angular CLI's kebab-case option to bound the Jest worker pool."""
    return [pnpm, "--filter", "isched-ui", "run", "test", "--max-workers=1"]


def configure_command(lockfile: Path, profile: Path, package_args: tuple[str, ...]) -> list[str]:
    """Configure the pinned producer with explicitly bounded build resources."""
    command = [
        sys.executable, "configure.py", "-j3", "--conan-lockfile", str(lockfile),
        "--conan-host-profile", str(profile), "--conan-build-profile", str(profile),
        "--conan-build-tools",
    ]
    command.extend(f"--cmake-arg={value}" for value in package_args)
    return [*command, "release"]


def generate_packages(build_dir: Path, staging: Path, output: Path,
                      records: list[dict], env: dict[str, str]) -> None:
    run("package-generation", [
        "cpack", "--config", str(build_dir / "CPackConfig.cmake"), "-B", str(staging),
    ], env=env, timeout=1800)
    expected = {record["package_filename"] for record in records}
    actual = {path.name for path in staging.iterdir() if path.is_file()}
    require(actual == expected,
            f"package output mismatch; expected={sorted(expected)}, actual={sorted(actual)}")
    for filename in sorted(expected):
        shutil.copy2(staging / filename, output / filename)


def os_release() -> dict[str, str]:
    values: dict[str, str] = {}
    for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if separator:
            values[key] = value.strip().strip('"')
    return values


def verify_target(target: dict) -> None:
    require(platform.machine() in {"x86_64", "amd64"},
            f"target={target['id']} requires x86_64, got {platform.machine()}")
    release = os_release()
    expected_id = "ubuntu" if target["distribution"] == "ubuntu" else "fedora"
    require(release.get("ID") == expected_id,
            f"target={target['id']} expected OS ID {expected_id}, got {release.get('ID')}")
    expected_version = target["distribution_version"]
    actual_version = release.get("VERSION_ID", "").strip('"')
    require(actual_version == expected_version,
            f"target={target['id']} expected release {expected_version}, got {actual_version}")
    compiler = target["compiler"]
    actual_major = run("compiler-version", [compiler["cxx"], "-dumpfullversion"],
                       timeout=30).strip().split(".", 1)[0]
    require(actual_major == compiler["version"],
            f"target={target['id']} expected GCC {compiler['version']}, got {actual_major}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--targets", type=Path, default=ROOT / "packaging/targets.json")
    parser.add_argument("--revision", default=os.environ.get("ISCHED_SOURCE_REVISION"))
    parser.add_argument("--version", default=os.environ.get("ISCHED_PACKAGE_VERSION"))
    parser.add_argument("--output-dir", type=Path, default=Path("/out"))
    args = parser.parse_args(argv)
    os.environ["ISCHED_BUILD_TARGET"] = args.target

    try:
        targets_path = args.targets.resolve()
        targets = load_targets(targets_path)
        target = next((record for record in targets if record["id"] == args.target), None)
        require(target is not None, f"unknown target: {args.target}")
        require(re.fullmatch(r"[0-9a-f]{40}", args.revision or "") is not None,
                "full lowercase 40-digit --revision/ISCHED_SOURCE_REVISION required")
        require(bool(args.version), "--version/ISCHED_PACKAGE_VERSION is required")
        verify_target(target)

        output = args.output_dir.resolve()
        if output.exists():
            require(output.is_dir() and not any(output.iterdir()),
                    f"refusing to reuse non-empty package output directory: {output}")
        output.mkdir(parents=True, exist_ok=True)
        work = Path("/tmp/isched-package-work")
        require(not work.exists(), f"refusing to reuse work directory: {work}")
        work.mkdir(parents=True)
        identity_path = work / "identity.json"
        records = [identity(target, role, args.revision, args.version)
                   for role in ("runtime", "developer")]
        identity_path.write_text(json.dumps(records, indent=2, sort_keys=True) + "\n",
                                 encoding="utf-8")

        tool_prefix = Path("/opt/isched-tools")
        run("tool-bootstrap", [sys.executable, "packaging/scripts/bootstrap_tools.py",
                                "--target", args.target, "--targets", str(targets_path),
                                "--prefix", str(tool_prefix)])
        node_bin = tool_prefix / "node/bin"
        conan_bin = tool_prefix / "conan/bin"
        env = dict(os.environ)
        env["PATH"] = os.pathsep.join((str(node_bin), str(conan_bin), env.get("PATH", "")))
        env.update(CMAKE_BUILD_PARALLEL_LEVEL="3", CTEST_PARALLEL_LEVEL="1",
                   NG_BUILD_MAX_WORKERS="1", VERBOSE="1")
        pnpm = str(node_bin / "pnpm")
        run("frontend-install", [pnpm, "install", "--frozen-lockfile"], env=env)
        run("frontend-test", frontend_test_command(pnpm), env=env)
        run("frontend-build", [pnpm, "--filter", "isched-ui", "run", "build"], env=env)
        run("frontend-embed", [sys.executable, "tools/embed_ui_assets.py"], env=env)

        frontend_index, frontend_notices, embedded_header = frontend_outputs(ROOT)
        gate = work / "frontend-evidence.json"
        gate.write_text(json.dumps({
            "schema_version": 1,
            "index_sha256": sha256(frontend_index),
            "notices_sha256": sha256(frontend_notices),
            "embedded_header_sha256": sha256(embedded_header),
        }, indent=2, sort_keys=True) + "\n", encoding="utf-8")

        build_dir = ROOT / "cmake-build-release"
        lockfile = ROOT / "conan.lock"
        host_profile = work / "profiles" / args.target
        host_profile.parent.mkdir(parents=True, exist_ok=True)
        host_profile.write_text(conan_profile(target), encoding="utf-8")
        for path in (lockfile, host_profile):
            require(path.is_file(), f"missing pinned build input: {path}")
        license_output = work / "licenses"
        package_args = (
            f"-DISCHED_ENABLE_PACKAGING=ON",
            f"-DISCHED_SOURCE_REVISION={args.revision}",
            f"-DISCHED_PACKAGE_VERSION={args.version}",
            f"-DISCHED_PACKAGE_TARGET={args.target}",
            f"-DISCHED_PACKAGE_FORMAT={target['package_format']}",
            f"-DISCHED_PACKAGE_IDENTITY_FILE={identity_path}",
            f"-DISCHED_PACKAGE_PROFILE_FILE={host_profile}",
            f"-DISCHED_PACKAGE_OUTPUT_DIR={output}",
            f"-DISCHED_PACKAGE_LICENSE_OUTPUT={license_output}",
            f"-DISCHED_FRONTEND_GATE_FILE={gate}",
        )
        configure = configure_command(lockfile, host_profile, package_args)
        run("configure-build", configure, env=env, timeout=7200)

        sys.path.insert(0, str(ROOT))
        import configure as configure_module  # pylint: disable=import-outside-toplevel
        tool_env = configure_module.conan_tool_environment(build_dir / "tool-bootstrap")
        tool_env.update({key: value for key, value in env.items() if key != "PATH"})
        tool_env["PATH"] = os.pathsep.join((
            str(node_bin), str(conan_bin), tool_env.get("PATH", "")
        ))
        show = run("ctest-discovery", ["ctest", "--test-dir", str(build_dir),
                                       "--show-only=json-v1"], env=tool_env)
        discovered = json.loads(show).get("tests", [])
        require(discovered, f"target={args.target} CTest discovered zero tests")
        run("ctest", ["ctest", "--test-dir", str(build_dir),
                      "-j1", "--verbose", "--output-on-failure", "--no-tests=error"], env=tool_env,
            timeout=7200)

        graph_json = build_dir / "conan-install.json"
        require(graph_json.is_file(), "Conan install did not write its locked graph JSON")
        run("license-collection", [
            sys.executable, "packaging/scripts/collect_licenses.py",
            "--conan-graph", str(graph_json), "--angular-notices", str(frontend_notices),
            "--output-dir", str(license_output),
        ], env=tool_env)
        generate_packages(build_dir, work / "cpack", output, records, tool_env)
        run("package-validation", [
            sys.executable, "packaging/scripts/validate_packages.py",
            "--target", args.target, "--revision", args.revision, "--targets", str(targets_path),
            "--directory", str(output),
        ], env=tool_env, timeout=600)
        evidence = {
            "schema_version": 1,
            "target": args.target,
            "source_revision": args.revision,
            "version": args.version,
            "packages": [
                {"role": record["role"], "filename": record["package_filename"],
                 "sha256": sha256(output / record["package_filename"])}
                for record in records
            ],
            "frontend": json.loads(gate.read_text(encoding="utf-8")),
            "ctest_discovered": len(discovered),
            "gates": ["frontend-test", "frontend-build", "frontend-embed", "ctest",
                      "package-validation"],
        }
        (output / "build-evidence.json").write_text(
            json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"[{args.target}] all package gates passed", flush=True)
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f"[{args.target}] build_target failed: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
