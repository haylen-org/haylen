from pathlib import Path

from haylen.core.configuration import Configuration
from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_manifest import SdkManifest


class SdkBuilder:
    """Builds the engine SDK of targets with the engine port that declares them and installs it, by default into `build/sdk/<port>-<configuration>`, where app builds find it. It is the only place that compiles the engine for apps, which `haylen sdk build` asks for by name and the app commands ask for when no installed SDK holds a slice of the current engine sources."""

    def __init__(self, context: ToolingContext, registry: BackendRegistry) -> None:
        self.context = context
        self.registry = registry

    def build(self, targets: tuple[str, ...], configuration: Configuration, architectures: tuple[str, ...] | None, output: Path | None, reason: str = "") -> SdkManifest:
        """Builds the SDK of backend targets that one engine port serves, in some architectures or the default ones of the targets, and says in its step why it builds when a reason is given."""
        matches = [self.registry.port_for(self.registry.target(name).description) for name in targets]
        ports = {port.name: port for port, _ in matches}
        if len(ports) != 1:
            names = ", ".join(f'"{name}"' for name in sorted(ports))
            raise MissingToolchain(f"The targets belong to the engine ports {names}. Build the SDK of each port with a command of its own.")
        port = next(iter(ports.values()))
        port_targets = tuple(target.name for _, target in matches)

        for name in port_targets:
            report = port.port.check_environment(name)
            if not report.ready:
                lines = "\n".join(f"  {problem}" for problem in report.missing)
                raise MissingToolchain(f'This machine lacks what the engine port "{port.description.name}" needs to build "{name}":\n{lines}')
        folder = output or Repository.SDKS / f"{port.description.name}-{configuration.value.lower()}"
        request = SdkRequest(port_targets, configuration, architectures, Repository.engine_revision(), folder)

        names = ", ".join(f'"{name}"' for name in port_targets)
        self.context.terminal.step(f'Building the engine SDK of {names} with the engine port "{port.description.name}" in the "{configuration.value}" configuration')
        if reason:
            self.context.terminal.note(reason)
        manifest = port.port.build_sdk(request)
        self.context.terminal.success(f"Installed the SDK into `{Repository.shown(manifest.folder)}`.")
        return manifest
