from types import MappingProxyType

from haylen.core.target_environment import TargetEnvironment
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_manifest import SdkManifest


class FakePort(EnginePort):
    """An engine port of the tests whose target has another name than the device target of the fake backend, since a port and a backend match by what they describe, never by name."""

    CAPABILITIES = frozenset({"dynamicLibraries"})

    @staticmethod
    def variant(name: str, capabilities: frozenset[str]) -> type:
        return type(name, (FakePort,), {"CAPABILITIES": capabilities})

    def describe(self) -> PortDescription:
        abis = MappingProxyType({"arm64": "fake-arm64", "x86_64": "fake-x86_64"})
        target = PortTarget("fake-device", "fakeos", TargetEnvironment.DEVICE, abis, "fakecc", "libfake", "1.0", ("fakeGl",), (), self.CAPABILITIES)
        return PortDescription("fake", 1, "1.0.0", (target,), ())

    def check_environment(self, target: str) -> EnvironmentReport:
        return EnvironmentReport((), ())

    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        raise NotImplementedError
