from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.frozen import Frozen
from haylen.core.target_environment import TargetEnvironment


@dataclass(frozen=True)
class SdkSlice:
    """The part of an SDK that serves one system and environment: the architectures it holds with their binary interface, the toolchain and the C++ runtime that compiled it, the oldest system version it runs on, its graphics APIs, the capabilities of the system that the engine uses there, its libraries by name and its files by name, such as its headers or its CMake package, both relative to the SDK folder. The libraries are `haylen` with the engine, `haylen-runtime`, `haylen-lua` with the Lua layer, which only the apps that run Lua link, and `haylen-player` with the Lua player, and a slice that merges libraries, as the Apple slices do, lists the merged ones."""

    ENGINE = "haylen"
    RUNTIME = "haylen-runtime"
    LUA = "haylen-lua"
    PLAYER = "haylen-player"

    system: str
    environment: TargetEnvironment
    architectures: tuple[str, ...]
    abi: str
    toolchain: str
    runtime: str
    minimum_system_version: str
    graphics_apis: tuple[str, ...]
    capabilities: frozenset[str]
    libraries: Mapping[str, str]
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
            libraries=Frozen.freeze(dict(entry["libraries"])),
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
            "libraries": dict(self.libraries),
            "files": dict(self.files),
        }
