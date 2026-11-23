from collections.abc import Sequence
from pathlib import Path

from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.target_environment import TargetEnvironment
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_manifest import SdkManifest


class ContentTool:
    """The content tool `haylen-content`, a host tool of the desktop SDK that builds, stages, verifies, inspects, compares and publishes protected releases and writes the bootstrap of release builds with the format library of the engine. The tooling finds it in an installed SDK of this machine that was built from the engine sources of this checkout, which the app commands that build in Release build first through the engine port whose SDK carries it when no installed SDK holds it. It prints key IDs and public keys only, so its output may reach the log of a command."""

    NAME = "haylen-content"

    def __init__(self, context: ToolingContext, executable: Path) -> None:
        self.context = context
        self.executable = executable

    @staticmethod
    def locate(context: ToolingContext, registry: BackendRegistry) -> ContentTool:
        """Finds the content tool in the installed SDKs, or fails with the command that builds the SDK of this machine, which carries it."""
        hosts = ContentTool.host_targets(registry)
        if hosts:
            commands = " or ".join(f'"haylen sdk build --target {name}"' for name in hosts)
            build = f"Build the SDK of this machine, which carries it, with {commands}."
        else:
            build = "Install the engine port that builds the SDK of this machine, which carries it, and build that SDK."
        manifests = SdkCatalog(context.environment).manifests()
        return ContentTool(context, ContentTool.find(manifests, Repository.engine_revision(), build))

    @staticmethod
    def host_targets(registry: BackendRegistry) -> tuple[str, ...]:
        """The targets of this machine, in the order of their names, whose engine port builds an SDK that carries the content tool."""
        names = []
        for target in registry.targets():
            description = target.description
            if description.system != Host.system() or description.environment is not TargetEnvironment.HOST or Host.system() not in description.hosts:
                continue
            try:
                port, _ = registry.port_for(description)
            except UnsupportedTarget:
                continue
            if ContentTool.NAME in port.description.host_tools:
                names.append(description.name)
        return tuple(sorted(names))

    @staticmethod
    def find(manifests: Sequence[SdkManifest], revision: str, build: str) -> Path:
        """Returns the content tool of this system and architecture from the SDKs of the current engine sources, from a Release SDK first, or fails with the sentence `build` that says how to install it."""
        system = Host.system()
        architecture = Host.architecture()
        reasons: list[str] = []
        ordered = sorted(manifests, key=lambda manifest: manifest.configuration is not Configuration.RELEASE)
        for manifest in ordered:
            for tool in manifest.host_tools:
                if tool.name != ContentTool.NAME or tool.system != system or tool.architecture != architecture:
                    continue
                shown = f"`{Repository.shown(manifest.folder)}`"
                path = manifest.path(tool.path)
                if manifest.revision != revision:
                    reasons.append(f"The SDK {shown} holds it, built from other engine sources than the ones of this checkout.")
                elif not path.is_file():
                    reasons.append(f"The SDK {shown} names `{Repository.shown(path)}`, which does not exist.")
                else:
                    return path

        lead = f'No installed SDK holds the content tool "{ContentTool.NAME}" for "{system}" on "{architecture}", which builds protected releases. {build}'
        if not reasons:
            raise IncompatibleSdk(lead)
        lines = "\n".join(f"  {reason}" for reason in reasons)
        raise IncompatibleSdk(f"{lead}\n{lines}")

    def run(self, arguments: Sequence[object]) -> None:
        """Runs a command of the tool quietly into the log of the command."""
        self.context.process.run([self.executable, *arguments])

    def capture(self, arguments: Sequence[object]) -> str:
        """Runs a command of the tool whose report the developer reads, such as a verification, and returns it."""
        return self.context.process.capture([self.executable, *arguments])
