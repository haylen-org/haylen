from collections.abc import Callable, Collection, Iterable, Mapping
from dataclasses import dataclass
from importlib import metadata

from haylen.core.app_kind import AppKind
from haylen.core.errors.incompatible_extension import IncompatibleExtension
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.schema_validator import SchemaValidator
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.contract import Contract
from haylen.platforms.contribution_type import ContributionType
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.override_section import OverrideSection
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.platform_description import PlatformDescription
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.target_description import TargetDescription


class BackendRegistry:
    """The backends and engine ports installed with the tooling, which it finds through their entry points, the internal ones like the external ones. It gives the core their descriptions without importing any of them by name, refuses an extension of another contract version and matches a backend target with the engine port that builds the engine for it by their contract and capabilities."""

    @dataclass(frozen=True)
    class Entry:
        """A backend with the name of its entry point and the description it declared."""

        name: str
        backend: PlatformBackend
        description: PlatformDescription

    @dataclass(frozen=True)
    class Target:
        """A target with the backend that declares it."""

        backend: PlatformBackend
        platform: PlatformDescription
        description: TargetDescription

    @dataclass(frozen=True)
    class Port:
        """An engine port with the name of its entry point and the description it declared."""

        name: str
        port: EnginePort
        description: PortDescription

    def __init__(self, context: ToolingContext, backends: Iterable[tuple[str, Callable[[], type]]], ports: Iterable[tuple[str, Callable[[], type]]]) -> None:
        self.context = context
        self.problems: list[str] = []
        self.entries = [entry for entry in (self.load_backend(name, loader) for name, loader in backends) if entry is not None]
        self.ports = [port for port in (self.load_port(name, loader) for name, loader in ports) if port is not None]
        self.check_names()

    @staticmethod
    def discover(context: ToolingContext) -> BackendRegistry:
        """Finds every backend and engine port that the Python environment of the tooling has installed."""
        backends = [(point.name, point.load) for point in metadata.entry_points(group=Contract.BACKEND_GROUP)]
        ports = [(point.name, point.load) for point in metadata.entry_points(group=Contract.ENGINE_PORT_GROUP)]
        return BackendRegistry(context, sorted(backends), sorted(ports))

    def load_backend(self, name: str, loader: Callable[[], type]) -> BackendRegistry.Entry | None:
        try:
            backend = loader()(self.context)
            if not isinstance(backend, PlatformBackend):
                raise TypeError("it is not a PlatformBackend")
            description = backend.describe()
        except Exception as error:
            self.problems.append(f'The backend "{name}" does not load: {error}. Install a version of it that works with this tooling, or uninstall it.')
            return None
        if description.contract != Contract.BACKEND:
            self.problems.append(
                f'The backend "{name}" implements the backend contract {description.contract}, and this tooling implements {Contract.BACKEND}. Install the version of the backend for contract {Contract.BACKEND}.'
            )
            return None
        return BackendRegistry.Entry(name, backend, description)

    def load_port(self, name: str, loader: Callable[[], type]) -> BackendRegistry.Port | None:
        try:
            port = loader()(self.context)
            if not isinstance(port, EnginePort):
                raise TypeError("it is not an EnginePort")
            description = port.describe()
        except Exception as error:
            self.problems.append(f'The engine port "{name}" does not load: {error}. Install a version of it that works with this tooling, or uninstall it.')
            return None
        if description.contract != Contract.ENGINE_PORT:
            self.problems.append(
                f'The engine port "{name}" implements the engine port contract {description.contract}, and this tooling implements {Contract.ENGINE_PORT}. Install the version of the port for contract {Contract.ENGINE_PORT}.'
            )
            return None
        return BackendRegistry.Port(name, port, description)

    def check_names(self) -> None:
        """Refuses two extensions that declare one target, one section of `haylen.json` or one contribution type of a platform, since nothing could tell which one serves it."""
        seen: dict[str, str] = {}
        for entry in self.entries:
            names = [f'the target "{target.name}"' for target in entry.description.targets]
            names += [f'the section "platforms.{section}"' for section in entry.description.override_sections]
            names += [f'the contribution type "{kind.name}" on "{platform}"' for kind in entry.description.contribution_types.values() for platform in kind.platforms]
            for name in names:
                if name in seen and seen[name] != entry.name:
                    self.problems.append(f'The backends "{seen[name]}" and "{entry.name}" both declare {name}. Uninstall one of them.')
                seen.setdefault(name, entry.name)

        port_targets: dict[str, str] = {}
        for port in self.ports:
            for target in port.description.targets:
                if target.name in port_targets:
                    self.problems.append(f'The engine ports "{port_targets[target.name]}" and "{port.name}" both build the engine for "{target.name}". Uninstall one of them.')
                port_targets.setdefault(target.name, port.name)

    def require_healthy(self) -> None:
        if self.problems:
            raise IncompatibleExtension("The installed backends and engine ports have these problems:\n" + "\n".join(f"  {problem}" for problem in self.problems))

    def targets(self) -> list[BackendRegistry.Target]:
        return [BackendRegistry.Target(entry.backend, entry.description, target) for entry in self.entries for target in entry.description.targets]

    def targets_named_by(self, slices: Collection[tuple[str, str]]) -> list[str]:
        """The names of the targets whose system and environment a list of slices names, such as the targets that an SDK was built for."""
        return [target.description.name for target in self.targets() if (target.description.system, target.description.environment.value) in slices]

    def target(self, name: str) -> BackendRegistry.Target:
        self.require_healthy()
        for target in self.targets():
            if target.description.name == name:
                return target
        known = ", ".join(f'"{target.description.name}"' for target in self.targets())
        raise UnsupportedTarget(f'No installed backend builds the target "{name}". The targets are {known}, and "haylen doctor" shows what each one needs.')

    def default_target(self, kind: AppKind) -> BackendRegistry.Target:
        """Returns the target that `app run` takes on this machine when the command names none, which a backend marks as the default of its host for the kind of the app."""
        self.require_healthy()
        host = Host.system()
        defaults = [target for target in self.targets() if kind in target.description.host_default and host in target.description.hosts]
        if len(defaults) != 1:
            raise UnsupportedTarget(f'No single target is the default of "{kind.value}" apps on "{host}", so name one with "--target". The command "haylen doctor" lists the targets.')
        return defaults[0]

    def port_for(self, target: TargetDescription) -> tuple[BackendRegistry.Port, PortTarget]:
        """Returns the engine port that builds the engine for a backend target, which it finds by the system, environment, toolchain, binary interfaces and capabilities of its targets, never by their names."""
        self.require_healthy()
        for port in self.ports:
            for candidate in port.description.targets:
                if BackendRegistry.mismatch(target, candidate) is None:
                    return port, candidate
        raise UnsupportedTarget(
            f'No installed engine port builds the engine for "{target.name}", which runs on "{target.system}" on "{target.environment.value}" with "{target.toolchain}". Install the port of that target into the environment of the tooling.'
        )

    @staticmethod
    def mismatch(target: TargetDescription, port_target: PortTarget) -> str | None:
        """Returns why a port target does not build the engine for a backend target, or nothing when it does."""
        if (target.system, target.environment, target.toolchain) != (port_target.system, port_target.environment, port_target.toolchain):
            return f'It builds for "{port_target.system}" on "{port_target.environment.value}" with "{port_target.toolchain}".'
        differing = [architecture for architecture, abi in target.abis.items() if port_target.abis.get(architecture) != abi]
        if differing:
            return f"It does not build the architectures {SchemaValidator.choices(differing, 'and')} with the binary interfaces of the backend."
        missing = sorted(target.requires - port_target.capabilities)
        if missing:
            return f"It lacks the capabilities {SchemaValidator.choices(missing, 'and')} that the backend needs."
        return None

    def override_sections(self) -> Mapping[str, OverrideSection]:
        return {name: section for entry in self.entries for name, section in entry.description.override_sections.items()}

    def contribution_types(self, platform: str) -> Mapping[str, ContributionType]:
        return {kind.name: kind for entry in self.entries for kind in entry.description.contribution_types.values() if platform in kind.platforms}
