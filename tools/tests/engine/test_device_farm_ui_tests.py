import json
import re

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.engine.device_farm_ui_tests import DeviceFarmUiTests
from tests.support.recorded_commands import RecordedCommands


class DeviceFarmUiTestsTest:
    """Device Farm runs the tests on the available devices that the request names, the newest system of each, and the run deletes its project and checks that it is gone."""

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
