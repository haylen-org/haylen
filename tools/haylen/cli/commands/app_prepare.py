import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository


class AppPrepareCommand(Command):
    PATH = ("app", "prepare")
    HELP = "Generate the native project of an app for a target in its build folder, the same way for the same inputs and writing only what changed, without building it."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_product(parser)
        Arguments.add_previous_release(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        product = session.pipeline.resolve(Arguments.request(arguments, session, arguments.previous_release), build_sdk=True)
        prepared = session.pipeline.prepare(product)
        state = "Prepared" if prepared.changed else "The inputs did not change, so nothing changed in"
        session.terminal.success(f"{state} the project `{Repository.shown(prepared.project)}`.")
        return prepared.describe()
