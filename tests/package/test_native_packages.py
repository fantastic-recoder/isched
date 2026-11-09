# SPDX-License-Identifier: MPL-2.0
"""Native package contract tests.

The validator CLI is the contract boundary: it must inspect package contents
and native metadata, not infer validity from a filename suffix. These tests
never install a package on the host. The helper is supplied by the later
packaging implementation task.
"""
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = ROOT / "packaging/scripts/validate_packages.py"
REVISION = "c0acb7bc9df46cecdd6fed5ce4062e079ac3553e"
FIXTURES = Path(__file__).parent / "fixtures/native"


class NativePackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not VALIDATOR.is_file():
            raise unittest.SkipTest("native package validator is not implemented yet")

    def test_initial_targets_have_native_formats_and_architectures(self):
        targets = json.loads((ROOT / "packaging/targets.json").read_text())["targets"]
        self.assertEqual(
            {target["id"]: (target["package_format"], target["native_architecture"])
             for target in targets},
            {"ubuntu-26.04": ("deb", "amd64"), "fedora-45": ("rpm", "x86_64")},
        )

    def test_wrong_internal_format_is_rejected_despite_matching_suffix(self):
        for target, fixture in (
            ("ubuntu-26.04", "wrong-internal-format-deb"),
            ("fedora-45", "wrong-internal-format-rpm"),
        ):
            with self.subTest(target=target):
                result = self._validate(target, FIXTURES / fixture)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(target, result.stdout + result.stderr)

    def test_identity_payload_integrity_and_dependency_violations_fail(self):
        cases = (
            "wrong-role",
            "wrong-version",
            "wrong-revision",
            "wrong-architecture",
            "wrong-release",
            "missing-payload",
            "missing-notices",
            "bad-checksum",
            "missing-dependencies",
            "invented-runtime-dependency",
        )
        for case in cases:
            with self.subTest(case=case):
                result = self._validate(
                    "ubuntu-26.04", FIXTURES / case, expected_revision=REVISION
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(
                    result.stdout.strip() or result.stderr.strip(),
                    "validation failures must include an actionable diagnostic",
                )

    def test_missing_empty_duplicate_misnamed_and_state_contaminated_outputs_fail(self):
        cases = (
            "missing",
            "empty",
            "duplicate",
            "misnamed",
            "state-contaminated",
        )
        for case in cases:
            with self.subTest(case=case):
                result = self._validate(
                    "ubuntu-26.04", FIXTURES / case, expected_revision=REVISION
                )
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(
                    result.stdout.strip() or result.stderr.strip(),
                    "validation failures must include an actionable diagnostic",
                )

    def _validate(self, target, directory, expected_revision=REVISION):
        return subprocess.run(
            [
                sys.executable,
                str(VALIDATOR),
                "--target",
                target,
                "--revision",
                expected_revision,
                "--directory",
                str(directory),
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )


if __name__ == "__main__":
    unittest.main()
