import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.configuration import Configuration
from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.repository import Repository
from haylen.platforms.sdk_request import SdkRequest


class SdkBuildCommand(Command):
    PATH = ("sdk", "build")
    HELP = "Build the engine SDK of one or more targets with the engine port that declares them and install it, by default into build/sdk/<port>-<configuration>, where app builds find it. This is the only command that compiles the engine for apps."

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
        registry = session.registry
        matches = [registry.port_for(registry.target(name).description) for name in arguments.targets]
        ports = {port.name: port for port, _ in matches}
        if len(ports) != 1:
            names = ", ".join(f'"{name}"' for name in sorted(ports))
            raise MissingToolchain(f"The targets belong to the engine ports {names}. Build the SDK of each port with a command of its own.")
        port = next(iter(ports.values()))
        port_targets = tuple(target.name for _, target in matches)
        configuration = Configuration(arguments.config)

        for name in port_targets:
            report = port.port.check_environment(name)
            if not report.ready:
                lines = "\n".join(f"  {problem}" for problem in report.missing)
                raise MissingToolchain(f'This machine lacks what the engine port "{port.description.name}" needs to build "{name}":\n{lines}')
        folder = Path(arguments.output).expanduser().resolve() if arguments.output else Repository.SDKS / f"{port.description.name}-{configuration.value.lower()}"
        architectures = tuple(arguments.architectures) if arguments.architectures else None
        request = SdkRequest(port_targets, configuration, architectures, Repository.engine_revision(), folder)

        session.terminal.step(
            f'Building the engine SDK of {", ".join(f'"{name}"' for name in port_targets)} with the engine port "{port.description.name}" in the "{configuration.value}" configuration'
        )
        manifest = port.port.build_sdk(request)
        session.terminal.success(f"Installed the SDK into `{Repository.shown(manifest.folder)}`.")
        return {**manifest.describe(), "folder": str(manifest.folder)}
