# SPDX-License-Identifier: MPL-2.0
"""Foundation identity tests; no registry, Docker or installed application needed."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "identity", ROOT / "packaging/scripts/package_identity.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
REVISION = "c0acb7bc9df46cecdd6fed5ce4062e079ac3553e"


class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.targets = module.load_targets()

    def test_initial_tuples_and_four_unique_roles(self):
        self.assertEqual({t["id"] for t in self.targets}, {"ubuntu-26.04", "fedora-45"})
        values = [module.identity(t, r, REVISION, f"0.0.0+git.{REVISION[:12]}")
                  for t in self.targets for r in module.ROLES]
        self.assertEqual(len({v["artifact_name"] for v in values}), 4)
        for value in values:
            self.assertIn(REVISION, value["artifact_name"])
            self.assertIn(value["role"], value["artifact_name"])
            self.assertEqual(
                value["package_filename"],
                f"{value['artifact_name']}.{value['format']}",
            )
            self.assertEqual(
                value["package_name"],
                module.ROLES[value["role"]],
            )

    def test_native_version_ordering(self):
        """Use each target's native version comparator, never string ordering."""
        tested = 0
        for target in self.targets:
            with self.subTest(target=target["id"]):
                if target["package_format"] == "deb":
                    if not shutil.which("dpkg"):
                        continue
                    subprocess.run(
                        ["dpkg", "--compare-versions", "0.0.0+git.abcdef123456",
                         "lt", "0.0.1"],
                        check=True,
                        timeout=10,
                    )
                    subprocess.run(
                        ["dpkg", "--compare-versions", "1.2.3", "lt", "1.2.4"],
                        check=True,
                        timeout=10,
                    )
                    tested += 1
                else:
                    if not shutil.which("rpm"):
                        continue
                    for left, right in [
                        ("0.0.0", "0.0.1"),
                        ("0.git.abcdef123456", "1"),
                        ("1.2.3", "1.2.4"),
                    ]:
                        expression = (
                            f'%{{lua:print(rpm.vercmp("{left}", "{right}"))}}'
                        )
                        result = subprocess.check_output(
                            ["rpm", "--eval", expression], text=True, timeout=10
                        ).strip()
                        self.assertEqual(result, "-1", (left, right, result))
                    tested += 1
        if not tested:
            self.skipTest("no native package version comparator is available")

    def test_native_fallback_and_tag_encoding(self):
        for target in self.targets:
            fallback = module.identity(target, "runtime", REVISION,
                                       f"0.0.0+git.{REVISION[:12]}")
            tagged = module.identity(target, "developer", REVISION, "1.2.3")
            self.assertEqual(tagged["native_version"], "1.2.3")
            if target["package_format"] == "rpm":
                self.assertEqual(fallback["native_version"], "0.0.0")
                self.assertEqual(fallback["native_release"], f"0.git.{REVISION[:12]}")
                self.assertEqual(tagged["native_release"], "1")
            else:
                self.assertEqual(fallback["native_version"], fallback["version"])

    def test_reject_incomplete_unsupported_or_unpinned_target(self):
        for key, value in [
            ("image", "ubuntu:latest"), ("platform", "linux/arm64"),
            ("architecture", "aarch64"), ("native_architecture", "arm64"),
            ("distribution_version", "latest"), ("package_format", "tar"),
            ("os_packages", ["gcc"]), ("os_packages", ["gcc=1;echo bad"]),
            ("distribution", "unsupported"), ("compiler", {}), ("id", "ubuntu-26.04"),
        ]:
            with self.subTest(key=key, value=value):
                target = copy.deepcopy(self.targets[0])
                target[key] = value
                with self.assertRaises(ValueError):
                    module.validate_target(target)

    def test_reject_duplicate_and_role_map(self):
        data = {"schema_version": 1, "roles": module.ROLES,
                "targets": [self.targets[0], self.targets[0]]}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "targets.json"
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "duplicate"):
                module.load_targets(path)
            data["targets"] = self.targets
            data["roles"] = {"runtime": "something-else"}
            path.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "mapping"):
                module.load_targets(path)

    def test_reject_bad_revision_role_and_version(self):
        for role, revision, version in [
            ("devel", REVISION, "1.0.0"), ("runtime", "abc", "1.0.0"),
            ("runtime", "A" * 40, "1.0.0"),
            ("runtime", REVISION, "v1.0.0"), ("runtime", REVISION, "01.0.0"),
            ("runtime", REVISION, "0.0.0+git.deadbeef"),
        ]:
            with self.subTest(role=role, revision=revision, version=version):
                with self.assertRaises(ValueError):
                    module.identity(self.targets[0], role, revision, version)

    def test_only_exact_semver_tags_and_actual_checkout(self):
        with patch.object(module, "git_output", side_effect=[REVISION, "archive-foo\nv1.2.3"]):
            self.assertEqual(module.checkout_version(ROOT, REVISION), "1.2.3")
        with patch.object(module, "git_output", side_effect=[REVISION, "v01.0.0\narchive"]):
            self.assertEqual(module.checkout_version(ROOT, REVISION),
                             f"0.0.0+git.{REVISION[:12]}")
        with patch.object(module, "git_output", side_effect=[REVISION, "v1.2.3\nv1.2.4"]):
            with self.assertRaisesRegex(ValueError, "ambiguous"):
                module.checkout_version(ROOT, REVISION)
        with patch.object(module, "git_output", return_value="0" * 40):
            with self.assertRaisesRegex(ValueError, "checkout"):
                module.checkout_version(ROOT, REVISION)


if __name__ == "__main__":
    unittest.main()
