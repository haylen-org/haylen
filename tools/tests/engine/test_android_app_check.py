import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine import android_app_check
from haylen.engine.android_app_check import AndroidAppCheck
from haylen.platforms.android.android_launcher import AndroidLauncher
from tests.support.recorded_commands import RecordedCommands


class AndroidAppCheckTest:
    """The start check of an app on Android passes an app that drew through its graphics API and kept running without an error, and names the first reason of a start that went wrong with the end of the log."""

    DRAWS = 'I/haylen  : [2026-11-19 02:24:19.878] [info] The app draws through "vulkan".'

    class Clock:
        """A clock that a sleep moves forward, so the bounds of the check pass without waiting."""

        def __init__(self) -> None:
            self.now = 0.0

        def monotonic(self) -> float:
            return self.now

        def sleep(self, seconds: float) -> None:
            self.now += seconds

    class Dumps:
        """The dumps of the log of a device, one after the other, which keeps the last one."""

        def __init__(self, logs: list[list[str]]) -> None:
            self.logs = logs
            self.read = 0

        def answer(self, command: list[str]) -> str:
            index = min(self.read, len(self.logs) - 1)
            self.read += 1
            return "\n".join(self.logs[index])

    @staticmethod
    def follow(tmp_path, monkeypatch, logs: list[list[str]], alive: bool = True) -> str:
        """Follows the start of an app whose log reads `logs` one dump after the other, keeping the last one, while its process runs or not."""
        (tmp_path / "android-sdk").mkdir(parents=True)
        clock = AndroidAppCheckTest.Clock()
        monkeypatch.setattr(android_app_check, "time", clock)
        monkeypatch.setattr(AndroidLauncher, "pid", lambda self, serial, package: "4242" if alive else "")
        recorded = RecordedCommands(tmp_path, {"ANDROID_HOME": str(tmp_path / "android-sdk")}, AndroidAppCheckTest.Dumps(logs).answer)
        return AndroidAppCheck(recorded.context, None).follow("emulator-5554", "com.haylen.tests")

    def test_an_app_that_draws_and_keeps_running_passes_with_its_graphics_api(self, tmp_path, monkeypatch):
        logs = [["I/haylen: Haylen 0.0.1 starts the app."], ["I/haylen: Haylen 0.0.1 starts the app.", AndroidAppCheckTest.DRAWS]]
        assert AndroidAppCheckTest.follow(tmp_path, monkeypatch, logs) == "vulkan"

    def test_a_crash_of_java_an_error_of_the_engine_and_an_ended_process_fail_the_start(self, tmp_path, monkeypatch):
        crash = ["E/AndroidRuntime: FATAL EXCEPTION: main", "E/AndroidRuntime: java.lang.UnsatisfiedLinkError: No implementation found for void dev.haylen.HaylenActivity.nativeTelevision(boolean)"]
        with pytest.raises(BuildFailed, match="logged an error while it started: FATAL EXCEPTION: main") as failed:
            AndroidAppCheckTest.follow(tmp_path / "crash", monkeypatch, [crash])
        assert "nativeTelevision" in failed.value.details

        with pytest.raises(BuildFailed, match=r"logged an error while it started: \[error\] The app stopped with an error"):
            AndroidAppCheckTest.follow(tmp_path / "error", monkeypatch, [[AndroidAppCheckTest.DRAWS, "E/haylen  : [error] The app stopped with an error."]])
        with pytest.raises(BuildFailed, match="stopped while it started"):
            AndroidAppCheckTest.follow(tmp_path / "ended", monkeypatch, [[AndroidAppCheckTest.DRAWS]], alive=False)
        with pytest.raises(BuildFailed, match="did not log the graphics API it draws through within 180 seconds"):
            AndroidAppCheckTest.follow(tmp_path / "silent", monkeypatch, [["I/haylen: Haylen 0.0.1 starts the app."]])
