# SPDX-License-Identifier: MPL-2.0
"""External SDK consumer fixture and opt-in target-container integration tests."""
import os
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
CONSUMER = Path(__file__).parent / "sdk-consumer"
VERIFIER = ROOT / "packaging/scripts/verify_sdk.py"
TARGETS = ("ubuntu-26.04", "fedora-45")


class SdkInstallTests(unittest.TestCase):
    def test_consumer_requires_installed_config_and_calls_out_of_line_symbol(self):
        cmake = (CONSUMER / "CMakeLists.txt").read_text()
        source = (CONSUMER / "main.cpp").read_text()
        self.assertIn("find_package(isched CONFIG REQUIRED)", cmake)
        self.assertIn("isched::isched", cmake)
        self.assertIn("#include <isched/isched.hpp>", source)
        self.assertIn("is_lifecycle_trace_enabled()", source)
        self.assertIn("set_lifecycle_trace(true)", source)

    def _require_container_test(self):
        if os.environ.get("ISCHED_RUN_PACKAGE_CONTAINERS") != "1":
            self.skipTest(
                "set ISCHED_RUN_PACKAGE_CONTAINERS=1 to enable target-container tests"
            )
        if not VERIFIER.is_file():
            self.skipTest("SDK verification helper is not implemented yet")

    def test_developer_package_compiles_links_and_runs_without_runtime_package(self):
        self._require_container_test()
        for target in TARGETS:
            with self.subTest(target=target):
                package = self._package_for(target)
                result = subprocess.run(
                    [
                        sys.executable,
                        str(VERIFIER),
                        "--target",
                        target,
                        "--package",
                        str(package),
                        "--consumer",
                        str(CONSUMER),
                    ],
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                    timeout=600,
                    check=False,
                )
                self.assertEqual(
                    result.returncode,
                    0,
                    f"{target} SDK verification failed:\n{result.stdout}\n{result.stderr}",
                )

    def _package_for(self, target):
        variable = "ISCHED_DEVEL_PACKAGE_" + target.upper().replace("-", "_").replace(".", "_")
        value = os.environ.get(variable)
        if not value:
            self.skipTest(f"set {variable} to the target-native developer package")
        package = Path(value).resolve()
        if not package.is_file():
            self.fail(f"{variable} does not name a package file: {package}")
        return package


class SdkFailureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not VERIFIER.is_file():
            raise unittest.SkipTest("SDK verification helper is not implemented yet")
        spec = importlib.util.spec_from_file_location("verify_sdk_tested", VERIFIER)
        cls.verifier = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.verifier
        spec.loader.exec_module(cls.verifier)

    def test_mocked_consumer_failure_is_nonzero_and_target_attributed(self):
        for target in TARGETS:
            failure = subprocess.CalledProcessError(
                29,
                ["consumer-build"],
                stderr=f"target={target} stage=consumer-build: injected failure",
            )
            stdout = io.StringIO()
            stderr = io.StringIO()
            with patch.object(sys, "argv", [
                "verify_sdk.py",
                "--target",
                target,
                "--package",
                str(FIXTURE_PACKAGE),
                "--consumer",
                str(CONSUMER),
            ]), patch.object(
                self.verifier.shutil, "which", return_value="/usr/bin/docker"
            ), patch.object(
                self.verifier.subprocess, "run", side_effect=failure
            ), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                try:
                    result = self.verifier.main()
                except (SystemExit, subprocess.CalledProcessError) as error:
                    result = getattr(error, "code", 1) or 1
            self.assertNotEqual(result, 0)
            diagnostic = stdout.getvalue() + stderr.getvalue()
            self.assertIn(target, diagnostic)
            self.assertIn("consumer-build", diagnostic)


FIXTURE_PACKAGE = (
    Path(__file__).parent
    / "fixtures/native/wrong-internal-format-rpm/isched-devel.rpm"
)

if __name__ == "__main__":
    unittest.main()
