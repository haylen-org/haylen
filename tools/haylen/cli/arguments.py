import argparse
from pathlib import Path

from haylen.cli.command_session import CommandSession
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository


class Arguments:
    """The arguments that several commands share, with the same names and defaults everywhere: the app folder, the target, the configurations and the architectures."""

    @staticmethod
    def add_app(parser: argparse.ArgumentParser) -> None:
        parser.add_argument("app", nargs="?", default=".", help='The app folder, which holds "haylen.json", the current folder by default.')

    @staticmethod
    def add_product(parser: argparse.ArgumentParser, target_required: bool = False, configuration: Configuration = Configuration.DEBUG) -> None:
        Arguments.add_app(parser)
        parser.add_argument(
            "--target",
            required=target_required,
            help='The target by its name, such as "ios-simulator", which "haylen doctor" lists. Without it a command takes the default target of the kind of the app on this machine.',
        )
        parser.add_argument(
            "--config",
            default=configuration.value,
            choices=[value.value for value in Configuration],
            help=f'The configuration of the app, "{configuration.value}" by default.',
        )
        parser.add_argument(
            "--engine-config",
            default="Release",
            choices=[configuration.value for configuration in Configuration],
            help='The configuration of the engine SDK, "Release" by default.',
        )
        parser.add_argument(
            "--architecture",
            action="append",
            dest="architectures",
            help="An architecture to build, which repeats. The default architectures of the target otherwise.",
        )

    @staticmethod
    def add_previous_release(parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "--previous-release",
            metavar="folder",
            help="The folder of the protected release that shipped last, which a Release build follows so the chunks that did not change keep their shards, such as the release that continuous integration keeps. The last release of the build folder otherwise.",
        )

    @staticmethod
    def app_folder(arguments: argparse.Namespace) -> Path:
        folder = Path(arguments.app).expanduser().resolve()
        if not AppManifest.is_app(folder):
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` holds no "{AppManifest.FILE}". Name the folder of an app, or create one with "haylen new <folder>".')
        return folder

    @staticmethod
    def request(arguments: argparse.Namespace, session: CommandSession, previous_release: str | None = None) -> ProductRequest:
        """The request of the product arguments, with the release that a Release build follows when a command takes `--previous-release`."""
        folder = Arguments.app_folder(arguments)
        target = arguments.target or session.registry.default_target(AppManifest.load(folder).kind).description.name
        architectures = tuple(arguments.architectures) if arguments.architectures else None
        configuration = Configuration(arguments.config)
        previous = None
        if previous_release is not None:
            if configuration is not Configuration.RELEASE:
                raise InvalidOption('The option "--previous-release" names the release that a Release build follows, so it takes "--config Release".')
            previous = Path(previous_release).expanduser().resolve()
            if not previous.is_dir():
                raise InvalidOption(f'The option "--previous-release" names `{previous}`, which is no folder. Name the folder of the protected release that shipped last.')
        return ProductRequest(folder, target, configuration, Configuration(arguments.engine_config), architectures, previous)
