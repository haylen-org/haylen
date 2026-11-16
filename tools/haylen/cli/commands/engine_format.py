import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.engine.source_format import SourceFormat


class EngineFormatCommand(Command):
    PATH = ("engine", "format")
    HELP = 'Format the C, C++ and Objective-C sources with "clang-format" and the Python of "tools" with "ruff", or check them with "--check", which also fails on multi-line lambdas without their "clang-format off" and "on" markers.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--check", action="store_true", help="Fail on what is not formatted instead of rewriting files.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        return SourceFormat(session.context).run(arguments.check)
