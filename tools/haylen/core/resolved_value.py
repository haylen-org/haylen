from dataclasses import dataclass


@dataclass(frozen=True)
class ResolvedValue:
    """A value of a resolved project with where it comes from, such as the key of `haylen.json`, a plugin or an SDK, and why it has that value, which `project inspect` shows."""

    value: object
    origin: str
    justification: str

    def describe(self) -> dict[str, object]:
        return {"value": self.value, "origin": self.origin, "justification": self.justification}
