import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.missing_capability import MissingCapability
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.repository import Repository
from haylen.platforms.port_capability import PortCapability
from haylen.platforms.sdk_join_request import SdkJoinRequest
from haylen.sdk.sdk_manifest import SdkManifest


class SdkJoinCommand(Command):
    PATH = ("sdk", "join")
    HELP = "Join engine SDKs of the same engine port, engine sources and configuration that hold different slices, such as SDKs that several machines built side by side, into one SDK, by default in build/sdk/<port>-<configuration>, where app builds find it."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("folders", nargs="+", metavar="folder", help="A folder of an SDK to join.")
        parser.add_argument("--output", help="The folder to install the joined SDK into instead of the build folder of the checkout.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        manifests = tuple(SdkManifest.load(Path(folder).expanduser().resolve()) for folder in arguments.folders)
        first = manifests[0]
        for manifest in manifests[1:]:
            if (manifest.port, manifest.revision, manifest.configuration) != (first.port, first.revision, first.configuration):
                raise IncompatibleSdk(
                    f"The SDKs `{Repository.shown(first.folder)}` and `{Repository.shown(manifest.folder)}` come from different engine ports, engine sources or configurations, so they do not join."
                )

        registry = session.registry
        registry.require_healthy()
        ports = [port for port in registry.ports if port.description.name == first.port]
        if not ports:
            raise UnsupportedTarget(f'No installed engine port is named "{first.port}", the port that built the SDKs to join.')
        port = ports[0]
        if PortCapability.JOIN not in port.description.capabilities:
            raise MissingCapability(f'The engine port "{first.port}" does not join SDKs, so build every slice of its SDK with one "haylen sdk build".')

        folder = Path(arguments.output).expanduser().resolve() if arguments.output else Repository.SDKS / f"{first.port}-{first.configuration.value.lower()}"
        session.terminal.step(f'Joining {len(manifests)} SDKs of the engine port "{first.port}" in the "{first.configuration.value}" configuration')
        manifest = port.port.join_sdks(SdkJoinRequest(manifests, folder))
        session.terminal.success(f"Installed the joined SDK into `{Repository.shown(manifest.folder)}`.")
        return {**manifest.describe(), "folder": str(manifest.folder)}
