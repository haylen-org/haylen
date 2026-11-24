import os
import re
import shlex
import shutil
import subprocess
import textwrap
import threading
import unicodedata
from collections.abc import Callable, Mapping, Sequence
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import TextIO


class Terminal:
    """Prints what the tooling does: the titles of its steps, the commands it runs, successes, warnings, errors, and the groups, tables and signs of what a command lists or sums up. A message marks reserved expressions with double quotes and paths and URLs with backticks. On a terminal with color, reserved expressions print in a calm color without their quotes, URLs print underlined, since terminals open them, and paths print bare. Without color, reserved expressions keep their quotes and paths and URLs print bare, and groups, tables and signs keep their structure as plain text."""

    class Sign(Enum):
        """A small colored symbol in front of a state or a result, with the plain character that a stream whose encoding lacks the symbol prints instead."""

        READY = ("\u25cf", "*", "32")
        IDLE = ("\u25cb", "o", "2")
        UNREACHABLE = ("\u00d7", "x", "33")
        PASSED = ("\u2713", "+", "32")
        FAILED = ("\u2717", "x", "31")
        WARNING = ("!", "!", "33")

        def __init__(self, symbol: str, plain: str, style: str) -> None:
            self.symbol = symbol
            self.plain = plain
            self.style = style

    @dataclass(frozen=True)
    class Cell:
        """A cell of a table with the sign in front of its text."""

        text: str
        sign: Terminal.Sign

    @dataclass(frozen=True)
    class Column:
        """A column of a table: its title, the style of its cells, such as the color of reserved expressions for an identifier to copy, and how it gives up room on a narrow terminal. A column whose `shrink` is 0 keeps its width, and the others shrink from the lowest number up, either wrapping their text between words or, with `cut`, shortening it in the middle. A column that `stacks` moves to a line of its own under each row when the table does not fit otherwise."""

        title: str
        style: str = ""
        shrink: int = 0
        cut: bool = False
        stacks: bool = False

    @dataclass(frozen=True)
    class Layout:
        """The widths of the columns of a table on this terminal, and the columns that print on lines of their own under each row."""

        widths: list[int]
        stacked: frozenset[int]

    MARKUP = re.compile(r'`([^`\n]+)`|"([^"\n]+)"')
    URL = re.compile(r"[a-z][a-z0-9+.-]*://", re.IGNORECASE)
    STEP = "1"
    MARKER = "1;34"
    COMMAND = "2"
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
    # The narrowest room that texts break into, below which they print whole and the terminal breaks them.
    NARROWEST = 24
    # The console mode of Windows that shows escape sequences, which consoles leave off for older programs.
    VIRTUAL_TERMINAL_PROCESSING = 0x0004
    # What an empty cell of a table shows, the columns between two columns and the indent of a table under the title of its group.
    EMPTY = "\u2014"
    GAP = 2
    INDENT = 2
    # The narrowest that a column that cuts its text in the middle shrinks to on a narrow terminal, which keeps enough of both ends to tell a path or a name.
    NARROWEST_CUT = 24

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
        # Whether the symbols of the signs show in the encoding of the output, and whether anything printed yet, after which a group starts with a blank line.
        self.symbols = Terminal.encodes(self.stdout, "".join(sign.symbol for sign in Terminal.Sign) + Terminal.EMPTY)
        self.printed = False

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
    def encodes(stream: TextIO, text: str) -> bool:
        """Tells whether the encoding of a stream holds a text, which a stream without an encoding of its own, such as one in memory, always does."""
        encoding = getattr(stream, "encoding", None)
        if not encoding:
            return True
        try:
            text.encode(encoding)
        except UnicodeEncodeError:
            return False
        return True

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
        rendered = Terminal.MARKUP.sub(lambda match: Terminal.mark(match, base), message)
        return f"{base}{rendered}\x1b[0m" if base else rendered

    @staticmethod
    def plain(message: str) -> str:
        """A message as a cell of a table shows it: the reserved expressions keep their quotes and paths and URLs lose their backticks, with or without color, since a table colors whole columns."""
        return Terminal.render(message, False)

    @staticmethod
    def mark(match: re.Match, base: str) -> str:
        """Colors a reserved expression or a URL of a message, and then returns to the style of its line."""
        if match[1] is None:
            return f"\x1b[{Terminal.EXPRESSION}m{match[2]}\x1b[0m{base}"
        if Terminal.URL.match(match[1]):
            return f"\x1b[{Terminal.LINK}m{match[1]}\x1b[0m{base}"
        return match[1]

    @staticmethod
    def command_line(parts: list[str]) -> str:
        return subprocess.list2cmdline(parts) if os.name == "nt" else shlex.join(parts)

    def step(self, title: str) -> None:
        self.write(f"{Terminal.paint('==>', self.stdout_color, Terminal.MARKER)} {Terminal.render(title, self.stdout_color, Terminal.STEP)}")

    def command(self, parts: list[str]) -> None:
        self.write(Terminal.paint(f"$ {Terminal.command_line(parts)}", self.stdout_color, Terminal.COMMAND))

    def group(self, title: str, count: int | None = None) -> None:
        """Prints the title of a group with the count of what it holds, after a blank line that sets it apart from what the command printed before it."""
        shown = Terminal.render(title, self.stdout_color, Terminal.EMPHASIS)
        if count is not None:
            shown += f"  {Terminal.paint(str(count), self.stdout_color, Terminal.LABEL)}"
        self.write(f"\n{shown}" if self.printed else shown)

    def item(self, sign: Terminal.Sign, message: str, indent: int = INDENT) -> None:
        """Prints one item of a group after its sign, such as a problem or a result, broken between its words at the width of the terminal under the start of its text."""
        lead = " " * indent
        lines = self.fit(message, indent + 2).split("\n")
        shown = [f"{lead}{self.sign(sign)} {Terminal.render(lines[0], self.stdout_color)}"]
        shown += [f"{lead}  {Terminal.render(line, self.stdout_color)}" for line in lines[1:]]
        self.write("\n".join(shown))

    def sign(self, sign: Terminal.Sign) -> str:
        """The symbol of a sign in its color, or its plain character where the encoding of the output lacks the symbol."""
        return Terminal.paint(sign.symbol if self.symbols else sign.plain, self.stdout_color, sign.style)

    def table(self, columns: Sequence[Terminal.Column], rows: Sequence[Sequence[str | Terminal.Cell]], indent: int = INDENT) -> None:
        """Prints a table with a row of dim titles and its columns aligned by the columns that their texts take. On a terminal narrower than the table, the columns that shrink give up room in their order, and when that is not enough a column that stacks prints on a line of its own under each row, so an identifier to copy always stays whole. An empty cell shows a dim dash."""
        layout = self.layout(columns, rows, indent)
        self.write(self.table_text(columns, rows, layout, indent))

    def grouped_table(self, columns: Sequence[Terminal.Column], groups: Sequence[tuple[str, Sequence[Sequence[str | Terminal.Cell]]]]) -> None:
        """Prints groups, each with its title and count and a table of the same columns, which line up from one group to the next."""
        layout = self.layout(columns, [row for _, rows in groups for row in rows], Terminal.INDENT)
        for title, rows in groups:
            self.group(title, len(rows))
            self.write(self.table_text(columns, rows, layout, Terminal.INDENT))

    def layout(self, columns: Sequence[Terminal.Column], rows: Sequence[Sequence[str | Terminal.Cell]], indent: int) -> Terminal.Layout:
        """The width of each column of a table, the widest of its title and its cells, narrowed to the room of the terminal, and the columns that stack under their rows because the table does not fit otherwise."""
        texts = [[Terminal.cell_text(cell, self.symbols) for cell in row] for row in rows]
        natural = [max([Terminal.visible_width(column.title), *(Terminal.visible_width(row[index]) for row in texts)]) for index, column in enumerate(columns)]
        floors = [Terminal.floor(column, [row[index] for row in texts], natural[index]) for index, column in enumerate(columns)]
        room = self.width() - indent
        widths = list(natural)
        stacking = [index for index, column in enumerate(columns) if column.stacks]
        if Terminal.shrink(columns, widths, floors, room) <= 0 or not stacking:
            return Terminal.Layout(widths, frozenset())

        inline = [index for index in range(len(columns)) if index not in stacking]
        inline_widths = [natural[index] for index in inline]
        Terminal.shrink([columns[index] for index in inline], inline_widths, [floors[index] for index in inline], room)
        widths = list(natural)
        for index, width in zip(inline, inline_widths, strict=True):
            widths[index] = width
        return Terminal.Layout(widths, frozenset(stacking))

    def table_text(self, columns: Sequence[Terminal.Column], rows: Sequence[Sequence[str | Terminal.Cell]], layout: Terminal.Layout, indent: int) -> str:
        """The lines of a table in the widths of its layout, with the cells that do not fit broken over several lines and the stacked cells on lines of their own under their row."""
        inline = [index for index in range(len(columns)) if index not in layout.stacked]
        widths = [layout.widths[index] for index in inline]
        titles = [columns[index].title for index in inline]
        lines = [Terminal.table_line([Terminal.paint(title, self.stdout_color, Terminal.LABEL) for title in titles], titles, widths, indent)]
        for row in rows:
            text = [Terminal.cell_text(row[index], self.symbols) for index in inline]
            parts = [Terminal.fit_cell(cell, widths[place], columns[index]) for place, (index, cell) in enumerate(zip(inline, text, strict=True))]
            for line in range(max(len(part) for part in parts)):
                plain = [part[line] if line < len(part) else "" for part in parts]
                shown = [self.style_cell(row[index], plain[place], line, columns[index]) for place, index in enumerate(inline)]
                lines.append(Terminal.table_line(shown, plain, widths, indent))
            for index in sorted(layout.stacked):
                stacked = Terminal.cell_text(row[index], self.symbols)
                lines.append(" " * (indent + Terminal.GAP) + self.style_cell(row[index], stacked, 0, columns[index]))
        return "\n".join(lines)

    @staticmethod
    def floor(column: Terminal.Column, texts: list[str], natural: int) -> int:
        """The narrowest that a column shrinks to: the width of its title, and for a column that wraps its longest word, so no word breaks, or for a column that cuts the narrowest cut, never more than its whole width."""
        title = Terminal.visible_width(column.title)
        if column.cut:
            return min(natural, max(title, Terminal.NARROWEST_CUT))
        longest = max([Terminal.visible_width(word) for text in texts for word in text.split()], default=0)
        return min(natural, max(title, longest))

    @staticmethod
    def shrink(columns: Sequence[Terminal.Column], widths: list[int], floors: list[int], room: int) -> int:
        """Narrows the columns that shrink, in their order, until the table fits the room of the terminal, never below their floor, and returns the columns by which it still does not fit. A room of 0 or less, as of output to a file, keeps every width."""
        if room <= 0:
            return 0
        excess = sum(widths) + Terminal.GAP * (len(widths) - 1) - room
        for index in sorted((index for index, column in enumerate(columns) if column.shrink), key=lambda index: columns[index].shrink):
            if excess <= 0:
                break
            taken = min(excess, max(0, widths[index] - floors[index]))
            widths[index] -= taken
            excess -= taken
        return excess

    @staticmethod
    def cell_text(cell: str | Terminal.Cell, symbols: bool) -> str:
        """The plain text that a cell shows, with the symbol of its sign in front and a dash for an empty cell."""
        if isinstance(cell, Terminal.Cell):
            return f"{cell.sign.symbol if symbols else cell.sign.plain} {cell.text}"
        if cell:
            return cell
        return Terminal.EMPTY if symbols else "-"

    @staticmethod
    def fit_cell(text: str, width: int, column: Terminal.Column) -> list[str]:
        """The lines of a cell in the width of its column: the text whole when it fits, else shortened in its middle or broken between its words, and a word wider than the column broken where the column ends."""
        if Terminal.visible_width(text) <= width:
            return [text]
        if column.cut:
            return [Terminal.shorten(text, width)]
        return textwrap.wrap(text, width, break_on_hyphens=False) or [""]

    def style_cell(self, cell: str | Terminal.Cell, plain: str, line: int, column: Terminal.Column) -> str:
        """Colors one line of a cell: the sign of its first line in the color of the sign, an empty cell dim and the text in the style of its column."""
        if not plain:
            return plain
        if isinstance(cell, Terminal.Cell) and line == 0:
            _, _, text = plain.partition(" ")
            return f"{self.sign(cell.sign)} {Terminal.paint(text, self.stdout_color, column.style)}"
        if not cell:
            return Terminal.paint(plain, self.stdout_color, Terminal.LABEL)
        return Terminal.paint(plain, self.stdout_color, column.style)

    @staticmethod
    def table_line(shown: list[str], plain: list[str], widths: list[int], indent: int) -> str:
        """Joins the cells of one line of a table, each padded by the columns that its plain text takes, without spaces after the last text."""
        padded = [text + " " * (width - Terminal.visible_width(text_plain)) for text, text_plain, width in zip(shown, plain, widths, strict=True)]
        return (" " * indent + (" " * Terminal.GAP).join(padded)).rstrip()

    def rows(self, rows: list[tuple[str, str]], keys: bool = False) -> None:
        """Prints rows of a name and a text with the names aligned, such as the summary of a running app with dim labels, or its keys and the commands of its console in the color of reserved expressions. A text wider than the terminal breaks between its words and goes on under the start of the texts."""
        width = max(len(name) for name, _ in rows)
        style = Terminal.EXPRESSION if keys else Terminal.LABEL
        indent = " " * (width + 4)
        room = self.width() - len(indent)
        for name, text in rows:
            lines = Terminal.wrap(text, room, self.stdout_color) if room >= Terminal.NARROWEST else [text]
            shown = [f"  {Terminal.paint(name.ljust(width), self.stdout_color, style)}  {Terminal.render(lines[0], self.stdout_color)}"]
            shown += [indent + Terminal.render(line, self.stdout_color) for line in lines[1:]]
            self.write("\n".join(shown))

    @staticmethod
    def wrap(message: str, width: int, color: bool) -> list[str]:
        """Breaks a message into lines of at most a number of columns between its words, measured as they show, and never inside a reserved expression, a path or a URL. A word wider than the line keeps a line of its own."""
        # The spaces inside the marks become a character that no message holds, so the words split around them.
        joined = Terminal.MARKUP.sub(lambda match: match[0].replace(" ", "\0"), message)
        lines: list[str] = []
        line = ""
        line_width = 0
        for word in joined.split():
            text = word.replace("\0", " ")
            columns = Terminal.visible_width(Terminal.render(text, color))
            if line and line_width + 1 + columns > width:
                lines.append(line)
                line = ""
            line_width = line_width + 1 + columns if line else columns
            line = f"{line} {text}" if line else text
        return [*lines, line]

    def width(self) -> int:
        """The columns that a line of the terminal holds without the terminal breaking it, or 0 when the output goes to a file or a pipe, which keep every line whole."""
        return shutil.get_terminal_size().columns - 1 if self.stdout.isatty() else 0

    def fit(self, message: str, used: int = 0) -> str:
        """Breaks every line of a message between its words at the width of the terminal, less the columns that print before it, such as those of the word "Error:", so the terminal never breaks a word. A line keeps its indent on each of its parts."""
        room = self.width() - used
        fitted = []
        for text in message.split("\n"):
            body = text.lstrip()
            indent = text[: len(text) - len(body)]
            parts = Terminal.wrap(body, room - len(indent), self.stdout_color) if room - len(indent) >= Terminal.NARROWEST else [body]
            fitted += [indent + part for part in parts]
        return "\n".join(fitted)

    @staticmethod
    def visible_width(text: str) -> int:
        """The columns of a terminal that a text takes, without the escape sequences of its colors."""
        return sum(map(Terminal.columns, Terminal.graphemes(Terminal.ESCAPE.sub("", text))))

    def info(self, message: str) -> None:
        self.write(Terminal.render(message, self.stdout_color))

    def note(self, message: str) -> None:
        """Prints a message that guides the developer in a dim color."""
        self.write(Terminal.render(message, self.stdout_color, Terminal.LABEL))

    def success(self, message: str) -> None:
        """Prints what a command achieved after the sign of a result that passed."""
        self.write(f"{self.sign(Terminal.Sign.PASSED)} {Terminal.render(message, self.stdout_color)}")

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
    def graphemes(text: str) -> list[str]:
        """The graphemes of a text, the characters that a person sees, each with the marks and joined characters that combine with it."""
        return [str(grapheme) for grapheme in unicodedata.iter_graphemes(text)]

    @staticmethod
    def columns(grapheme: str) -> int:
        """The columns of a terminal that a grapheme takes: those of its first character, which is two for a wide character of East Asian scripts, none for a mark without a character to combine with and one for any other."""
        first = grapheme[0]
        if unicodedata.combining(first):
            return 0
        return 2 if unicodedata.east_asian_width(first) in "WF" else 1

    @staticmethod
    def shorten(text: str, width: int) -> str:
        """Fits a line into a number of columns by turning its middle into an ellipsis between whole graphemes, so its start and its end, such as the file that a tool works on, stay visible."""
        graphemes = Terminal.graphemes(text)
        if sum(map(Terminal.columns, graphemes)) <= width:
            return text
        if width < 1:
            return ""
        room = width - 1
        head = ""
        head_width = 0
        for grapheme in graphemes:
            if head_width + Terminal.columns(grapheme) > (room + 1) // 2:
                break
            head += grapheme
            head_width += Terminal.columns(grapheme)

        tail = ""
        tail_width = 0
        for grapheme in reversed(graphemes):
            if tail_width + Terminal.columns(grapheme) > room - head_width:
                break
            tail = grapheme + tail
            tail_width += Terminal.columns(grapheme)
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
            self.printed = True
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
