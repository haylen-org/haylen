import subprocess
from pathlib import Path

import pytest

from haylen.platforms.apple.xcode_tools import XcodeTools


class XcodeToolsTest:
    @staticmethod
    def answer(monkeypatch: pytest.MonkeyPatch, answers: dict[str, object]) -> None:
        """Answers each tool of Xcode by its name with a completed process, or raises the error that its answer holds."""

        def run(command: list, **options: object) -> subprocess.CompletedProcess:
            outcome = answers[command[0]]
            if isinstance(outcome, BaseException):
                raise outcome
            return outcome

        monkeypatch.setattr(subprocess, "run", run)

    def test_reads_the_version_of_xcode_and_its_folder(self, monkeypatch):
        XcodeToolsTest.answer(
            monkeypatch,
            {
                "xcodebuild": subprocess.CompletedProcess([], 0, "Xcode 27.2\nBuild version 27C5\n", ""),
                "xcode-select": subprocess.CompletedProcess([], 0, "/Applications/Xcode.app/Contents/Developer\n", ""),
            },
        )

        status = XcodeTools.xcode()

        assert (status.version, status.path, status.problem) == ("Xcode 27.2", Path("/Applications/Xcode.app/Contents/Developer"), None)

    def test_an_xcodebuild_that_does_not_answer_in_time_is_reported_with_the_bound(self, monkeypatch):
        XcodeToolsTest.answer(monkeypatch, {"xcodebuild": subprocess.TimeoutExpired(["xcodebuild", "-version"], XcodeTools.ANSWER_SECONDS)})

        status = XcodeTools.xcode()

        expected = (
            f'The command "xcodebuild" did not answer "xcodebuild -version" within {XcodeTools.ANSWER_SECONDS} seconds, so it hangs or the machine is too busy to start it. {XcodeTools.XCODE_HINT}'
        )
        assert status.problem == expected

    def test_an_xcodebuild_that_does_not_start_is_reported_with_the_reason_of_the_system(self, monkeypatch):
        XcodeToolsTest.answer(monkeypatch, {"xcodebuild": FileNotFoundError(2, "No such file or directory")})

        status = XcodeTools.xcode()

        assert status.problem == f'The command "xcodebuild" does not start, and the system says "No such file or directory". {XcodeTools.XCODE_HINT}'

    def test_an_xcodebuild_that_fails_is_reported_with_what_it_says(self, monkeypatch):
        said = "xcode-select: error: tool 'xcodebuild' requires Xcode, but active developer directory '/Library/Developer/CommandLineTools' is a command line tools instance"
        XcodeToolsTest.answer(monkeypatch, {"xcodebuild": subprocess.CompletedProcess([], 1, "", said + "\n")})

        status = XcodeTools.xcode()

        assert status.problem == f'The command "xcodebuild -version" failed, and it says "{said}". {XcodeTools.XCODE_HINT}'

    def test_an_xcrun_that_does_not_start_is_not_taken_for_a_missing_sdk(self, monkeypatch):
        XcodeToolsTest.answer(monkeypatch, {"xcrun": PermissionError(13, "Permission denied")})

        status = XcodeTools.sdk("iphonesimulator", "iOS")

        assert status.problem == 'The command "xcrun" does not start, and the system says "Permission denied".'

    def test_a_missing_sdk_names_the_platform_to_install(self, monkeypatch):
        XcodeToolsTest.answer(monkeypatch, {"xcrun": subprocess.CompletedProcess([], 1, "", 'xcrun: error: SDK "xros" cannot be located\n')})

        status = XcodeTools.sdk("xros", "visionOS")

        assert status.problem == 'Xcode has no SDK "xros". Install the platform "visionOS" in the components of the settings of Xcode, or with "xcodebuild -downloadPlatform visionOS".'
