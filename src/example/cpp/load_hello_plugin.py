#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Load the `hello_plugin` resolver plugin into a running isched server.

The script prompts for a platform-administrator username and password, logs in
via GraphQL, and then calls the built-in `loadPlugin` mutation to register the
example resolver plugin.

Example:
    python3 src/example/cpp/load_hello_plugin.py \
        --server-url http://127.0.0.1:18080/graphql \
        --plugin-path cmake-build-debug/src/example/cpp/libhello_plugin.so
"""

from __future__ import annotations

import argparse
import getpass
import json
import secrets
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

DEFAULT_SERVER_URL = "http://127.0.0.1:18080/graphql"
DEFAULT_PLUGIN_ID = "hello_plugin"


def parse_args(argv: list[str]) -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        Parsed CLI options.
    """

    parser = argparse.ArgumentParser(
        description="Authenticate as a platform admin and load the hello_plugin resolver plugin.",
    )
    parser.add_argument(
        "--server-url",
        default=DEFAULT_SERVER_URL,
        help=f"GraphQL endpoint URL (default: {DEFAULT_SERVER_URL})",
    )
    parser.add_argument(
        "--plugin-path",
        help="Absolute or relative path to the built hello_plugin shared library.",
    )
    parser.add_argument(
        "--admin-username",
        help="Platform-admin username/email. If omitted, the script prompts interactively.",
    )
    parser.add_argument(
        "--admin-password",
        help="Platform-admin password. If omitted, the script prompts interactively.",
    )
    parser.add_argument(
        "--plugin-id",
        default=DEFAULT_PLUGIN_ID,
        help=f"Plugin id to register with loadPlugin (default: {DEFAULT_PLUGIN_ID})",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="After loading the plugin, execute a sample echo query to verify the resolver.",
    )
    return parser.parse_args(argv)


def repo_root() -> Path:
    """Return the repository root based on the script location."""

    return Path(__file__).resolve().parents[3]


def candidate_plugin_paths() -> list[Path]:
    """Return common build outputs for the hello_plugin shared library."""

    root = repo_root()
    return [
        root / "cmake-build-debug" / "src" / "example" / "cpp" / "libhello_plugin.so",
        root / "cmake-build-release" / "src" / "example" / "cpp" / "libhello_plugin.so",
        root / "build" / "release" / "libhello_plugin.so",
        root / "build" / "release" / "src" / "example" / "cpp" / "libhello_plugin.so",
    ]


def resolve_plugin_path(explicit_path: str | None) -> Path:
    """Resolve the plugin library path.

    Args:
        explicit_path: Optional user-supplied path.

    Returns:
        The first existing plugin shared library path.

    Raises:
        FileNotFoundError: If no usable plugin library could be found.
    """

    if explicit_path:
        return Path(explicit_path).expanduser().resolve()

    for candidate in candidate_plugin_paths():
        if candidate.exists():
            return candidate.resolve()

    searched = "\n  - ".join(str(path) for path in candidate_plugin_paths())
    raise FileNotFoundError(
        "Unable to locate a built hello_plugin shared library."
        f"\nSearched:\n  - {searched}\n"
        "Build the target first, or pass --plugin-path explicitly."
    )


def origin_from_server_url(server_url: str) -> str:
    """Derive the scheme/host/port origin from a GraphQL endpoint URL."""

    parsed = urllib.parse.urlsplit(server_url)
    if not parsed.scheme or not parsed.netloc:
        raise ValueError(f"Invalid server URL: {server_url}")
    return f"{parsed.scheme}://{parsed.netloc}"


def graphql_request(
    server_url: str,
    query: str,
    variables: dict[str, object] | None = None,
    headers: dict[str, str] | None = None,
) -> dict[str, object]:
    """Send a GraphQL request and decode the JSON response."""

    payload = {"query": query, "variables": variables or {}}
    req_headers = {"Content-Type": "application/json"}
    if headers:
        req_headers.update(headers)

    request = urllib.request.Request(
        url=server_url,
        method="POST",
        headers=req_headers,
        data=json.dumps(payload).encode("utf-8"),
    )

    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"GraphQL request failed with HTTP {exc.code}: {body}") from exc
    except urllib.error.URLError as exc:
        raise RuntimeError(f"GraphQL request to {server_url} failed: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"Server returned invalid JSON: {exc}") from exc


def ensure_data(response: dict[str, object], label: str) -> dict[str, object]:
    """Return the GraphQL `data` payload or raise a readable error."""

    errors = response.get("errors")
    if isinstance(errors, list) and errors:
        first_error = errors[0] if isinstance(errors[0], dict) else {}
        message = first_error.get("message", "unknown GraphQL error") if isinstance(first_error, dict) else "unknown GraphQL error"
        raise RuntimeError(f"{label} failed: {message}")

    data = response.get("data")
    if not isinstance(data, dict):
        raise RuntimeError(f"{label} failed: missing GraphQL data")
    return data


def prompt_for_credentials(args: argparse.Namespace) -> tuple[str, str]:
    """Collect admin credentials, prompting only for missing values."""

    username = args.admin_username or input("Admin username/email: ").strip()
    if not username:
        raise RuntimeError("Admin username/email is required")

    password = args.admin_password or getpass.getpass("Admin password: ")
    if not password:
        raise RuntimeError("Admin password is required")

    return username, password


def login(server_url: str, username: str, password: str) -> str:
    """Log in as the requested administrator and return the JWT token."""

    response = graphql_request(
        server_url,
        """mutation($email: String!, $password: String!) {
             login(email: $email, password: $password) { token }
           }""",
        {"email": username, "password": password},
    )
    data = ensure_data(response, f"login({username})")
    token = data.get("login", {}).get("token") if isinstance(data.get("login"), dict) else None
    if not isinstance(token, str) or not token:
        raise RuntimeError("login succeeded but did not return a JWT token")
    return token


def load_plugin(server_url: str, token: str, plugin_path: Path, plugin_id: str) -> dict[str, object]:
    """Call the GraphQL `loadPlugin` mutation."""

    origin = origin_from_server_url(server_url)
    csrf_token = f"hello-plugin-{secrets.token_hex(16)}"
    response = graphql_request(
        server_url,
        """mutation($input: LoadPluginInput!) {
             isched {
               loadPlugin(input: $input) {
                 success
                 pluginId
                 errors
                 plugin { id kind name version libraryPath }
               }
             }
           }""",
        {
            "input": {
                "libraryPath": str(plugin_path),
                "kind": "resolver",
                "pluginId": plugin_id,
            }
        },
        {
            "Authorization": f"Bearer {token}",
            "Origin": origin,
            "Referer": f"{origin}/isched",
            "X-CSRF-Token": csrf_token,
        },
    )
    return ensure_data(response, "loadPlugin(hello_plugin)")


def verify_echo_query(server_url: str, token: str) -> dict[str, object]:
    """Probe the example echo field after loading the plugin."""

    origin = origin_from_server_url(server_url)
    csrf_token = f"hello-plugin-verify-{secrets.token_hex(16)}"
    response = graphql_request(
        server_url,
        """query {
             isched_examples {
               echo(message: \"hello from loader\")
             }
           }""",
        None,
        {
            "Authorization": f"Bearer {token}",
            "Origin": origin,
            "Referer": f"{origin}/isched",
            "X-CSRF-Token": csrf_token,
        },
    )
    return ensure_data(response, "echo verification query")


def main(argv: list[str] | None = None) -> int:
    """Program entry point."""

    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        plugin_path = resolve_plugin_path(args.plugin_path)
        username, password = prompt_for_credentials(args)
        token = login(args.server_url, username, password)
        load_result = load_plugin(args.server_url, token, plugin_path, args.plugin_id)

        plugin_payload = load_result.get("isched", {}).get("loadPlugin")
        if not isinstance(plugin_payload, dict):
            raise RuntimeError("loadPlugin returned an unexpected response shape")

        if not plugin_payload.get("success"):
            errors = plugin_payload.get("errors", [])
            if isinstance(errors, list) and errors:
                raise RuntimeError(f"loadPlugin failed: {errors[0]}")
            raise RuntimeError("loadPlugin failed without details")

        print(f"Loaded plugin '{plugin_payload.get('pluginId', args.plugin_id)}' from {plugin_path}")
        plugin_details = plugin_payload.get("plugin")
        if isinstance(plugin_details, dict):
            print(json.dumps(plugin_details, indent=2))

        if args.verify:
            verification = verify_echo_query(args.server_url, token)
            echo_value = verification.get("isched_examples", {}).get("echo")
            print(f"Verification echo result: {echo_value!r}")

        return 0
    except (FileNotFoundError, ValueError, RuntimeError) as exc:
        print(f"[load_hello_plugin] ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

