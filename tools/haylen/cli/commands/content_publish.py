import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.content_arguments import ContentArguments
from haylen.content.release_package import ReleasePackage


class ContentPublishCommand(Command):
    PATH = ("content", "publish")
    HELP = (
        "Publish the content of an app as the next generation of a channel of a publication tree for the content profile of a target, which a content server delivers to installed apps as an update."
    )
    # The channel that an app follows when "content.updates.channel" of "haylen.json" names none, as `AppConfig` of the engine reads it.
    CHANNEL = "stable"

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        ContentArguments.add_target(parser)
        parser.add_argument(
            "--channel",
            default=ContentPublishCommand.CHANNEL,
            help=f'The channel of the update, such as "beta", which "content.updates.channel" of "haylen.json" names, "{ContentPublishCommand.CHANNEL}" by default, as the engine reads it.',
        )
        parser.add_argument("--output", required=True, help="The folder of the publication tree, which the content server serves as it is.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        target, profile = ContentArguments.profile(arguments, session)
        source = ReleasePackage.resolve(Arguments.app_folder(arguments), target, session.context, session.registry)
        tree = Path(arguments.output).expanduser()
        report = ContentArguments.releases(session).publish(source, profile, arguments.channel, tree)
        return {"tree": str(tree.resolve()), "channel": arguments.channel, "profile": profile, "report": ContentArguments.report(session, report)}
