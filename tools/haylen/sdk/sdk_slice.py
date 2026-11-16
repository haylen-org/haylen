from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.frozen import Frozen
from haylen.core.target_environment import TargetEnvironment


@dataclass(frozen=True)
class SdkSlice:
    """The part of an SDK that serves one system and environment: the architectures it holds with their binary interface, the toolchain and the C++ runtime that compiled it, the oldest system version it runs on, its graphics APIs, the capabilities of the system that the engine uses there and its files by name, such as its static library, its headers or its CMake package, relative to the SDK folder."""

    system: str
    environment: TargetEnvironment
    architectures: tuple[str, ...]
    abi: str
    toolchain: str
    runtime: str
    minimum_system_version: str
    graphics_apis: tuple[str, ...]
    capabilities: frozenset[str]
    files: Mapping[str, str]

    @staticmethod
    def parse(entry: Mapping[str, object]) -> SdkSlice:
        return SdkSlice(
            system=str(entry["system"]),
            environment=TargetEnvironment(entry["environment"]),
            architectures=tuple(entry["architectures"]),
            abi=str(entry["abi"]),
            toolchain=str(entry["toolchain"]),
            runtime=str(entry["runtime"]),
            minimum_system_version=str(entry["minimumSystemVersion"]),
            graphics_apis=tuple(entry["graphicsApis"]),
            capabilities=frozenset(entry["capabilities"]),
            files=Frozen.freeze(dict(entry.get("files", {}))),
        )

    def describe(self) -> dict[str, object]:
        return {
            "system": self.system,
            "environment": self.environment.value,
            "architectures": list(self.architectures),
            "abi": self.abi,
            "toolchain": self.toolchain,
            "runtime": self.runtime,
            "minimumSystemVersion": self.minimum_system_version,
            "graphicsApis": list(self.graphics_apis),
            "capabilities": sorted(self.capabilities),
            "files": dict(self.files),
        }
