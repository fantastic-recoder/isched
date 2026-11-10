#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Validate pinned target inputs and derive order-independent native identities."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
ROLES = {"runtime": "isched-runtime", "developer": "isched-devel"}
FORMATS = {"ubuntu": ("deb", "DEB", "amd64"), "fedora": ("rpm", "RPM", "x86_64")}
SHA = re.compile(r"[0-9a-f]{40}")
SEMVER = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)")
IMAGE = re.compile(r"[a-z0-9./:_-]+@sha256:[0-9a-f]{64}")


def require(condition: bool, message: str) -> None:
    """Raise an actionable input error, independent of Python optimization flags."""
    if not condition:
        raise ValueError(message)


def validate_target(target: dict) -> dict:
    """Reject unsupported or incomplete tuples rather than guessing defaults."""
    require(isinstance(target, dict), "each target record must be an object")
    target_id = target.get("id", "<missing>")
    distro = target.get("distribution")
    require(distro in FORMATS, f"unsupported distribution: {distro!r}")
    fmt, generator, arch = FORMATS[distro]
    release = target.get("distribution_version", "")
    pattern = r"\d{2}\.\d{2}" if distro == "ubuntu" else r"[1-9]\d*"
    require(isinstance(release, str) and re.fullmatch(pattern, release) is not None,
            "release must be an explicit numeric distribution version")
    require(target.get("id") == f"{distro}-{release}", "inconsistent target id")
    for key, expected in {
        "platform": "linux/amd64", "architecture": "x86_64",
        "package_format": fmt, "generator": generator, "native_architecture": arch,
        "dockerfile": f"packaging/docker/{distro}/Dockerfile",
    }.items():
        require(target.get(key) == expected, f"{target_id}: invalid {key}")
    image = target.get("image", "")
    require(isinstance(image, str) and IMAGE.fullmatch(image) is not None,
            "image must have an immutable SHA-256 manifest digest")
    require(image.startswith(f"{distro}:{release}@"), "image tag differs from tuple")
    compiler = target.get("compiler", {})
    require(isinstance(compiler, dict), f"{target_id}: compiler must be an object")
    require(compiler.get("cppstd") == "23" and compiler.get("libcxx") == "libstdc++11",
            "compiler must declare C++23 and libstdc++11 ABI")
    require(isinstance(compiler.get("version"), str)
            and re.fullmatch(r"[1-9]\d*", compiler["version"]) is not None,
            "compiler major version required")
    for key in ("c", "cxx"):
        require(isinstance(compiler.get(key), str)
                and re.fullmatch(r"/usr/bin/[a-z+0-9-]+", compiler[key]) is not None,
                f"explicit compiler executable required: {key}")
    version = compiler.get("package_version", "")
    require(isinstance(version, str) and re.fullmatch(r"[0-9][a-zA-Z0-9.+:~-]*", version)
            is not None, "exact compiler package version required")
    packages = target.get("os_packages", [])
    require(isinstance(packages, list) and bool(packages), "exact OS tool pins required")
    require(all(isinstance(p, str) and re.fullmatch(r"[a-zA-Z0-9.+:=~_-]+", p)
                for p in packages), "unsafe OS package pin")
    require(len(packages) == len(set(packages)), "duplicate OS package pin")
    if fmt == "deb":
        require(all("=" in p and len(p.split("=")) == 2 and p.split("=")[1]
                    for p in packages), "DEB OS package pin lacks exact version")
        require(f"gcc-{compiler['version']}={version}" in packages
                and f"g++-{compiler['version']}={version}" in packages,
                "compiler package pins disagree")
    else:
        require(all(re.fullmatch(r".+-[0-9][^/]*\.(x86_64|noarch)", p)
                    for p in packages), "RPM OS tool pin requires exact NEVRA")
        require(f"gcc-{version}.x86_64" in packages
                and f"gcc-c++-{version}.x86_64" in packages, "compiler package pins disagree")
    return target


def conan_profile(target: dict) -> str:
    """Render the explicit Conan host/build profile from validated compiler inputs."""
    validate_target(target)
    compiler = target["compiler"]
    return (
        "# SPDX-License-Identifier: MPL-2.0\n"
        "[settings]\n"
        "os=Linux\n"
        "arch=x86_64\n"
        "build_type=Release\n"
        "compiler=gcc\n"
        f"compiler.version={compiler['version']}\n"
        f"compiler.cppstd={compiler['cppstd']}\n"
        f"compiler.libcxx={compiler['libcxx']}\n"
        "\n"
        "[conf]\n"
        "tools.build:jobs=3\n"
        f"tools.build:compiler_executables={{\"c\": \"{compiler['c']}\", "
        f"\"cpp\": \"{compiler['cxx']}\"}}\n"
    )


def validate_target_set(data: dict) -> list[dict]:
    """Validate one explicit target-set object and reject duplicate identities."""
    require(isinstance(data, dict), "target set must be a JSON object")
    require(data.get("schema_version") == 1, "unsupported targets schema")
    require(data.get("roles") == ROLES, "runtime/developer role mapping required")
    records = data.get("targets")
    require(isinstance(records, list) and bool(records), "empty target set")
    targets = [validate_target(t) for t in records]
    require(len({t["id"] for t in targets}) == len(targets), "duplicate target identity")
    return sorted(targets, key=lambda t: t["id"])


def load_targets(path: Path = ROOT / "packaging/targets.json") -> list[dict]:
    """Load an explicit target set; reject duplicates before emitting a matrix."""
    return validate_target_set(json.loads(path.read_text()))


def git_output(root: Path, *args: str) -> str:
    command = ["git", "-C", str(root), *args]
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except OSError as error:
        raise ValueError(f"git query failed ({' '.join(command)}): {error}") from error
    try:
        stdout, stderr = process.communicate(timeout=30)
    except subprocess.TimeoutExpired as error:
        process.kill()
        process.communicate()
        raise ValueError(f"git query timed out ({' '.join(command)})") from error
    if process.returncode:
        raise ValueError(f"git query failed ({' '.join(command)}): {stderr.strip()}")
    return stdout.strip()


def checkout_version(root: Path, revision: str) -> str:
    """Use only exact-at-HEAD conventional tags and the actual checkout revision."""
    require(SHA.fullmatch(revision) is not None, "full lowercase 40-digit source SHA required")
    require(git_output(root, "rev-parse", "HEAD") == revision, "revision differs from checkout")
    tags = git_output(root, "tag", "--points-at", "HEAD").splitlines()
    versions = sorted({tag[1:] for tag in tags if tag.startswith("v")
                       and SEMVER.fullmatch(tag[1:])})
    require(len(versions) <= 1, "ambiguous exact semver tags at HEAD")
    return versions[0] if versions else f"0.0.0+git.{revision[:12]}"


def identity(target: dict, role: str, revision: str, version: str) -> dict:
    """Return canonical and native version fields without changing version policy."""
    validate_target(target)
    require(role in ROLES, f"unsupported package role: {role}")
    require(SHA.fullmatch(revision) is not None, "full lowercase 40-digit source SHA required")
    fallback = f"0.0.0+git.{revision[:12]}"
    require(SEMVER.fullmatch(version) is not None or version == fallback,
            "version must be semver or the canonical revision-derived fallback")
    native_version = version
    native_release = ""
    if target["package_format"] == "rpm":
        native_version = "0.0.0" if version == fallback else version
        native_release = f"0.git.{revision[:12]}" if version == fallback else "1"
    name = (f"isched-{role}-{version}-{target['distribution']}-"
            f"{target['distribution_version']}-x86_64-{target['package_format']}-{revision}")
    return {
        "role": role, "package_name": ROLES[role], "version": version,
        "native_version": native_version, "native_release": native_release,
        "source_revision": revision, "target": target["id"],
        "distribution": target["distribution"],
        "distribution_version": target["distribution_version"],
        "architecture": "x86_64", "native_architecture": target["native_architecture"],
        "format": target["package_format"], "artifact_name": name,
        "package_filename": f"{name}.{target['package_format']}",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--targets", type=Path, default=ROOT / "packaging/targets.json")
    parser.add_argument("--matrix", action="store_true")
    parser.add_argument("--target")
    parser.add_argument("--revision")
    parser.add_argument("--version", help="explicit canonical input inside Git-free Docker context")
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    try:
        targets = load_targets(args.targets)
        if args.matrix:
            print(json.dumps({"include": targets}, sort_keys=True))
            return 0
        require(bool(args.target) and bool(args.revision), "--target and --revision required")
        target = next((t for t in targets if t["id"] == args.target), None)
        require(target is not None, f"unknown target: {args.target}")
        version = args.version or checkout_version(args.source_root, args.revision)
        print(json.dumps([identity(target, r, args.revision, version) for r in ROLES],
                         indent=2, sort_keys=True))
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError) as exc:
        print(f"package identity: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
