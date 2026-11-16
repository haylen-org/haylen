import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.engine.consumers import Consumers


class EngineConsumersCommand(Command):
    PATH = ("engine", "consumers")
    HELP = 'Build the consumer project of "engine/tests/consumer", which adds the engine like another repository does, in each way that "--mode" names or in every way: through "add_subdirectory" and CPM from the sources, and through "find_package" from the installed SDK of this machine.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--mode", action="append", dest="modes", choices=list(Consumers.MODES), help='A way to add the engine, "subdirectory", "cpm" or "package", which repeats. Every way by default.'
        )
        parser.add_argument("--config", default="Release", choices=["Debug", "Release"], help='The configuration of the consumer project, "Release" by default, which the SDK of "package" matches.')
        parser.add_argument(
            "--sdk",
            help='The folder of an installed SDK, with its "haylen-sdk.json", whose slice of this machine "package" finds the engine in, by default the installed desktop SDK of this machine, which "haylen sdk build --target player" builds.',
        )

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        return Consumers(session.context, session.registry).run(arguments.modes or list(Consumers.MODES), arguments.config, arguments.sdk)
