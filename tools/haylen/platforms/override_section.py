from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.frozen import Frozen


@dataclass(frozen=True)
class OverrideSection:
    """A section of `platforms` in `haylen.json` that a backend reads, such as `ios` or `android`: the platforms it applies to, the JSON Schema of its settings and the keys whose values are contributions of the app, such as `infoPlist`, which merge with the contributions of the plugins by the rules of their type. Every section also takes the keys that the core reads, `minimumSystemVersion` and `files`. A setting whose schema gives a `default` takes it when the app leaves the setting out."""

    name: str
    platforms: frozenset[str]
    schema: Mapping[str, object]
    contribution_keys: Mapping[str, str]
    description: str

    @property
    def defaults(self) -> Mapping[str, object]:
        """The values of the settings that the app leaves out, from the `default` of their schemas."""
        properties = self.schema.get("properties", {})
        return Frozen.freeze({key: setting["default"] for key, setting in properties.items() if "default" in setting})
