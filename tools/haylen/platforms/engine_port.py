from abc import ABC, abstractmethod

from haylen.core.tooling_context import ToolingContext
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_description import PortDescription
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_manifest import SdkManifest


class EnginePort(ABC):
    """The contract of an engine port, which compiles the engine, its platform runtime, its services, its GPU devices and its shaders for some targets and installs them as an SDK with its manifest. It is apart from the backend that builds apps for the same targets: installing one never implies the other, and the registry checks that both declare the same system, environment, architectures, binary interfaces and toolchain before a target counts as supported. A port registers itself through the entry point group `haylen.engine_ports`."""

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @abstractmethod
    def describe(self) -> PortDescription:
        """Declares the targets of the port and the tools it needs. It never touches the machine."""

    @abstractmethod
    def check_environment(self, target: str) -> EnvironmentReport:
        """Finds the tools that building the SDK of a target needs on this machine."""

    @abstractmethod
    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        """Builds the engine for the targets of a request, installs the SDK into its folder and returns its manifest, which the port writes there too."""
