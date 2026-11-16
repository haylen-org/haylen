import os
import re
import shlex
import shutil
import subprocess
import threading
import unicodedata
from collections.abc import Callable, Mapping
from pathlib import Path
from typing import TextIO


class Terminal:
    """Prints what the tooling does: the titles of its steps, the commands it runs, successes, warnings, errors and the summary of a running app. A message marks reserved expressions with double quotes and paths and URLs with backticks. On a terminal with color, reserved expressions print in a calm color without their quotes, URLs print underlined, since terminals open them, and paths print bare. Without color, reserved expressions keep their quotes and paths and URLs print bare."""

    MARKUP = re.compile(r'`([^`\n]+)`|"([^"\n]+)"')
    URL = re.compile(r"[a-z][a-z0-9+.-]*://", re.IGNORECASE)
    STEP = "1"
    MARKER = "1;34"
    COMMAND = "2"
    SUCCESS = "32"
    WARNING = "1;33"
    ERROR = "1;31"
    EMPHASIS = "1"
    LABEL = "2"
    # A soft teal of the 256 colors that every terminal with escape sequences shows, which sets reserved expressions apart from the prose without drawing the eye to them.
    EXPRESSION = "38;5;73"
    LINK = "4"
    # The escape sequences of colors and of the cursor that tools print, which take no columns of the line.
    ESCAPE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]|\x1b[@-Z\\-_]")
    ELLIPSIS = "\u2026"
    # The console mode of Windows that shows escape sequences, which consoles leave off for older programs.
    VIRTUAL_TERMINAL_PROCESSING = 0x0004

    def __init__(self, stdout: TextIO, stderr: TextIO, environment: Mapping[str, str], log: Path, *, structured: bool = False) -> None:
        # Structured output keeps the standard output for the JSON document of the result, so everything the terminal prints goes to the error output, and so does the output of the tools that stream.
        self.structured = structured
        self.stdout = stderr if structured else stdout
        self.stderr = stderr
        self.stdout_color = Terminal.shows_color(self.stdout, environment)
        self.stderr_color = Terminal.shows_color(stderr, environment)
        # Whether the tools print their whole output in the terminal, which "--verbose" turns on, and the log that keeps it otherwise.
        self.verbose = False
        self.log = log
        # The line that the developer types in the console of a running app, which the terminal keeps at the bottom while it prints, and the lock that keeps the prints of every thread whole.
        self.prompt: Callable[[], str] | None = None
        self.lock = threading.RLock()

    @property
    def tool_output(self) -> TextIO | None:
        """Where a tool whose output streams writes it: the standard output of the command, or the error output with structured output, whose standard output holds only the JSON document of the result."""
        return self.stderr if self.structured else None

    @staticmethod
    def shows_color(stream: TextIO, environment: Mapping[str, str]) -> bool:
        """Tells whether a stream prints color: never with `NO_COLOR`, always with `FORCE_COLOR`, and otherwise on a terminal that is not `dumb`."""
        if environment.get("NO_COLOR"):
            return False
        terminal = stream.isatty() and Terminal.enable_escapes(stream)
        return bool(environment.get("FORCE_COLOR")) or (terminal and environment.get("TERM") != "dumb")

    @staticmethod
    def enable_escapes(stream: TextIO) -> bool:
        """Turns on escape sequences in a Windows console and tells whether the terminal of a stream shows them, which every other terminal does."""
        if os.name != "nt":
            return True
        import ctypes
        import msvcrt

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        handle = msvcrt.get_osfhandle(stream.fileno())
        mode = ctypes.c_uint32()
        return bool(kernel32.GetConsoleMode(handle, ctypes.byref(mode))) and bool(kernel32.SetConsoleMode(handle, mode.value | Terminal.VIRTUAL_TERMINAL_PROCESSING))

    @staticmethod
    def paint(text: str, color: bool, style: str) -> str:
        return f"\x1b[{style}m{text}\x1b[0m" if color and style else text

    @staticmethod
    def render(message: str, color: bool, style: str = "") -> str:
        """Turns the marks of a message into color within the style of the line, or without color keeps the quotes of reserved expressions and drops the backticks of paths and URLs."""
        if not color:
            return Terminal.MARKUP.sub(lambda match: match[0] if match[1] is None else match[1], message)
        base = f"\x1b[{style}m" if style else ""

        def mark(match: re.Match) -> str:
            if match[1] is None:
                return f"\x1b[{Terminal.EXPRESSION}m{match[2]}\x1b[0m{base}"
            if Terminal.URL.match(match[1]):
                return f"\x1b[{Terminal.LINK}m{match[1]}\x1b[0m{base}"
            return match[1]

        rendered = Terminal.MARKUP.sub(mark, message)
        return f"{base}{rendered}\x1b[0m" if base else rendered

    @staticmethod
    def command_line(parts: list[str]) -> str:
        return subprocess.list2cmdline(parts) if os.name == "nt" else shlex.join(parts)

    def step(self, title: str) -> None:
        self.write(f"{Terminal.paint('==>', self.stdout_color, Terminal.MARKER)} {Terminal.render(title, self.stdout_color, Terminal.STEP)}")

    def command(self, parts: list[str]) -> None:
        self.write(Terminal.paint(f"$ {Terminal.command_line(parts)}", self.stdout_color, Terminal.COMMAND))

    def heading(self, text: str) -> None:
        self.write(Terminal.paint(text, self.stdout_color, Terminal.EMPHASIS))

    def rows(self, rows: list[tuple[str, str]], keys: bool = False) -> None:
        """Prints rows of a name and a text with the names aligned, such as the summary of a running app with dim labels, or its keys in the color of reserved expressions."""
        width = max(len(name) for name, _ in rows)
        style = Terminal.EXPRESSION if keys else Terminal.LABEL
        for name, text in rows:
            self.write(f"  {Terminal.paint(name.ljust(width), self.stdout_color, style)}  {Terminal.render(text, self.stdout_color)}")

    def info(self, message: str) -> None:
        self.write(Terminal.render(message, self.stdout_color))

    def note(self, message: str) -> None:
        """Prints a message that guides the developer in a dim color."""
        self.write(Terminal.render(message, self.stdout_color, Terminal.LABEL))

    def success(self, message: str) -> None:
        self.write(Terminal.render(message, self.stdout_color, Terminal.SUCCESS))

    def warning(self, message: str, *notes: str) -> None:
        """Prints a warning with the notes that tell what to do about it, each on a line of its own."""
        lines = [f"{Terminal.paint('Warning:', self.stderr_color, Terminal.WARNING)} {Terminal.render(message, self.stderr_color)}"]
        lines += [f"  {Terminal.render(note, self.stderr_color)}" for note in notes]
        self.write("\n".join(lines), error=True)

    def error(self, message: str, details: str = "") -> None:
        """Prints an error with its cause and what to do in one block, followed by the output of the tool that failed, as it is."""
        lines = [f"{Terminal.paint('Error:', self.stderr_color, Terminal.ERROR)} {Terminal.render(message, self.stderr_color, Terminal.EMPHASIS)}"]
        if details:
            lines.append(Terminal.paint(details, self.stderr_color, Terminal.COMMAND))
        self.write("\n".join(lines), error=True)

    def clear(self) -> None:
        """Clears the screen and the scrollback of a terminal, and nothing elsewhere."""
        if self.stdout_color:
            with self.lock:
                print("\x1b[2J\x1b[3J\x1b[H", end="", file=self.stdout, flush=True)
                self.draw_prompt()

    @staticmethod
    def columns(character: str) -> int:
        """The columns of a terminal that a character takes: none for a mark that combines with the character before it, two for a wide character of East Asian scripts and one for any other."""
        if unicodedata.combining(character):
            return 0
        return 2 if unicodedata.east_asian_width(character) in "WF" else 1

    @staticmethod
    def shorten(text: str, width: int) -> str:
        """Fits a line into a number of columns by turning its middle into an ellipsis, so its start and its end, such as the file that a tool works on, stay visible."""
        if sum(map(Terminal.columns, text)) <= width:
            return text
        if width < 1:
            return ""
        room = width - 1
        head = ""
        head_width = 0
        for character in text:
            if head_width + Terminal.columns(character) > (room + 1) // 2:
                break
            head += character
            head_width += Terminal.columns(character)

        tail = ""
        tail_width = 0
        for character in reversed(text):
            if tail_width + Terminal.columns(character) > room - head_width:
                break
            tail = character + tail
            tail_width += Terminal.columns(character)
        return f"{head}{Terminal.ELLIPSIS}{tail}"

    def progress(self, line: str) -> None:
        """Shows the latest line of a quiet tool in place on a terminal with color, without the colors of the tool and shortened in its middle to the width of the terminal, and nothing elsewhere."""
        if self.stdout_color:
            width = shutil.get_terminal_size().columns - 1
            text = Terminal.ESCAPE.sub("", line).expandtabs().strip()
            print(f"\r\x1b[2K{Terminal.paint(Terminal.shorten(text, width), True, Terminal.COMMAND)}", end="", file=self.stdout, flush=True)

    def end_progress(self) -> None:
        if self.stdout_color:
            print("\r\x1b[2K", end="", file=self.stdout, flush=True)

    def verbatim(self, text: str, error: bool = False) -> None:
        """Prints text that must stay as it is, such as a snippet to copy or a diff."""
        self.write(text, error)

    def write(self, text: str, error: bool = False) -> None:
        """Prints a line from any thread. While the developer types in a console, the line goes above the line being typed, which stays at the bottom, so the two never mix."""
        with self.lock:
            if self.prompt is not None:
                print("\r\x1b[2K", end="", file=self.stdout, flush=True)
            print(text, file=self.stderr if error else self.stdout, flush=True)
            self.draw_prompt()

    def show_prompt(self, prompt: Callable[[], str] | None) -> None:
        """Keeps the line that a console draws at the bottom of the terminal, or takes it away with nothing."""
        with self.lock:
            if self.prompt is not None and prompt is None:
                print("\r\x1b[2K", end="", file=self.stdout, flush=True)
            self.prompt = prompt
            self.draw_prompt()

    def draw_prompt(self) -> None:
        """Draws the line of a console again, such as after a key edited it."""
        with self.lock:
            if self.prompt is not None:
                print(f"\r\x1b[2K{self.prompt()}", end="", file=self.stdout, flush=True)
