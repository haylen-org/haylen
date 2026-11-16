import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.engine.fuzzers import Fuzzers


class EngineFuzzCommand(Command):
    PATH = ("engine", "fuzz")
    HELP = "Build the fuzzers of the parsers that read hostile bytes with a Clang that links libFuzzer, and run each one for a while."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--seconds", type=int, default=60, help="How long each fuzzer runs, 60 seconds by default.")
        parser.add_argument("--llvm", help='The "bin" folder of an LLVM whose Clang links libFuzzer, which "HAYLEN_LLVM" also names, or else the LLVM of a package manager.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        return Fuzzers(session.context).run(arguments.seconds, arguments.llvm)
