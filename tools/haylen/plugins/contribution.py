from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Contribution:
    """One typed thing that a plugin or an app brings to the generated project of the platforms it lists, such as a Swift package, a Gradle module or a web module. Its owner is the plugin id, or `app` for an override of `haylen.json`, its origin names the file and the place it comes from, and its folder is the plugin or app folder that its paths are relative to."""

    type: str
    platforms: tuple[str, ...]
    fields: Mapping[str, object]
    owner: str
    origin: str
    folder: Path

    def applies_to(self, platform: str) -> bool:
        return platform in self.platforms

    def describe(self) -> dict[str, object]:
        return {"type": self.type, "owner": self.owner, "origin": self.origin, "fields": self.fields}
