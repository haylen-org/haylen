import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.content_arguments import ContentArguments
from haylen.content.protected_releases import ProtectedReleases
from haylen.core.app_manifest import AppManifest


class ContentInspectCommand(Command):
    PATH = ("content", "inspect")
    HELP = 'Show the manifests, shards and files of a protected release of an app, the last one of the content profile of a target or the one that "--release" names.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        ContentArguments.add_target(parser)
        parser.add_argument("--release", help="The release folder to inspect instead of the last release of the build folder.")
        parser.add_argument("--chunks", action="store_true", help="Show every chunk of every file too.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        _, profile = ContentArguments.profile(arguments, session)
        app = AppManifest.load(Arguments.app_folder(arguments))
        release = ProtectedReleases.last(app, profile, Path(arguments.release).expanduser() if arguments.release else None)
        report = ContentArguments.releases(session).inspect(app, release.folder, arguments.chunks)
        return {"release": str(release.folder), "report": ContentArguments.report(session, report)}
