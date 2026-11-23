from dataclasses import dataclass

from haylen.platforms.port_capability import PortCapability
from haylen.platforms.port_target import PortTarget


@dataclass(frozen=True)
class PortDescription:
    """What an engine port declares before it builds anything: its name, the version of the engine port contract it implements, its own version, the targets it compiles the engine for, the host tools it needs, the host tools of the tooling that its SDK carries, such as the content tool that Release builds of apps run, and the optional operations it offers."""

    name: str
    contract: int
    version: str
    targets: tuple[PortTarget, ...]
    tools: tuple[str, ...]
    host_tools: tuple[str, ...]
    capabilities: frozenset[PortCapability] = frozenset()

    def target(self, name: str) -> PortTarget | None:
        return next((target for target in self.targets if target.name == name), None)
