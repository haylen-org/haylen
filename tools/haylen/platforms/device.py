from dataclasses import dataclass


@dataclass(frozen=True)
class Device:
    """A device, simulator or emulator that a backend launches apps on, with the identifier that `--device` takes, its name, its kind, its state and the targets that run on it."""

    identifier: str
    name: str
    kind: str
    state: str
    targets: tuple[str, ...]

    def describe(self) -> dict[str, object]:
        return {"identifier": self.identifier, "name": self.name, "kind": self.kind, "state": self.state, "targets": list(self.targets)}
