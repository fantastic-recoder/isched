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
import tempfile
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


class FrontendOutputTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location(
            "build_target_tested", ROOT / "packaging/scripts/build_target.py"
        )
        cls.builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.builder)

    def test_application_builder_output_layout_and_required_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            outputs = (
                root / "src/ui/dist/isched-ui/browser/index.html",
                root / "src/ui/dist/isched-ui/3rdpartylicenses.txt",
                root / "src/main/cpp/isched/backend/isched_ui_assets.hpp",
            )
            for path in outputs:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("generated content\n")
            self.assertEqual(self.builder.frontend_outputs(root), outputs)
            for path in outputs:
                for content in (None, ""):
                    with self.subTest(path=path, content=content):
                        if content is None:
                            path.unlink()
                        else:
                            path.write_text(content)
                        with self.assertRaisesRegex(ValueError, "non-empty"):
                            self.builder.frontend_outputs(root)
                        path.write_text("generated content\n")


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
                            self.builder, "checkout_version",
                            return_value=f"0.0.0+git.{REVISION[:12]}",
                        ), \
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
