import platform
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from haylen.core.host import Host
from haylen.toolchains.apple_sdk import AppleSdk


@dataclass(frozen=True)
class DesktopSystem:
    """A desktop system that the desktop port compiles the engine for, on a machine of that system, and that the desktop and player backends build apps for: its architectures with their binary interfaces, the toolchain and the C++ runtime of its builds, its graphics APIs in the order the engine prefers them, the shader languages of their devices and the oldest version of the system that the engine runs on."""

    # A desktop lets every app load native libraries, map executable memory and start processes, which the engine uses there.
    CAPABILITIES = frozenset({"dynamicLibraries", "executableMemory", "processes"})
    # Windows applies the UTF-8 code page of the application manifest, through which the engine reads its command line, from Windows 10 version 1903 on.
    WINDOWS_MINIMUM = "10.0.18362"
    # A Linux app runs on the glibc of the machine that built it or a newer one, which only a Linux machine tells, so another machine declares no floor.
    NO_FLOOR = "0"

    name: str
    abis: Mapping[str, str]
    toolchain: str
    runtime: str
    graphics_apis: tuple[str, ...]
    shader_languages: tuple[str, ...]

    @staticmethod
    def all() -> tuple[DesktopSystem, ...]:
        return (
            DesktopSystem(
                "macos",
                MappingProxyType({"arm64": "arm64-apple-macos", "x86_64": "x86_64-apple-macos"}),
                "clang",
                "libc++",
                ("metal",),
                ("msl-macos", "msl-ios", "msl-ios-simulator"),
            ),
            DesktopSystem(
                "windows",
                MappingProxyType({"x86_64": "x86_64-pc-windows-msvc", "arm64": "aarch64-pc-windows-msvc"}),
                "msvc",
                "ucrt",
                ("direct3D12", "direct3D11", "vulkan", "openGl"),
                ("hlsl5", "spirv", "glsl430"),
            ),
            DesktopSystem(
                "linux",
                MappingProxyType({"x86_64": "x86_64-linux-gnu", "arm64": "aarch64-linux-gnu"}),
                "gnu",
                "libstdc++",
                ("vulkan", "openGl"),
                ("spirv", "glsl430"),
            ),
        )

    @staticmethod
    def names() -> frozenset[str]:
        return frozenset(system.name for system in DesktopSystem.all())

    @staticmethod
    def of(name: str) -> DesktopSystem:
        return next(system for system in DesktopSystem.all() if system.name == name)

    @staticmethod
    def host() -> DesktopSystem:
        """The desktop system of this machine. A machine of another system takes the Linux row, whose targets it cannot build, which its checks of the environment say."""
        return DesktopSystem.of(Host.system()) if Host.system() in DesktopSystem.names() else DesktopSystem.of("linux")

    @property
    def is_host(self) -> bool:
        return self.name == Host.system()

    @property
    def default_architectures(self) -> tuple[str, ...]:
        """The architecture of this machine on its own system, since the desktop port builds only for the machine it runs on, or the first one elsewhere."""
        if self.is_host and Host.architecture() in self.abis:
            return (Host.architecture(),)
        return (next(iter(self.abis)),)

    @property
    def minimum_system_version(self) -> str:
        if self.name == "macos":
            return AppleSdk.MINIMUM_VERSIONS["macOS"]
        if self.name == "windows":
            return DesktopSystem.WINDOWS_MINIMUM
        library, version = platform.libc_ver()
        return version if self.is_host and library == "glibc" and version else DesktopSystem.NO_FLOOR
