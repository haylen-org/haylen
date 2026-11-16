import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.content_arguments import ContentArguments
from haylen.content.release_package import ReleasePackage
from haylen.core.repository import Repository


class ContentBuildCommand(Command):
    PATH = ("content", "build")
    HELP = "Build the protected release of an app for the content profile of a target with the content tool of the SDK of this machine, following the release before it so unchanged content keeps its shards."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        ContentArguments.add_target(parser)
        parser.add_argument(
            "--previous-release",
            help="The release folder that shipped last, which the new release follows instead of the last release of the build folder, such as the one that continuous integration keeps.",
        )

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        target, profile = ContentArguments.profile(arguments, session)
        source = ReleasePackage.resolve(Arguments.app_folder(arguments), target, session.context, session.registry)
        previous = Path(arguments.previous_release).expanduser() if arguments.previous_release else None
        release = ContentArguments.releases(session).build(source, profile, previous)

        shipped = "" if release.shipped == release.folder else f", and the part that ships with the app in `{Repository.shown(release.shipped)}`"
        session.terminal.success(f"Built the protected release `{Repository.shown(release.folder)}`{shipped}.")
        return release.describe()
