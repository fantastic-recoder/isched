# SPDX-License-Identifier: MPL-2.0
"""Opt-in clean target-container runtime installation tests.

Normal package-suite runs are host-safe and skip container integrations. The
expected helper interface is `verify_runtime.py --target TARGET --package FILE`;
the helper is responsible for a fresh target-release container, unprivileged
startup, loader/GraphQL/UI/asset checks, bounded waits, and owned-resource
cleanup. The package path is supplied per target through the environment.
"""
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
VERIFIER = ROOT / "packaging/scripts/verify_runtime.py"
TARGETS = ("ubuntu-26.04", "fedora-45")


class RuntimeInstallTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get("ISCHED_RUN_PACKAGE_CONTAINERS") != "1":
            raise unittest.SkipTest(
                "set ISCHED_RUN_PACKAGE_CONTAINERS=1 to enable target-container tests"
            )
        if not VERIFIER.is_file():
            raise unittest.SkipTest("runtime verification helper is not implemented yet")

    def test_runtime_package_installs_and_serves_health_ui_and_asset(self):
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
                    ],
                    cwd=ROOT,
                    capture_output=True,
                    text=True,
                    timeout=300,
                    check=False,
                )
                self.assertEqual(
                    result.returncode,
                    0,
                    f"{target} runtime verification failed:\n{result.stdout}\n{result.stderr}",
                )

    def _package_for(self, target):
        variable = "ISCHED_RUNTIME_PACKAGE_" + target.upper().replace("-", "_").replace(".", "_")
        value = os.environ.get(variable)
        if not value:
            self.skipTest(f"set {variable} to the target-native runtime package")
        package = Path(value).resolve()
        if not package.is_file():
            self.fail(f"{variable} does not name a package file: {package}")
        return package


class RuntimeFailureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not VERIFIER.is_file():
            raise unittest.SkipTest("runtime verification helper is not implemented yet")
        spec = importlib.util.spec_from_file_location("verify_runtime_tested", VERIFIER)
        cls.verifier = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.verifier
        spec.loader.exec_module(cls.verifier)

    def test_mocked_runtime_failures_return_nonzero_with_target_and_stage(self):
        for target in TARGETS:
            for stage in ("install", "loader", "start", "health", "ui", "asset"):
                with self.subTest(target=target, stage=stage):
                    failure = subprocess.CalledProcessError(
                        23,
                        ["container-operation"],
                        stderr=f"target={target} stage={stage}: injected failure",
                    )
                    stdout = io.StringIO()
                    stderr = io.StringIO()
                    with patch.object(sys, "argv", [
                        "verify_runtime.py",
                        "--target",
                        target,
                        "--package",
                        str(FIXTURE_PACKAGE),
                    ]), patch.object(
                        self.verifier.subprocess, "run", side_effect=failure
                    ), contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                        try:
                            result = self.verifier.main()
                        except (SystemExit, subprocess.CalledProcessError) as error:
                            result = getattr(error, "code", 1) or 1
                    self.assertNotEqual(result, 0)
                    diagnostic = stdout.getvalue() + stderr.getvalue()
                    self.assertIn(target, diagnostic)
                    self.assertIn(stage, diagnostic)


FIXTURE_PACKAGE = (
    Path(__file__).parent
    / "fixtures/native/wrong-internal-format-deb/isched-runtime.deb"
)

if __name__ == "__main__":
    unittest.main()
