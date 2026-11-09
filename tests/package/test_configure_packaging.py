# SPDX-License-Identifier: MPL-2.0
"""Verify optional packaging inputs do not change ordinary configure behavior."""
import importlib.util
import json
from pathlib import Path
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
