#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Query isched bootstrap mode (`systemState.seedModeActive`) via GraphQL.

Examples:
    python3 tools/query_bootstrap_mode.py
    python3 tools/query_bootstrap_mode.py --port 18080
    python3 tools/query_bootstrap_mode.py --url http://127.0.0.1:8080/graphql --raw
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Query isched bootstrap mode from GraphQL systemState.",
    )
    parser.add_argument("--host", default="127.0.0.1", help="Server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8080, help="Server port (default: 8080)")
    parser.add_argument(
        "--url",
        help="Full GraphQL endpoint URL. If set, --host/--port are ignored.",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Print raw JSON response instead of a human-readable line.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    url = args.url or f"http://{args.host}:{args.port}/graphql"

    query = {
        "query": "query { systemState { seedModeActive } }",
    }

    request = urllib.request.Request(
        url=url,
        method="POST",
        headers={"Content-Type": "application/json"},
        data=json.dumps(query).encode("utf-8"),
    )

    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.URLError as exc:
        print(f"[query_bootstrap_mode] ERROR: request to {url} failed: {exc}", file=sys.stderr)
        return 2
    except json.JSONDecodeError as exc:
        print(f"[query_bootstrap_mode] ERROR: invalid JSON response: {exc}", file=sys.stderr)
        return 3

    if args.raw:
        print(json.dumps(payload, indent=2))
        return 0

    if payload.get("errors"):
        print(f"[query_bootstrap_mode] ERROR: GraphQL errors: {payload['errors']}", file=sys.stderr)
        return 4

    try:
        seed_mode_active = payload["data"]["systemState"]["seedModeActive"]
    except (KeyError, TypeError):
        print("[query_bootstrap_mode] ERROR: unexpected response shape", file=sys.stderr)
        print(json.dumps(payload, indent=2), file=sys.stderr)
        return 5

    print(f"bootstrap mode: {'ON' if seed_mode_active else 'OFF'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

