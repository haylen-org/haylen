import argparse
from abc import ABC, abstractmethod
from collections.abc import Mapping

from haylen.cli.command_session import CommandSession


class Command(ABC):
    """A command of the command line, such as `app build`: the words that name it, its help, its options and what it does. It calls the operations of the core and returns what structured output prints, so the terminal and continuous integration see the same result."""

    PATH: tuple[str, ...] = ()
    HELP = ""

    @abstractmethod
    def configure(self, parser: argparse.ArgumentParser) -> None:
        """Adds the arguments and options of the command."""

    @abstractmethod
    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object] | None:
        """Runs the command and returns its result, which `--json` prints as the result of the command."""
