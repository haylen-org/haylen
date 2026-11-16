import subprocess
import sys
import threading
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path
from typing import TextIO

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.development.console_line import ConsoleLine
from haylen.development.development_server import DevelopmentServer
from haylen.development.key_reader import KeyReader
from haylen.process import Process
from haylen.terminal import Terminal


class RunSession:
    """What the terminal shows and takes while an app runs: the summary of the run, then the log of the app and the keys that reload it, restart it, open its developer console, show its statistics, its inspector and its debug drawings, capture a frame of its GPU, clear the screen, quit and list the keys. The console takes the keys while it is open, with a line at the bottom of the terminal that the log never mixes with, and Escape gives them back. A session without a development server, such as one of a release, offers only the keys that need none."""

    # The keys of a running app with what each does, in the order that their list shows them.
    KEYS = {
        "r": "Reload what changed",
        "R": "Restart the app",
        "d": "Open the developer console, which runs commands in the app",
        "s": "Show the statistics of the app, compact, full or off in turn",
        "i": "Show or hide the inspector of the app",
        "p": "Turn the debug drawings of the app on or off",
        "g": "Capture a frame of the GPU",
        "c": "Clear the screen",
        "q": "Quit",
        "h": "List the keys",
    }
    DEVELOPMENT_KEYS = ("r", "R", "d", "s", "i", "p", "g")
    # The lines of the developer console that the keys of the statistics, the inspector, the drawings and the GPU capture run.
    KEY_LINES = {"s": "stats", "i": "inspector", "p": "draw", "g": "gpuCapture"}
    CONSOLE_HINT = 'The console runs each line in the app. Type "help" to list the commands. Tab completes, the arrows go through the lines typed before, and Escape goes back to the keys.'
    # How long a process that the session stops takes to end before the session kills it.
    STOP_SECONDS = 10

    def __init__(self, context: ToolingContext, development: DevelopmentServer | None = None, address: str | None = None) -> None:
        self.context = context
        self.terminal = context.terminal
        self.development = development
        # The WebSocket address that the app connects to the development server at.
        self.address = address
        self.keys = {key: text for key, text in RunSession.KEYS.items() if development is not None or key not in RunSession.DEVELOPMENT_KEYS}
        self.reader = KeyReader(sys.stdin) if sys.stdin.isatty() else None
        self.stop: Callable[[], None] | None = None
        self.process: subprocess.Popen | None = None
        self.quitting = threading.Event()
        self.console = ConsoleLine()
        self.typing = False

    @property
    def arguments(self) -> list[str]:
        """The arguments of the command line that connect the app to the development server."""
        return ["--dev-server", self.address] if self.address is not None else []

    def end(self, failed: bool) -> None:
        """Gives the terminal back, and stops the app that the session follows when a signal or an error ends the session, since the app would otherwise run on with no one to relay its log."""
        self.terminal.show_prompt(None)
        if self.reader is not None:
            self.reader.close()
        if not failed or self.process is None or self.process.poll() is not None:
            return

        self.quit()
        try:
            self.process.wait(RunSession.STOP_SECONDS)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()

    def show(self, rows: list[tuple[str, str]], stop: Callable[[], None] | None = None) -> None:
        """Prints the summary of the run with its keys, and takes the keys from then on. Quitting calls the stop of the platform, or else ends the process that the session follows."""
        self.stop = stop
        self.terminal.verbatim("")
        self.terminal.rows(rows)
        self.terminal.verbatim("")
        self.list_keys()
        if self.reader is not None:
            Terminal.enable_escapes(self.terminal.stdout)
            self.reader.open()
            threading.Thread(target=self.take_keys, daemon=True).start()

    def list_keys(self) -> None:
        self.terminal.rows(list(self.keys.items()), keys=True)
        self.terminal.verbatim("")

    def take_keys(self) -> None:
        while key := self.reader.key():
            self.press(key)

    def press(self, key: str) -> None:
        """Does what a key asks, and leaves every other key alone, such as the keys of the arrows. While the console is open, every key edits its line."""
        if self.typing:
            self.type(key)
            return
        if key not in self.keys:
            return

        if key == "r":
            self.development.reload()
        elif key == "R":
            self.development.restart()
        elif key == "d":
            self.open_console()
        elif key in RunSession.KEY_LINES:
            self.development.send_line(RunSession.KEY_LINES[key])
        elif key == "c":
            self.terminal.clear()
        elif key == "q":
            self.quit()
        else:
            self.list_keys()

    def open_console(self) -> None:
        self.typing = True
        self.console.commands = self.development.commands
        self.terminal.note(RunSession.CONSOLE_HINT)
        self.terminal.show_prompt(lambda: self.console.render(self.terminal.stdout_color))
        self.development.request_commands()

    def close_console(self) -> None:
        self.typing = False
        self.console.replace("")
        self.terminal.show_prompt(None)

    def type(self, key: str) -> None:
        """Edits the line of the console with a key: Enter runs the line in every connected app, Tab completes it, and Escape closes the console."""
        if key == "Escape":
            self.close_console()
            return

        self.console.commands = self.development.commands
        if key == "Tab":
            candidates = self.console.complete()
            if candidates:
                self.terminal.note("  ".join(candidates))
            self.terminal.draw_prompt()
            return

        line = self.console.edit(key)
        if not line:
            self.terminal.draw_prompt()
            return
        self.terminal.write(Terminal.paint(ConsoleLine.PROMPT, self.terminal.stdout_color, Terminal.EXPRESSION) + line)
        self.development.send_line(line)
        self.development.request_commands()

    def quit(self) -> None:
        self.quitting.set()
        if self.stop is not None:
            self.stop()
        elif self.process is not None:
            self.process.terminate()

    def follow(
        self,
        command: Sequence[object],
        *,
        cwd: Path | None = None,
        line: Callable[[str], str | None] | None = None,
        environment: Mapping[str, str] | None = None,
    ) -> None:
        """Streams the output of the process that runs the app, or that relays its log, until it ends, passing each line of its standard output and its standard error through the function of the line when one is given, which leaves out a line by returning nothing, with the variables of the environment added to those of the command. The lines print through the terminal, so they go above the line of the console. It fails when the process fails, unless the developer quit."""
        parts = [str(part) for part in command]
        if self.terminal.verbose:
            self.terminal.command(parts)
        variables = {**self.context.environment, **(environment or {})}
        # The input of the terminal belongs to the keys of the session, so the process never takes them.
        try:
            self.process = subprocess.Popen(
                parts,
                cwd=cwd or Repository.ROOT,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=variables,
            )
        except FileNotFoundError as error:
            raise Process.missing(parts[0]) from error
        if self.quitting.is_set():
            self.process.terminate()

        errors = threading.Thread(target=self.relay, args=(self.process.stderr, line, True), daemon=True)
        errors.start()
        self.relay(self.process.stdout, line, False)
        errors.join()
        code = self.process.wait()
        if self.quitting.is_set() or code == 0:
            return

        name = Path(parts[0]).name
        if code < 0:
            raise BuildFailed(f'The command "{name}" was stopped by the signal {-code}.')
        raise BuildFailed(f'The command "{name}" failed with exit code {code}, and its output above shows why.')

    def relay(self, stream: TextIO, line: Callable[[str], str | None] | None, error: bool) -> None:
        for text in stream:
            stripped = text.rstrip("\n")
            shown = line(stripped) if line is not None else stripped
            if shown is not None:
                self.terminal.verbatim(shown, error)
