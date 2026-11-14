# SPDX-License-Identifier: MPL-2.0
"""Stream subprocess diagnostics while retaining separately parseable output."""

from __future__ import annotations

import codecs
import os
from pathlib import Path
import signal
import subprocess
import sys
from threading import Thread


def run_logged(command: list[str], *, cwd: Path | None = None,
               env: dict[str, str] | None = None, timeout: int | None = None,
               check: bool = True, capture: bool = True) -> subprocess.CompletedProcess:
    """Tee both pipes live, including partial lines and output before a timeout."""
    if not capture:
        return subprocess.run(command, cwd=cwd, env=env, timeout=timeout, check=check)
    stdout: list[str] = []
    stderr: list[str] = []

    def forward(pipe, destination, chunks: list[str]) -> None:
        decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
        with pipe:
            while data := pipe.read1(8192):
                text = decoder.decode(data)
                chunks.append(text)
                destination.write(text)
                destination.flush()
            text = decoder.decode(b"", final=True)
            chunks.append(text)
            destination.write(text)
            destination.flush()

    with subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, start_new_session=True) as process:
        readers = [
            Thread(target=forward, args=(process.stdout, sys.stdout, stdout)),
            Thread(target=forward, args=(process.stderr, sys.stderr, stderr)),
        ]
        for reader in readers:
            reader.start()
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired as error:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            for reader in readers:
                reader.join()
            raise subprocess.TimeoutExpired(
                command, timeout, output="".join(stdout), stderr="".join(stderr)
            ) from error
        finally:
            for reader in readers:
                reader.join()
    result = subprocess.CompletedProcess(command, process.returncode,
                                         "".join(stdout), "".join(stderr))
    if check:
        result.check_returncode()
    return result
