import json
import subprocess
import threading
import time
from collections.abc import Iterable, Sequence

from haylen.terminal import Terminal


class UnifiedLog:
    """Relays what the engine of an app process writes to the unified log of Apple platforms while the block runs, and keeps the errors it logged, which fill as they arrive. The engine logs under the bundle identifier of the app as the subsystem and the category `haylen`. The prefix runs `log` inside a simulator."""

    CATEGORY = "haylen"
    # Events reach the stream a moment after the app writes them, so the last lines of an app that ended still arrive within this time.
    GRACE_SECONDS = 1.0

    def __init__(self, terminal: Terminal, executable: str, prefix: Sequence[object]) -> None:
        self.terminal = terminal
        self.executable = executable
        self.prefix = [str(part) for part in prefix]
        self.errors: list[str] = []
        self.stream: subprocess.Popen | None = None

    def __enter__(self) -> list[str]:
        predicate = f'process == "{self.executable}" AND category == "{UnifiedLog.CATEGORY}"'
        command = [*self.prefix, "log", "stream", "--style", "ndjson", "--level", "debug", "--predicate", predicate]
        self.stream = subprocess.Popen(command, stdout=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
        # The stream names its filter on the first line and delivers events from then on, so the app starts after that line.
        self.stream.stdout.readline()
        threading.Thread(target=UnifiedLog.relay, args=(self.stream.stdout, self.errors, self.terminal), daemon=True).start()
        return self.errors

    def __exit__(self, *_: object) -> None:
        time.sleep(UnifiedLog.GRACE_SECONDS)
        self.stream.terminate()
        self.stream.wait()

    @staticmethod
    def relay(lines: Iterable[str], errors: list[str], terminal: Terminal) -> None:
        """Prints the message of every event of a stream in the `ndjson` style, with warnings and errors on the error output like the engine on a desktop, and keeps the errors."""
        for line in lines:
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                terminal.verbatim(line.rstrip())
                continue
            message = event.get("eventMessage", "")
            failed = event.get("messageType") in ("Error", "Fault")
            if failed:
                errors.append(message)
            terminal.verbatim(message, error=failed)
