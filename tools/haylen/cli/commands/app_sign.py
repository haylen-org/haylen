import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.option_values import OptionValues
from haylen.core.repository import Repository
from haylen.platforms.signing_request import SigningRequest


class AppSignCommand(Command):
    PATH = ("app", "sign")
    HELP = 'Create the signing key of an app for a target whose backend makes keys, such as the upload key of Android release builds in "platform/android/keystore", which never reaches a repository.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("--target", required=True, help='The target whose key to create, such as "android".')
        parser.add_argument("--kind", default="release", choices=["release", "debug"], help='The key that signs release builds, or a debug key of the project, "release" by default.')
        OptionValues.add(parser, 'An option of the key, such as "--option alias=upload", "--option password=<password>", "--option name=<distinguished name>" or "--option force=true".')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        files = session.pipeline.sign(arguments.target, SigningRequest(folder, arguments.kind, OptionValues.parse(arguments.option)))
        for file in files:
            session.terminal.success(f"Wrote `{Repository.shown(Path(file))}`.")
        return {"files": list(files)}
