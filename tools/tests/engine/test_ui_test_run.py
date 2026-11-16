import os
import re
import sys

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.engine.ui_test_run import UiTestRun
from tests.support.tooling_fixture import ToolingFixture


class UiTestRunTest:
    def test_a_runner_that_starts_no_suite_stops_with_a_sentence(self, tmp_path):
        terminal = ToolingFixture(tmp_path).context.terminal
        command = [sys.executable, "-c", "import time; time.sleep(30)"]
        with pytest.raises(BuildFailed, match=re.escape("The UI test runner started no test within 1 seconds")):
            UiTestRun(terminal).run(command, os.environ, start_seconds=1)

    def test_a_runner_that_starts_runs_to_its_end(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        command = [sys.executable, "-c", "import time; print(\"Test Suite 'All tests' started at now\", flush=True); time.sleep(2)"]
        UiTestRun(fixture.context.terminal).run(command, os.environ, start_seconds=1)
        assert "Test Suite 'All tests' started" in fixture.output.getvalue()

    def test_every_scheme_runs_its_tests_without_a_debugger(self):
        spec = (Repository.ENGINE / "tests" / "ui" / "apple" / "project.yml").read_text(encoding="utf-8")
        schemes = spec.split("\nschemes:\n")[1]
        assert re.findall(r"^  (\w+):$", schemes, re.MULTILINE) == ["iOS", "tvOS", "visionOS"]
        assert schemes.count("debugEnabled: false") == 3
        assert "testTargets" not in spec
