# SPDX-License-Identifier: MPL-2.0
"""Verify optional packaging inputs do not change ordinary configure behavior."""
import importlib.util
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("isched_configure", ROOT / "configure.py")
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)


class ConfigureTests(unittest.TestCase):
    def test_cmake_frontend_gate_uses_angular_output_layout_and_checks_freshness(self):
        source = (ROOT / "CMakeLists.txt").read_text()
        gate = source.split("find_package(Python3 REQUIRED COMPONENTS Interpreter)", 1)[1]
        gate = gate.split("file(GLOB_RECURSE UI_SRC_FILES", 1)[0]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            outputs = {
                "index_sha256": root / "src/ui/dist/isched-ui/browser/index.html",
                "notices_sha256": root / "src/ui/dist/isched-ui/3rdpartylicenses.txt",
                "embedded_header_sha256": root / "src/main/cpp/isched/backend/isched_ui_assets.hpp",
            }
            evidence = {"schema_version": 1}
            for key, path in outputs.items():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fresh generated content\n")
                evidence[key] = hashlib.sha256(path.read_bytes()).hexdigest()
            (root / "evidence.json").write_text(json.dumps(evidence))
            (root / "identity.json").write_text("[]")
            script = root / "gate.cmake"
            script.write_text(
                'set(ISCHED_ENABLE_PACKAGING ON)\n'
                'set(PNPM_EXE "/pinned/pnpm")\n'
                f'set(ISCHED_SOURCE_REVISION "{"a" * 40}")\n'
                'set(ISCHED_PACKAGE_VERSION "1.0.0")\n'
                'set(ISCHED_PACKAGE_TARGET "ubuntu-26.04")\n'
                'set(ISCHED_PACKAGE_FORMAT "deb")\n'
                'set(ISCHED_PACKAGE_IDENTITY_FILE "${CMAKE_SOURCE_DIR}/identity.json")\n'
                'set(ISCHED_PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/out")\n'
                'set(ISCHED_PACKAGE_LICENSE_OUTPUT "${CMAKE_SOURCE_DIR}/licenses")\n'
                'set(ISCHED_FRONTEND_GATE_FILE "${CMAKE_SOURCE_DIR}/evidence.json")\n'
                + gate
            )
            for scenario in ("fresh", "stale", "missing"):
                with self.subTest(scenario=scenario):
                    notices = outputs["notices_sha256"]
                    if scenario == "stale":
                        notices.write_text("changed after evidence was generated\n")
                    elif scenario == "missing":
                        notices.unlink()
                    result = subprocess.run(
                        ["cmake", "-P", str(script)], cwd=root,
                        capture_output=True, text=True, timeout=30,
                    )
                    if scenario == "fresh":
                        self.assertEqual(result.returncode, 0, result.stderr)
                    else:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn(
                            "frontend evidence does not match" if scenario == "stale"
                            else "requires freshly generated frontend input",
                            result.stderr,
                        )

    def test_ordinary_profiles_unchanged(self):
        for profile, build_type in (("debug", "Debug"), ("release", "Release")):
            with self.subTest(profile=profile):
                opts = module.parse_args([profile, "--no-clean", "--no-build"])
                self.assertEqual(opts.profiles, [profile])
                self.assertFalse(opts.clean)
                self.assertFalse(opts.build)
                self.assertFalse(opts.conan_build_tools)
                with patch.object(module, "run") as run:
                    module.conan_install(Path("/tmp/build"), build_type, opts)
                self.assertEqual(run.call_args.args[0],
                                 ["conan", "install", str(ROOT), "-of", "/tmp/build",
                                  "-s", f"build_type={build_type}", "--build=missing"])

    def test_opt_in_requires_all_explicit_inputs(self):
        with self.assertRaises(SystemExit):
            module.parse_args(["release", "--conan-build-tools"])
        with self.assertRaises(SystemExit):
            module.parse_args(["release", "--conan-lockfile=/does/not/exist"])

    def test_pinned_input_arguments_and_no_shell(self):
        opts = module.parse_args([
            "release", "--no-clean", "--conan-build-tools",
            "--conan-lockfile", str(ROOT / "conan.lock"),
            "--conan-host-profile", str(ROOT / "packaging/conan/profiles/ubuntu-26.04"),
            "--conan-build-profile", str(ROOT / "packaging/conan/profiles/ubuntu-26.04"),
            "--cmake-arg=-DCMAKE_CXX_STANDARD=23",
        ])
        self.assertEqual(opts.cmake_args, ("-DCMAKE_CXX_STANDARD=23",))
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(module, "run") as run, patch.object(
                module, "conan_tool_environment", return_value={"PATH": "/pinned/bin"}
            ):
                module.conan_install(Path(directory), "Release", opts)
            self.assertIn("--tool-requires=cmake/3.22.6", run.call_args_list[0].args[0])
            self.assertIn("--profile:host", run.call_args.args[0])
            self.assertIn("--profile:build", run.call_args.args[0])
            self.assertIn(f"--out-file={directory}/conan-install.json", run.call_args.args[0])
            self.assertEqual(run.call_args.kwargs["env"]["PATH"], "/pinned/bin")

    def test_activation_rejects_missing_or_wrong_tools(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            with self.assertRaises(SystemExit):
                module.conan_tool_environment(directory)
            nodes = {}
            for name, version in [("cmake", "3.22.6"), ("ninja", "1.12.1")]:
                folder = directory / name
                (folder / "bin").mkdir(parents=True)
                executable = folder / "bin" / name
                executable.touch()
                executable.chmod(0o755)
                nodes[name] = {"ref": f"{name}/{version}#abcd", "context": "build",
                               "package_folder": str(folder)}
            graph = directory / "conan-install.json"
            graph.write_text(json.dumps({"graph": {"nodes": nodes}}))
            env = module.conan_tool_environment(directory)
            self.assertTrue(env["PATH"].startswith(str(directory / "cmake/bin")))
            nodes["cmake"]["context"] = "host"
            graph.write_text(json.dumps({"graph": {"nodes": nodes}}))
            with self.assertRaises(SystemExit):
                module.conan_tool_environment(directory)

    def test_command_failures_propagate(self):
        import subprocess
        with patch.object(module.subprocess, "run",
                          side_effect=subprocess.CalledProcessError(19, ["cmake"])):
            with self.assertRaises(SystemExit):
                module.run(["cmake"])

    def test_pinned_conan_failure_does_not_fall_back_to_host_cmake(self):
        opts = module.parse_args([
            "release", "--no-clean", "--no-build", "--conan-build-tools",
            "--conan-lockfile", str(ROOT / "conan.lock"),
            "--conan-host-profile", str(ROOT / "packaging/conan/profiles/ubuntu-26.04"),
            "--conan-build-profile", str(ROOT / "packaging/conan/profiles/ubuntu-26.04"),
        ])
        with tempfile.TemporaryDirectory() as temporary:
            build_dir = Path(temporary) / "build"
            with patch.dict(module.BUILD_PROFILES, {"release": (build_dir, "Release")}), \
                    patch.object(module, "run", side_effect=SystemExit("Conan failed")) as run, \
                    patch.object(module, "cmake_configure") as configure:
                with self.assertRaisesRegex(SystemExit, "Conan failed"):
                    module.configure_profile("release", opts)
            self.assertEqual(run.call_count, 1)
            configure.assert_not_called()


if __name__ == "__main__":
    unittest.main()
