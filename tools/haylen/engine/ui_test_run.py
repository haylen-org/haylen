import re
import subprocess
import threading
from collections.abc import Mapping, Sequence
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.terminal import Terminal


class UiTestRun:
    """Runs the tests of a built UI test runner, such as `xcodebuild test-without-building` or the instrumentation of Android, printing their output as it comes and returning its lines. The runner must start its first test within a bounded time, which a line of its output tells: a runner that never connects, such as one that waits for a debugger, ends the run with a sentence that says so instead of holding it."""

    START_SECONDS = 300
    # The line of xcodebuild that starts a test suite.
    STARTED = re.compile(r"Test Suite .* started")
    # The lines of a runner that a requirement of the platform stopped, each with the sentence that says what the person does once to meet it.
    REQUIREMENTS = (
        (
            re.compile(r"Timed out while enabling automation mode"),
            'iOS runs UI tests on a device only once the person enabled UI Automation with the passcode of the device. Turn on "Enable UI Automation" in Settings > Developer on the device, which asks for the passcode once, and run the tests again.',
        ),
    )

    def __init__(self, terminal: Terminal, started: re.Pattern[str] = STARTED) -> None:
        self.terminal = terminal
        self.started = started

    def run(self, command: Sequence[object], environment: Mapping[str, str], start_seconds: int = START_SECONDS) -> list[str]:
        parts = [str(part) for part in command]
        lines: list[str] = []
        started = threading.Event()
        expired = threading.Event()
        process = subprocess.Popen(parts, env=dict(environment), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
        timer = threading.Timer(start_seconds, UiTestRun.stop_unstarted, (process, started, expired))
        timer.start()
        try:
            for line in process.stdout:
                if self.started.search(line):
                    started.set()
                lines.append(line.rstrip("\n"))
                self.terminal.verbatim(lines[-1])
            code = process.wait()
        finally:
            timer.cancel()
            process.stdout.close()

        if expired.is_set():
            raise BuildFailed(
                f"The UI test runner started no test within {start_seconds} seconds, so the run stopped it. The simulator or the device may be stuck: restart it and run the tests again."
            )
        if code != 0:
            for pattern, requirement in UiTestRun.REQUIREMENTS:
                if any(pattern.search(line) for line in lines):
                    raise BuildFailed(requirement)
            raise BuildFailed(f'The command "{Path(parts[0]).name}" failed with exit code {code}, and its output above shows which UI tests failed.')
        return lines

    @staticmethod
    def stop_unstarted(process: subprocess.Popen, started: threading.Event, expired: threading.Event) -> None:
        """Stops a runner that has not started a test when the bound of its start expires."""
        if not started.is_set():
            expired.set()
            process.kill()
