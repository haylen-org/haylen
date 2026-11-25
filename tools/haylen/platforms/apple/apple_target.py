from dataclasses import dataclass
from types import MappingProxyType

from haylen.core.app_kind import AppKind
from haylen.core.target_environment import TargetEnvironment
from haylen.platforms.port_target import PortTarget
from haylen.platforms.target_description import TargetDescription


@dataclass(frozen=True)
class AppleTarget:
    """One Apple target with everything the backend and the engine port know of it: the name that commands take, the system and the platform name of plugins, where it runs, the slice of `Haylen.xcframework` that holds its engine with the SDK of Xcode that builds the slice, the Xcode platform and the target folder of the template that its app target takes, the lists of system frameworks of the engine that it links, the device family of a target whose devices the app does not choose, which the iPhone and iPad targets leave empty for `IosDevices`, the folder suffix of its products, the destination of xcodebuild and the family of its simulators or devices."""

    TOOLCHAIN = "xcode"
    RUNTIME = "libc++"
    GRAPHICS_APIS = ("metal",)
    RELEASE_PROFILE = "apple"

    name: str
    system: str
    platform: str
    environment: TargetEnvironment
    slice: str
    sdk: str
    architectures: tuple[str, ...]
    minimum_version: str
    capabilities: frozenset[str]
    shader_languages: tuple[str, ...]
    xcframework_platform: str
    xcframework_variant: str | None
    xcode_platform: str
    folder: str
    frameworks: str
    device_family: str
    products: str
    destination: str
    family: str | None
    description: str

    @property
    def abi(self) -> str:
        """The binary interface of the target, which every architecture of its slice shares: the system and environment of the Apple platform with the C++ library of the system."""
        return f"apple-{self.slice}"

    @property
    def runs_on_mac(self) -> bool:
        """Whether the app runs on this Mac, as a macOS app or a Mac Catalyst app, whose bundle holds its files in `Contents`."""
        return self.environment is TargetEnvironment.HOST

    @property
    def simulator(self) -> bool:
        return self.environment is TargetEnvironment.SIMULATOR

    @property
    def device(self) -> bool:
        return self.environment is TargetEnvironment.DEVICE

    @property
    def launch_screen(self) -> bool:
        """Whether the app shows a launch screen with the splash of `haylen.json`, which iOS, iPadOS, Mac Catalyst and tvOS do."""
        return self.folder in ("ios", "tvos")

    def default_architectures(self, host: str) -> tuple[str, ...]:
        """The architectures that a request without `--architecture` builds: the one of this Mac for what runs on it or on its simulators, and the one of devices otherwise."""
        if self.device:
            return self.architectures
        return (host,) if host in self.architectures else self.architectures[:1]

    def describe(self, host: str) -> TargetDescription:
        return TargetDescription(
            name=self.name,
            system=self.system,
            platform=self.platform,
            environment=self.environment,
            abis=MappingProxyType(dict.fromkeys(self.architectures, self.abi)),
            default_architectures=self.default_architectures(host),
            toolchain=AppleTarget.TOOLCHAIN,
            kinds=frozenset({AppKind.LUA, AppKind.CPP}),
            hosts=frozenset({"macos"}),
            host_default=frozenset({AppKind.CPP}) if self.name == "macos" else frozenset(),
            requires=frozenset(),
            release_profile=AppleTarget.RELEASE_PROFILE,
            description=self.description,
        )

    def port_target(self) -> PortTarget:
        """The target of the engine port that builds the slice of this target, named after the slice, which matches this target by what it describes."""
        return PortTarget(
            name=self.slice,
            system=self.system,
            environment=self.environment,
            abis=MappingProxyType(dict.fromkeys(self.architectures, self.abi)),
            toolchain=AppleTarget.TOOLCHAIN,
            runtime=AppleTarget.RUNTIME,
            minimum_system_version=self.minimum_version,
            graphics_apis=AppleTarget.GRAPHICS_APIS,
            shader_languages=self.shader_languages,
            capabilities=self.capabilities,
        )
