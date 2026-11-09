# SPDX-License-Identifier: MPL-2.0
"""Native package contract tests.

The validator CLI is the contract boundary: it must inspect package contents
and native metadata, not infer validity from a filename suffix. These tests
never install a package on the host. The helper is supplied by the later
packaging implementation task.
"""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

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


class GeneratedPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sys.path.insert(0, str(ROOT / "packaging/scripts"))
        spec = importlib.util.spec_from_file_location("native_validator_tested", VALIDATOR)
        cls.validator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.validator)
        spec = importlib.util.spec_from_file_location(
            "native_builder_tested", ROOT / "packaging/scripts/build_target.py"
        )
        cls.builder = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.builder)

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cmake", "cpack", "dpkg-deb", "dpkg-shlibdeps")),
        "native package regression requires CMake, CPack and Debian inspection tools",
    )
    def test_real_cpack_metadata_and_role_notice_layout(self):
        self._check_generated_packages("ubuntu-26.04")

    @unittest.skipUnless(
        all(shutil.which(tool) for tool in ("cmake", "cpack", "rpm", "rpmbuild", "rpm2cpio")),
        "native package regression requires CMake, CPack and RPM inspection tools",
    )
    def test_real_rpm_metadata_and_role_notice_layout(self):
        self._check_generated_packages("fedora-45")

    def _check_generated_packages(self, target_id):
        from package_identity import identity, load_targets

        target = next(item for item in load_targets() if item["id"] == target_id)
        records = [identity(target, role, REVISION, "1.0.0")
                   for role in ("runtime", "developer")]
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            for name in (
                "LICENSE.md", "README.md", "packaging/sdk/README.md",
                "packaging/sdk/conanfile.txt", "packaging/sdk/conan.lock",
                "packaging/requirements-conan.lock",
                "packaging/conan/profiles/ubuntu-26.04",
            ):
                destination = source / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / name, destination)
            (root / "identity.json").write_text(json.dumps(records))
            for role in ("runtime", "developer"):
                directory = root / "licenses" / role
                (directory / "licenses").mkdir(parents=True)
                notice = b"third-party license fixture\n"
                (directory / "licenses/NOTICE").write_bytes(notice)
                (directory / "license-inventory.json").write_text(json.dumps({
                    "schema_version": 1, "role": role,
                    "notices": [{"path": "licenses/NOTICE",
                                 "sha256": hashlib.sha256(notice).hexdigest()}],
                }))
            (source / "header.hpp").write_text("// public header fixture\n")
            (source / "libisched.a").write_bytes(b"static archive fixture")
            for name in ("ischedConfig.cmake", "ischedTargets.cmake"):
                (source / name).write_text("# relocatable export fixture\n")
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.19)\n'
                'project(isched NONE)\n'
                'set(CMAKE_INSTALL_LIBDIR lib)\n'
                'include(GNUInstallDirs)\n'
                'set(ISCHED_ENABLE_PACKAGING ON)\n'
                f'set(ISCHED_SOURCE_REVISION "{REVISION}")\n'
                'set(ISCHED_PACKAGE_VERSION "1.0.0")\n'
                f'set(ISCHED_PACKAGE_TARGET "{target_id}")\n'
                f'set(ISCHED_PACKAGE_FORMAT "{target["package_format"]}")\n'
                f'set(ISCHED_PACKAGE_IDENTITY_FILE "{root / "identity.json"}")\n'
                f'set(ISCHED_PACKAGE_LICENSE_OUTPUT "{root / "licenses"}")\n'
                f'set(ISCHED_PACKAGE_OUTPUT_DIR "{root / "out"}")\n'
                'install(PROGRAMS /bin/true DESTINATION bin RENAME isched_srv COMPONENT runtime)\n'
                'foreach(header IN ITEMS isched.hpp backend/isched_Server.hpp '
                'backend/isched_common.hpp shared/config/isched_config.hpp)\n'
                '  get_filename_component(directory "${header}" DIRECTORY)\n'
                '  get_filename_component(name "${header}" NAME)\n'
                '  install(FILES header.hpp DESTINATION "include/isched/${directory}" '
                'RENAME "${name}" COMPONENT devel)\n'
                'endforeach()\n'
                'install(FILES libisched.a DESTINATION lib COMPONENT devel)\n'
                'install(FILES ischedConfig.cmake ischedTargets.cmake '
                'DESTINATION lib/cmake/isched COMPONENT devel)\n'
                f'include("{ROOT / "packaging/cmake/Package.cmake"}")\n'
            )
            result = subprocess.run(
                ["cmake", "-S", str(source), "-B", str(root / "build"),
                 "-DCMAKE_INSTALL_PREFIX=/usr"],
                capture_output=True, text=True, timeout=120,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            (root / "out").mkdir()
            self.builder.generate_packages(
                root / "build", root / "cpack", root / "out", records, dict(os.environ),
            )
            validated = self.validator.validate(target["id"], REVISION, root / "out")
            self.assertEqual({item["role"] for item in validated}, {"runtime", "developer"})
            developer = root / "out" / records[1]["package_filename"]
            metadata, paths, contents = self.validator._metadata(developer, target)
            if target["package_format"] == "deb":
                self.assertEqual(metadata["depends"], "")
            self.assertNotIn("isched-runtime", metadata["depends"])
            self.assertIn("usr/share/doc/isched-devel/licenses/NOTICE", paths)
            for scenario in ("missing", "corrupt"):
                with self.subTest(scenario=scenario):
                    broken = dict(contents)
                    if scenario == "missing":
                        del broken["usr/share/doc/isched-devel/licenses/NOTICE"]
                    else:
                        broken["usr/share/doc/isched-devel/licenses/NOTICE"] = b"corrupted"
                    with self.assertRaisesRegex(self.validator.ValidationError, "checksum mismatch"):
                        self.validator._validate_payload(target, "developer", paths, broken)
            original_metadata = self.validator._metadata

            def without_runtime_dependencies(package, selected_target):
                values, names, data = original_metadata(package, selected_target)
                return {**values, "depends": ""}, names, data

            with patch.object(self.validator, "_metadata", side_effect=without_runtime_dependencies):
                with self.assertRaisesRegex(self.validator.ValidationError, "no declared native dependencies"):
                    self.validator.validate(target["id"], REVISION, root / "out")


if __name__ == "__main__":
    unittest.main()
