#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""
run_e2e.py — Launch a fresh isched server instance backed by a temporary data
directory, execute all Playwright e2e tests, then clean up.

Usage
-----
    python3 tools/run_e2e.py [options] [-- <playwright args>]

Options
-------
    --build-type TYPE   CMake build type.  Selects cmake-build-<TYPE>.
                        Default: debug
    --keep-data         Keep the temporary data directory after the run.
    --port PORT         Port for the server to listen on (default: 18080).
    -- <args>           Any trailing arguments are forwarded to
                        `pnpm playwright test` verbatim.

Examples
--------
    # Run all e2e tests against the debug build:
    python3 tools/run_e2e.py

    # Run against the release build, keep the data dir for inspection:
    python3 tools/run_e2e.py --build-type release --keep-data

    # Run only the bootstrap spec:
    python3 tools/run_e2e.py -- e2e/bootstrap.spec.ts
"""

import argparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
UI_DIR = REPO_ROOT / "src" / "ui"
SERVER_HOST = "127.0.0.1"
DEFAULT_PORT = 18080
HEALTH_TIMEOUT_S = 30


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def server_binary(build_type: str) -> Path:
    """Return the absolute path to the isched_srv binary for *build_type*."""
    return (
        REPO_ROOT
        / f"cmake-build-{build_type}"
        / "src"
        / "main"
        / "cpp"
        / "isched"
        / "isched_srv"
    )


def wait_for_server(url: str, timeout_s: int = HEALTH_TIMEOUT_S) -> None:
    """Poll *url* until it returns a non-5xx response or *timeout_s* elapses."""
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with urllib.request.urlopen(url, timeout=2) as resp:
                if resp.status < 500:
                    return
        except Exception:
            pass
        time.sleep(0.25)
    raise TimeoutError(f"Server not ready at {url} after {timeout_s}s")


# ---------------------------------------------------------------------------
# Dev proxy verification (spec 013, T034)
# ---------------------------------------------------------------------------


def verify_graphql_forwarding(host: str, port: int, timeout_s: float = 5.0) -> list[str]:
    """Check that *host:port* forwards both HTTP and WebSocket traffic on /graphql.

    Works against the backend directly or against the Angular dev server
    (``ng serve`` with ``proxy.conf.json``). Returns a list of failures
    (empty on success). Uses only the standard library.
    """
    import base64
    import json
    import socket

    failures: list[str] = []

    # HTTP: a GraphQL query must be answered via POST /graphql.
    req = urllib.request.Request(
        f"http://{host}:{port}/graphql",
        data=json.dumps({"query": "query { isched { health { status } } }"}).encode(),
        headers={"content-type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout_s) as resp:
            body = json.loads(resp.read().decode())
            if resp.status != 200 or "data" not in body:
                failures.append(f"HTTP /graphql unexpected response: {resp.status} {body}")
    except Exception as exc:  # noqa: BLE001 - report any transport failure
        failures.append(f"HTTP /graphql failed: {exc}")

    # WebSocket: handshake must return 101 with the graphql-transport-ws subprotocol.
    key = base64.b64encode(os.urandom(16)).decode()
    handshake = (
        "GET /graphql HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Protocol: graphql-transport-ws\r\n\r\n"
    )
    try:
        with socket.create_connection((host, port), timeout=timeout_s) as sock:
            sock.settimeout(timeout_s)
            sock.sendall(handshake.encode())
            status_line = sock.recv(1024).split(b"\r\n", 1)[0].decode(errors="replace")
            if " 101 " not in status_line:
                failures.append(f"WebSocket upgrade on /graphql not accepted: {status_line!r}")
    except Exception as exc:  # noqa: BLE001
        failures.append(f"WebSocket upgrade on /graphql failed: {exc}")

    return failures


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run isched Playwright e2e tests with a fresh server instance.",
        epilog="Extra arguments after '--' are forwarded to 'pnpm playwright test'.",
    )
    parser.add_argument(
        "--build-type",
        default="debug",
        metavar="TYPE",
        help=(
            "CMake build type used to locate the server binary "
            "(selects cmake-build-<TYPE>). Default: debug"
        ),
    )
    parser.add_argument(
        "--keep-data",
        action="store_true",
        help="Do not remove the temporary data directory after the run.",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=DEFAULT_PORT,
        metavar="PORT",
        help=f"Port for the isched server (default: {DEFAULT_PORT}).",
    )

    parser.add_argument(
        "--verify-proxy",
        nargs="?",
        const="",
        default=None,
        metavar="HOST:PORT",
        help=(
            "Instead of running Playwright, start the server and verify HTTP + WebSocket "
            "forwarding on /graphql at HOST:PORT (default: the server itself; pass "
            "127.0.0.1:4200 to check the Angular dev-server proxy)."
        ),
    )

    # Everything after '--' goes to playwright.
    if "--" in argv:
        split = argv.index("--")
        own_argv, playwright_argv = argv[:split], argv[split + 1 :]
    else:
        own_argv, playwright_argv = argv, []

    ns = parser.parse_args(own_argv)
    ns.playwright_args = playwright_argv
    return ns


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)

    binary = server_binary(args.build_type)
    if not binary.exists():
        print(f"[run_e2e] ERROR: Server binary not found: {binary}", file=sys.stderr)
        print(
            "[run_e2e]        Build the project first with:  python3 configure.py",
            file=sys.stderr,
        )
        return 1

    data_dir = Path(tempfile.mkdtemp(prefix="isched-e2e-"))
    print(f"[run_e2e] Temporary data directory : {data_dir}")

    health_url = f"http://{SERVER_HOST}:{args.port}/graphql"

    server_proc: subprocess.Popen | None = None
    exit_code = 1

    try:
        env = {
            **os.environ,
            "ISCHED_SERVER_HOST": SERVER_HOST,
            "ISCHED_SERVER_PORT": str(args.port),
            # Signals Playwright's global-setup to skip launching its own server.
            "ISCHED_EXTERNAL_SERVER": "1",
        }

        print(f"[run_e2e] Starting : {binary} --data-dir {data_dir}")
        server_proc = subprocess.Popen(
            [str(binary), "--data-dir", str(data_dir)],
            cwd=str(REPO_ROOT),
            env=env,
        )

        print(f"[run_e2e] Waiting for server at {health_url} …")
        wait_for_server(health_url)
        print("[run_e2e] Server is ready.")

        if args.verify_proxy is not None:
            target = args.verify_proxy or f"{SERVER_HOST}:{args.port}"
            host, _, port_s = target.rpartition(":")
            failures = verify_graphql_forwarding(host or SERVER_HOST, int(port_s))
            for failure in failures:
                print(f"[run_e2e] FAIL: {failure}", file=sys.stderr)
            if not failures:
                print(f"[run_e2e] OK: HTTP + WebSocket forwarding verified at {target}")
            exit_code = 1 if failures else 0
            return exit_code

        playwright_cmd = ["pnpm", "playwright", "test", *args.playwright_args]
        print(f"[run_e2e] Running  : {' '.join(playwright_cmd)}")
        result = subprocess.run(playwright_cmd, cwd=str(UI_DIR), env=env)
        exit_code = result.returncode

    except TimeoutError as exc:
        print(f"[run_e2e] ERROR: {exc}", file=sys.stderr)
        exit_code = 1

    except KeyboardInterrupt:
        print("\n[run_e2e] Interrupted.", file=sys.stderr)
        exit_code = 130

    finally:
        if server_proc is not None and server_proc.poll() is None:
            print("[run_e2e] Stopping server …")
            server_proc.send_signal(signal.SIGTERM)
            try:
                server_proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                print("[run_e2e] Server did not stop — sending SIGKILL.", file=sys.stderr)
                server_proc.kill()
                server_proc.wait()

        if args.keep_data:
            print(f"[run_e2e] Data directory kept : {data_dir}")
        else:
            print(f"[run_e2e] Removing data directory …")
            shutil.rmtree(data_dir, ignore_errors=True)

    print(f"[run_e2e] Done (exit {exit_code}).")
    return exit_code


if __name__ == "__main__":
    sys.exit(main())

