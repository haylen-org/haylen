from dataclasses import dataclass

from haylen.platforms.port_target import PortTarget


@dataclass(frozen=True)
class PortDescription:
    """What an engine port declares before it builds anything: its name, the version of the engine port contract it implements, its own version, the targets it compiles the engine for and the host tools it needs."""

    name: str
    contract: int
    version: str
    targets: tuple[PortTarget, ...]
    tools: tuple[str, ...]

    def target(self, name: str) -> PortTarget | None:
        return next((target for target in self.targets if target.name == name), None)

    def describe(self) -> dict[str, object]:
        return {
            "name": self.name,
            "contract": self.contract,
            "version": self.version,
            "targets": [target.describe() for target in self.targets],
            "tools": list(self.tools),
        }
