import os
import signal
from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.mac_app_check import MacAppCheck
from tests.support.recorded_commands import RecordedCommands


class MacAppCheckTest:
    """The quit check of a Mac app passes an app whose process ends with the status 0 once its window closes or the Dock quits it, and fails an app that crashes, keeps running or stops before it is asked to, and a Mac that keeps its windows from the check."""

    # An app that shows its window by writing the file of the window once it waits for the signal that the scripts of the test stand in for, and then ends as `ending` says.
    APP = "#!/bin/sh\ntrap '{ending}' USR1\ntouch \"$0.window\"\nwhile :; do sleep 0.05; done\n"

    class Mac:
        """Answers the scripts of the check: the app shows one window once it wrote the file of the window, and closing it or quitting the app sends the app the signal it waits for."""

        def __init__(self, app: Path) -> None:
            self.window = app.with_name(app.name + ".window")
            self.asked: list[str] = []

        def answer(self, command: list[str]) -> str:
            script = " ".join(command)
            if "count windows" in script:
                return "1" if self.window.exists() else "0"
            self.asked.append("closeButton" if "AXCloseButton" in script else "quitEvent")
            os.kill(int(command[-1]), signal.SIGUSR1)
            return ""

    @staticmethod
    def check(tmp_path, monkeypatch, ending: str, way: str) -> list[str]:
        """Runs the check of one way of quitting on an app that ends as `ending` says once the check quits it, and returns the ways that the check asked for."""
        monkeypatch.setattr(MacAppCheck, "SETTLE_SECONDS", 0)
        monkeypatch.setattr(MacAppCheck, "QUIT_SECONDS", 1)
        monkeypatch.setattr(MacAppCheck, "POLL_SECONDS", 0.01)
        app = tmp_path / "app"
        app.write_text(MacAppCheckTest.APP.format(ending=ending), encoding="utf-8")
        app.chmod(0o755)
        mac = MacAppCheckTest.Mac(app)
        mac.window.unlink(missing_ok=True)
        recorded = RecordedCommands(tmp_path, answer=mac.answer)
        MacAppCheck(recorded.context, None).check(app, way)
        return mac.asked

    def test_an_app_that_ends_with_the_status_0_passes_both_ways(self, tmp_path, monkeypatch):
        assert MacAppCheckTest.check(tmp_path, monkeypatch, "exit 0", "closeButton") == ["closeButton"]
        assert MacAppCheckTest.check(tmp_path, monkeypatch, "exit 0", "quitEvent") == ["quitEvent"]

    def test_a_crash_another_status_and_a_process_that_keeps_running_fail(self, tmp_path, monkeypatch):
        with pytest.raises(BuildFailed, match="stopped by the signal 11 after quitting it as the Dock does"):
            MacAppCheckTest.check(tmp_path, monkeypatch, "kill -SEGV $$", "quitEvent")
        with pytest.raises(BuildFailed, match="ended with the status 3 after closing its window with the close button"):
            MacAppCheckTest.check(tmp_path, monkeypatch, "exit 3", "closeButton")
        with pytest.raises(BuildFailed, match="kept running 1 seconds after closing its window with the close button"):
            MacAppCheckTest.check(tmp_path, monkeypatch, ":", "closeButton")

    def test_an_app_that_stops_before_it_is_quit_fails(self, tmp_path, monkeypatch):
        monkeypatch.setattr(MacAppCheckTest, "APP", "#!/bin/sh\necho 'The app could not start.'\nexit 1\n")
        with pytest.raises(BuildFailed, match="stopped while it started") as failed:
            MacAppCheckTest.check(tmp_path, monkeypatch, "", "closeButton")
        assert "The app could not start." in failed.value.details

    def test_a_mac_that_keeps_the_windows_from_the_check_names_the_setting(self, tmp_path, monkeypatch):
        app = tmp_path / "app"
        app.write_text(MacAppCheckTest.APP.format(ending="exit 0"), encoding="utf-8")
        app.chmod(0o755)
        recorded = RecordedCommands(tmp_path)
        recorded.context.process.attempt = lambda command: (1, b"")
        with pytest.raises(BuildFailed, match='Allow the app that runs the check, such as the terminal, in "Accessibility"'):
            MacAppCheck(recorded.context, None).check(app, "closeButton")
