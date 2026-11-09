#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Install and smoke-test one runtime package in a clean target container."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import pwd
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from urllib.error import URLError
from urllib.parse import urljoin, urlsplit
from urllib.request import Request, urlopen

sys.path.insert(0, str(Path(__file__).resolve().parent))
from package_identity import ROOT, load_targets, require


def _failure_stage(error: BaseException, default: str) -> str:
    text = "\n".join(str(getattr(error, attr, "") or "") for attr in ("stdout", "stderr"))
    stages = [word.partition("=")[2].strip(":") for word in text.split() if word.startswith("stage=")]
    return stages[-1] if stages else default


def run(stage: str, command: list[str], *, timeout: int = 300,
        capture: bool = True, check: bool = True) -> subprocess.CompletedProcess:
    target = os.environ.get("ISCHED_BUILD_TARGET", "unknown")
    print(f"[{target}] stage={stage} $ {' '.join(command)}", flush=True)
    try:
        result = subprocess.run(command, check=check, timeout=timeout, text=True,
                                capture_output=capture)
    except (OSError, subprocess.SubprocessError) as error:
        actual_stage = _failure_stage(error, stage)
        detail = ""
        if isinstance(error, subprocess.CalledProcessError):
            detail = f"\nstdout:\n{error.stdout or ''}\nstderr:\n{error.stderr or ''}"
        raise RuntimeError(f"target={target} stage={actual_stage} failed: {error}{detail}") from error
    if result.stdout:
        print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr, flush=True)
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
        run("install", ["apt-get", "update"], timeout=600)
        run("install", ["apt-get", "install", "-y", "--no-install-recommends", str(package)], timeout=900)
        run("install-cleanup", ["apt-get", "clean"])
    else:
        run("install", ["dnf", "install", "-y", "--setopt=install_weak_deps=False", str(package)], timeout=900)


def _check_loader() -> str:
    binary = Path("/usr/bin/isched_srv")
    require(binary.is_file(), "installed runtime package is missing /usr/bin/isched_srv")
    if not shutil.which("ldd"):
        raise RuntimeError("target runtime image has no ldd loader-inspection tool")
    output = run("loader", ["ldd", str(binary)]).stdout
    require("not found" not in output, f"unresolved shared-object dependency:\n{output}")
    return output


def _free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def _http_json(url: str, payload: dict, timeout: float = 2.0) -> tuple[int, dict]:
    request = Request(url, data=json.dumps(payload).encode("utf-8"),
                      headers={"Content-Type": "application/json"}, method="POST")
    with urlopen(request, timeout=timeout) as response:
        raw = response.read(2_000_000)
        return response.status, json.loads(raw)


def _http_text(url: str, timeout: float = 2.0) -> tuple[int, str, str]:
    request = Request(url, headers={"Accept": "text/html,*/*"})
    with urlopen(request, timeout=timeout) as response:
        content_type = response.headers.get("Content-Type", "")
        return response.status, response.read(2_000_000).decode("utf-8", "replace"), content_type


def _verify_server() -> dict:
    unprivileged = pwd.getpwnam("nobody")
    work = Path(tempfile.mkdtemp(prefix="isched-runtime-", dir="/tmp"))
    data = work / "data"
    data.mkdir()
    os.chown(work, unprivileged.pw_uid, unprivileged.pw_gid)
    os.chown(data, unprivileged.pw_uid, unprivileged.pw_gid)
    port = _free_port()
    environment = dict(os.environ)
    environment.update({
        "HOME": str(work), "ISCHED_SERVER_HOST": "127.0.0.1",
        "ISCHED_SERVER_PORT": str(port), "ISCHED_MIN_THREADS": "1",
        "ISCHED_MAX_THREADS": "2", "ISCHED_LOG_LEVEL": "warn",
    })
    log_path = work / "server.log"
    process: subprocess.Popen | None = None
    try:
        with log_path.open("wb") as log:
            print(f"[{os.environ.get('ISCHED_BUILD_TARGET', 'unknown')}] stage=start "
                  "$ /usr/bin/isched_srv --data-dir <temporary>", flush=True)
            process = subprocess.Popen(
                ["/usr/bin/isched_srv", "--data-dir", str(data)],
                cwd=work, env=environment, stdout=log, stderr=subprocess.STDOUT,
                start_new_session=True, user=unprivileged.pw_uid,
                group=unprivileged.pw_gid,
            )
            base_url = f"http://127.0.0.1:{port}"
            deadline = time.monotonic() + 30
            health: dict | None = None
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f"target={os.environ.get('ISCHED_BUILD_TARGET')} "
                                       f"stage=start server exited with {process.returncode}")
                try:
                    status, health = _http_json(
                        base_url + "/graphql", {"query": "{ isched { health { status } } }"})
                    if status == 200 and isinstance(health, dict) and "data" in health:
                        break
                except (URLError, TimeoutError, json.JSONDecodeError, OSError):
                    time.sleep(0.25)
            else:
                raise RuntimeError(f"target={os.environ.get('ISCHED_BUILD_TARGET')} "
                                   "stage=health timed out waiting for GraphQL response")
            require(not health.get("errors"), f"GraphQL health query returned errors: {health}")
            health_value = health.get("data", {}).get("isched", {}).get("health")
            require(isinstance(health_value, dict) and health_value.get("status"),
                    f"GraphQL health response has no status: {health}")

            ui_status, html, content_type = _http_text(base_url + "/isched")
            require(ui_status == 200 and "html" in content_type.lower(),
                    f"embedded UI response is not HTML (status={ui_status}, type={content_type})")
            require("<html" in html.lower(), "embedded UI response has no HTML document")
            references = re.findall(r"(?:src|href)=[\"']([^\"']+\.(?:js|css))(?:\?[^\"']*)?[\"']", html, re.I)
            require(bool(references), "embedded UI index contains no referenced JavaScript or CSS asset")
            asset = next((value for value in references
                          if not urlsplit(value).scheme and not value.startswith("//")), None)
            require(asset is not None, "embedded UI has no local JS/CSS asset reference")
            parsed_asset = urlsplit(asset)
            asset_path = parsed_asset.path
            if asset_path.startswith("/"):
                if not asset_path.startswith("/isched/"):
                    asset_path = "/isched" + asset_path
            else:
                asset_path = "/isched/" + asset_path
            asset_url = base_url + asset_path
            asset_path = urlsplit(asset_url).path
            require(asset_path.startswith("/"), f"unsafe UI asset path: {asset}")
            asset_request = Request(asset_url)
            with urlopen(asset_request, timeout=5) as response:
                asset_bytes = response.read(10_000_000)
                asset_status = response.status
            require(asset_status == 200 and bool(asset_bytes),
                    f"embedded UI asset did not load: {asset} (status={asset_status})")
            return {"health": health_value, "ui_path": "/isched", "asset_path": urlsplit(asset_url).path,
                    "asset_bytes": len(asset_bytes), "port": port}
    except Exception as error:
        diagnostic = ""
        if log_path.is_file():
            try:
                lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
                diagnostic = "\nserver log tail:\n" + "\n".join(lines[-80:])[-8000:]
            except OSError as log_error:
                diagnostic = f"\nserver log unavailable: {log_error}"
        raise RuntimeError(f"{error}{diagnostic}") from error
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)
        shutil.rmtree(work, ignore_errors=True)


def _in_container(target: dict, package: Path, evidence: Path) -> None:
    release = os_release()
    expected_id = "ubuntu" if target["distribution"] == "ubuntu" else "fedora"
    require(release.get("ID") == expected_id and
            release.get("VERSION_ID") == target["distribution_version"],
            f"runtime verifier must run in {target['id']}, got {release.get('ID')} {release.get('VERSION_ID')}")
    require(platform.machine() in {"x86_64", "amd64"}, "runtime verifier requires linux/amd64")
    forbidden_tools = ("gcc", "g++", "cmake", "ninja", "node", "pnpm", "conan")
    present = [tool for tool in forbidden_tools if shutil.which(tool)]
    require(not present, f"runtime verification environment contains build tools: {present}")
    _install_package(target, package)
    loader = _check_loader()
    smoke = _verify_server()
    evidence.write_text(json.dumps({
        "schema_version": 1, "target": target["id"], "package": package.name,
        "loader": "resolved", "loader_output_sha256": hashlib.sha256(loader.encode()).hexdigest(),
        "smoke": smoke, "seeded": False,
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _container_run(target: dict, package: Path, evidence: Path) -> None:
    runtime = shutil.which("docker")
    require(runtime is not None, "Docker is required for clean runtime verification")
    container: str | None = None
    try:
        image = target["image"]
        created = run("container-create", [runtime, "create", "--platform", target["platform"],
                                            "--entrypoint", "/bin/sleep", image, "infinity"])
        container = created.stdout.strip().splitlines()[-1]
        run("container-start", [runtime, "start", container])
        scripts = ROOT / "packaging/scripts"
        run("container-prepare", [runtime, "exec", container, "mkdir", "-p", "/opt/isched/packaging/scripts"])
        for source, destination in (
            (package, f"{container}:/tmp/{package.name}"),
            (scripts / "package_identity.py", f"{container}:/opt/isched/packaging/scripts/package_identity.py"),
            (scripts / "verify_runtime.py", f"{container}:/opt/isched/packaging/scripts/verify_runtime.py"),
            (ROOT / "packaging/targets.json", f"{container}:/opt/isched/packaging/targets.json"),
        ):
            run("container-copy", [runtime, "cp", str(source), destination])
        distro = target["distribution"]
        install_python = (["apt-get", "update"] if distro == "ubuntu" else ["dnf", "makecache"])
        run("container-python", [runtime, "exec", container, *install_python], timeout=600)
        python_install = (["apt-get", "install", "-y", "--no-install-recommends", "python3", "ca-certificates"]
                          if distro == "ubuntu" else ["dnf", "install", "-y", "python3", "ca-certificates"])
        run("container-python", [runtime, "exec", container, *python_install], timeout=600)
        run("runtime-install", [runtime, "exec", container, "python3",
                                "/opt/isched/packaging/scripts/verify_runtime.py",
                                "--target", target["id"], "--package", f"/tmp/{package.name}",
                                "--in-container", "--evidence", "/tmp/runtime-verification.json"], timeout=1800)
        evidence.parent.mkdir(parents=True, exist_ok=True)
        run("evidence-copy", [runtime, "cp", f"{container}:/tmp/runtime-verification.json", str(evidence)])
    finally:
        if container:
            run("container-cleanup", [runtime, "rm", "-f", container], check=False)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--in-container", action="store_true")
    parser.add_argument("--evidence", type=Path, default=Path("runtime-verification.json"))
    args = parser.parse_args(argv)
    os.environ["ISCHED_BUILD_TARGET"] = args.target
    try:
        targets = load_targets()
        target = next((record for record in targets if record["id"] == args.target), None)
        require(target is not None, f"unknown target: {args.target}")
        package = args.package.resolve()
        require(package.is_file() and package.stat().st_size > 0, f"runtime package missing or empty: {package}")
        if args.in_container:
            _in_container(target, package, args.evidence.resolve())
        else:
            _container_run(target, package, args.evidence.resolve())
        print(f"[{args.target}] runtime verification passed; evidence={args.evidence}", flush=True)
        return 0
    except (ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError, RuntimeError) as error:
        print(f"[{args.target}] runtime verification failed: {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
