#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Validate complete target evidence and stage its runtime and developer artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))
import validate_packages
from package_identity import ROOT, checkout_version, identity, load_targets, require

SHA256 = re.compile(r"[0-9a-f]{64}")
EVIDENCE_FILES = {
    "build-evidence.json", "runtime-verification.json", "sdk-verification.json",
}
REQUIRED_GATES = {
    "frontend-test", "frontend-build", "frontend-embed", "ctest", "package-validation",
}


def _read_json(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"invalid {label} {path.name}: {error}") from error
    require(isinstance(value, dict), f"{label} must be a JSON object: {path.name}")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _require_digest(value: Any, label: str) -> str:
    require(isinstance(value, str) and SHA256.fullmatch(value) is not None,
            f"{label} must be a lowercase SHA-256 digest")
    return value


def _check_verification_evidence(
    target: dict[str, Any], records: dict[str, dict[str, Any]], directory: Path,
    package_hashes: dict[str, str],
) -> None:
    target_id = target["id"]
    runtime = _read_json(
        directory / "runtime-verification.json", "runtime verification evidence",
    )
    runtime_identity = records["runtime"]
    require(runtime.get("schema_version") == 1 and runtime.get("target") == target_id
            and runtime.get("package") == runtime_identity["package_filename"],
            f"target={target_id} runtime verification identity mismatch")
    runtime_hash = _require_digest(runtime.get("package_sha256"), "runtime verification package hash")
    require(runtime_hash == package_hashes["runtime"],
            f"target={target_id} runtime evidence package hash differs from build evidence")
    require(runtime.get("loader") == "resolved" and runtime.get("seeded") is False,
            f"target={target_id} runtime loader/seed gate evidence is incomplete")
    _require_digest(runtime.get("loader_output_sha256"), "runtime loader evidence hash")
    smoke = runtime.get("smoke")
    require(isinstance(smoke, dict)
            and isinstance(smoke.get("health"), dict)
            and smoke["health"].get("status") == "UP"
            and smoke.get("ui_path") == "/isched"
            and isinstance(smoke.get("asset_path"), str)
            and smoke["asset_path"].startswith("/isched/")
            and isinstance(smoke.get("asset_bytes"), int)
            and not isinstance(smoke.get("asset_bytes"), bool)
            and smoke["asset_bytes"] > 0,
            f"target={target_id} runtime GraphQL/UI/asset evidence is incomplete")

    sdk = _read_json(directory / "sdk-verification.json", "SDK verification evidence")
    sdk_identity = records["developer"]
    require(sdk.get("schema_version") == 1 and sdk.get("target") == target_id
            and sdk.get("package") == sdk_identity["package_filename"],
            f"target={target_id} SDK verification identity mismatch")
    sdk_hash = _require_digest(sdk.get("package_sha256"), "SDK verification package hash")
    require(sdk_hash == package_hashes["developer"],
            f"target={target_id} SDK evidence package hash differs from build evidence")
    require(sdk.get("compiler") == target["compiler"]["version"],
            f"target={target_id} SDK compiler evidence mismatch")
    sdk_smoke = sdk.get("smoke")
    require(isinstance(sdk_smoke, dict)
            and sdk_smoke.get("consumer") == "configured-built-linked-ran"
            and sdk_smoke.get("runtime_package") is False,
            f"target={target_id} SDK compile/link/run evidence is incomplete")
    lock_digest = _require_digest(sdk_smoke.get("conan_lock_sha256"), "SDK Conan lock evidence hash")
    _, package_paths, package_contents = validate_packages._metadata(
        directory / sdk_identity["package_filename"], target,
    )
    lock_path = "usr/share/isched/developer/conan.lock"
    require(lock_path in package_paths and lock_path in package_contents,
            f"target={target_id} SDK package lacks shipped Conan lockfile")
    actual_lock_digest = hashlib.sha256(package_contents[lock_path]).hexdigest()
    require(lock_digest == actual_lock_digest,
            f"target={target_id} SDK evidence Conan lock hash differs from shipped lockfile")


def validate_target_output(target_id: str, revision: str, directory: Path,
                           targets_path: Path = ROOT / "packaging/targets.json") -> list[dict[str, Any]]:
    """Fail closed unless packages and evidence describe the same validated target."""
    require(re.fullmatch(r"[0-9a-f]{40}", revision) is not None,
            "--revision must be a full lowercase 40-digit SHA")
    targets = load_targets(targets_path)
    target = next((item for item in targets if item["id"] == target_id), None)
    require(target is not None, f"unknown target: {target_id}")
    require(directory.is_dir(), f"target={target_id} build output directory does not exist: {directory}")
    entries = list(directory.iterdir())
    require(all(path.is_file() and not path.is_symlink() for path in entries),
            f"target={target_id} build output contains a directory, symlink, or non-file")

    canonical_version = checkout_version(ROOT, revision)
    records = {role: identity(target, role, revision, canonical_version)
               for role in ("runtime", "developer")}
    package_names = {record["package_filename"] for record in records.values()}
    expected_names = package_names | EVIDENCE_FILES
    actual_names = [path.name for path in entries]
    require(len(actual_names) == len(set(actual_names)) and set(actual_names) == expected_names,
            f"target={target_id} output file set mismatch; expected={sorted(expected_names)}, "
            f"actual={sorted(actual_names)}")

    evidence_path = directory / "build-evidence.json"
    evidence = _read_json(evidence_path, "build evidence")
    require(evidence.get("schema_version") == 1 and evidence.get("target") == target_id
            and evidence.get("source_revision") == revision
            and evidence.get("version") == canonical_version,
            f"target={target_id} build evidence has stale or mismatched identity/revision/version")
    package_evidence = evidence.get("packages")
    require(isinstance(package_evidence, list) and len(package_evidence) == 2,
            f"target={target_id} build evidence must contain both package roles")
    evidence_by_role: dict[str, dict[str, Any]] = {}
    for package in package_evidence:
        require(isinstance(package, dict)
                and set(package) == {"role", "filename", "sha256"},
                f"target={target_id} malformed package evidence record")
        role = package["role"]
        require(isinstance(role, str) and role in records and role not in evidence_by_role,
                f"target={target_id} duplicate or unknown package role evidence: {role!r}")
        require(package["filename"] == records[role]["package_filename"],
                f"target={target_id} {role} evidence filename/identity mismatch")
        digest = _require_digest(package["sha256"], f"{role} package evidence hash")
        path = directory / package["filename"]
        require(path.stat().st_size > 0, f"target={target_id} empty package: {path.name}")
        require(_sha256(path) == digest,
                f"target={target_id} {role} package checksum differs from build evidence")
        evidence_by_role[role] = package
    require(set(evidence_by_role) == set(records),
            f"target={target_id} build evidence does not contain both roles")

    gates = evidence.get("gates")
    require(isinstance(gates, list) and all(isinstance(gate, str) for gate in gates)
            and len(gates) == len(set(gates))
            and set(gates) == REQUIRED_GATES,
            f"target={target_id} successful build/test/package gates are incomplete")
    discovered = evidence.get("ctest_discovered")
    require(isinstance(discovered, int) and not isinstance(discovered, bool) and discovered > 0,
            f"target={target_id} build evidence reports no discovered CTest tests")
    frontend = evidence.get("frontend")
    require(isinstance(frontend, dict) and frontend.get("schema_version") == 1,
            f"target={target_id} frontend build evidence is missing")
    for field in ("index_sha256", "notices_sha256", "embedded_header_sha256"):
        _require_digest(frontend.get(field), f"frontend {field}")

    _check_verification_evidence(
        target, records, directory,
        {role: item["sha256"] for role, item in evidence_by_role.items()},
    )

    validated = validate_packages.validate(target_id, revision, directory, targets_path)
    require(isinstance(validated, list) and len(validated) == 2,
            f"target={target_id} native validator did not confirm both roles")
    validated_by_role: dict[str, dict[str, Any]] = {}
    for item in validated:
        require(isinstance(item, dict), f"target={target_id} malformed native validation result")
        role = item.get("role")
        require(isinstance(role, str) and role in records and role not in validated_by_role
                and item.get("filename") == records[role]["package_filename"]
                and item.get("identity") == records[role],
                f"target={target_id} native validator identity mismatch for role {role!r}")
        validated_by_role[role] = item
    require(set(validated_by_role) == set(records),
            f"target={target_id} native validation did not confirm both roles")
    return [{**records[role], "package_sha256": evidence_by_role[role]["sha256"]}
            for role in ("runtime", "developer")]


def stage_artifacts(target_id: str, revision: str, directory: Path, output: Path,
                    github_output: Path | None = None,
                    targets_path: Path = ROOT / "packaging/targets.json") -> None:
    require(not directory.is_symlink(), "package input directory must not be a symlink")
    source = directory.resolve()
    output_path = output.absolute()
    require(not output_path.is_symlink(), "artifact output directory must not be a symlink")
    destination = output_path.resolve()
    require(destination != source and not destination.is_relative_to(source)
            and not source.is_relative_to(destination),
            "artifact output and package input directories must be disjoint")
    require(not destination.exists(), f"refusing to overwrite existing artifact directory: {destination}")

    records = validate_target_output(target_id, revision, source, targets_path)
    destination.mkdir(parents=True)
    try:
        output_names: dict[str, str] = {}
        for validated_record in records:
            record = {key: value for key, value in validated_record.items()
                      if key != "package_sha256"}
            role = record["role"]
            role_dir = destination / role
            role_dir.mkdir()
            source_package = source / record["package_filename"]
            package_digest = _sha256(source_package)
            require(package_digest == validated_record["package_sha256"],
                    f"target={target_id} {role} package changed after native validation")
            shutil.copyfile(source_package, role_dir / record["package_filename"])
            require(_sha256(role_dir / record["package_filename"]) == package_digest,
                    f"target={target_id} staged {role} package checksum mismatch")
            metadata = {**record, "package_sha256": package_digest}
            (role_dir / "metadata.json").write_text(
                json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
            output_names[role] = record["artifact_name"]
            print(f"[{target_id}] staged {role}: {record['artifact_name']}", flush=True)
        require(len(set(output_names.values())) == 2,
                f"target={target_id} runtime/developer artifact identity collision")
        if github_output is not None:
            with github_output.open("a", encoding="utf-8") as stream:
                for role in ("runtime", "developer"):
                    stream.write(f"{role}_artifact_name={output_names[role]}\n")
    except (OSError, ValueError, TypeError, KeyError):
        shutil.rmtree(destination, ignore_errors=True)
        raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--targets", type=Path, default=ROOT / "packaging/targets.json",
                        help="validated target-set JSON (defaults to the production set)")
    parser.add_argument("--github-output", type=Path,
                        help="optional Actions output file for the two full artifact names")
    args = parser.parse_args(argv)
    try:
        stage_artifacts(args.target, args.revision, args.directory,
                        args.output_dir, args.github_output, args.targets.resolve())
        return 0
    except (ValueError, KeyError, TypeError, OSError, RuntimeError) as error:
        print(f"prepare_artifacts failed: target={args.target}: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
