#!/usr/bin/env python3
import argparse, os, re, subprocess, sys
from pathlib import Path
from collections import defaultdict

# Assume this script is placed in tools/ under repo root
ROOT = Path(__file__).resolve().parents[1]

EXTS = {".cpp", ".cc", ".c", ".hpp", ".hh", ".h", ".ixx"}
SEARCH_DIRS = ["src", "tests"]

def camel_to_snake(s: str) -> str:
    # Split acronym-then-proper-case and lower-to-upper boundaries
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", s)
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", s)
    # Normalize separators to underscores
    s = re.sub(r"[^a-zA-Z0-9]+", "_", s)
    return s.strip("_").lower()

def to_snake_case_filename(name: str) -> str:
    stem, ext = os.path.splitext(name)
    return f"{camel_to_snake(stem)}{ext.lower()}"

def find_git():
    try:
        subprocess.run(["git", "--version"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return True
    except Exception:
        return False

def is_case_only_change(old: Path, new: Path) -> bool:
    try:
        return old.resolve().parent == new.resolve().parent and old.name.lower() == new.name.lower() and old.name != new.name
    except Exception:
        return old.name.lower() == new.name.lower() and old.name != new.name

def collect_files():
    files = []
    for d in SEARCH_DIRS:
        p = ROOT / d
        if not p.exists():
            continue
        for f in p.rglob("*"):
            if f.is_file() and f.suffix in EXTS:
                files.append(f.relative_to(ROOT))
    return files

def build_rename_map(files):
    mapping = {}
    for rel in files:
        new_name = to_snake_case_filename(rel.name)
        if new_name != rel.name:
            mapping[str(rel)] = str(rel.with_name(new_name))
    # Detect collisions
    rev = defaultdict(list)
    for src, dst in mapping.items():
        rev[dst].append(src)
    collisions = {dst: srcs for dst, srcs in rev.items() if len(srcs) > 1}
    return mapping, collisions

def git_mv(src: Path, dst: Path):
    dst.parent.mkdir(parents=True, exist_ok=True)
    if is_case_only_change(src, dst):
        tmp = dst.with_name(dst.name + ".__tmp__")
        subprocess.run(["git", "mv", str(src), str(tmp)], check=True)
        subprocess.run(["git", "mv", str(tmp), str(dst)], check=True)
    else:
        subprocess.run(["git", "mv", str(src), str(dst)], check=True)

def fs_mv(src: Path, dst: Path):
    dst.parent.mkdir(parents=True, exist_ok=True)
    if is_case_only_change(src, dst):
        tmp = dst.with_name(dst.name + ".__tmp__")
        os.rename(src, tmp)
        os.rename(tmp, dst)
    else:
        os.rename(src, dst)

INCLUDE_RE = re.compile(r'(#[ \t]*include[ \t]*[<"])([^">]*?/)?([^/">]+)([">"])')

def update_includes_and_cmake(mapping):
    # Index by basename to update path-insensitive includes
    by_basename = {}
    for old, new in mapping.items():
        old_base = os.path.basename(old)
        new_base = os.path.basename(new)
        by_basename.setdefault(old_base, set()).add(new_base)

    files_to_scan = []
    for d in SEARCH_DIRS:
        p = ROOT / d
        if not p.exists():
            continue
        for f in p.rglob("*"):
            if not f.is_file():
                continue
            if f.suffix in EXTS or f.name == "CMakeLists.txt" or f.suffix == ".cmake":
                files_to_scan.append(f)

    def rewrite_includes(text: str) -> str:
        def repl(m):
            prefix, pathprefix, fname, suffix = m.groups()
            if fname in by_basename:
                # Assume 1-to-1 by basename after collision check
                newfname = list(by_basename[fname])[0]
                return f"{prefix}{pathprefix or ''}{newfname}{suffix}"
            return m.group(0)
        return INCLUDE_RE.sub(repl, text)

    def rewrite_cmake(text: str) -> str:
        # Replace exact filename matches inside quotes or bare tokens
        for old, new in mapping.items():
            old_base = os.path.basename(old)
            new_base = os.path.basename(new)
            # Replace full relative path if present
            text = text.replace(old, new)
            # Replace basenames when listed without path
            text = re.sub(rf'(?<![\w/.-]){re.escape(old_base)}(?![\w/.-])', new_base, text)
        return text

    changed = 0
    for f in files_to_scan:
        try:
            data = f.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue
        new_data = data
        if f.suffix in EXTS:
            new_data = rewrite_includes(new_data)
        if f.name == "CMakeLists.txt" or f.suffix == ".cmake":
            new_data = rewrite_cmake(new_data)
        if new_data != data:
            f.write_text(new_data, encoding="utf-8")
            changed += 1
    return changed

def main():
    ap = argparse.ArgumentParser(description="Rename C/C++ source/header files to snake_case and update references.")
    ap.add_argument("--apply", action="store_true", help="Perform renames; otherwise, dry-run")
    args = ap.parse_args()

    files = collect_files()
    mapping, collisions = build_rename_map(files)

    print(f"Found {len(files)} source/header files.")
    print(f"{len(mapping)} files need renaming to snake_case.")
    if collisions:
        print("Collision(s) detected after snake_case normalization:")
        for dst, srcs in collisions.items():
            print(f"  {dst}: {srcs}")
        print("Aborting. Decide unique names (e.g., suffix one with _impl or similar) before re-running.")
        sys.exit(1)

    if not mapping:
        print("Everything already in snake_case. Nothing to do.")
        return

    print("Planned renames:")
    for old, new in sorted(mapping.items()):
        print(f"  {old} -> {new}")

    if not args.apply:
        print("\nDry run only. Re-run with --apply to perform git mv and update references.")
        return

    use_git = find_git()
    for old, new in mapping.items():
        src = ROOT / old
        dst = ROOT / new
        if use_git:
            git_mv(src, dst)
        else:
            fs_mv(src, dst)

    changed = update_includes_and_cmake(mapping)
    print(f"Updated {changed} files with new include/CMake references.")

    # Sanity check: search for lingering old basenames
    lingering = []
    old_basenames = set(os.path.basename(o) for o in mapping.keys())
    for d in SEARCH_DIRS:
        p = ROOT / d
        if not p.exists():
            continue
        for f in p.rglob("*"):
            if f.is_file() and (f.suffix in EXTS or f.name == "CMakeLists.txt" or f.suffix == ".cmake"):
                try:
                    txt = f.read_text(encoding="utf-8", errors="ignore")
                except Exception:
                    continue
                for ob in old_basenames:
                    if ob in txt:
                        lingering.append((f, ob))
                        break
    if lingering:
        print("Warning: Some files may still reference old basenames:", file=sys.stderr)
        for f, ob in lingering[:20]:
            print(f"  {f}: contains {ob}", file=sys.stderr)

if __name__ == "__main__":
    main()
