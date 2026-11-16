from dataclasses import dataclass
from enum import StrEnum
from pathlib import Path


@dataclass(frozen=True)
class ToolStatus:
    """What a check of the environment found of one tool: whether it runs on the host or is part of the target, its version and path, and the problem with what to do when the tool is missing or unsuitable."""

    class Role(StrEnum):
        HOST = "host"
        TARGET = "target"

    name: str
    role: Role
    version: str | None
    path: Path | None
    problem: str | None

    @property
    def ready(self) -> bool:
        return self.problem is None

    def describe(self) -> dict[str, object]:
        return {"name": self.name, "role": self.role.value, "version": self.version, "path": str(self.path) if self.path else None, "problem": self.problem}
