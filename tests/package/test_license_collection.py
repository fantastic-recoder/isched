# SPDX-License-Identifier: MPL-2.0
"""Unit tests for package-role license inventory generation."""

import json
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
COLLECTOR = ROOT / "packaging/scripts/collect_licenses.py"


class LicenseCollectionTests(unittest.TestCase):
    def test_role_payloads_include_host_dependencies_and_runtime_angular_notices(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "conan-cache" / "pkg"
            (package / "licenses").mkdir(parents=True)
            (package / "licenses" / "LICENSE.txt").write_text("dependency license\n")
            angular = root / "3rdpartylicenses.txt"
            angular.write_text("frontend dependency notices\n")
            graph = root / "graph.json"
            graph.write_text(json.dumps({
                "graph": {
                    "nodes": {
                        "0": {"ref": "conanfile", "recipe": "Consumer", "context": "host"},
                        "1": {
                            "ref": "sample/1.2.3#recipe", "name": "sample",
                            "context": "host", "license": "MIT",
                            "package_folder": str(package),
                        },
                        "2": {
                            "ref": "b2/5.6.1#recipe", "name": "b2",
                            "context": "build", "license": "BSL-1.0",
                        },
                        "3": {
                            "ref": "catch2/3.4.0#recipe", "name": "catch2",
                            "context": "host", "license": "BSL-1.0",
                        },
                    },
                    "error": None,
                },
            }))
            output = root / "license-output"
            result = self._run(graph, angular, output)

            self.assertEqual(result.returncode, 0, result.stderr)
            runtime = output / "runtime/licenses/conan/sample_1.2.3_recipe/LICENSE.txt"
            developer = output / "developer/licenses/conan/sample_1.2.3_recipe/LICENSE.txt"
            self.assertEqual(runtime.read_text(), "dependency license\n")
            self.assertEqual(developer.read_text(), "dependency license\n")
            self.assertTrue((output / "runtime/licenses/angular/3rdpartylicenses.txt").is_file())
            self.assertFalse((output / "developer/licenses/angular").exists())
            inventory = (output / "runtime/license-inventory.json").read_text()
            self.assertNotIn(str(root), inventory)
            self.assertNotIn("catch2", inventory.lower())
            self.assertNotIn("b2/", inventory.lower())
            developer_inventory = json.loads(
                (output / "developer/license-inventory.json").read_text()
            )
            self.assertEqual(developer_inventory["role"], "developer")
            self.assertNotIn("angular", json.dumps(developer_inventory).lower())
            for record in developer_inventory["notices"]:
                notice = output / "developer" / record["path"]
                self.assertTrue(notice.is_file())
                self.assertEqual(hashlib.sha256(notice.read_bytes()).hexdigest(), record["sha256"])

    def test_missing_dependency_notice_fails_without_partial_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "empty-cache-package"
            package.mkdir()
            angular = root / "3rdpartylicenses.txt"
            angular.write_text("frontend notices\n")
            graph = root / "graph.json"
            graph.write_text(json.dumps({
                "graph": {
                    "nodes": {
                        "1": {
                            "ref": "missing/1.0#recipe", "name": "missing",
                            "context": "host", "license": "MIT",
                            "package_folder": str(package),
                        },
                    },
                    "error": None,
                },
            }))
            output = root / "license-output"
            result = self._run(graph, angular, output)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing/1.0", result.stderr)
            self.assertFalse(output.exists())

    def test_stale_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "license-output"
            output.mkdir()
            (output / "stale.txt").write_text("stale\n")
            angular = root / "3rdpartylicenses.txt"
            angular.write_text("frontend notices\n")
            graph = root / "graph.json"
            graph.write_text(json.dumps({
                "graph": {
                    "nodes": {
                        "1": {
                            "ref": "sample/1.0#recipe", "name": "sample",
                            "context": "host", "license": "MIT",
                        },
                    },
                    "error": None,
                },
            }))
            result = self._run(graph, angular, output)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("stale output", result.stderr)
            self.assertEqual((output / "stale.txt").read_text(), "stale\n")

    @staticmethod
    def _run(graph, angular, output):
        return subprocess.run(
            [
                sys.executable,
                str(COLLECTOR),
                "--conan-graph", str(graph),
                "--angular-notices", str(angular),
                "--output-dir", str(output),
            ],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )


if __name__ == "__main__":
    unittest.main()
