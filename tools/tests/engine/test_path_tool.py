import shutil
import subprocess
from pathlib import Path

import pytest

from haylen.toolchains.path_tool import PathTool


class PathToolTest:
    TOOL = PathTool("cmake", "Install CMake.")
    FOUND = Path("/opt/cmake/bin/cmake")

    @staticmethod
    def answer(monkeypatch: pytest.MonkeyPatch, outcome: object) -> None:
        """Puts the tool on the path and answers its "--version" with a completed process, or raises the error that `outcome` holds."""
        monkeypatch.setattr(shutil, "which", lambda name: str(PathToolTest.FOUND))

        def run(command: list, **options: object) -> subprocess.CompletedProcess:
            if isinstance(outcome, BaseException):
                raise outcome
            return outcome

        monkeypatch.setattr(subprocess, "run", run)

    def test_reads_the_version_from_the_first_line_that_the_tool_prints(self, monkeypatch):
        PathToolTest.answer(monkeypatch, subprocess.CompletedProcess([], 0, "cmake version 4.4.0\n\nCMake suite maintained by Kitware.\n", ""))

        status = PathToolTest.TOOL.status()

        assert (status.version, status.path, status.problem) == ("cmake version 4.4.0", PathToolTest.FOUND, None)

    def test_a_tool_that_does_not_answer_in_time_is_reported_with_the_bound(self, monkeypatch):
        PathToolTest.answer(monkeypatch, subprocess.TimeoutExpired(["cmake", "--version"], PathTool.VERSION_SECONDS))

        status = PathToolTest.TOOL.status()

        assert (
            status.problem == f'The command "cmake" at `{PathToolTest.FOUND}` did not answer "--version" within {PathTool.VERSION_SECONDS} seconds, so it hangs or the machine is too busy to start it.'
        )

    def test_a_tool_that_does_not_start_is_reported_with_the_reason_of_the_system(self, monkeypatch):
        PathToolTest.answer(monkeypatch, PermissionError(13, "Access is denied"))

        status = PathToolTest.TOOL.status()

        assert status.problem == f'The command "cmake" at `{PathToolTest.FOUND}` does not start, and the system says "Access is denied". Install it again.'
