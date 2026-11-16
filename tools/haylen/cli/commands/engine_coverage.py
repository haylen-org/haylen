import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.engine.coverage import Coverage
from haylen.engine.engine_tree import EngineTree


class EngineCoverageCommand(Command):
    PATH = ("engine", "coverage")
    HELP = "Measure the code coverage of the engine tests with the source-based coverage of LLVM, and write its report."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--sanitizers", choices=EngineTree.SANITIZERS, help="Measure with AddressSanitizer and UndefinedBehaviorSanitizer, or with ThreadSanitizer, too.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        return Coverage(session.context).run(arguments.sanitizers)
