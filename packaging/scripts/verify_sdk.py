#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Verify the installed developer SDK from a clean same-target environment."""

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
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_identity import ROOT, load_targets, require
from logged_process import run_logged


def _failure_stage(error: BaseException, default: str) -> str:
    text = "\n".join(str(getattr(error, attr, "") or "") for attr in ("stdout", "stderr"))
    stages = [word.partition("=")[2].strip(":") for word in text.split() if word.startswith("stage=")]
    return stages[-1] if stages else default


def run(stage: str, command: list[str], *, timeout: int = 1200,
        capture: bool = True, check: bool = True,
        env: dict[str, str] | None = None) -> subprocess.CompletedProcess:
    target = os.environ.get("ISCHED_BUILD_TARGET", "unknown")
    print(f"[{target}] stage={stage} $ {' '.join(command)}", flush=True)
    try:
        result = run_logged(command, check=check, timeout=timeout,
                            capture=capture, env=env)
    except (OSError, subprocess.SubprocessError) as error:
        actual_stage = _failure_stage(error, stage)
        detail = ""
        if isinstance(error, subprocess.CalledProcessError):
            detail = f"\nstdout:\n{error.stdout or ''}\nstderr:\n{error.stderr or ''}"
        raise RuntimeError(f"target={target} stage={actual_stage} failed: {error}{detail}") from error
    return result


def os_release() -> dict[str, str]:
    result: dict[str, str] = {}
    for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if separator:
            result[key] = value.strip().strip('"')
    return result


def _install_package(target: dict, package: Path) -> None:
    if target["package_format"] == "deb":
        run("sdk-install", ["apt-get", "update"], timeout=600)
        run("sdk-install", ["apt-get", "install", "-y", "--no-install-recommends", str(package)], timeout=900)
        run("sdk-install-cleanup", ["apt-get", "clean"])
    else:
        run("sdk-install", ["dnf", "install", "-y", "--setopt=install_weak_deps=False", str(package)], timeout=900)


def _assert_runtime_absent(target: dict) -> None:
    if target["package_format"] == "deb":
        probe = run("runtime-absence", ["dpkg-query", "-W", "-f=${db:Status-Status}", "isched-runtime"],
                    check=False)
        require(probe.returncode != 0, "SDK verification unexpectedly installed isched-runtime")
    else:
        probe = run("runtime-absence", ["rpm", "-q", "isched-runtime"], check=False)
        require(probe.returncode != 0, "SDK verification unexpectedly installed isched-runtime")


def _find_one(root: Path, pattern: str) -> Path:
    results = [path for path in root.rglob(pattern) if path.is_file()]
    require(len(results) == 1, f"expected one {pattern} under {root}, found {len(results)}")
    return results[0]


def _activated(script: Path, command: list[str], *, stage: str, timeout: int = 1200,
               env: dict[str, str] | None = None) -> str:
    result = run(stage, ["bash", "-c", 'source "$1"; shift; exec "$@"',
                         "isched-sdk-env", str(script), *command], timeout=timeout, env=env)
    return result.stdout


def _verify_consumer(target: dict, consumer: Path) -> dict:
    require(consumer.is_dir(), f"SDK consumer fixture missing: {consumer}")
    conan_home = Path(tempfile.mkdtemp(prefix="isched-sdk-conan-", dir="/tmp"))
    scratch = Path(tempfile.mkdtemp(prefix="isched-sdk-consumer-", dir="/tmp"))
    try:
        environment = dict(os.environ, CONAN_HOME=str(conan_home), VERBOSE="1")
        run("conan-profile", [str(Path("/opt/isched-tools/conan/bin/conan")),
                              "profile", "detect", "--force"], timeout=180, env=environment)
        manifest = Path("/usr/share/isched/developer/conanfile.txt")
        lockfile = Path("/usr/share/isched/developer/conan.lock")
        profile = Path(f"/usr/share/isched/developer/profiles/{target['id']}")
        for path in (manifest, lockfile, profile):
            require(path.is_file(), f"developer package is missing shipped Conan input: {path}")
        dependencies = scratch / "dependencies"
        conan = "/opt/isched-tools/conan/bin/conan"
        tool_manifest = scratch / "tools.txt"
        versions = json.loads((ROOT / "packaging/tool-versions.json").read_text(encoding="utf-8"))
        cmake_version = versions["cmake"]
        tool_manifest.write_text(
            "[tool_requires]\ncmake/" + cmake_version +
            "\n\n[generators]\nVirtualBuildEnv\n", encoding="utf-8")
        tools = scratch / "tools"
        run("conan-cmake", [conan, "install", str(tool_manifest),
                            "--profile:build=default", f"--output-folder={tools}",
                            "-c:a", "tools.build:jobs=3",
                            "--build=missing"], timeout=3600, env=environment)
        activation = _find_one(tools, "conanbuild.sh")
        version_output = _activated(activation, ["cmake", "--version"], stage="cmake-version",
                                    env=environment)
        require(re.search(rf"cmake version {re.escape(cmake_version)}\b", version_output, re.I) is not None,
                f"SDK verifier activated unexpected CMake version: {version_output}")
        _activated(activation, [conan, "install", str(manifest),
                                f"--lockfile={lockfile}", f"--profile:host={profile}",
                                "--profile:build=default", f"--output-folder={dependencies}",
                                "-c:a", "tools.build:jobs=3",
                                "--build=missing"], stage="conan-dependencies",
                   timeout=7200, env=environment)
        toolchain = _find_one(dependencies, "conan_toolchain.cmake")
        copied_consumer = scratch / "consumer"
        shutil.copytree(consumer, copied_consumer)
        build = scratch / "build"
        _activated(activation, ["cmake", "-S", str(copied_consumer), "-B", str(build),
                               "-DCMAKE_BUILD_TYPE=Release",
                               f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                               f"-DCMAKE_PREFIX_PATH={dependencies}"],
                   stage="consumer-configure", timeout=900, env=environment)
        build_consumer(activation, build, environment)
        run("consumer-run", [str(build / "isched_sdk_consumer")], timeout=60)
        config_files = list(Path("/usr").glob("lib*/cmake/isched/ischedTargets*.cmake"))
        require(bool(config_files), "installed SDK export targets are not present in the system prefix")
        export_text = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in config_files)
        forbidden = (str(ROOT), "/home/", ".conan2", "cmake-build-release")
        require(not any(fragment in export_text for fragment in forbidden),
                "installed SDK export references a producer source/build/cache path")
        return {"consumer": "configured-built-linked-ran", "runtime_package": False,
                "conan_lock_sha256": hashlib.sha256(lockfile.read_bytes()).hexdigest(),
                "consumer_build": str(build)}
    finally:
        shutil.rmtree(conan_home, ignore_errors=True)
        shutil.rmtree(scratch, ignore_errors=True)


def build_consumer(activation: Path, build: Path, environment: dict[str, str]) -> None:
    """Build the external consumer with three jobs in its isolated tool environment."""
    _activated(activation, ["cmake", "--build", str(build), "-j3"],
               stage="consumer-build", timeout=3600, env=environment)


def _in_container(target: dict, package: Path, consumer: Path, evidence: Path,
                  targets_path: Path) -> None:
    release = os_release()
    expected_id = "ubuntu" if target["distribution"] == "ubuntu" else "fedora"
    require(release.get("ID") == expected_id and
            release.get("VERSION_ID") == target["distribution_version"],
            f"SDK verifier must run in {target['id']}, got {release.get('ID')} {release.get('VERSION_ID')}")
    require(platform.machine() in {"x86_64", "amd64"}, "SDK verifier requires linux/amd64")
    _install_package(target, package)
    _assert_runtime_absent(target)
    compiler = target["compiler"]["cxx"]
    actual_major = run("compiler-version", [compiler, "-dumpfullversion"],
                       timeout=30).stdout.strip().split(".", 1)[0]
    require(actual_major == target["compiler"]["version"],
            f"target={target['id']} expected GCC {target['compiler']['version']}, got {actual_major}")
    # This disposable consumer environment gets the same independently pinned
    # Conan CLI used by the producer; no producer cache or source tree is copied.
    from bootstrap_tools import bootstrap  # pylint: disable=import-outside-toplevel
    bootstrap(target["id"], Path("/opt/isched-tools"), targets_path)
    smoke = _verify_consumer(target, consumer)
    evidence.write_text(json.dumps({
        "schema_version": 1, "target": target["id"], "package": package.name,
        "package_sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
        "compiler": actual_major, "smoke": smoke,
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _container_run(target: dict, package: Path, consumer: Path, evidence: Path,
                   targets_path: Path) -> None:
    runtime = shutil.which("docker")
    require(runtime is not None, "Docker is required for clean SDK verification")
    container_id: str | None = None
    try:
        created = run("container-create", [runtime, "create", "--platform", target["platform"],
                                           "--entrypoint", "/bin/sleep", target["image"], "infinity"])
        container_id = created.stdout.strip().splitlines()[-1]
        run("container-start", [runtime, "start", container_id])
        scripts = ROOT / "packaging/scripts"
        run("container-prepare", [runtime, "exec", container_id, "mkdir", "-p",
                                  "/opt/isched/packaging/scripts", "/opt/isched/packaging"])
        copies = [
            (package, f"{container_id}:/tmp/{package.name}"),
            (scripts / "package_identity.py", f"{container_id}:/opt/isched/packaging/scripts/package_identity.py"),
            (scripts / "bootstrap_tools.py", f"{container_id}:/opt/isched/packaging/scripts/bootstrap_tools.py"),
            (scripts / "verify_sdk.py", f"{container_id}:/opt/isched/packaging/scripts/verify_sdk.py"),
            (targets_path, f"{container_id}:/opt/isched/packaging/targets.json"),
            (ROOT / "packaging/tool-versions.json", f"{container_id}:/opt/isched/packaging/tool-versions.json"),
            (ROOT / "packaging/requirements-bootstrap.lock", f"{container_id}:/opt/isched/packaging/requirements-bootstrap.lock"),
            (ROOT / "packaging/requirements-conan.lock", f"{container_id}:/opt/isched/packaging/requirements-conan.lock"),
            (consumer, f"{container_id}:/opt/isched/consumer"),
        ]
        for source, destination in copies:
            run("container-copy", [runtime, "cp", str(source), destination])
        target_data = target
        if target_data["distribution"] == "ubuntu":
            run("container-os-deps", [runtime, "exec", container_id, "apt-get", "update"], timeout=600)
            package_install = ["apt-get", "install", "-y", "--no-install-recommends", *target_data["os_packages"]]
        else:
            run("container-os-deps", [runtime, "exec", container_id, "dnf", "makecache"], timeout=600)
            package_install = ["dnf", "install", "-y", "--setopt=install_weak_deps=False", *target_data["os_packages"]]
        run("container-os-deps", [runtime, "exec", container_id, *package_install], timeout=1800)
        run("sdk-install", [runtime, "exec", container_id, "python3",
                            "/opt/isched/packaging/scripts/verify_sdk.py", "--target", target["id"],
                            "--targets", "/opt/isched/packaging/targets.json",
                            "--package", f"/tmp/{package.name}", "--consumer", "/opt/isched/consumer",
                            "--in-container", "--evidence", "/tmp/sdk-verification.json"], timeout=14400)
        evidence.parent.mkdir(parents=True, exist_ok=True)
        run("evidence-copy", [runtime, "cp", f"{container_id}:/tmp/sdk-verification.json", str(evidence)])
    finally:
        if container_id:
            run("container-cleanup", [runtime, "rm", "-f", container_id], check=False)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--targets", type=Path, default=ROOT / "packaging/targets.json")
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--consumer", type=Path, required=True)
    parser.add_argument("--in-container", action="store_true")
    parser.add_argument("--evidence", type=Path, default=Path("sdk-verification.json"))
    args = parser.parse_args(argv)
    os.environ["ISCHED_BUILD_TARGET"] = args.target
    try:
        targets_path = args.targets.resolve()
        targets = load_targets(targets_path)
        target = next((record for record in targets if record["id"] == args.target), None)
        require(target is not None, f"unknown target: {args.target}")
        package, consumer = args.package.resolve(), args.consumer.resolve()
        require(package.is_file() and package.stat().st_size > 0,
                f"developer package missing or empty: {package}")
        require(consumer.is_dir(), f"consumer fixture missing: {consumer}")
        if args.in_container:
            _in_container(target, package, consumer, args.evidence.resolve(), targets_path)
        else:
            _container_run(target, package, consumer, args.evidence.resolve(), targets_path)
        print(f"[{args.target}] SDK verification passed; evidence={args.evidence}", flush=True)
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f"[{args.target}] SDK verification failed: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
