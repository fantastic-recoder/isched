# SPDX-License-Identifier: MPL-2.0
"""Static workflow checks for safe, uniquely named package downloads."""

from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/cmake-multi-platform.yml"
IDENTITY = ROOT / "packaging/scripts/package_identity.py"
UPLOAD_SHA = "ea165f8d65b6e75b540449e92b4886f43607fa02"


class WorkflowContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_only_pull_requests_and_master_pushes_with_read_only_permissions(self):
        self.assertRegex(self.workflow, r"(?m)^  pull_request:\s*$")
        self.assertRegex(self.workflow, r"(?m)^  push:\s*$")
        self.assertRegex(self.workflow, r"(?m)^    branches: \[master\]$")
        self.assertRegex(self.workflow, r"(?m)^permissions:\n  contents: read$")
        self.assertNotRegex(self.workflow, r"(?m)^\s+[A-Za-z_-]+:\s+write(?:$|\s)")

    def test_exact_initial_matrix_is_independent_and_fail_fast_is_disabled(self):
        result = subprocess.run(
            [sys.executable, str(IDENTITY), "--matrix"], cwd=ROOT,
            capture_output=True, text=True, timeout=30, check=True,
        )
        import json

        self.assertEqual({item["id"] for item in json.loads(result.stdout)["include"]},
                         {"ubuntu-26.04", "fedora-45"})
        self.assertRegex(self.workflow, r"(?m)^\s+fail-fast: false$")
        self.assertRegex(self.workflow, r"(?m)^\s+matrix: \$\{\{ fromJSON\(needs\.target-matrix\.outputs\.matrix\) \}\}$")

    def test_both_role_staging_precedes_exactly_two_sha_pinned_uploads(self):
        staging_at = self.workflow.index("name: Validate and stage both package roles")
        upload_positions = [match.start() for match in re.finditer(
            r"(?m)^\s+uses: actions/upload-artifact@", self.workflow,
        )]
        self.assertEqual(len(upload_positions), 2)
        self.assertTrue(all(staging_at < position for position in upload_positions))
        self.assertEqual(self.workflow.count(f"actions/upload-artifact@{UPLOAD_SHA}"), 2)
        self.assertEqual(self.workflow.count("if-no-files-found: error"), 2)
        self.assertIn("--github-output \"$GITHUB_OUTPUT\"", self.workflow)
        self.assertIn("steps.artifacts.outputs.runtime_artifact_name", self.workflow)
        self.assertIn("steps.artifacts.outputs.developer_artifact_name", self.workflow)
        self.assertIn("dist/artifacts/${{ matrix.id }}/runtime/", self.workflow)
        self.assertIn("dist/artifacts/${{ matrix.id }}/developer/", self.workflow)

    def test_no_continue_on_error_privileged_artifact_consumer_or_release_publication(self):
        forbidden = (
            "continue-on-error", "pull_request_target", "actions/download-artifact",
            "action-gh-release", "softprops/action-gh-release", "gh release",
            "release:", "contents: write", "permissions: write-all",
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, self.workflow)

    def test_build_logs_are_unbuffered_traced_and_not_clipped_by_buildkit(self):
        self.assertIn("env.BUILDKIT_STEP_LOG_MAX_SIZE=-1", self.workflow)
        self.assertIn("env.BUILDKIT_STEP_LOG_MAX_SPEED=-1", self.workflow)
        self.assertIn("docker buildx inspect --bootstrap", self.workflow)
        self.assertIn("python3 -u packaging/scripts/build_local.py", self.workflow)
        self.assertEqual(self.workflow.count("set -euxo pipefail"), 4)
        for distribution in ("ubuntu", "fedora"):
            dockerfile = (ROOT / "packaging/docker" / distribution / "Dockerfile").read_text()
            with self.subTest(distribution=distribution):
                self.assertEqual(dockerfile.count('SHELL ["/bin/sh", "-exc"]'), 3)
                self.assertEqual(dockerfile.count("ENV PYTHONUNBUFFERED=1"), 3)
                self.assertEqual(dockerfile.count("packaging/scripts/logged_process.py"), 2)


if __name__ == "__main__":
    unittest.main()
