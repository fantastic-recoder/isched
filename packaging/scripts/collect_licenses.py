#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Collect dependency and embedded-frontend notices for native package roles."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Any


_NOTICE_NAME = re.compile(r"^(license|copying|notice|copyright)([-_.].*)?$", re.IGNORECASE)
_EXCLUDED_NAMES = {"b2", "catch2", "cmake", "ninja"}


class CollectionError(RuntimeError):
    """An input or required notice is missing or malformed."""


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _load_graph(path: Path) -> list[dict[str, Any]]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CollectionError(f"cannot read Conan graph JSON {path}: {error}") from error

    graph = data.get("graph")
    nodes = graph.get("nodes") if isinstance(graph, dict) else None
    if not isinstance(nodes, dict):
        raise CollectionError("Conan graph JSON has no graph.nodes object")
    if graph.get("error"):
        raise CollectionError(f"Conan dependency graph is incomplete: {graph['error']}")

    selected: list[dict[str, Any]] = []
    for node in nodes.values():
        if not isinstance(node, dict) or not node.get("ref"):
            continue
        if node.get("recipe") == "Consumer" or node.get("name") is None:
            continue
        if node.get("context") != "host":
            continue
        name = str(node.get("name") or str(node["ref"]).split("/", 1)[0]).lower()
        if name in _EXCLUDED_NAMES:
            continue
        if not isinstance(node.get("license"), str) or not node["license"].strip():
            raise CollectionError(f"Conan dependency {node['ref']} has no license metadata")
        selected.append(node)

    if not selected:
        raise CollectionError("Conan graph contains no host library dependencies")
    return sorted(selected, key=lambda node: str(node["ref"]).casefold())


def _notice_files(node: dict[str, Any]) -> list[tuple[Path, Path]]:
    """Return (file, package-relative path) pairs from cache/source license areas."""
    candidates: list[tuple[Path, Path]] = []
    package_folder = node.get("package_folder")
    source_folder = node.get("source_folder")
    recipe_folder = node.get("recipe_folder")
    for area, value in (("package", package_folder), ("source", source_folder),
                        ("recipe", recipe_folder)):
        if isinstance(value, str) and value:
            root = Path(value)
            if root.is_dir():
                candidates.extend(((root / "licenses", Path(area) / "licenses"),
                                   (root, Path(area))))

    result: list[tuple[Path, Path]] = []
    seen: set[Path] = set()
    for root, prefix in candidates:
        if not root.is_dir():
            continue
        resolved_root = root.resolve()
        try:
            files = sorted(root.rglob("*"), key=lambda path: path.as_posix().casefold())
        except OSError:
            continue
        for path in files:
            if not path.is_file() or not _NOTICE_NAME.match(path.name):
                continue
            if any(part.casefold() in {"test", "tests"} for part in path.relative_to(root).parts[:-1]):
                continue
            try:
                resolved = path.resolve(strict=True)
                relative = prefix / path.relative_to(root)
                resolved.relative_to(resolved_root)
            except (OSError, ValueError):
                continue
            if resolved in seen:
                continue
            seen.add(resolved)
            result.append((resolved, relative))
    return result


def _copy_notice(source: Path, destination: Path, role_root: Path) -> dict[str, str]:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    return {
        "path": destination.relative_to(role_root).as_posix(),
        "sha256": _sha256(destination),
    }


def collect(graph_path: Path, angular_notice: Path, output_dir: Path) -> dict[str, Any]:
    if not graph_path.is_file():
        raise CollectionError(f"Conan graph input does not exist: {graph_path}")
    if not angular_notice.is_file() or angular_notice.stat().st_size == 0:
        raise CollectionError(f"Angular production notices are missing or empty: {angular_notice}")
    dependencies = _load_graph(graph_path)
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    if output_dir.exists():
        if not output_dir.is_dir() or any(output_dir.iterdir()):
            raise CollectionError(f"license output directory is not empty (stale output?): {output_dir}")
        output_dir.rmdir()

    manifest: dict[str, Any]
    with tempfile.TemporaryDirectory(
        prefix=f".{output_dir.name}.", dir=output_dir.parent
    ) as temporary_directory:
        staged_output = Path(temporary_directory) / "payload"
        staged_output.mkdir()
        manifest = _collect_into(dependencies, angular_notice, staged_output)
        staged_output.rename(output_dir)
    return manifest


def _collect_into(
    dependencies: list[dict[str, Any]], angular_notice: Path, output_dir: Path
) -> dict[str, Any]:
    role_records: dict[str, list[dict[str, str]]] = {"runtime": [], "developer": []}
    dependency_records: list[dict[str, Any]] = []

    for node in dependencies:
        ref = str(node["ref"])
        directory_name = re.sub(r"[^A-Za-z0-9._+-]+", "_", ref)
        notices = _notice_files(node)
        if not notices:
            raise CollectionError(
                f"no license/notice files found in Conan cache/source for {ref} "
                f"(declared license: {node['license']})"
            )

        copied_by_role: dict[str, list[dict[str, str]]] = {"runtime": [], "developer": []}
        for source, relative in notices:
            for role in role_records:
                target = output_dir / role / "licenses" / "conan" / directory_name / relative
                record = _copy_notice(source, target, output_dir / role)
                role_records[role].append(record)
                copied_by_role[role].append(record)
        dependency_records.append({
            "reference": ref,
            "declared_license": node["license"],
            "roles": copied_by_role,
        })

    angular_target = output_dir / "runtime" / "licenses" / "angular" / angular_notice.name
    angular_record = _copy_notice(angular_notice, angular_target, output_dir / "runtime")
    manifest = {
        "schema_version": 1,
        "dependencies": dependency_records,
        "roles": {
            "runtime": role_records["runtime"] + [angular_record],
            "developer": role_records["developer"],
        },
    }
    for role in role_records:
        role_manifest = {
            "schema_version": 1,
            "role": role,
            "dependencies": [
                {
                    "reference": dependency["reference"],
                    "declared_license": dependency["declared_license"],
                    "notices": dependency["roles"][role],
                }
                for dependency in dependency_records
            ],
            "notices": manifest["roles"][role],
        }
        path = output_dir / role / "license-inventory.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(role_manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--conan-graph", type=Path, required=True)
    parser.add_argument("--angular-notices", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        manifest = collect(args.conan_graph, args.angular_notices, args.output_dir)
    except (CollectionError, OSError) as error:
        print(f"collect_licenses: {error}", file=sys.stderr)
        return 2
    print(
        "collected notices for "
        f"{len(manifest['dependencies'])} Conan dependencies; "
        f"runtime={len(manifest['roles']['runtime'])}, "
        f"developer={len(manifest['roles']['developer'])} notice files"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
