#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Mechanical migration helper: rewrites C++ tests to use the new built-in
``isched`` GraphQL namespace.

Transforms:
  1. GraphQL operation strings (regular ``"..."`` and raw ``R"(...)"``) whose
     top-level body contains at least one *built-in* field have that body
     wrapped in ``isched { ... }``.
  2. nlohmann::json access chains rooted at ``.data["<builtin>"]`` (or
     ``.value("data", ...)`` chains) get an extra ``["isched"]`` /
     ``.value("isched", ...)`` step inserted.

This is intentionally conservative. It only touches strings/accesses where the
field name is in the built-in set; user-defined / custom-schema fields are
left alone. It does NOT handle:
  - GraphQL fragments (``...FragName``) — none of our tests use them.
  - Inline fragments.
  - Subscription-only queries where the user has split-up multi-line strings
    via ``"a" "b" "c"`` — those are joined token-wise first, then handled.

Run from repo root.  Default scope: ``src/test/cpp``.

    python3 tools/migrate_isched_namespace.py [path ...]

Add ``--dry-run`` to preview without writing.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Iterable

# ---------------------------------------------------------------------------
# Built-in field sets (mirror src/main/cpp/isched/backend/isched_GqlExecutor.cpp).
# ---------------------------------------------------------------------------
BUILTIN_FIELDS: set[str] = {
    # Query
    "hello", "version", "uptime", "clientCount", "health", "serverInfo", "info",
    "metrics", "serverMetrics", "tenantMetrics", "env", "configprops",
    "activeConfiguration", "configurationHistory", "currentUser", "user",
    "users", "organization", "organizations", "dataSources", "systemState",
    "roles", "schemaDocuments", "schemaDocument", "plugins", "plugin",
    "pluginOpenApiDocument", "platformBootstrapStatus",
    # Mutation
    "echo", "bootstrapPlatformAdmin", "completePlatformBootstrap",
    "applyConfiguration", "activateSnapshot", "rollbackConfiguration",
    "login", "logout", "revokeSession", "revokeAllSessions",
    "terminateAllSessions", "createRole", "deleteRole",
    "createOrganization", "updateOrganization", "deleteOrganization",
    "createUser", "updateUser", "deleteUser",
    "createDataSource", "updateDataSource", "deleteDataSource",
    "updateTenantConfig", "shutdown", "createPlatformAdmin",
    "uploadSchemaDocument", "loadPlugin", "unloadPlugin",
    # Subscription
    "configurationActivated", "healthChanged",
    "serverMetricsUpdated", "tenantMetricsUpdated",
}


# ---------------------------------------------------------------------------
# Tiny GraphQL helpers.
# ---------------------------------------------------------------------------
def _find_matching_brace(s: str, open_idx: int) -> int:
    """Return index of the matching ``}`` for the ``{`` at ``s[open_idx]``."""
    assert s[open_idx] == "{"
    depth = 0
    i = open_idx
    in_string = False
    string_quote = ""
    while i < len(s):
        ch = s[i]
        if in_string:
            if ch == "\\" and i + 1 < len(s):
                i += 2
                continue
            if ch == string_quote:
                in_string = False
            i += 1
            continue
        if ch in ('"', "'"):
            in_string = True
            string_quote = ch
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1  # unbalanced


_TOP_FIELD_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def _top_level_fields(body: str) -> list[str]:
    """Return the top-level field selection names inside an operation body
    (the text between the outermost ``{`` and ``}``)."""
    out: list[str] = []
    depth = 0
    i = 0
    n = len(body)
    while i < n:
        ch = body[i]
        if ch == "{":
            depth += 1
            i += 1
            continue
        if ch == "}":
            depth -= 1
            i += 1
            continue
        if ch == "(":
            # Skip parenthesised argument list.
            pdepth = 1
            i += 1
            while i < n and pdepth > 0:
                if body[i] == "(":
                    pdepth += 1
                elif body[i] == ")":
                    pdepth -= 1
                i += 1
            continue
        if depth != 0:
            i += 1
            continue
        if ch.isalpha() or ch == "_":
            m = _TOP_FIELD_RE.match(body, i)
            if m:
                name = m.group(0)
                # Skip alias-form ``alias: field`` — we keep the field name.
                end = m.end()
                j = end
                while j < n and body[j] in " \t\r\n":
                    j += 1
                if j < n and body[j] == ":":
                    # alias — advance past ``: realName``
                    j += 1
                    while j < n and body[j] in " \t\r\n":
                        j += 1
                    m2 = _TOP_FIELD_RE.match(body, j)
                    if m2:
                        out.append(m2.group(0))
                        i = m2.end()
                        continue
                out.append(name)
                i = end
                continue
        i += 1
    return out


def transform_graphql_body(operation_body: str) -> tuple[str, bool]:
    """Wrap ``operation_body`` (text **between** the outermost ``{ }``) with
    ``isched { ... }`` when any top-level field is a built-in.

    Returns ``(new_body, changed)``.
    """
    fields = _top_level_fields(operation_body)
    if not any(f in BUILTIN_FIELDS for f in fields):
        return operation_body, False
    # Don't double-wrap if already wrapped.
    if fields == ["isched"]:
        return operation_body, False
    # Preserve leading/trailing whitespace for nicer diffs.
    stripped = operation_body.strip("\r\n")
    return f" isched {{{operation_body}}} ", True


def _process_gql_string(content: str) -> tuple[str, bool]:
    """Transform a single GraphQL operation source string."""
    # Locate the outermost ``{`` of the operation body.
    # Skip optional ``query|mutation|subscription Name? (...)? `` prefix.
    m = re.search(r"\b(query|mutation|subscription)\b", content)
    search_from = m.end() if m else 0
    open_idx = content.find("{", search_from)
    if open_idx == -1:
        return content, False
    close_idx = _find_matching_brace(content, open_idx)
    if close_idx == -1:
        return content, False
    body = content[open_idx + 1 : close_idx]
    new_body, changed = transform_graphql_body(body)
    if not changed:
        return content, False
    return content[: open_idx + 1] + new_body + content[close_idx:], True


# ---------------------------------------------------------------------------
# C++ string-literal scanning.
# ---------------------------------------------------------------------------
# Matches a C++ raw string literal: R"delim(...)delim".
_RAW_STRING_RE = re.compile(r'R"([^()\\\s]{0,16})\((.*?)\)\1"', re.DOTALL)
# Matches a regular C++ string literal (possibly with escapes, no newline).
_STR_RE = re.compile(r'"((?:\\.|[^"\\\n])*)"')


def _looks_like_graphql(s: str) -> bool:
    if "{" not in s:
        return False
    head = s.lstrip()
    if head.startswith(("query", "mutation", "subscription", "{")):
        return True
    # Multi-token concatenations sometimes begin mid-name; be permissive.
    return False


def _rewrite_raw_strings(text: str) -> tuple[str, int]:
    changes = 0

    def repl(m: re.Match) -> str:
        nonlocal changes
        delim, body = m.group(1), m.group(2)
        if not _looks_like_graphql(body):
            return m.group(0)
        new_body, changed = _process_gql_string(body)
        if changed:
            changes += 1
            return f'R"{delim}({new_body}){delim}"'
        return m.group(0)

    new = _RAW_STRING_RE.sub(repl, text)
    return new, changes


def _rewrite_regular_strings(text: str) -> tuple[str, int]:
    """Handle adjacent ``"..."`` ``"..."`` runs by joining them, rewriting,
    then re-emitting as a single string literal."""
    changes = 0
    out: list[str] = []
    i = 0
    n = len(text)
    while i < n:
        ch = text[i]
        # Skip line comments.
        if ch == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j == -1 else j + 1
            out.append(text[i:j])
            i = j
            continue
        if ch == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j == -1 else j + 2
            out.append(text[i:j])
            i = j
            continue
        # Skip raw strings (already handled elsewhere) so we don't touch them.
        if ch == "R" and i + 1 < n and text[i + 1] == '"':
            m = _RAW_STRING_RE.match(text, i)
            if m:
                out.append(m.group(0))
                i = m.end()
                continue
        if ch != '"':
            out.append(ch)
            i += 1
            continue
        # Collect a run of adjacent string literals (separated only by
        # whitespace) so we can analyse the logical concatenation.
        run_start = i
        pieces: list[str] = []
        while i < n and text[i] == '"':
            m = _STR_RE.match(text, i)
            if not m:
                break
            pieces.append(m.group(1))
            i = m.end()
            # Skip inter-literal whitespace.
            while i < n and text[i] in " \t\r\n":
                i += 1
        if i == run_start:
            # Couldn't consume the quote (e.g. multi-line literal): advance one
            # char to guarantee progress and avoid an infinite loop.
            out.append(text[i])
            i += 1
            continue
        joined_logical = "".join(pieces)
        if _looks_like_graphql(joined_logical):
            new_body, changed = _process_gql_string(joined_logical)
            if changed:
                # Escape special chars for the C++ string literal.
                esc = (
                    new_body.replace("\\", "\\\\")
                    .replace('"', '\\"')
                    .replace("\n", "\\n")
                )
                out.append(f'"{esc}"')
                changes += 1
                continue
        # Emit original verbatim.
        out.append(text[run_start:i])
    return "".join(out), changes


# ---------------------------------------------------------------------------
# JSON access rewrites.
# ---------------------------------------------------------------------------
# ``.data["<builtin>"]`` (single index step) → ``.data["isched"]["<builtin>"]``
_DATA_BRACKET_RE = re.compile(
    r'(\.data)(\["(' + "|".join(sorted(BUILTIN_FIELDS, key=len, reverse=True)) + r')"\])'
)
# ``["data"]["<builtin>"]`` → ``["data"]["isched"]["<builtin>"]``
_DATA_BRACKET2_RE = re.compile(
    r'(\["data"\])(\["(' + "|".join(sorted(BUILTIN_FIELDS, key=len, reverse=True)) + r')"\])'
)
# ``.value("data", ...).value("<builtin>", ...)`` → insert ``.value("isched", json::object())`` between.
_VALUE_RE = re.compile(
    r'(\.value\("data",\s*[^)]*\))\s*\.value\("(' + "|".join(sorted(BUILTIN_FIELDS, key=len, reverse=True)) + r')",'
)


def _rewrite_json_access(text: str) -> tuple[str, int]:
    changes = 0

    def r1(m: re.Match) -> str:
        nonlocal changes
        changes += 1
        return f'{m.group(1)}["isched"]{m.group(2)}'

    text = _DATA_BRACKET_RE.sub(r1, text)
    text = _DATA_BRACKET2_RE.sub(r1, text)

    def r3(m: re.Match) -> str:
        nonlocal changes
        changes += 1
        return f'{m.group(1)}.value("isched", nlohmann::json::object()).value("{m.group(2)}",'

    text = _VALUE_RE.sub(r3, text)
    return text, changes


# ---------------------------------------------------------------------------
# Driver.
# ---------------------------------------------------------------------------
def process_file(path: pathlib.Path, dry_run: bool) -> tuple[int, int]:
    text = path.read_text()
    new_text, raw_changes = _rewrite_raw_strings(text)
    new_text, std_changes = _rewrite_regular_strings(new_text)
    new_text, json_changes = _rewrite_json_access(new_text)
    total_str = raw_changes + std_changes
    if total_str + json_changes == 0:
        return 0, 0
    if not dry_run:
        path.write_text(new_text)
    print(f"  {path}: gql={total_str} json={json_changes}")
    return total_str, json_changes


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("paths", nargs="*", default=["src/test/cpp"])
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args(argv)

    total_gql = 0
    total_json = 0
    files: list[pathlib.Path] = []
    for root in args.paths:
        rp = pathlib.Path(root)
        if rp.is_file():
            files.append(rp)
            continue
        files.extend(p for p in rp.rglob("*.cpp"))
        files.extend(p for p in rp.rglob("*.hpp"))

    for f in sorted(set(files)):
        g, j = process_file(f, args.dry_run)
        total_gql += g
        total_json += j

    print(
        f"\nDone. graphql_string_rewrites={total_gql} "
        f"json_access_rewrites={total_json} "
        f"{'(dry run)' if args.dry_run else ''}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

