#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Inspect native packages and enforce the two-role artifact contract."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import struct
import sys
import tarfile
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_identity import ROOT, load_targets, require


class ValidationError(RuntimeError):
    """An artifact does not satisfy the native package contract."""


def command(args: list[str], target: str) -> str:
    try:
        result = subprocess.run(args, check=True, capture_output=True, text=True, timeout=180)
    except (OSError, subprocess.SubprocessError) as error:
        detail = ""
        if isinstance(error, subprocess.CalledProcessError):
            detail = (error.stderr or error.stdout or "").strip()
        raise ValidationError(
            f"target={target} native inspection failed for {' '.join(args)}: {error} {detail}"
        ) from error
    return result.stdout.strip()


def _safe_member(name: str, target: str) -> str:
    normalized = name.removeprefix("./")
    path = PurePosixPath(normalized)
    if path.is_absolute() or any(part in {"", ".", ".."} for part in path.parts):
        raise ValidationError(f"target={target} package contains unsafe payload path: {name!r}")
    return path.as_posix()


def _capture(path: str) -> bool:
    return (
        path.endswith("/package-identity.json")
        or path.endswith("/license-inventory.json")
        or path == "usr/bin/isched_srv"
        or "/licenses/" in f"/{path}/"
        or path.endswith("/cmake/isched/ischedConfig.cmake")
        or path.endswith("/cmake/isched/ischedTargets.cmake")
    )


def _deb_payload(package: Path, target: str) -> tuple[dict[str, bytes], set[str]]:
    listing = command(["dpkg-deb", "--contents", str(package)], target)
    payload_names: set[str] = set()
    for line in listing.splitlines():
        name = line.split(" ", maxsplit=5)[-1].strip()
        if name:
            payload_names.add(_safe_member(name, target))
    archive = subprocess.run(
        ["dpkg-deb", "--fsys-tarfile", str(package)],
        check=True, capture_output=True, timeout=180,
    ).stdout
    selected: dict[str, bytes] = {}
    try:
        with tarfile.open(fileobj=io.BytesIO(archive), mode="r:*") as tar:
            for member in tar:
                path = _safe_member(member.name, target)
                payload_names.add(path)
                if _capture(path) and member.isfile():
                    stream = tar.extractfile(member)
                    if stream is not None:
                        selected[path] = stream.read()
    except (OSError, tarfile.TarError, subprocess.SubprocessError) as error:
        raise ValidationError(f"target={target} invalid DEB payload archive: {error}") from error
    return selected, payload_names


def _rpm_cpio_payload(package: Path, target: str) -> tuple[dict[str, bytes], set[str]]:
    archive = subprocess.run(
        ["rpm2cpio", str(package)], check=True, capture_output=True, timeout=180,
    ).stdout
    selected: dict[str, bytes] = {}
    names: set[str] = set()
    offset = 0
    try:
        while offset < len(archive):
            if archive[offset:offset + 6] not in {b"070701", b"070702"}:
                raise ValidationError(f"target={target} malformed RPM cpio payload")
            header = archive[offset:offset + 110]
            if len(header) != 110:
                raise ValidationError(f"target={target} truncated RPM cpio header")
            fields = [int(header[i:i + 8], 16) for i in range(6, 110, 8)]
            mode, file_size, name_size = fields[1], fields[6], fields[11]
            name_start = offset + 110
            name_end = name_start + name_size
            if name_size < 1 or name_end > len(archive):
                raise ValidationError(f"target={target} invalid RPM cpio filename")
            raw_name = archive[name_start:name_end - 1].decode("utf-8", errors="strict")
            offset = (name_end + 3) & ~3
            path = _safe_member(raw_name, target)
            if path == "TRAILER!!!":
                break
            names.add(path)
            data_end = offset + file_size
            if data_end > len(archive):
                raise ValidationError(f"target={target} truncated RPM payload file: {path}")
            if _capture(path) and mode & 0o170000 == 0o100000:
                selected[path] = archive[offset:data_end]
            offset = (data_end + 3) & ~3
    except (UnicodeDecodeError, ValueError, IndexError) as error:
        raise ValidationError(f"target={target} invalid RPM cpio payload: {error}") from error
    return selected, names


def _metadata(package: Path, target: dict) -> tuple[dict[str, str], set[str], dict[str, bytes]]:
    target_id = target["id"]
    if target["package_format"] == "deb":
        if not shutil.which("dpkg-deb"):
            raise ValidationError(f"target={target_id} requires dpkg-deb for native inspection")
        values = {}
        for field in ("Package", "Version", "Architecture", "Depends"):
            values[field.lower()] = command(["dpkg-deb", "-f", str(package), field], target_id)
        identity_files, paths = _deb_payload(package, target_id)
        return values, paths, identity_files

    if not shutil.which("rpm") or not shutil.which("rpm2cpio"):
        raise ValidationError(f"target={target_id} requires rpm and rpm2cpio for native inspection")
    rows = command([
        "rpm", "-qp", "--qf", "%{NAME}\n%{VERSION}\n%{RELEASE}\n%{ARCH}\n",
        str(package),
    ], target_id).splitlines()
    if len(rows) != 4:
        raise ValidationError(f"target={target_id} RPM metadata fields are incomplete")
    values = dict(zip(("package", "version", "release", "architecture"), rows))
    values["depends"] = command(["rpm", "-qp", "--requires", str(package)], target_id)
    identity_files, paths = _rpm_cpio_payload(package, target_id)
    return values, paths, identity_files


def _identity(package: Path, target: dict, revision: str,
              expected_role: str, embedded: dict[str, bytes]) -> dict[str, Any]:
    target_id = target["id"]
    identity_path = f"usr/share/isched/{expected_role}/package-identity.json"
    raw = embedded.get(identity_path)
    if raw is None:
        raise ValidationError(f"target={target_id} {package.name} lacks {identity_path}")
    try:
        identity = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValidationError(f"target={target_id} invalid embedded identity in {package.name}: {error}") from error
    fields = {
        "role", "package_name", "version", "native_version", "native_release",
        "source_revision", "target", "distribution", "distribution_version",
        "architecture", "native_architecture", "format", "artifact_name", "package_filename",
    }
    if not isinstance(identity, dict) or set(identity) != fields:
        raise ValidationError(f"target={target_id} embedded identity has unknown or missing fields")
    if identity["role"] != expected_role or identity["source_revision"] != revision:
        raise ValidationError(f"target={target_id} {package.name} role/revision identity mismatch")
    if identity["target"] != target_id or identity["format"] != target["package_format"]:
        raise ValidationError(f"target={target_id} {package.name} target/format identity mismatch")
    if (identity["package_name"] != f"isched-{expected_role if expected_role == 'runtime' else 'devel'}"
            or identity["distribution"] != target["distribution"]
            or identity["distribution_version"] != target["distribution_version"]
            or identity["architecture"] != "x86_64"
            or identity["native_architecture"] != target["native_architecture"]):
        raise ValidationError(f"target={target_id} {package.name} package role or target dimensions mismatch")
    version = identity["version"]
    if not (re.fullmatch(r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)", version)
            or version == f"0.0.0+git.{revision[:12]}"):
        raise ValidationError(f"target={target_id} invalid canonical version: {version!r}")
    if target["package_format"] == "deb":
        expected_native_version, expected_release = version, ""
    elif version == f"0.0.0+git.{revision[:12]}":
        expected_native_version, expected_release = "0.0.0", f"0.git.{revision[:12]}"
    else:
        expected_native_version, expected_release = version, "1"
    if (identity["native_version"] != expected_native_version
            or identity["native_release"] != expected_release
            or package.name != identity["package_filename"]):
        raise ValidationError(f"target={target_id} {package.name} native/canonical version mismatch")
    return identity


def _validate_payload(target: dict, role: str, paths: set[str],
                      contents: dict[str, bytes]) -> None:
    target_id = target["id"]
    if role == "runtime":
        required = {
            "usr/bin/isched_srv", "usr/share/doc/isched-runtime/LICENSE.md",
            "usr/share/doc/isched-runtime/license-inventory.json",
        }
        require(any(path.startswith("usr/share/doc/isched-runtime/licenses/") for path in paths),
                f"target={target_id} runtime package has no third-party notices")
    else:
        required = {
            "usr/include/isched/isched.hpp",
            "usr/include/isched/backend/isched_Server.hpp",
            "usr/include/isched/backend/isched_common.hpp",
            "usr/include/isched/shared/config/isched_config.hpp",
            "usr/share/doc/isched-devel/LICENSE.md",
            "usr/share/doc/isched-devel/license-inventory.json",
            "usr/share/isched/developer/conanfile.txt",
            "usr/share/isched/developer/conan.lock",
            "usr/share/isched/developer/requirements-conan.lock",
        }
        require(any(path.endswith("/libisched.a") for path in paths),
                f"target={target_id} developer package lacks static libisched.a")
        require(any(path.endswith("/cmake/isched/ischedConfig.cmake") for path in paths)
                and any(path.endswith("/cmake/isched/ischedTargets.cmake") for path in paths),
                f"target={target_id} developer package lacks relocatable CMake exports")
        require(any(path.startswith("usr/share/doc/isched-devel/licenses/") for path in paths),
                f"target={target_id} developer package has no third-party notices")
    missing = required - paths
    if missing:
        raise ValidationError(f"target={target_id} {role} package missing payload: {sorted(missing)}")
    if role == "runtime":
        executable = contents.get("usr/bin/isched_srv")
        if (executable is None or len(executable) < 20 or executable[:4] != b"\x7fELF"
                or executable[4] != 2 or executable[5] != 1
                or struct.unpack_from("<H", executable, 18)[0] != 62):
            raise ValidationError(f"target={target_id} runtime executable is not a little-endian x86_64 ELF")
    inventory_path = f"usr/share/doc/isched-{role}/license-inventory.json"
    raw_inventory = contents.get(inventory_path)
    if raw_inventory is None:
        raise ValidationError(f"target={target_id} {role} license inventory is unreadable")
    try:
        inventory = json.loads(raw_inventory.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValidationError(f"target={target_id} invalid {role} license inventory: {error}") from error
    if inventory.get("schema_version") != 1 or inventory.get("role") != role:
        raise ValidationError(f"target={target_id} {role} license inventory role/schema mismatch")
    notices = inventory.get("notices")
    if not isinstance(notices, list) or not notices:
        raise ValidationError(f"target={target_id} {role} license inventory contains no notice records")
    for record in notices:
        if not isinstance(record, dict) or set(record) != {"path", "sha256"}:
            raise ValidationError(f"target={target_id} {role} license inventory record is malformed")
        notice_path = f"usr/share/doc/isched-{role}/{record['path']}"
        notice = contents.get(notice_path)
        if notice is None or hashlib.sha256(notice).hexdigest() != record["sha256"]:
            raise ValidationError(f"target={target_id} {role} notice missing or checksum mismatch: {record['path']}")
    if role == "developer":
        if any("angular" in str(record.get("path", "")).lower() for record in notices):
            raise ValidationError(f"target={target_id} Angular notices must not be added to developer package")
    allowed_prefixes = (
        ("usr/bin/isched_srv", "usr/share/doc/isched-runtime/", "usr/share/isched/runtime/")
        if role == "runtime" else
        ("usr/include/isched/", "usr/share/doc/isched-devel/", "usr/share/isched/developer/",
         "usr/lib/", "usr/lib64/", "usr/lib/x86_64-linux-gnu/")
    )
    allowed_files = {"usr/bin/isched_srv", "usr/include/isched"}
    disallowed = []
    for path in paths:
        if path in {".", "usr", "usr/bin", "usr/include", "usr/lib", "usr/lib64",
                    "usr/lib/x86_64-linux-gnu", "usr/share", "usr/share/doc", "usr/share/isched"}:
            continue
        if any(prefix.startswith(path + "/") for prefix in allowed_prefixes):
            continue
        if path in allowed_files or any(path.startswith(prefix) for prefix in allowed_prefixes):
            if role == "developer" and path.startswith(("usr/lib/", "usr/lib64/", "usr/lib/x86_64-linux-gnu/")):
                if not (path.endswith(".a") or "/cmake/isched/" in path
                        or path.endswith("/cmake") or path.endswith("/cmake/isched")):
                    disallowed.append(path)
            continue
        disallowed.append(path)
    if disallowed:
        raise ValidationError(f"target={target_id} {role} package contains out-of-allowlist payload: {sorted(disallowed)[:12]}")
    for path, data in contents.items():
        if path.endswith("/cmake/isched/ischedConfig.cmake") or path.endswith("/cmake/isched/ischedTargets.cmake"):
            text = data.decode("utf-8", errors="replace")
            if any(fragment in text for fragment in ("/home/", "/tmp/", ".conan2", "cmake-build-release")):
                raise ValidationError(f"target={target_id} SDK export leaks producer/cache path: {path}")
    forbidden = re.compile(r"(^|/)(?:\.conan2?|node_modules|cmake-build[^/]*|tests?|credentials?|secrets?)(?:/|$)|(?:\.db|\.sqlite3?)$|\.env(?:$|\.)", re.I)
    contaminated = sorted(path for path in paths if forbidden.search(path))
    if contaminated:
        raise ValidationError(f"target={target_id} {role} package contains forbidden state: {contaminated[:8]}")


def validate(target_id: str, revision: str, directory: Path) -> list[dict[str, Any]]:
    targets = load_targets()
    target = next((record for record in targets if record["id"] == target_id), None)
    require(target is not None, f"unknown target: {target_id}")
    require(re.fullmatch(r"[0-9a-f]{40}", revision) is not None,
            "--revision must be a full lowercase 40-digit SHA")
    require(directory.is_dir(), f"target={target_id} package directory does not exist: {directory}")
    suffix = "." + target["package_format"]
    packages = sorted(path for path in directory.iterdir() if path.is_file() and path.name.endswith(suffix))
    require(len(packages) == 2, f"target={target_id} expected exactly two {suffix} packages, found {len(packages)}")
    if any(path.is_dir() for path in directory.iterdir()):
        raise ValidationError(f"target={target_id} unexpected directory in package output: {directory}")
    extras = {path.name for path in directory.iterdir() if path.is_file()} - {path.name for path in packages} - {
        "build-evidence.json", "runtime-verification.json", "sdk-verification.json"
    }
    if extras:
        raise ValidationError(f"target={target_id} unexpected package output files: {sorted(extras)}")
    seen_roles: set[str] = set()
    validated: list[dict[str, Any]] = []
    for package in packages:
        if package.stat().st_size == 0:
            raise ValidationError(f"target={target_id} package is empty: {package.name}")
        roles = [role for role in ("runtime", "developer") if f"isched-{role}-" in package.name]
        if len(roles) != 1:
            raise ValidationError(f"target={target_id} package filename has no unique role: {package.name}")
        role = roles[0]
        metadata, paths, contents = _metadata(package, target)
        identity = _identity(package, target, revision, role, contents)
        expected_native = {
            "package": identity["package_name"],
            "version": identity["native_version"],
            "architecture": identity["native_architecture"],
        }
        if target["package_format"] == "rpm":
            expected_native["release"] = identity["native_release"]
        for key, expected_value in expected_native.items():
            if metadata.get(key) != expected_value:
                raise ValidationError(
                    f"target={target_id} {package.name} native {key} mismatch: "
                    f"expected {expected_value!r}, got {metadata.get(key)!r}"
                )
        dependency_text = metadata.get("depends", "")
        if not dependency_text.strip():
            raise ValidationError(f"target={target_id} {package.name} has no declared native dependencies")
        forbidden_role = "isched-devel" if role == "runtime" else "isched-runtime"
        if forbidden_role in dependency_text:
            raise ValidationError(f"target={target_id} {package.name} invents {forbidden_role} dependency")
        _validate_payload(target, role, paths, contents)
        seen_roles.add(role)
        validated.append({"role": role, "filename": package.name, "identity": identity})
    if seen_roles != {"runtime", "developer"}:
        raise ValidationError(f"target={target_id} expected runtime and developer roles, found {sorted(seen_roles)}")
    evidence_path = directory / "build-evidence.json"
    if evidence_path.exists():
        try:
            evidence = json.loads(evidence_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ValidationError(f"target={target_id} invalid build evidence: {error}") from error
        if (evidence.get("target") != target_id or evidence.get("source_revision") != revision
                or not isinstance(evidence.get("packages"), list)):
            raise ValidationError(f"target={target_id} build evidence identity mismatch")
        recorded = {item.get("filename"): item for item in evidence["packages"] if isinstance(item, dict)}
        if len(recorded) != 2 or set(recorded) != {item["filename"] for item in validated}:
            raise ValidationError(f"target={target_id} build evidence package set mismatch")
        for item in validated:
            path = directory / item["filename"]
            expected_hash = recorded[item["filename"]].get("sha256")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if expected_hash != digest:
                raise ValidationError(f"target={target_id} package checksum mismatch: {path.name}")
    return validated


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        results = validate(args.target, args.revision, args.directory.resolve())
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError, ValidationError) as error:
        print(f"package validation: target={args.target}: {error}", file=sys.stderr)
        return 1
    for result in results:
        print(f"target={args.target} validated role={result['role']} file={result['filename']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
