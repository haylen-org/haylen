import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.content_arguments import ContentArguments
from haylen.content.key_folder import KeyFolder
from haylen.core.app_manifest import AppManifest
from haylen.core.repository import Repository


class ContentKeysCommand(Command):
    PATH = ("content", "keys")
    HELP = 'Show the key IDs and the public signing key of an app, creating its keys the first time, or add a content key for the next builds with "--rotate". No command prints a key.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("--rotate", action="store_true", help="Add a content key that encrypts the content of the next builds, while the earlier keys stay for installed content.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        app = AppManifest.load(Arguments.app_folder(arguments))
        report = ContentArguments.releases(session).show_keys(app, arguments.rotate)
        lines = ContentArguments.report(session, report)
        folder = KeyFolder.of(app.identifier, session.context.environment).folder
        session.terminal.info(f"The keys of the app live in `{Repository.shown(folder)}`.")
        return {"folder": str(folder), "report": lines}
