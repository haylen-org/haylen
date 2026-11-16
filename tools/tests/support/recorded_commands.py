import io
from collections.abc import Callable, Mapping, Sequence
from pathlib import Path

from haylen.core.tooling_context import ToolingContext
from haylen.process import Process
from haylen.terminal import Terminal


class RecordedCommands:
    """A tooling context whose native tools never run: every command that the code under test runs or captures is recorded with its options, and a capture answers with what `answer` returns for its command line."""

    def __init__(self, folder: Path, environment: Mapping[str, str] | None = None, answer: Callable[[list[str]], str] | None = None) -> None:
        self.output = io.StringIO()
        terminal = Terminal(self.output, self.output, {}, folder / "logs" / "test.log")
        process = Process(terminal)
        process.run = self.run
        process.capture = self.capture
        process.attempt = self.attempt
        self.context = ToolingContext(terminal, process, 4, dict(environment or {}))
        self.answer = answer or (lambda command: "")
        self.commands: list[list[str]] = []
        self.options: list[dict[str, object]] = []

    def run(self, command: Sequence[object], **options: object) -> None:
        self.commands.append([str(part) for part in command])
        self.options.append(options)

    def attempt(self, command: Sequence[object]) -> tuple[int, bytes]:
        parts = [str(part) for part in command]
        self.commands.append(parts)
        self.options.append({})
        return 0, self.answer(parts).encode("utf-8")

    def capture(self, command: Sequence[object], **options: object) -> str:
        parts = [str(part) for part in command]
        self.commands.append(parts)
        self.options.append(options)
        return self.answer(parts)
