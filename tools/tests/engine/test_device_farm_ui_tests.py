import json
import re

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.engine.device_farm_ui_tests import DeviceFarmUiTests
from tests.support.recorded_commands import RecordedCommands


class DeviceFarmUiTestsTest:
    """Device Farm runs the tests on the available devices that the request names, the newest system of each, the run keeps the files of every test that did not pass, and it deletes its project and checks that it is gone."""

    DEVICES = [
        {"arn": "arn:pixel-14", "name": "Google Pixel 9", "os": "14", "availability": "AVAILABLE"},
        {"arn": "arn:pixel-16", "name": "Google Pixel 9", "os": "16", "availability": "HIGHLY_AVAILABLE"},
        {"arn": "arn:galaxy", "name": "Samsung Galaxy S24", "os": "15", "availability": "TEMPORARY_NOT_AVAILABLE"},
    ]

    def test_the_run_takes_the_newest_available_device_of_each_name(self):
        assert DeviceFarmUiTests.selection(DeviceFarmUiTestsTest.DEVICES, ["Google Pixel 9"]) == ["arn:pixel-16"]
        with pytest.raises(UnsupportedTarget, match=re.escape('Device Farm has no available device named "Samsung Galaxy S24". It offers "Google Pixel 9", "Samsung Galaxy S24".')):
            DeviceFarmUiTests.selection(DeviceFarmUiTestsTest.DEVICES, ["Samsung Galaxy S24"])

    def test_a_project_that_stays_after_its_deletion_fails_the_run(self, tmp_path):
        remaining = {"projects": [{"arn": "arn:project"}]}
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps(remaining) if "list-projects" in command else "")
        with pytest.raises(BuildFailed, match=re.escape('The project "arn:project" of Device Farm is still there after its deletion.')):
            DeviceFarmUiTests(recorded.context).delete("arn:project")
        assert recorded.commands[0] == ["aws", "devicefarm", "delete-project", "--arn", "arn:project", "--region", "us-west-2", "--output", "json"]
        remaining["projects"] = []
        DeviceFarmUiTests(recorded.context).delete("arn:project")

    def test_the_files_of_every_test_that_did_not_pass_go_into_a_folder_of_their_own(self, tmp_path, monkeypatch):
        tests = [{"arn": "arn:passed", "name": "testA", "result": "PASSED"}, {"arn": "arn:failed", "name": "testB", "result": "FAILED", "message": "PermissionTests failed"}]
        answers = {"list-suites": {"suites": [{"arn": "arn:suite"}]}, "list-tests": {"tests": tests}}
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps(answers[command[2]]))
        farm = DeviceFarmUiTests(recorded.context)
        downloaded = []
        monkeypatch.setattr(farm, "download", lambda arn, folder: downloaded.append((arn, folder)))
        assert farm.failures("arn:job", tmp_path / "apple-iphone-13") == ['The test "testB" ended with FAILED: PermissionTests failed']
        assert downloaded == [("arn:failed", tmp_path / "apple-iphone-13" / "testb")]

    def test_the_results_of_the_jobs_print_as_a_table_with_the_sign_of_each_result(self, tmp_path):
        recorded = RecordedCommands(tmp_path)
        results = [{"device": "Google Pixel 9", "result": "PASSED", "message": ""}, {"device": "Apple iPhone 13", "result": "FAILED", "message": 'The test "testB" ended with FAILED.'}]
        terminal = recorded.context.terminal
        terminal.group("Results", len(results))
        terminal.table(DeviceFarmUiTests.RESULT_COLUMNS, [DeviceFarmUiTests.result_row(result) for result in results])

        assert recorded.output.getvalue().splitlines() == [
            "Results  2",
            "  Device           Result    Message",
            "  Google Pixel 9   \u2713 Passed  \u2014",
            '  Apple iPhone 13  \u2717 Failed  The test "testB" ended with FAILED.',
        ]
