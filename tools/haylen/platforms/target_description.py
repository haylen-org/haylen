from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.app_kind import AppKind
from haylen.core.target_environment import TargetEnvironment


@dataclass(frozen=True)
class TargetDescription:
    """A target that a backend accepts, by the name that commands take, such as `ios-simulator`: its system, the platform name that plugins and native libraries use for it, where it runs, the architectures it builds with their binary interfaces, the ones a request without `--architecture` builds, its toolchain, the kinds of apps it builds, the systems of the machines that build it, the capabilities that it needs of the engine port, such as `dynamicLibraries`, and the content profile that its protected releases are built for, which targets of one store share. A target may be the default of `app run` on its host for some kinds of apps."""

    name: str
    system: str
    platform: str
    environment: TargetEnvironment
    abis: Mapping[str, str]
    default_architectures: tuple[str, ...]
    toolchain: str
    kinds: frozenset[AppKind]
    hosts: frozenset[str]
    host_default: frozenset[AppKind]
    requires: frozenset[str]
    release_profile: str
    description: str

    @property
    def architectures(self) -> tuple[str, ...]:
        return tuple(self.abis)

    def describe(self) -> dict[str, object]:
        return {
            "name": self.name,
            "system": self.system,
            "platform": self.platform,
            "environment": self.environment.value,
            "architectures": dict(self.abis),
            "defaultArchitectures": list(self.default_architectures),
            "toolchain": self.toolchain,
            "kinds": sorted(kind.value for kind in self.kinds),
            "hosts": sorted(self.hosts),
            "requires": sorted(self.requires),
            "releaseProfile": self.release_profile,
            "description": self.description,
        }
