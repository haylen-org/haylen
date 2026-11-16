from collections.abc import Mapping
from dataclasses import dataclass


@dataclass(frozen=True)
class HostTool:
    """A program of an SDK that runs on the machine that builds apps, never on the target, such as the content tool that builds protected releases, with the system and architecture it runs on and its path relative to the SDK folder."""

    name: str
    system: str
    architecture: str
    path: str

    @staticmethod
    def parse(entry: Mapping[str, object]) -> HostTool:
        return HostTool(str(entry["name"]), str(entry["system"]), str(entry["architecture"]), str(entry["path"]))

    def describe(self) -> dict[str, str]:
        return {"name": self.name, "system": self.system, "architecture": self.architecture, "path": self.path}
