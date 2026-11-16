import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.content_arguments import ContentArguments
from haylen.core.app_manifest import AppManifest


class ContentCompactCommand(Command):
    PATH = ("content", "compact")
    HELP = "Remove the manifests and packs of a publication tree of an app that no channel reaches within its last generations."

    # A compaction keeps the current generation of every channel and the one before it, so apps that update during it still find their packs.
    KEPT_GENERATIONS = 2

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("tree", help="The folder of the publication tree.")
        parser.add_argument("--keep", type=int, default=ContentCompactCommand.KEPT_GENERATIONS, help="The generations of every channel to keep, 2 by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        app = AppManifest.load(Arguments.app_folder(arguments))
        tree = Path(arguments.tree).expanduser()
        report = ContentArguments.releases(session).compact(app, tree, arguments.keep)
        return {"tree": str(tree.resolve()), "report": ContentArguments.report(session, report)}
