import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.content_arguments import ContentArguments
from haylen.core.app_manifest import AppManifest


class ContentDiffCommand(Command):
    PATH = ("content", "diff")
    HELP = "Compare two protected releases of an app: the chunks, shards and files that the later one adds, reuses and drops, and what an installed app downloads to reach it."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("earlier", help="The folder of the earlier release.")
        parser.add_argument("later", help="The folder of the later release.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        app = AppManifest.load(Arguments.app_folder(arguments))
        earlier = Path(arguments.earlier).expanduser()
        later = Path(arguments.later).expanduser()
        report = ContentArguments.releases(session).diff(app, earlier, later)
        return {"earlier": str(earlier.resolve()), "later": str(later.resolve()), "report": ContentArguments.report(session, report)}
