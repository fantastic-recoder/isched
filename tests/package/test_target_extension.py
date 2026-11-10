# SPDX-License-Identifier: MPL-2.0
"""Target extension tests preserve production identities and fail closed."""

import copy
import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
FIXTURE = ROOT / "tests/package/fixtures/additional-target.json"
spec = importlib.util.spec_from_file_location(
    "identity", ROOT / "packaging/scripts/package_identity.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
REVISION = "c0acb7bc9df46cecdd6fed5ce4062e079ac3553e"
VERSION = "1.2.3"


class TargetExtensionTests(unittest.TestCase):
    def test_production_target_pins_are_immutable(self):
        targets = module.load_targets()
        self.assertEqual([target["id"] for target in targets], ["fedora-45", "ubuntu-26.04"])
        expected = {
            "ubuntu-26.04": "b7a97fb7930f82395e7dc307ed6e5a22169c2f053ca86eb7322379f0363ce704",
            "fedora-45": "9a0441f1cdf2470db0e895109650514c89f8087f22b65327c5118f2486e908ae",
        }
        for target in targets:
            digest = hashlib.sha256(json.dumps(
                target, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
            self.assertEqual(digest, expected[target["id"]])

    def test_three_target_identity_and_matrix_are_input_order_independent(self):
        source = json.loads(FIXTURE.read_text(encoding="utf-8"))
        targets = module.validate_target_set(source)
        self.assertEqual([target["id"] for target in targets],
                         ["fedora-44", "fedora-45", "ubuntu-26.04"])
        production = {target["id"]: target for target in module.load_targets()}
        fixture_production = {target["id"]: target for target in targets
                              if target["id"] != "fedora-44"}
        self.assertEqual(fixture_production, production)
        reversed_data = dict(source, targets=list(reversed(source["targets"])))
        self.assertEqual(targets, module.validate_target_set(reversed_data))

        records = [module.identity(target, role, REVISION, VERSION)
                   for target in targets for role in ("runtime", "developer")]
        reverse_records = [module.identity(target, role, REVISION, VERSION)
                           for target in reversed(targets)
                           for role in ("runtime", "developer")]
        self.assertEqual(sorted(json.dumps(record, sort_keys=True) for record in records),
                         sorted(json.dumps(record, sort_keys=True) for record in reverse_records))
        self.assertEqual(len({record["artifact_name"] for record in records}), 6)
        self.assertEqual({record["target"] for record in records},
                         {"ubuntu-26.04", "fedora-44", "fedora-45"})

        output = io.StringIO()
        with patch.object(sys, "argv", ["package_identity.py", "--targets", str(FIXTURE), "--matrix"]):
            with contextlib.redirect_stdout(output):
                self.assertEqual(module.main(), 0)
        self.assertEqual(json.loads(output.getvalue())["include"], targets)

    def test_duplicate_incomplete_unsupported_and_unpinned_records_fail_closed(self):
        data = json.loads(FIXTURE.read_text(encoding="utf-8"))
        cases = []

        duplicate = copy.deepcopy(data)
        duplicate["targets"].append(copy.deepcopy(duplicate["targets"][1]))
        cases.append(("duplicate target identity", duplicate))

        incomplete = copy.deepcopy(data)
        incomplete["targets"][1]["os_packages"] = []
        cases.append(("exact OS tool pins required", incomplete))

        invalid_record = copy.deepcopy(data)
        invalid_record["targets"][1] = "fedora-44"
        cases.append(("each target record must be an object", invalid_record))

        invalid_set = []
        cases.append(("target set must be a JSON object", invalid_set))

        unsupported = copy.deepcopy(data)
        unsupported["targets"][1]["distribution"] = "debian"
        cases.append(("unsupported distribution", unsupported))

        unpinned = copy.deepcopy(data)
        unpinned["targets"][1]["image"] = "fedora:44"
        cases.append(("immutable SHA-256", unpinned))

        for message, candidate in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    module.validate_target_set(candidate)

    def test_additional_target_profile_comes_from_its_declared_compiler(self):
        target = next(item for item in module.load_targets(FIXTURE) if item["id"] == "fedora-44")
        expected = (ROOT / "packaging/conan/profiles/fedora-45").read_text(encoding="utf-8")
        self.assertEqual(module.conan_profile(target), expected)
        self.assertIn("compiler.version=16\n", module.conan_profile(target))
        self.assertIn('"cpp": "/usr/bin/g++"', module.conan_profile(target))

    def test_existing_target_python_pins_remain_unchanged(self):
        versions = json.loads((ROOT / "packaging/tool-versions.json").read_text(encoding="utf-8"))
        self.assertEqual(versions["python_versions"]["ubuntu-26.04"], "3.14")
        self.assertEqual(versions["python_versions"]["fedora-45"], "3.15")
        self.assertEqual(versions["python_versions"]["fedora-44"], "3.14")

    def test_cmake_accepts_extended_tuple_and_rejects_mismatched_family(self):
        source = (ROOT / "packaging/cmake/Package.cmake").read_text(encoding="utf-8")
        validation = source.split('set(_isched_identity_output ', 1)[0]
        target = next(item for item in module.load_targets(FIXTURE) if item["id"] == "fedora-44")
        records = [module.identity(target, role, REVISION, VERSION)
                   for role in ("runtime", "developer")]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "profile").write_text(module.conan_profile(target), encoding="utf-8")
            script = root / "validate.cmake"
            script.write_text(
                'cmake_minimum_required(VERSION 3.19)\n'
                'set(ISCHED_ENABLE_PACKAGING ON)\n'
                f'set(ISCHED_SOURCE_REVISION "{REVISION}")\n'
                f'set(ISCHED_PACKAGE_VERSION "{VERSION}")\n'
                'set(ISCHED_PACKAGE_TARGET "fedora-44")\n'
                'set(ISCHED_PACKAGE_FORMAT "rpm")\n'
                f'set(ISCHED_PACKAGE_IDENTITY_FILE "{root / "identity.json"}")\n'
                f'set(ISCHED_PACKAGE_PROFILE_FILE "{root / "profile"}")\n'
                f'set(ISCHED_PACKAGE_LICENSE_OUTPUT "{root / "licenses"}")\n'
                f'set(ISCHED_PACKAGE_OUTPUT_DIR "{root / "out"}")\n'
                + validation, encoding="utf-8",
            )
            for scenario in ("valid", "wrong-family", "wrong-native-arch"):
                candidate = copy.deepcopy(records)
                if scenario == "wrong-family":
                    candidate[1]["distribution"] = "ubuntu"
                    candidate[1]["distribution_version"] = "26.04"
                elif scenario == "wrong-native-arch":
                    for record in candidate:
                        record["native_architecture"] = "amd64"
                (root / "identity.json").write_text(json.dumps(candidate), encoding="utf-8")
                result = subprocess.run(
                    ["cmake", "-P", str(script)], capture_output=True, text=True, timeout=30,
                )
                with self.subTest(scenario=scenario):
                    if scenario == "valid":
                        self.assertEqual(result.returncode, 0, result.stderr)
                    else:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("tuple mismatch", result.stderr)


if __name__ == "__main__":
    unittest.main()
