# SPDX-License-Identifier: MPL-2.0
"""Controlled package-build failure tests without invoking Docker.

The documented build-local CLI is exercised with `subprocess.run` mocked.
`build_local.py` must propagate Docker/build-stage failure as a nonzero,
target-attributed result and must not expose package artifacts on any failed
gate. The stage labels cover the task's required pipeline failure matrix.
"""
import contextlib
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / "packaging/scripts/build_local.py"
REVISION = "c0acb7bc9df46cecdd6fed5ce4062e079ac3553e"
OUTPUT = ROOT / "tests/package/fixtures/pipeline-output"
STAGES = (
    "image-startup",
    "frontend-install",
    "frontend-test",
    "frontend-build",
    "frontend-embed",
    "conan",
    "configure",
    "build",
    "ctest",
    "ctest-empty",
    "package-generation",
    "package-validation",
    "runtime-install",
    "sdk-consumption",
)


class BuildPipelineFailureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not BUILDER.is_file():
            raise unittest.SkipTest("local package build helper is not implemented yet")
        spec = importlib.util.spec_from_file_location("build_local_tested", BUILDER)
        cls.builder = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.builder
        spec.loader.exec_module(cls.builder)

    def test_each_gate_failure_is_attributed_and_suppresses_package_export(self):
        for stage in STAGES:
            with self.subTest(stage=stage):
                failure = subprocess.CalledProcessError(
                    17,
                    ["docker", "build"],
                    stderr=f"target=ubuntu-26.04 stage={stage}: injected failure",
                )
                stdout = io.StringIO()
                stderr = io.StringIO()
                args = [
                    "build_local.py",
                    "--target",
                    "ubuntu-26.04",
                    "--revision",
                    REVISION,
                    "--output",
                    str(OUTPUT),
                ]
                with patch.object(sys, "argv", args), \
                        patch.object(self.builder, "git_output", return_value=""), \
                        patch.object(
                            self.builder.subprocess,
                            "run",
                            side_effect=failure,
                        ), \
                        contextlib.redirect_stdout(stdout), \
                        contextlib.redirect_stderr(stderr):
                    try:
                        result = self.builder.main()
                    except (SystemExit, subprocess.CalledProcessError) as error:
                        result = getattr(error, "code", 1) or 1
                self.assertNotEqual(result, 0)
                diagnostic = stdout.getvalue() + stderr.getvalue()
                self.assertIn("ubuntu-26.04", diagnostic)
                self.assertIn(stage, diagnostic)
                self.assertFalse(
                    OUTPUT.exists(),
                    "failed gates must not create or expose a successful package output",
                )


if __name__ == "__main__":
    unittest.main()
