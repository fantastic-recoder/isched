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
import json
import os
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
    lockfile: Path | None = None
    host_profile: Path | None = None
    build_profile: Path | None = None
    cmake_args: tuple[str, ...] = ()
    conan_build_tools: bool = False


# --------------------------------------------------------------------------- #
# Helpers
# --------------------------------------------------------------------------- #

def log(msg: str) -> None:
    print(f"[configure] {msg}", flush=True)


def run(cmd: list[str], *, cwd: Path | None = None,
        env: dict[str, str] | None = None) -> None:
    """Run ``cmd`` and raise SystemExit on non-zero exit."""
    pretty = " ".join(cmd)
    log(f"$ {pretty}")
    try:
        subprocess.run(cmd, cwd=cwd, env=env, check=True)
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

def conan_install(build_dir: Path, build_type: str, opts: Options | None = None) -> None:
    cmd = [
        "conan", "install", str(REPO_ROOT),
        "-of", str(build_dir),
        "-s", f"build_type={build_type}",
        "--build=missing",
    ]
    env = None
    if opts:
        for flag, value in [
            ("--lockfile", opts.lockfile), ("--profile:host", opts.host_profile),
            ("--profile:build", opts.build_profile),
        ]:
            if value is not None:
                cmd.extend([flag, str(value)])
        if opts.conan_build_tools:
            # Root tool_requires do not automatically enter every transitive
            # recipe's environment (e.g. bzip2 assumes cmake exists). Bootstrap
            # only the same locked tools before installing the library graph.
            bootstrap = build_dir / "tool-bootstrap"
            bootstrap.mkdir(parents=True, exist_ok=True)
            run([
                "conan", "install", "--tool-requires=cmake/3.22.6",
                "--tool-requires=ninja/1.12.1", "--build=missing",
                "--lockfile", str(opts.lockfile),
                "--profile:host", str(opts.host_profile),
                "--profile:build", str(opts.build_profile),
                "-of", str(bootstrap), "--format=json",
                f"--out-file={bootstrap / 'conan-install.json'}",
            ])
            env = conan_tool_environment(bootstrap)
            cmd.extend(["--format=json", f"--out-file={build_dir / 'conan-install.json'}"])
    if env is None:
        run(cmd)
    else:
        run(cmd, env=env)


def conan_tool_environment(build_dir: Path) -> dict[str, str]:
    """Activate the locked CMake/Ninja executables, without evaluating shell files.

    Conan activates dependency-build environments while installing. For the
    project's subsequent CMake invocation, use the installed build-context tool
    folders from Conan's machine-readable install result. Never fall back to
    the host CMake/Ninja when this opt-in mode is requested.
    """
    try:
        graph = json.loads((build_dir / "conan-install.json").read_text())
        nodes = graph["graph"]["nodes"].values()
        paths = []
        for name, version in (("cmake", "3.22.6"), ("ninja", "1.12.1")):
            matches = [node for node in nodes
                       if (node.get("ref") or "").startswith(f"{name}/{version}#")
                       and node.get("context") == "build"]
            if len(matches) != 1 or not matches[0].get("package_folder"):
                sys.exit(f"[configure] ERROR: installed pinned {name}/{version} not found")
            directory = Path(matches[0]["package_folder"]) / "bin"
            executable = directory / (name + (".exe" if IS_WINDOWS else ""))
            if not executable.is_file():
                sys.exit(f"[configure] ERROR: missing Conan tool executable: {executable}")
            paths.append(str(directory))
        return dict(os.environ, PATH=os.pathsep.join([*paths, os.environ.get("PATH", "")]))
    except (OSError, ValueError, KeyError, TypeError) as exc:
        sys.exit(f"[configure] ERROR: invalid Conan tool activation metadata: {exc}")


def cmake_configure(build_dir: Path, build_type: str, *,
                    extra_args: tuple[str, ...] = (),
                    env: dict[str, str] | None = None) -> None:
    toolchain = conan_toolchain_path(build_dir)
    cmake = shutil.which("cmake", path=env["PATH"]) if env is not None else "cmake"
    generator = ["-G", "Ninja"] if env is not None else detect_generator()
    cmd = [
        cmake, str(REPO_ROOT),
        "-B", str(build_dir),
        *generator,
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    if env is not None:
        cmd.append(f"-DCMAKE_MAKE_PROGRAM={shutil.which('ninja', path=env['PATH'])}")
    cmd.extend(extra_args)
    run(cmd, env=env)


def cmake_build(build_dir: Path, build_type: str, jobs: int | None, *,
                env: dict[str, str] | None = None) -> None:
    cmake = shutil.which("cmake", path=env["PATH"]) if env is not None else "cmake"
    cmd = [cmake, "--build", str(build_dir), "--config", build_type]
    if jobs is not None:
        cmd += ["--parallel", str(jobs)]
    else:
        cmd += ["--parallel"]
    run(cmd, env=env)


def configure_profile(name: str, opts: Options) -> None:
    build_dir, build_type = BUILD_PROFILES[name]
    log(f"=== Profile: {name} ({build_type}) -> {build_dir} ===")

    if opts.clean and build_dir.exists():
        log(f"cleaning existing build dir: {build_dir}")
        shutil.rmtree(build_dir)

    build_dir.mkdir(parents=True, exist_ok=True)

    conan_install(build_dir, build_type, opts)
    env = conan_tool_environment(build_dir) if opts.conan_build_tools else None
    cmake_configure(build_dir, build_type, extra_args=opts.cmake_args, env=env)
    if opts.build:
        cmake_build(build_dir, build_type, opts.jobs, env=env)


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
    p.add_argument("--conan-lockfile", type=Path)
    p.add_argument("--conan-host-profile", type=Path)
    p.add_argument("--conan-build-profile", type=Path)
    p.add_argument("--cmake-arg", action="append", default=[],
                   help="Additional CMake argument; use --cmake-arg=-DNAME=VALUE")
    p.add_argument("--conan-build-tools", action="store_true",
                   help="Require installed Conan CMake 3.22.6 and Ninja 1.12.1")
    args = p.parse_args(argv)
    for path in (args.conan_lockfile, args.conan_host_profile, args.conan_build_profile):
        if path is not None and not path.is_file():
            p.error(f"Conan input does not exist: {path}")
    if args.conan_build_tools and not all(
        (args.conan_lockfile, args.conan_host_profile, args.conan_build_profile)
    ):
        p.error("--conan-build-tools requires an explicit lockfile and both profiles")

    profiles = args.profiles or list(BUILD_PROFILES.keys())
    return Options(
        profiles=profiles,
        clean=args.clean,
        build=args.build,
        jobs=args.jobs,
        verbose=args.verbose,
        lockfile=args.conan_lockfile.resolve() if args.conan_lockfile else None,
        host_profile=args.conan_host_profile.resolve() if args.conan_host_profile else None,
        build_profile=args.conan_build_profile.resolve() if args.conan_build_profile else None,
        cmake_args=tuple(args.cmake_arg),
        conan_build_tools=args.conan_build_tools,
    )


# --------------------------------------------------------------------------- #
# Entry point
# --------------------------------------------------------------------------- #

def main(argv: list[str] | None = None) -> int:
    opts = parse_args(sys.argv[1:] if argv is None else argv)

    ensure_tool("conan")
    if not opts.conan_build_tools:
        ensure_tool("cmake")

    # Best-effort: detect a default Conan profile on first use; ignore errors.
    if opts.host_profile is None and opts.build_profile is None:
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
