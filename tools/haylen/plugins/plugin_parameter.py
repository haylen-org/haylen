from collections.abc import Mapping
from dataclasses import dataclass


@dataclass(frozen=True)
class PluginParameter:
    """A value that an app gives a plugin in `haylen.json`, with its type, the platforms it applies to and its default. A `file` is a path relative to the app folder."""

    CHECKS = {
        "string": lambda value: isinstance(value, str),
        "file": lambda value: isinstance(value, str),
        "number": lambda value: isinstance(value, int | float) and not isinstance(value, bool),
        "integer": lambda value: isinstance(value, int) and not isinstance(value, bool),
        "boolean": lambda value: isinstance(value, bool),
        "array": lambda value: isinstance(value, tuple | list),
        "object": lambda value: isinstance(value, Mapping),
    }
    TYPE_WORDS = {
        "string": "a text",
        "file": "the path of a file",
        "number": "a number",
        "integer": "a whole number",
        "boolean": "true or false",
        "array": "a list",
        "object": "an object",
    }

    name: str
    type: str
    description: str
    platforms: tuple[str, ...] | None
    required: bool
    has_default: bool
    default: object

    @staticmethod
    def parse(name: str, entry: Mapping[str, object]) -> PluginParameter:
        platforms = entry.get("platforms")
        return PluginParameter(
            name=name,
            type=str(entry["type"]),
            description=str(entry["description"]),
            platforms=tuple(str(platform) for platform in platforms) if platforms is not None else None,
            required=bool(entry.get("required", False)),
            has_default="default" in entry,
            default=entry.get("default"),
        )

    def accepts(self, value: object) -> bool:
        return PluginParameter.CHECKS[self.type](value)

    def applies_to(self, platform: str) -> bool:
        return self.platforms is None or platform in self.platforms

    @property
    def type_words(self) -> str:
        return PluginParameter.TYPE_WORDS[self.type]
