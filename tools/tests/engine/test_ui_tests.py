import io
import json
from pathlib import Path

import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.engine.ui_tests import UiTests
from haylen.engine.unified_log import UnifiedLog
from haylen.terminal import Terminal
from tests.support.recorded_commands import RecordedCommands


class UiTestsTest:
    """The UI tests run the scheme of their platform on one simulator with the identifier of the app, take a simulator of iOS, tvOS or visionOS and an app with tests, and fail on the errors that the engine logs meanwhile."""

    def test_the_runner_runs_the_scheme_of_the_platform_on_the_simulator_and_names_the_app_to_it(self):
        project = (Path("/build/HaylenUITests.xcodeproj"), "visionos-simulator", "SIMULATOR-ID", Path("/build/derived"))
        common = ["-project", "/build/HaylenUITests.xcodeproj", "-scheme", "visionOS", "-destination", "id=SIMULATOR-ID", "-derivedDataPath", "/build/derived"]
        assert [str(part) for part in UiTests.command("build-for-testing", *project)] == ["xcodebuild", "build-for-testing", *common]
        limits = ["-test-timeouts-enabled", "YES", "-maximum-test-execution-time-allowance", "600", "-collect-test-diagnostics", "never"]
        assert [str(part) for part in UiTests.command("test-without-building", *project)] == ["xcodebuild", "test-without-building", *common, *limits]
        env = UiTests.environment({"HOME": "/Users/me"}, "com.haylen.tests", Path("/build/screenshots"))
        assert env == {"HOME": "/Users/me", "TEST_RUNNER_HAYLEN_UI_APP": "com.haylen.tests", "TEST_RUNNER_HAYLEN_UI_OUTPUT": "/build/screenshots"}

    def test_the_runner_builds_for_ios_tvos_and_visionos_with_the_tests_of_the_app(self):
        spec = (UiTests.PROJECT / "project.yml").read_text(encoding="utf-8")
        for scheme, _ in UiTests.PLATFORMS.values():
            assert f"  {scheme}:\n    type: bundle.ui-testing" in spec
        assert "path: ${HAYLEN_UI_TESTS}" in spec
        assert f"PRODUCT_BUNDLE_IDENTIFIER: {UiTests.RUNNER.removesuffix('.xctrunner')}" in spec

    def test_the_ui_tests_take_a_simulator_of_ios_tvos_or_visionos_and_an_app_with_tests(self, tmp_path):
        tests = UiTests(RecordedCommands(tmp_path).context, None)
        with pytest.raises(UnsupportedTarget, match='"ios-simulator", "tvos-simulator", "visionos-simulator"'):
            tests.run(str(tmp_path), "catalyst", "Debug", None)
        (tmp_path / "haylen.json").write_text("{}", encoding="utf-8")
        with pytest.raises(InvalidManifest, match="has no UI tests"):
            UiTests.tests(str(tmp_path))
        (tmp_path / "ui-tests").mkdir()
        (tmp_path / "ui-tests" / "AppTests.swift").write_text("", encoding="utf-8")
        assert UiTests.tests(str(tmp_path)) == (tmp_path.resolve(), tmp_path.resolve() / "ui-tests")

    def test_the_unified_log_keeps_the_errors_it_relays(self):
        events = [json.dumps({"messageType": "Default", "eventMessage": "Started."}), json.dumps({"messageType": "Error", "eventMessage": "The view parted."}), "Filtering the log data."]
        errors: list[str] = []
        stdout, stderr = io.StringIO(), io.StringIO()
        UnifiedLog.relay(iter(line + "\n" for line in events), errors, Terminal(stdout, stderr, {}, Path("test.log")))
        assert errors == ["The view parted."]
        assert stderr.getvalue() == "The view parted.\n"
        assert stdout.getvalue() == "Started.\nFiltering the log data.\n"
