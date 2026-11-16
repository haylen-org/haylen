import contextlib
import os
import signal
import sys
import tempfile
import textwrap
import time
import urllib.request
from unittest import mock

import pytest

from haylen.core.host import Host
from haylen.engine.headless_browser import HeadlessBrowser
from tests.support.recorded_commands import RecordedCommands


class HeadlessBrowserTest:
    @pytest.mark.skipif(sys.platform == "win32", reason="The browser of the test is a shell script.")
    def test_removes_the_profile_only_once_every_process_of_the_browser_ended(self, tmp_path, monkeypatch):
        # A browser that leaves a helper behind, which writes the profile until it ends, as the helpers of Chrome do while it exits.
        chrome = tmp_path / "chrome"
        helper = tmp_path / "helper"
        script = f"""\
            #!/bin/sh
            for argument in "$@"; do
                case "$argument" in --user-data-dir=*) profile="${{argument#--user-data-dir=}}" ;; esac
            done
            sh -c 'trap "touch \\"$1/late\\"; exit" TERM; while :; do sleep 0.05; done' helper "$profile" &
            echo $! > "{helper}"
            exec sleep 60
            """
        chrome.write_text(textwrap.dedent(script), encoding="utf-8")
        chrome.chmod(0o755)
        profile = tmp_path / "profile"
        profile.mkdir()
        monkeypatch.setattr(tempfile, "mkdtemp", lambda: str(profile))
        monkeypatch.setattr(urllib.request, "urlopen", lambda *arguments, **options: mock.MagicMock())

        context = RecordedCommands(tmp_path, {"HAYLEN_CHROME": str(chrome)}).context
        with HeadlessBrowser(context, [], "the test"):
            deadline = time.monotonic() + 10
            while not helper.exists() or not helper.read_text(encoding="utf-8").strip():
                assert time.monotonic() < deadline, "The browser of the test did not start its helper."
                time.sleep(0.05)
            helper_id = int(helper.read_text(encoding="utf-8"))
        try:
            with pytest.raises(ProcessLookupError):
                os.kill(helper_id, 0)
            assert not profile.exists()
        finally:
            with contextlib.suppress(ProcessLookupError):
                os.kill(helper_id, signal.SIGKILL)

    @pytest.mark.skipif(sys.platform == "win32", reason="Windows ends the tree of the browser with taskkill.")
    def test_takes_a_group_of_processes_that_wait_for_their_parent_as_ended_on_macos(self, tmp_path, monkeypatch):
        # The kernel of macOS answers a group that holds only processes that ended and wait for their parent with EPERM, while a refusal on other systems stays an error.
        def killpg(_: int, number: int) -> None:
            if number == 0:
                raise PermissionError(1, "Operation not permitted")

        monkeypatch.setattr(os, "killpg", killpg)
        browser = HeadlessBrowser(RecordedCommands(tmp_path, {}).context, [], "the test")
        process = mock.Mock(pid=4242)
        monkeypatch.setattr(Host, "system", staticmethod(lambda: "macos"))
        browser.end_process_tree(process)

        monkeypatch.setattr(Host, "system", staticmethod(lambda: "linux"))
        with pytest.raises(PermissionError):
            browser.end_process_tree(process)
