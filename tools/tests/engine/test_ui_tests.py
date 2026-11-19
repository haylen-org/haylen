import io
import json
import re
from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.engine.android_ui_tests import AndroidUiTests
from haylen.engine.ui_tests import UiTests
from haylen.engine.unified_log import UnifiedLog
from haylen.terminal import Terminal
from tests.support.recorded_commands import RecordedCommands


class UiTestsTest:
    """The UI tests run the scheme of their platform on one simulator or device with the identifier of the app, the runner of a device carries the signature of the team of the app, the Android runner runs its instrumentation against the app, both run the classes that the run names alone, the Android run gives every setting it changed its value back, each runner takes the tests of its own folder, a simulator run fails on the errors that the engine logs meanwhile, and a failed simulator run keeps the log of the simulator over the whole run."""

    def test_the_runner_runs_the_scheme_of_the_platform_and_its_info_names_the_app(self):
        project = (Path("/build/HaylenUITests.xcodeproj"), "visionOS", "SIMULATOR-ID", Path("/build/derived"))
        common = ["-project", "/build/HaylenUITests.xcodeproj", "-scheme", "visionOS", "-destination", "id=SIMULATOR-ID", "-derivedDataPath", "/build/derived"]
        assert [str(part) for part in UiTests.command("build-for-testing", *project)] == ["xcodebuild", "build-for-testing", *common]
        limits = ["-test-timeouts-enabled", "YES", "-maximum-test-execution-time-allowance", "600", "-collect-test-diagnostics", "never"]
        assert [str(part) for part in UiTests.command("test-without-building", *project)] == ["xcodebuild", "test-without-building", *common, *limits]
        only = ["-only-testing:visionOS/MenuTests", "-only-testing:visionOS/CameraTests"]
        assert [str(part) for part in UiTests.command("test-without-building", *project, ["MenuTests", "CameraTests"])] == ["xcodebuild", "test-without-building", *common, *limits, *only]
        env = UiTests.generation({"HOME": "/Users/me"}, Path("/app/ui-tests/apple"), Path("/build/runner"), "com.haylen.tests", "/build/screenshots")
        assert env == {
            "HOME": "/Users/me",
            "HAYLEN_UI_TESTS": "/app/ui-tests/apple",
            "HAYLEN_UI_RUNNER": "/build/runner",
            "HAYLEN_UI_APP": "com.haylen.tests",
            "HAYLEN_UI_OUTPUT": "/build/screenshots",
        }

    def test_the_log_of_a_failed_simulator_run_covers_the_whole_run(self):
        show = ["xcrun", "simctl", "spawn", "SIMULATOR-ID", "log", "show", "--last"]
        assert UiTests.log_command("SIMULATOR-ID", 59.0) == [*show, "2m", "--style", "compact"]
        assert UiTests.log_command("SIMULATOR-ID", 61.0) == [*show, "3m", "--style", "compact"]

    def test_the_runner_of_a_device_is_signed_for_the_team_that_signed_the_app(self):
        description = "Executable=/build/App.app/App\nAuthority=Apple Development: Someone (ABCDE12345)\nTeamIdentifier=FGHIJ67890\n"
        assert UiTests.team(description) == "FGHIJ67890"
        assert UiTests.signing("FGHIJ67890") == ["-allowProvisioningUpdates", "DEVELOPMENT_TEAM=FGHIJ67890", "CODE_SIGN_STYLE=Automatic", "CODE_SIGN_IDENTITY=Apple Development"]
        with pytest.raises(BuildFailed, match="carries no signature of a development team"):
            UiTests.team("Executable=/build/App.app/App\nSignature=adhoc\nTeamIdentifier=not set\n")

    def test_the_runner_builds_for_ios_tvos_and_visionos_with_the_tests_of_the_app(self):
        spec = (UiTests.PROJECT / "project.yml").read_text(encoding="utf-8")
        for scheme, _ in UiTests.PLATFORMS.values():
            assert f"  {scheme}:\n    type: bundle.ui-testing" in spec
        assert set(UiTests.DEVICES.values()) == {scheme for scheme, _ in UiTests.PLATFORMS.values()}
        assert "path: ${HAYLEN_UI_TESTS}" in spec
        for variable in ("HAYLEN_UI_RUNNER", "HAYLEN_UI_APP", "HAYLEN_UI_OUTPUT"):
            assert f"${{{variable}}}" in spec
        assert f"PRODUCT_BUNDLE_IDENTIFIER: {UiTests.RUNNER.removesuffix('.xctrunner')}" in spec

    def test_the_ui_tests_take_an_apple_or_android_platform_and_an_app_with_tests_of_its_runner(self, tmp_path):
        tests = UiTests(RecordedCommands(tmp_path).context, None)
        with pytest.raises(UnsupportedTarget, match='"ios-simulator", "tvos-simulator", "visionos-simulator", "ios", "tvos", "visionos", "android"'):
            tests.run(str(tmp_path), "catalyst", "Debug", None)
        with pytest.raises(UnsupportedTarget, match='so "--device-farm" takes no "--ui-test"'):
            tests.run(str(tmp_path), "ios", "Debug", None, ["Google Pixel 9"], ["CameraTests"])
        (tmp_path / "haylen.json").write_text("{}", encoding="utf-8")
        with pytest.raises(InvalidManifest, match=re.escape('has no UI tests for "apple". Write them in Swift')):
            UiTests.tests(str(tmp_path), "apple")
        (tmp_path / "ui-tests" / "apple").mkdir(parents=True)
        (tmp_path / "ui-tests" / "apple" / "AppTests.swift").write_text("", encoding="utf-8")
        assert UiTests.tests(str(tmp_path), "apple") == (tmp_path.resolve(), tmp_path.resolve() / "ui-tests" / "apple")
        with pytest.raises(InvalidManifest, match=re.escape('has no UI tests for "android". Write them in Kotlin')):
            UiTests.tests(str(tmp_path), "android")

    def test_the_android_runner_builds_with_the_tests_and_the_identifier_of_the_app_and_instruments_itself(self, tmp_path):
        recorded = RecordedCommands(tmp_path)
        command = [str(part) for part in AndroidUiTests(recorded.context, None).gradle(Path("/app/ui-tests/android"), "com.haylen.tests", Path("/build/android-ui-tests"))]
        assert command[1:4] == ["-p", str(AndroidUiTests.PROJECT), ":runner:assembleDebug"]
        assert command[-3:] == ["-PhaylenUiTests=/app/ui-tests/android", "-PhaylenUiApp=com.haylen.tests", "-PhaylenBuildDirectory=/build/android-ui-tests"]
        assert AndroidUiTests.instrument() == ["am", "instrument", "-w", "-r", "com.haylen.uitests.test/androidx.test.runner.AndroidJUnitRunner"]
        classes = ["-e", "class", "com.haylen.uitests.DeviceTests,com.haylen.uitests.PermissionTests"]
        assert AndroidUiTests.instrument(["DeviceTests", "PermissionTests"]) == ["am", "instrument", "-w", "-r", *classes, "com.haylen.uitests.test/androidx.test.runner.AndroidJUnitRunner"]
        runner = (AndroidUiTests.PROJECT / "runner" / "build.gradle.kts").read_text(encoding="utf-8")
        assert f'namespace = "{AndroidUiTests.RUNNER}"' in runner
        assert 'experimentalProperties["android.experimental.self-instrumenting"] = true' in runner
        manifest = (AndroidUiTests.PROJECT / "runner" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")
        assert 'android:value="${haylenUiApp}"' in manifest

    def test_the_android_run_passes_only_when_the_instrumentation_ended_and_no_test_failed(self):
        passed = ["INSTRUMENTATION_STATUS: test=testA", "INSTRUMENTATION_STATUS_CODE: 0", "INSTRUMENTATION_RESULT: stream=", "OK (1 test)", "INSTRUMENTATION_CODE: -1"]
        assert AndroidUiTests.passed(passed)
        assert not AndroidUiTests.passed(["INSTRUMENTATION_STATUS: test=testA", "INSTRUMENTATION_STATUS_CODE: -2", "INSTRUMENTATION_CODE: -1"])
        assert not AndroidUiTests.passed(["INSTRUMENTATION_STATUS: test=testA", "INSTRUMENTATION_RESULT: shortMsg=Process crashed."])

    def test_the_android_run_gives_each_secure_setting_back_the_value_it_had(self, tmp_path):
        (tmp_path / "sdk" / "platform-tools").mkdir(parents=True)
        (tmp_path / "sdk" / "platform-tools" / "adb").write_text("", encoding="utf-8")
        recorded = RecordedCommands(tmp_path, {"ANDROID_HOME": str(tmp_path / "sdk")})
        tests = AndroidUiTests(recorded.context, None)
        for value in ("confirmed", "", None):
            tests.restore_setting("SERIAL", "immersive_mode_confirmations", value)
        assert [command[-5:] for command in recorded.commands] == [
            ["settings", "put", "secure", "immersive_mode_confirmations", "confirmed"],
            ["settings", "put", "secure", "immersive_mode_confirmations", "''"],
            ["shell", "settings", "delete", "secure", "immersive_mode_confirmations"],
        ]

    def test_the_unified_log_keeps_the_errors_it_relays_by_the_level_of_their_record(self):
        started = "[2026-11-19 10:00:00.000] [info] Started."
        silent = "[2026-11-19 10:00:00.100] [warning] The audio output does not answer, so the app runs without sound until the system opens it."
        parted = "[2026-11-19 10:00:00.200] [error] The view parted."
        # The unified log gives a warning of the engine the type of an error, so only the level of the record tells them apart.
        events = [
            json.dumps({"messageType": "Info", "eventMessage": started}),
            json.dumps({"messageType": "Error", "eventMessage": silent}),
            json.dumps({"messageType": "Error", "eventMessage": parted}),
            json.dumps({"messageType": "Info", "eventMessage": "[error] printed by a script"}),
            "Filtering the log data.",
        ]
        errors: list[str] = []
        stdout, stderr = io.StringIO(), io.StringIO()
        UnifiedLog.relay(iter(line + "\n" for line in events), errors, Terminal(stdout, stderr, {}, Path("test.log")))
        assert errors == [parted]
        assert stderr.getvalue() == f"{silent}\n{parted}\n"
        assert stdout.getvalue() == f"{started}\n[error] printed by a script\nFiltering the log data.\n"
