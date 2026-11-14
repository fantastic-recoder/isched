# SPDX-License-Identifier: MPL-2.0
"""Regression coverage for live Docker pipeline diagnostics."""

import contextlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
from threading import Event
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "packaging/scripts"))
from logged_process import run_logged


class LoggedProcessTests(unittest.TestCase):
    def test_partial_stdout_and_stderr_are_visible_before_process_exits(self):
        with tempfile.TemporaryDirectory() as directory:
            release = Path(directory) / "release"
            stdout_seen, stderr_seen = Event(), Event()

            class LiveOutput(io.StringIO):
                def __init__(self, seen):
                    super().__init__()
                    self.seen = seen

                def write(self, text):
                    result = super().write(text)
                    if text:
                        self.seen.set()
                    if stdout_seen.is_set() and stderr_seen.is_set():
                        release.touch()
                    return result

            stdout, stderr = LiveOutput(stdout_seen), LiveOutput(stderr_seen)
            script = (
                "import os, pathlib, sys, time\n"
                "os.write(1, b'partial stdout')\n"
                "os.write(2, b'partial stderr')\n"
                "deadline = time.monotonic() + 5\n"
                "while not pathlib.Path(sys.argv[1]).exists():\n"
                "    if time.monotonic() > deadline: sys.exit(9)\n"
                "    time.sleep(0.01)\n"
            )
            with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                result = run_logged([sys.executable, "-c", script, str(release)], timeout=10)
            self.assertEqual(result.stdout, "partial stdout")
            self.assertEqual(result.stderr, "partial stderr")
            self.assertEqual(stdout.getvalue(), result.stdout)
            self.assertEqual(stderr.getvalue(), result.stderr)

    def test_json_stdout_is_not_polluted_by_stderr(self):
        with contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            result = run_logged([
                sys.executable, "-c",
                "import os; os.write(1, b'{\"tests\": []}'); os.write(2, b'warning')",
            ])
        self.assertEqual(result.stdout, '{"tests": []}')
        self.assertEqual(result.stderr, "warning")

    def test_failure_streams_diagnostics_and_preserves_exit_code(self):
        for check in (True, False):
            stdout, stderr = io.StringIO(), io.StringIO()
            with self.subTest(check=check), contextlib.redirect_stdout(stdout), \
                    contextlib.redirect_stderr(stderr):
                command = [sys.executable, "-c",
                           "import os; os.write(2, b'compiler error'); exit(17)"]
                if check:
                    with self.assertRaises(subprocess.CalledProcessError) as raised:
                        run_logged(command)
                    self.assertEqual(raised.exception.returncode, 17)
                    self.assertEqual(raised.exception.stderr, "compiler error")
                else:
                    self.assertEqual(run_logged(command, check=False).returncode, 17)
            self.assertEqual(stderr.getvalue(), "compiler error")

    def test_timeout_keeps_partial_output_and_terminates_descendants(self):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            with self.assertRaises(subprocess.TimeoutExpired) as raised:
                run_logged([
                    sys.executable, "-c",
                    "import os, subprocess, sys, time; "
                    "os.write(1, b'before timeout'); "
                    "subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)']); "
                    "time.sleep(30)",
                ], timeout=1)
        self.assertEqual(stdout.getvalue(), "before timeout")
        self.assertEqual(raised.exception.stdout, "before timeout")


if __name__ == "__main__":
    unittest.main()
