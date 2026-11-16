from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.target_environment import TargetEnvironment


@dataclass(frozen=True)
class PortTarget:
    """A target that an engine port compiles the engine for: its name, its system, where it runs, its architectures with their binary interfaces, its toolchain, the C++ runtime it links, the oldest system version it runs on, its graphics APIs, the shader languages of its devices and the capabilities of the system that the engine uses there, such as `dynamicLibraries`, `executableMemory` and `processes`. A backend target and a port target serve the same target when their system, environment, toolchain and binary interfaces match and the port offers the capabilities that the backend needs, whatever their names."""

    name: str
    system: str
    environment: TargetEnvironment
    abis: Mapping[str, str]
    toolchain: str
    runtime: str
    minimum_system_version: str
    graphics_apis: tuple[str, ...]
    shader_languages: tuple[str, ...]
    capabilities: frozenset[str]

    def describe(self) -> dict[str, object]:
        return {
            "name": self.name,
            "system": self.system,
            "environment": self.environment.value,
            "architectures": dict(self.abis),
            "toolchain": self.toolchain,
            "runtime": self.runtime,
            "minimumSystemVersion": self.minimum_system_version,
            "graphicsApis": list(self.graphics_apis),
            "shaderLanguages": list(self.shader_languages),
            "capabilities": sorted(self.capabilities),
        }
