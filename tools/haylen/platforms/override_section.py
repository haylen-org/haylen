from collections.abc import Mapping
from dataclasses import dataclass


@dataclass(frozen=True)
class OverrideSection:
    """A section of `platforms` in `haylen.json` that a backend reads, such as `ios` or `android`: the platforms it applies to, the JSON Schema of its settings and the keys whose values are contributions of the app, such as `infoPlist`, which merge with the contributions of the plugins by the rules of their type. Every section also takes the keys that the core reads, `minimumSystemVersion` and `files`."""

    name: str
    platforms: frozenset[str]
    schema: Mapping[str, object]
    contribution_keys: Mapping[str, str]
    description: str
