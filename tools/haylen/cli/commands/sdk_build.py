import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.configuration import Configuration
from haylen.sdk.sdk_builder import SdkBuilder


class SdkBuildCommand(Command):
    PATH = ("sdk", "build")
    HELP = "Build the engine SDK of one or more targets with the engine port that declares them and install it, by default into build/sdk/<port>-<configuration>, where app builds find it. The app commands that build run it themselves for an SDK that is missing or stale."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--target",
            action="append",
            dest="targets",
            required=True,
            help="A target whose engine to build, which repeats. Every target must belong to one engine port.",
        )
        parser.add_argument(
            "--config",
            default="Release",
            choices=[configuration.value for configuration in Configuration],
            help='The configuration of the SDK, "Release" by default.',
        )
        parser.add_argument(
            "--architecture",
            action="append",
            dest="architectures",
            help="An architecture to build, which repeats. The default architectures of the targets otherwise.",
        )
        parser.add_argument("--output", help='The folder to install the SDK into instead of the build folder of the checkout, such as a folder that "HAYLEN_SDK_PATH" names.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        output = Path(arguments.output).expanduser().resolve() if arguments.output else None
        architectures = tuple(arguments.architectures) if arguments.architectures else None
        manifest = SdkBuilder(session.context, session.registry).build(tuple(arguments.targets), Configuration(arguments.config), architectures, output)
        return {**manifest.describe(), "folder": str(manifest.folder)}
