from dataclasses import dataclass


@dataclass(frozen=True)
class Device:
    """A device, simulator or emulator that a backend launches apps on, with the identifier that `--device` takes, its name, its kind, the system it runs with its version, its state and the targets that run on it. The version is empty while the backend cannot read it, such as on an Android device that waits for its authorization."""

    identifier: str
    name: str
    kind: str
    system: str
    version: str
    state: str
    targets: tuple[str, ...]

    def describe(self) -> dict[str, object]:
        return {
            "identifier": self.identifier,
            "name": self.name,
            "kind": self.kind,
            "system": self.system,
            "version": self.version,
            "state": self.state,
            "targets": list(self.targets),
        }
