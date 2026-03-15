#!/usr/bin/env python3
"""
embed_ui_assets.py — Compile Angular dist output into a C++ header.

Scans src/ui/dist/isched-ui/browser/, computes SHA-256 ETags, and writes
src/main/cpp/isched/backend/isched_ui_assets.hpp with one embedded byte
array per file plus a lookup table (ISCHED_UI_ASSET_MAP).

Exits 1 if any gzip-compressed file exceeds 500 KB (NFR-UI-002).
Must be run from the repository root.
"""

import gzip
import hashlib
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

DIST_DIR = Path("src/ui/dist/isched-ui/browser")
OUTPUT_HEADER = Path("src/main/cpp/isched/backend/isched_ui_assets.hpp")
MAX_COMPRESSED_SIZE = 500 * 1024  # 500 KB (NFR-UI-002)

MIME_MAP = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".mjs": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
    ".json": "application/json",
    ".txt": "text/plain; charset=utf-8",
    ".woff": "font/woff",
    ".woff2": "font/woff2",
    ".ttf": "font/ttf",
    ".eot": "application/vnd.ms-fontobject",
    ".map": "application/json",
}


def compute_etag(data: bytes) -> str:
    """Return first 16 hex chars of SHA-256 of content."""
    return hashlib.sha256(data).hexdigest()[:16]


def to_c_identifier(path_relative: str) -> str:
    """Convert a relative URL path to a valid C identifier."""
    return "isched_ui_" + "".join(
        c if c.isalnum() else "_" for c in path_relative
    ).strip("_")


def xxd_array(data: bytes, var_name: str) -> str:
    """Generate 'static const unsigned char var_name[] = {...};' block."""
    hex_values = ", ".join(f"0x{b:02x}" for b in data)
    if not hex_values:
        hex_values = "0x00"
    # Format in rows of 12 bytes
    bytes_list = [f"0x{b:02x}" for b in data]
    rows = [bytes_list[i : i + 12] for i in range(0, len(bytes_list), 12)]
    formatted = ",\n    ".join(", ".join(row) for row in rows)
    if not formatted:
        formatted = "0x00"
    return f"static const unsigned char {var_name}[] = {{\n    {formatted}\n}};"


def collect_assets(dist_dir: Path):
    """
    Yield (url_path, file_path, data) tuples for all files under dist_dir.
    url_path is relative to / (e.g. '/index.html').
    """
    for file_path in sorted(dist_dir.rglob("*")):
        if not file_path.is_file():
            continue
        rel = file_path.relative_to(dist_dir)
        url_path = "/" + str(rel).replace("\\", "/")
        data = file_path.read_bytes()
        yield url_path, file_path, data


def check_compressed_size(url_path: str, data: bytes) -> None:
    """Abort if gzip-compressed size exceeds MAX_COMPRESSED_SIZE."""
    compressed = gzip.compress(data, compresslevel=9)
    size = len(compressed)
    if size > MAX_COMPRESSED_SIZE:
        print(
            f"ERROR: {url_path} compressed size {size} bytes exceeds "
            f"limit of {MAX_COMPRESSED_SIZE} bytes (NFR-UI-002)",
            file=sys.stderr,
        )
        sys.exit(1)


def generate_header(assets: list) -> str:
    """
    Build the full contents of isched_ui_assets.hpp.
    assets: list of (url_path, c_name, mime, etag, data)
    """
    lines = []
    lines.append("// GENERATED FILE — do not edit. Produced by tools/embed_ui_assets.py.")
    lines.append("// Re-run via CMake target 'isched_ui_embed' or: python3 tools/embed_ui_assets.py")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <cstddef>")
    lines.append("#include <cstdint>")
    lines.append("#include <span>")
    lines.append("#include <string_view>")
    lines.append("")
    lines.append("namespace isched::v0_0_1::backend {")
    lines.append("")

    # Emit each asset's byte array
    for url_path, c_name, mime, etag, data in assets:
        lines.append(f"// {url_path} ({len(data)} bytes, etag={etag})")
        lines.append(xxd_array(data, c_name))
        lines.append("")

    # Struct definition
    lines.append("struct UiAssetEntry {")
    lines.append("    std::span<const uint8_t> data;")
    lines.append("    std::string_view mime_type;")
    lines.append("    std::string_view etag;")
    lines.append("};")
    lines.append("")

    # Map entry struct
    lines.append("struct UiAssetMapEntry {")
    lines.append("    std::string_view url_path;")
    lines.append("    UiAssetEntry asset;")
    lines.append("};")
    lines.append("")

    # Emit the asset map
    lines.append(f"static const UiAssetMapEntry ISCHED_UI_ASSET_MAP[] = {{")
    for url_path, c_name, mime, etag, data in assets:
        size = len(data)
        lines.append(f'    {{"{url_path}", {{')
        lines.append(f'        std::span<const uint8_t>({c_name}, {size}),')
        lines.append(f'        "{mime}",')
        lines.append(f'        "{etag}"')
        lines.append("    }},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const std::size_t ISCHED_UI_ASSET_MAP_SIZE = {len(assets)};")
    lines.append("")
    lines.append("} // namespace isched::v0_0_1::backend")
    lines.append("")

    return "\n".join(lines)


def main() -> None:
    if not DIST_DIR.exists():
        print(
            f"ERROR: dist directory not found: {DIST_DIR}\n"
            "Run 'pnpm --filter isched-ui run build' first.",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"Scanning {DIST_DIR} ...")
    assets = []
    used_names: set[str] = set()

    for url_path, file_path, data in collect_assets(DIST_DIR):
        check_compressed_size(url_path, data)

        etag = compute_etag(data)
        mime = MIME_MAP.get(file_path.suffix.lower(), "application/octet-stream")

        # Build unique C identifier
        c_name = to_c_identifier(url_path)
        # Deduplicate identifiers
        base = c_name
        i = 0
        while c_name in used_names:
            i += 1
            c_name = f"{base}_{i}"
        used_names.add(c_name)

        compressed_size = len(gzip.compress(data, compresslevel=9))
        print(
            f"  {url_path:<55} {len(data):>8} bytes  "
            f"(gz {compressed_size:>7} bytes)  etag={etag}"
        )
        assets.append((url_path, c_name, mime, etag, data))

    header = generate_header(assets)

    OUTPUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_HEADER.write_text(header, encoding="utf-8")
    print(f"\nWrote {OUTPUT_HEADER}  ({len(assets)} assets)")


if __name__ == "__main__":
    main()
