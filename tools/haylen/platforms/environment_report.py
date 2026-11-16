from dataclasses import dataclass

from haylen.platforms.tool_status import ToolStatus


@dataclass(frozen=True)
class EnvironmentReport:
    """What a backend or an engine port found on this machine for a target: every tool it needs with its status, and the problems that are not about one tool, each with what to do."""

    tools: tuple[ToolStatus, ...]
    problems: tuple[str, ...]

    @property
    def ready(self) -> bool:
        return not self.problems and all(tool.ready for tool in self.tools)

    @property
    def missing(self) -> tuple[str, ...]:
        return (*(tool.problem for tool in self.tools if tool.problem), *self.problems)

    def describe(self) -> dict[str, object]:
        return {"ready": self.ready, "tools": [tool.describe() for tool in self.tools], "problems": list(self.problems)}
