import re
import subprocess
import threading
from collections.abc import Mapping, Sequence

from haylen.core.errors.build_failed import BuildFailed
from haylen.terminal import Terminal


class UiTestRun:
    """Runs the tests of a built UI test runner with `xcodebuild test-without-building`, printing their output as it comes. The runner must start its first test suite within a bounded time: a runner that never connects, such as one that waits for a debugger, ends the run with a sentence that says so instead of holding it."""

    START_SECONDS = 300
    STARTED = re.compile(r"Test Suite .* started")

    def __init__(self, terminal: Terminal) -> None:
        self.terminal = terminal

    def run(self, command: Sequence[object], environment: Mapping[str, str], start_seconds: int = START_SECONDS) -> None:
        parts = [str(part) for part in command]
        started = threading.Event()
        expired = threading.Event()
        process = subprocess.Popen(parts, env=dict(environment), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
        timer = threading.Timer(start_seconds, UiTestRun.stop_unstarted, (process, started, expired))
        timer.start()
        try:
            for line in process.stdout:
                if UiTestRun.STARTED.search(line):
                    started.set()
                self.terminal.verbatim(line.rstrip("\n"))
            code = process.wait()
        finally:
            timer.cancel()
            process.stdout.close()

        if expired.is_set():
            raise BuildFailed(f"The UI test runner started no test within {start_seconds} seconds, so the run stopped it. The simulator may be stuck: shut it down and run the tests again.")
        if code != 0:
            raise BuildFailed(f'The command "xcodebuild" failed with exit code {code}, and its output above shows which UI tests failed.')

    @staticmethod
    def stop_unstarted(process: subprocess.Popen, started: threading.Event, expired: threading.Event) -> None:
        """Stops a runner that has not started a test suite when the bound of its start expires."""
        if not started.is_set():
            expired.set()
            process.kill()
