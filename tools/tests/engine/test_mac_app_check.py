import signal
from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.mac_app_check import MacAppCheck
from tests.support.manual_clock import ManualClock
from tests.support.recorded_commands import RecordedCommands


class MacAppCheckTest:
    """The quit check of a Mac app passes an app whose process ends with the status 0 once its window closes or the Dock quits it, and fails an app that crashes, keeps running or stops before it is asked to, and a Mac that keeps its windows from the check. The app runs on a clock that the test moves, so no result depends on how fast the machine runs."""

    EXECUTABLE = Path("App.app/Contents/MacOS/App")
    # How long a real process of the tests may take to end before the test fails.
    PROCESS_SECONDS = 60

    class App:
        """The process of an app on the clock of the test: it shows its window `SHOWS_SECONDS` after it started and ends with `status` `delay` seconds after the Mac quits it, or keeps running when `status` is `None`. An app with `ends` ends by itself at that time."""

        PID = 4242
        SHOWS_SECONDS = 1.5

        def __init__(self, clock: ManualClock, status: int | None, delay: float, ends: float | None = None, output: tuple[str, ...] = ()) -> None:
            self.clock = clock
            self.status = status
            self.delay = delay
            self.ends = ends
            self.output = output
            self.started = clock.now
            self.pid = MacAppCheckTest.App.PID
            self.stopped = False

        def shows_window(self) -> bool:
            return self.poll() is None and self.clock.now - self.started >= MacAppCheckTest.App.SHOWS_SECONDS

        def quit(self) -> None:
            if self.status is not None:
                self.ends = self.clock.now + self.delay

        def poll(self) -> int | None:
            if self.ends is None or self.clock.now < self.ends:
                return None
            return self.status

        def wait(self, seconds: float) -> int | None:
            if self.ends is not None and self.ends <= self.clock.now + seconds:
                self.clock.now = max(self.clock.now, self.ends)
                return self.status
            self.clock.now += seconds
            return None

        def failed(self, reason: str) -> BuildFailed:
            return BuildFailed(f"{reason} The last lines of its log follow.", details="\n".join(self.output))

        def stop(self) -> None:
            self.stopped = True

    class Mac:
        """Answers the scripts of the check: the app counts one window once it shows it, and closing the window or quitting the app quits it."""

        def __init__(self, app: MacAppCheckTest.App) -> None:
            self.app = app
            self.asked: list[str] = []

        def answer(self, command: list[str]) -> str:
            assert command[-1] == str(MacAppCheckTest.App.PID)
            script = " ".join(command)
            if "count windows" in script:
                return "1" if self.app.shows_window() else "0"
            self.asked.append("closeButton" if "AXCloseButton" in script else "quitEvent")
            self.app.quit()
            return ""

    @staticmethod
    def check(tmp_path, app: MacAppCheckTest.App, way: str) -> list[str]:
        """Runs the check of one way of quitting on `app` and returns the ways that the check asked for."""
        mac = MacAppCheckTest.Mac(app)
        recorded = RecordedCommands(tmp_path, answer=mac.answer)
        MacAppCheck(recorded.context, None, app.clock, lambda executable: app).check(MacAppCheckTest.EXECUTABLE, way)
        return mac.asked

    @staticmethod
    def script(tmp_path, text: str) -> Path:
        script = tmp_path / "app"
        script.write_text(f"#!/bin/sh\n{text}\n", encoding="utf-8")
        script.chmod(0o755)
        return script

    def test_an_app_that_ends_with_the_status_0_passes_both_ways(self, tmp_path):
        assert MacAppCheckTest.check(tmp_path, MacAppCheckTest.App(ManualClock(), 0, 0.1), "closeButton") == ["closeButton"]
        assert MacAppCheckTest.check(tmp_path, MacAppCheckTest.App(ManualClock(), 0, 0.1), "quitEvent") == ["quitEvent"]

    def test_an_app_that_takes_a_while_to_end_within_the_bound_passes(self, tmp_path):
        clock = ManualClock()
        app = MacAppCheckTest.App(clock, 0, MacAppCheck.QUIT_SECONDS - 1)
        assert MacAppCheckTest.check(tmp_path, app, "quitEvent") == ["quitEvent"]
        assert clock.now >= MacAppCheckTest.App.SHOWS_SECONDS + MacAppCheck.SETTLE_SECONDS + MacAppCheck.QUIT_SECONDS - 1

    def test_a_crash_another_status_and_a_process_that_keeps_running_fail(self, tmp_path):
        with pytest.raises(BuildFailed, match="stopped by the signal 11 after quitting it as the Dock does"):
            MacAppCheckTest.check(tmp_path, MacAppCheckTest.App(ManualClock(), -signal.SIGSEGV, 2), "quitEvent")
        with pytest.raises(BuildFailed, match="ended with the status 3 after closing its window with the close button"):
            MacAppCheckTest.check(tmp_path, MacAppCheckTest.App(ManualClock(), 3, 2), "closeButton")

        running = MacAppCheckTest.App(ManualClock(), None, 0)
        with pytest.raises(BuildFailed, match=f"kept running {MacAppCheck.QUIT_SECONDS} seconds after closing its window with the close button"):
            MacAppCheckTest.check(tmp_path, running, "closeButton")
        assert running.stopped

    def test_an_app_that_stops_before_it_is_quit_fails(self, tmp_path):
        app = MacAppCheckTest.App(ManualClock(), 1, 0, ends=0.0, output=("The app could not start.",))
        with pytest.raises(BuildFailed, match="stopped while it started") as failed:
            MacAppCheckTest.check(tmp_path, app, "closeButton")
        assert "The app could not start." in failed.value.details

        late = MacAppCheckTest.App(ManualClock(), 0, 0, ends=MacAppCheckTest.App.SHOWS_SECONDS + 1)
        with pytest.raises(BuildFailed, match="stopped before the check quit it"):
            MacAppCheckTest.check(tmp_path, late, "closeButton")

    def test_an_app_without_a_window_fails_once_the_start_bound_ran_out(self, tmp_path, monkeypatch):
        monkeypatch.setattr(MacAppCheckTest.App, "SHOWS_SECONDS", MacAppCheck.START_SECONDS + 1)
        app = MacAppCheckTest.App(ManualClock(), 0, 0)
        with pytest.raises(BuildFailed, match=f"showed no window within {MacAppCheck.START_SECONDS} seconds"):
            MacAppCheckTest.check(tmp_path, app, "closeButton")
        assert app.stopped

    def test_a_mac_that_keeps_the_windows_from_the_check_names_the_setting(self, tmp_path):
        app = MacAppCheckTest.App(ManualClock(), 0, 0)
        recorded = RecordedCommands(tmp_path)
        recorded.context.process.attempt = lambda command: (1, b"")
        with pytest.raises(BuildFailed, match='Allow the app that runs the check, such as the terminal, in "Accessibility"'):
            MacAppCheck(recorded.context, None, app.clock, lambda executable: app).check(MacAppCheckTest.EXECUTABLE, "closeButton")

    def test_the_process_of_an_app_keeps_its_last_lines_and_ends_when_it_stops(self, tmp_path):
        ended = MacAppCheck.Run(MacAppCheckTest.script(tmp_path, "echo 'The app could not start.'\nexit 1"))
        assert ended.wait(MacAppCheckTest.PROCESS_SECONDS) == 1
        ended.reader.join(MacAppCheckTest.PROCESS_SECONDS)
        assert not ended.reader.is_alive()
        assert ended.failed("The app stopped while it started.").details == "The app could not start."

        running = MacAppCheck.Run(MacAppCheckTest.script(tmp_path, "exec sleep 600"))
        assert running.poll() is None
        running.stop()
        assert running.poll() == -signal.SIGKILL
