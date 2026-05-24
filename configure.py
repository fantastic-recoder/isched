#!/usr/bin/env python3
"""
isched build configurator.

Runs ``conan install`` followed by ``cmake`` configure for one or more build
types. By default it produces both Debug and Release trees:

    cmake-build-debug/   (Debug)
    cmake-build-release/ (Release)

Usage::

    python3 configure.py                  # both Debug and Release: clean + configure + build
    python3 configure.py debug            # Debug only
    python3 configure.py release          # Release only
    python3 configure.py --no-build       # configure only, don't build
    python3 configure.py --no-clean debug # incremental reconfigure + build
    python3 configure.py --jobs 8         # parallel build with 8 jobs

Exit code is non-zero if any step fails. Each step's stdout/stderr is streamed
to the terminal in real time.
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

# --------------------------------------------------------------------------- #
# Configuration
# --------------------------------------------------------------------------- #

REPO_ROOT = Path(__file__).resolve().parent

# Map of short name -> (build directory, CMake build-type string)
BUILD_PROFILES: dict[str, tuple[Path, str]] = {
    "debug":   (REPO_ROOT / "cmake-build-debug",   "Debug"),
    "release": (REPO_ROOT / "cmake-build-release", "Release"),
}

IS_WINDOWS = sys.platform.startswith("win")


@dataclass
class Options:
    profiles: list[str]
    clean: bool
    build: bool
    jobs: int | None
    verbose: bool


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #

def log(msg: str) -> None:
    print(f"[configure] {msg}", flush=True)


def run(cmd: list[str], *, cwd: Path | None = None) -> None:
    """Run ``cmd`` and raise SystemExit on non-zero exit."""
    pretty = " ".join(cmd)
    log(f"$ {pretty}")
    try:
        subprocess.run(cmd, cwd=cwd, check=True)
    except FileNotFoundError as exc:
        sys.exit(f"[configure] ERROR: tool not found: {exc.filename}")
    except subprocess.CalledProcessError as exc:
        sys.exit(f"[configure] ERROR: command failed (exit {exc.returncode}): {pretty}")


def ensure_tool(name: str) -> None:
    if shutil.which(name) is None:
        sys.exit(f"[configure] ERROR: required tool '{name}' not found in PATH")


def detect_generator() -> list[str]:
    """Return CMake -G arguments. Prefer Ninja if available."""
    if shutil.which("ninja"):
        return ["-G", "Ninja"]
    # Fall back to platform default (NMake on Windows, Make on Unix).
    return []


def conan_toolchain_path(build_dir: Path) -> Path:
    """Locate ``conan_toolchain.cmake`` written by ``conan install -of``."""
    # Conan 2 places it at the root of the output folder on Unix and inside
    # build/<config>/generators on some Windows MSVC profiles.
    candidates = [
        build_dir / "conan_toolchain.cmake",
        build_dir / "build" / "generators" / "conan_toolchain.cmake",
        build_dir / "build" / "Debug"   / "generators" / "conan_toolchain.cmake",
        build_dir / "build" / "Release" / "generators" / "conan_toolchain.cmake",
    ]
    for c in candidates:
        if c.is_file():
            return c
    sys.exit(
        f"[configure] ERROR: could not locate conan_toolchain.cmake under {build_dir}\n"
        f"  searched: {[str(c) for c in candidates]}"
    )


# --------------------------------------------------------------------------- #
# Build steps
# --------------------------------------------------------------------------- #

def conan_install(build_dir: Path, build_type: str) -> None:
    run([
        "conan", "install", str(REPO_ROOT),
        "-of", str(build_dir),
        "-s", f"build_type={build_type}",
        "--build=missing",
    ])


def cmake_configure(build_dir: Path, build_type: str) -> None:
    toolchain = conan_toolchain_path(build_dir)
    cmd = [
        "cmake", str(REPO_ROOT),
        "-B", str(build_dir),
        *detect_generator(),
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    run(cmd)


def cmake_build(build_dir: Path, build_type: str, jobs: int | None) -> None:
    cmd = ["cmake", "--build", str(build_dir), "--config", build_type]
    if jobs is not None:
        cmd += ["--parallel", str(jobs)]
    else:
        cmd += ["--parallel"]
    run(cmd)


def configure_profile(name: str, opts: Options) -> None:
    build_dir, build_type = BUILD_PROFILES[name]
    log(f"=== Profile: {name} ({build_type}) -> {build_dir} ===")

    if opts.clean and build_dir.exists():
        log(f"cleaning existing build dir: {build_dir}")
        shutil.rmtree(build_dir)

    build_dir.mkdir(parents=True, exist_ok=True)

    conan_install(build_dir, build_type)
    cmake_configure(build_dir, build_type)
    if opts.build:
        cmake_build(build_dir, build_type, opts.jobs)


# --------------------------------------------------------------------------- #
# Argument parsing
# --------------------------------------------------------------------------- #

def parse_args(argv: list[str]) -> Options:
    p = argparse.ArgumentParser(
        prog="configure.py",
        description="Configure (and optionally build) the isched project.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument(
        "profiles",
        nargs="*",
        choices=sorted(BUILD_PROFILES.keys()) + [],  # allow zero
        help="Build profiles to configure. Default: debug release.",
    )
    p.add_argument(
        "--no-clean", dest="clean", action="store_false",
        help="Keep the existing build directory (default is to wipe it).",
    )
    p.set_defaults(clean=True)
    p.add_argument(
        "--no-build", dest="build", action="store_false",
        help="Skip the 'cmake --build' step (default is to build).",
    )
    p.set_defaults(build=True)
    p.add_argument(
        "-j", "--jobs", type=int, default=None,
        help="Parallel build jobs (default: detected by CMake).",
    )
    p.add_argument(
        "--verbose", action="store_true",
        help="Print extra diagnostic information.",
    )
    args = p.parse_args(argv)

    profiles = args.profiles or list(BUILD_PROFILES.keys())
    return Options(
        profiles=profiles,
        clean=args.clean,
        build=args.build,
        jobs=args.jobs,
        verbose=args.verbose,
    )


# --------------------------------------------------------------------------- #
# Entry point
# --------------------------------------------------------------------------- #

def main(argv: list[str] | None = None) -> int:
    opts = parse_args(sys.argv[1:] if argv is None else argv)

    ensure_tool("conan")
    ensure_tool("cmake")

    # Best-effort: detect a default Conan profile on first use; ignore errors.
    try:
        subprocess.run(["conan", "profile", "detect", "--exist-ok"],
                       check=False)
    except FileNotFoundError:
        pass

    for profile in opts.profiles:
        configure_profile(profile, opts)

    log("All profiles configured successfully.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
