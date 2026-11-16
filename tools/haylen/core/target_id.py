from dataclasses import dataclass

from haylen.core.configuration import Configuration
from haylen.core.target_environment import TargetEnvironment


@dataclass(frozen=True, order=True)
class TargetId:
    """One effective target of a build: the operating system, the processor architecture, the binary interface, where it runs, the configuration and the toolchain that compiles it. A product for several architectures holds one target per architecture."""

    system: str
    architecture: str
    abi: str
    environment: TargetEnvironment
    configuration: Configuration
    toolchain: str

    @property
    def text(self) -> str:
        """The target in one word, such as `ios-arm64-simulator-debug`, which logs and folders use."""
        return f"{self.system}-{self.architecture}-{self.environment.value}-{self.configuration.value.lower()}"

    def describe(self) -> dict[str, str]:
        return {
            "system": self.system,
            "architecture": self.architecture,
            "abi": self.abi,
            "environment": self.environment.value,
            "configuration": self.configuration.value,
            "toolchain": self.toolchain,
        }
