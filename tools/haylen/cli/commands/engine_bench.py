import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.commands.test import TestCommand
from haylen.engine.benchmarks import Benchmarks


class EngineBenchCommand(Command):
    PATH = ("engine", "bench")
    HELP = "Build a benchmark of the engine in Release and run it on this machine."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--suite",
            default="sprites",
            choices=Benchmarks.SUITES,
            help="The sprite benchmark on the GPU, the core, algorithm, procedural, physics, UI, render, particle, content, node or animation benchmark on the CPU, or the Lua sprite benchmark.",
        )
        parser.add_argument(
            "--graphics-api",
            choices=TestCommand.GRAPHICS_APIS,
            help='A name of "gpu::Device::kApiNames", such as "vulkan", which a benchmark that draws draws through instead of the null device of the headless host or the first API of the window.',
        )
        parser.add_argument("--gpu", action="store_true", help="Run a benchmark that draws once on every graphics API that the build compiled, past the ones that cannot start on this machine.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        return Benchmarks(session.context).run(arguments.suite, arguments.graphics_api, arguments.gpu)
