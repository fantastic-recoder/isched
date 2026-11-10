# SPDX-License-Identifier: MPL-2.0
"""Contract and fail-closed tests for workflow artifact staging."""

import hashlib
import importlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "packaging/scripts/prepare_artifacts.py"
FIXTURE_ROOT = Path(__file__).parent / ".artifact-identity-fixture"
EXTENDED_TARGETS = Path(__file__).parent / "fixtures/additional-target.json"
REVISION = "c0acb7bc9df46cecdd6fed5ce4062e079ac3553e"
VERSION = "1.2.3"
SDK_LOCK = b"fixture SDK dependency lock\n"

sys.path.insert(0, str(ROOT / "packaging/scripts"))
from package_identity import identity, load_targets  # noqa: E402

spec = importlib.util.spec_from_file_location("prepare_artifacts_tested", SCRIPT)
prepare_artifacts = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = prepare_artifacts
spec.loader.exec_module(prepare_artifacts)
verify_runtime = importlib.import_module("verify_runtime")
verify_sdk = importlib.import_module("verify_sdk")


class ArtifactIdentityTests(unittest.TestCase):
    def test_native_version_round_trips_to_canonical_version(self):
        targets = {target["id"]: target for target in load_targets()}
        cases = (
            ("ubuntu-26.04", "4.5.6", "4.5.6", ""),
            ("fedora-45", "4.5.6", "4.5.6", "1"),
            ("ubuntu-26.04", f"0.0.0+git.{REVISION[:12]}",
             f"0.0.0+git.{REVISION[:12]}", ""),
            ("fedora-45", f"0.0.0+git.{REVISION[:12]}", "0.0.0",
             f"0.git.{REVISION[:12]}"),
        )
        for target_id, canonical, native, release in cases:
            for role in ("runtime", "developer"):
                with self.subTest(target=target_id, version=canonical, role=role):
                    result = identity(targets[target_id], role, REVISION, canonical)
                    self.assertEqual(result["version"], canonical)
                    self.assertEqual(result["native_version"], native)
                    self.assertEqual(result["native_release"], release)
                    self.assertEqual(result["source_revision"], REVISION)
                    self.assertEqual(result["role"], role)
                    self.assertEqual(result["artifact_name"],
                                     f"isched-{role}-{canonical}-{targets[target_id]['distribution']}-"
                                     f"{targets[target_id]['distribution_version']}-"
                                     f"x86_64-{result['format']}-{REVISION}")

    def test_full_artifact_names_are_unique_and_independent_of_target_order(self):
        targets = load_targets()
        names_by_identity = {}
        for ordered_targets in (targets, list(reversed(targets))):
            names = [identity(target, role, REVISION, VERSION)["artifact_name"]
                     for target in ordered_targets for role in ("runtime", "developer")]
            self.assertEqual(len(names), 4)
            self.assertEqual(len(set(names)), 4)
            self.assertTrue(all(name.endswith(REVISION) for name in names))
            names_by_identity[tuple(sorted(names))] = names
        self.assertEqual(len(names_by_identity), 1)


class ArtifactStagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.targets = {target["id"]: target for target in load_targets()}
        cls.target = cls.targets["ubuntu-26.04"]
        cls.records = {
            role: identity(cls.target, role, REVISION, VERSION)
            for role in ("runtime", "developer")
        }

    def setUp(self):
        shutil.rmtree(FIXTURE_ROOT, ignore_errors=True)
        self.input = FIXTURE_ROOT / "input"
        self.output = FIXTURE_ROOT / "staged"
        self.input.mkdir(parents=True)
        self.package_bytes = {}
        package_records = []
        for role, record in self.records.items():
            payload = f"fixture package bytes: {role}\n".encode()
            self.package_bytes[role] = payload
            (self.input / record["package_filename"]).write_bytes(payload)
            package_records.append({
                "role": role,
                "filename": record["package_filename"],
                "sha256": hashlib.sha256(payload).hexdigest(),
            })

        self._write_json("build-evidence.json", {
            "schema_version": 1,
            "target": self.target["id"],
            "source_revision": REVISION,
            "version": VERSION,
            "packages": package_records,
            "frontend": {
                "schema_version": 1,
                "index_sha256": "1" * 64,
                "notices_sha256": "2" * 64,
                "embedded_header_sha256": "3" * 64,
            },
            "ctest_discovered": 21,
            "gates": ["frontend-test", "frontend-build", "frontend-embed",
                      "ctest", "package-validation"],
        })
        self._write_json("runtime-verification.json", {
            "schema_version": 1,
            "target": self.target["id"],
            "package": self.records["runtime"]["package_filename"],
            "package_sha256": hashlib.sha256(self.package_bytes["runtime"]).hexdigest(),
            "loader": "resolved",
            "loader_output_sha256": "4" * 64,
            "smoke": {
                "health": {"status": "UP"},
                "ui_path": "/isched",
                "asset_path": "/isched/main.js",
                "asset_bytes": 100,
                "port": 18080,
            },
            "seeded": False,
        })
        self._write_json("sdk-verification.json", {
            "schema_version": 1,
            "target": self.target["id"],
            "package": self.records["developer"]["package_filename"],
            "package_sha256": hashlib.sha256(self.package_bytes["developer"]).hexdigest(),
            "compiler": self.target["compiler"]["version"],
            "smoke": {
                "consumer": "configured-built-linked-ran",
                "runtime_package": False,
                "conan_lock_sha256": hashlib.sha256(SDK_LOCK).hexdigest(),
                "consumer_build": "consumer-build",
            },
        })

    def tearDown(self):
        shutil.rmtree(FIXTURE_ROOT, ignore_errors=True)

    def _write_json(self, filename, data):
        (self.input / filename).write_text(json.dumps(data), encoding="utf-8")

    def _stage(self, *, output=None, github_output=None, revision=REVISION,
               targets_path=None):
        native_results = [
            {"role": role, "filename": record["package_filename"], "identity": record}
            for role, record in self.records.items()
        ]
        sdk_contents = {"usr/share/isched/developer/conan.lock": SDK_LOCK}
        with patch.object(prepare_artifacts, "checkout_version", return_value=VERSION), \
                patch.object(prepare_artifacts.validate_packages, "validate",
                             return_value=native_results), \
                patch.object(prepare_artifacts.validate_packages, "_metadata",
                             return_value=({}, set(sdk_contents), sdk_contents)):
            prepare_artifacts.stage_artifacts(
                self.target["id"], revision, self.input, output or self.output,
                github_output, targets_path or ROOT / "packaging/targets.json",
            )

    def _fails_without_staging(self, pattern, *, revision=REVISION):
        sdk_contents = {"usr/share/isched/developer/conan.lock": SDK_LOCK}
        with patch.object(prepare_artifacts, "checkout_version", return_value=VERSION), \
                patch.object(prepare_artifacts.validate_packages, "validate") as native_validate, \
                patch.object(prepare_artifacts.validate_packages, "_metadata",
                             return_value=({}, set(sdk_contents), sdk_contents)), \
                self.assertRaisesRegex((ValueError, RuntimeError), pattern):
            prepare_artifacts.stage_artifacts(
                self.target["id"], revision, self.input, self.output,
            )
        self.assertFalse(self.output.exists(), "invalid target evidence must stage neither role")
        return native_validate

    def test_stages_two_exact_role_directories_and_checksum_metadata(self):
        evidence = json.loads((self.input / "build-evidence.json").read_text())
        evidence["packages"].reverse()
        self._write_json("build-evidence.json", evidence)
        github_output = FIXTURE_ROOT / "actions-output"
        self._stage(github_output=github_output)

        self.assertEqual({path.name for path in self.output.iterdir()}, {"runtime", "developer"})
        self.assertEqual(github_output.read_text().splitlines(), [
            f"runtime_artifact_name={self.records['runtime']['artifact_name']}",
            f"developer_artifact_name={self.records['developer']['artifact_name']}",
        ])
        for role, record in self.records.items():
            with self.subTest(role=role):
                staged = self.output / role
                self.assertEqual({path.name for path in staged.iterdir()},
                                 {record["package_filename"], "metadata.json"})
                metadata = json.loads((staged / "metadata.json").read_text())
                self.assertEqual(set(metadata), set(record) | {"package_sha256"})
                self.assertEqual({key: value for key, value in metadata.items()
                                  if key != "package_sha256"}, record)
                self.assertEqual(metadata["package_sha256"], hashlib.sha256(
                    (staged / record["package_filename"]).read_bytes()).hexdigest())
                self.assertEqual((staged / record["package_filename"]).read_bytes(),
                                 self.package_bytes[role])

    def test_stale_revision_and_canonical_version_are_rejected(self):
        evidence = json.loads((self.input / "build-evidence.json").read_text())
        evidence["source_revision"] = "a" * 40
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("identity/revision/version")

    def test_wrong_requested_revision_and_stale_package_filename_are_rejected(self):
        self._fails_without_staging("output file set mismatch", revision="a" * 40)

    def test_wrong_canonical_version_is_rejected(self):
        evidence = json.loads((self.input / "build-evidence.json").read_text())
        evidence["version"] = "9.9.9"
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("identity/revision/version")

    def test_bad_hash_and_duplicate_role_records_are_rejected(self):
        evidence = json.loads((self.input / "build-evidence.json").read_text())
        evidence["packages"][0]["sha256"] = "not-a-hash"
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("lowercase SHA-256")

        evidence["packages"][0]["sha256"] = hashlib.sha256(
            self.package_bytes[evidence["packages"][0]["role"]]).hexdigest()
        evidence["packages"][1] = dict(evidence["packages"][0])
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("duplicate or unknown package role")

    def test_changed_package_and_duplicate_files_are_rejected(self):
        package = self.input / self.records["runtime"]["package_filename"]
        package.write_bytes(b"changed after package validation")
        self._fails_without_staging("checksum differs from build evidence")

        package.write_bytes(self.package_bytes["runtime"])
        (self.input / "unexpected.deb").write_bytes(b"duplicate role package")
        self._fails_without_staging("output file set mismatch")

        (self.input / "unexpected.deb").unlink()
        (self.input / self.records["developer"]["package_filename"]).unlink()
        self._fails_without_staging("output file set mismatch")

    def test_missing_gate_runtime_or_sdk_evidence_blocks_both_roles(self):
        evidence = json.loads((self.input / "build-evidence.json").read_text())
        evidence["gates"].remove("ctest")
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("gates are incomplete")

        evidence["gates"].append("ctest")
        self._write_json("build-evidence.json", evidence)
        runtime = json.loads((self.input / "runtime-verification.json").read_text())
        runtime["seeded"] = True
        self._write_json("runtime-verification.json", runtime)
        self._fails_without_staging("loader/seed gate")

        runtime["seeded"] = False
        self._write_json("runtime-verification.json", runtime)
        sdk = json.loads((self.input / "sdk-verification.json").read_text())
        sdk["smoke"]["runtime_package"] = True
        self._write_json("sdk-verification.json", sdk)
        self._fails_without_staging("SDK compile/link/run")

    def test_stale_runtime_and_sdk_package_hash_evidence_blocks_staging(self):
        for filename, role in (("runtime-verification.json", "runtime"),
                               ("sdk-verification.json", "developer")):
            with self.subTest(role=role):
                evidence = json.loads((self.input / filename).read_text())
                evidence["package_sha256"] = "0" * 64
                self._write_json(filename, evidence)
                label = "runtime" if role == "runtime" else "SDK"
                self._fails_without_staging(f"{label} evidence package hash differs")
                evidence["package_sha256"] = hashlib.sha256(self.package_bytes[role]).hexdigest()
                self._write_json(filename, evidence)

    def test_updating_build_hash_cannot_reuse_stale_role_verification(self):
        build_evidence_original = json.loads((self.input / "build-evidence.json").read_text())
        for role in ("runtime", "developer"):
            with self.subTest(role=role):
                record = self.records[role]
                package = self.input / record["package_filename"]
                changed_bytes = self.package_bytes[role] + b"changed package bytes\n"
                package.write_bytes(changed_bytes)
                build_evidence = json.loads(json.dumps(build_evidence_original))
                changed_digest = hashlib.sha256(changed_bytes).hexdigest()
                for item in build_evidence["packages"]:
                    if item["role"] == role:
                        item["sha256"] = changed_digest
                self._write_json("build-evidence.json", build_evidence)
                label = "runtime" if role == "runtime" else "SDK"
                self._fails_without_staging(f"{label} evidence package hash differs")
                package.write_bytes(self.package_bytes[role])
                self._write_json("build-evidence.json", build_evidence_original)

    def test_runtime_health_must_be_up_and_sdk_lock_hash_must_match_package(self):
        runtime = json.loads((self.input / "runtime-verification.json").read_text())
        runtime["smoke"]["health"]["status"] = "DOWN"
        self._write_json("runtime-verification.json", runtime)
        self._fails_without_staging("runtime GraphQL/UI/asset evidence is incomplete")
        runtime["smoke"]["health"]["status"] = "UP"
        self._write_json("runtime-verification.json", runtime)

        sdk = json.loads((self.input / "sdk-verification.json").read_text())
        sdk["smoke"]["conan_lock_sha256"] = "6" * 64
        self._write_json("sdk-verification.json", sdk)
        self._fails_without_staging("Conan lock hash differs from shipped lockfile")

    def test_malformed_role_and_gate_values_fail_closed(self):
        evidence = json.loads((self.input / "build-evidence.json").read_text())
        evidence["packages"][0]["role"] = []
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("duplicate or unknown package role")

        evidence["packages"][0]["role"] = "runtime"
        evidence["gates"] = [[], "frontend-test", "frontend-build", "frontend-embed",
                             "ctest", "package-validation"]
        self._write_json("build-evidence.json", evidence)
        self._fails_without_staging("successful build/test/package gates are incomplete")

    def test_optional_target_set_is_forwarded_to_native_validator(self):
        native_results = [
            {"role": role, "filename": record["package_filename"], "identity": record}
            for role, record in self.records.items()
        ]
        sdk_contents = {"usr/share/isched/developer/conan.lock": SDK_LOCK}
        with patch.object(prepare_artifacts, "checkout_version", return_value=VERSION), \
                patch.object(prepare_artifacts.validate_packages, "validate",
                             return_value=native_results) as native_validate, \
                patch.object(prepare_artifacts.validate_packages, "_metadata",
                             return_value=({}, set(sdk_contents), sdk_contents)):
            prepare_artifacts.stage_artifacts(
                self.target["id"], REVISION, self.input, self.output,
                targets_path=EXTENDED_TARGETS,
            )
        self.assertEqual(native_validate.call_args.args[3], EXTENDED_TARGETS)

    def test_native_validator_must_confirm_exactly_both_identities(self):
        wrong_results = [{"role": "runtime", "filename": "wrong.deb", "identity": {}}]
        sdk_contents = {"usr/share/isched/developer/conan.lock": SDK_LOCK}
        with patch.object(prepare_artifacts, "checkout_version", return_value=VERSION), \
                patch.object(prepare_artifacts.validate_packages, "validate",
                             return_value=wrong_results), \
                patch.object(prepare_artifacts.validate_packages, "_metadata",
                             return_value=({}, set(sdk_contents), sdk_contents)), \
                self.assertRaisesRegex(ValueError, "native validator did not confirm both roles"):
            prepare_artifacts.stage_artifacts(
                self.target["id"], REVISION, self.input, self.output,
            )
        self.assertFalse(self.output.exists())

    def test_stale_output_directory_is_never_overwritten(self):
        self.output.mkdir()
        marker = self.output / "stale"
        marker.write_text("do not overwrite")
        with self.assertRaisesRegex(ValueError, "refusing to overwrite"):
            self._stage()
        self.assertEqual(marker.read_text(), "do not overwrite")


class VerifierEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.targets = {target["id"]: target for target in load_targets()}

    def setUp(self):
        self.root = Path(__file__).parent / ".verifier-evidence-fixture"
        shutil.rmtree(self.root, ignore_errors=True)
        self.root.mkdir()
        self.package = self.root / "package.deb"
        self.package.write_bytes(b"verified package bytes")
        self.digest = hashlib.sha256(self.package.read_bytes()).hexdigest()

    def tearDown(self):
        shutil.rmtree(self.root, ignore_errors=True)

    def test_runtime_evidence_records_the_installed_package_hash(self):
        evidence = self.root / "runtime-verification.json"
        target = self.targets["ubuntu-26.04"]
        with patch.object(verify_runtime, "os_release", return_value={
                "ID": "ubuntu", "VERSION_ID": "26.04"}), \
                patch.object(verify_runtime.platform, "machine", return_value="x86_64"), \
                patch.object(verify_runtime.shutil, "which", return_value=None), \
                patch.object(verify_runtime, "_install_package"), \
                patch.object(verify_runtime, "_check_loader", return_value="loader resolved"), \
                patch.object(verify_runtime, "_verify_server", return_value={
                    "health": {"status": "UP"}, "ui_path": "/isched",
                    "asset_path": "/isched/main.js", "asset_bytes": 10,
                }):
            verify_runtime._in_container(target, self.package, evidence)
        self.assertEqual(json.loads(evidence.read_text())["package_sha256"], self.digest)

    def test_sdk_evidence_records_the_installed_package_hash(self):
        evidence = self.root / "sdk-verification.json"
        consumer = self.root / "consumer"
        consumer.mkdir()
        target = self.targets["ubuntu-26.04"]
        bootstrap_tools = importlib.import_module("bootstrap_tools")
        with patch.object(verify_sdk, "os_release", return_value={
                "ID": "ubuntu", "VERSION_ID": "26.04"}), \
                patch.object(verify_sdk.platform, "machine", return_value="x86_64"), \
                patch.object(verify_sdk, "_install_package"), \
                patch.object(verify_sdk, "_assert_runtime_absent"), \
                patch.object(verify_sdk.subprocess, "check_output", return_value="15.2.0"), \
                patch.object(bootstrap_tools, "bootstrap"), \
                patch.object(verify_sdk, "_verify_consumer", return_value={
                    "consumer": "configured-built-linked-ran",
                    "runtime_package": False,
                    "conan_lock_sha256": "1" * 64,
                }):
            verify_sdk._in_container(
                target, self.package, consumer, evidence, EXTENDED_TARGETS,
            )
        self.assertEqual(json.loads(evidence.read_text())["package_sha256"], self.digest)

if __name__ == "__main__":
    unittest.main()
