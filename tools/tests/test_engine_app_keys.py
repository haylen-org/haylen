import json
import re
from importlib import resources

from haylen.core.repository import Repository


class EngineAppKeysTest:
    """The engine reads the runtime keys of `haylen.json` and the tooling validates the whole file, so the keys that `AppConfig.cpp` takes in each section are the keys of the schema in the same section."""

    SOURCE = Repository.ENGINE / "src" / "core" / "AppConfig.cpp"
    KNOWN_KEYS = re.compile(r'requireKnownKeys\(\w+, \{([^}]*)\}, "((?:[^"\\]|\\.)*)"')
    SECTION = re.compile(r'^the \\"([a-zA-Z.]+)\\" section of')
    # The sections that the tooling alone reads, which the engine checks only for their types.
    TOOLING_SECTIONS = ("cpp", "platforms")

    @staticmethod
    def engine_sections() -> dict[tuple[str, ...], set[str]]:
        """Returns the keys that each `requireKnownKeys` of `AppConfig.cpp` takes, by the path of its section in the schema."""
        sections: dict[tuple[str, ...], set[str]] = {}
        for keys, context in EngineAppKeysTest.KNOWN_KEYS.findall(EngineAppKeysTest.SOURCE.read_text(encoding="utf-8")):
            sections[EngineAppKeysTest.schema_path(context)] = set(re.findall(r'"([A-Za-z]+)"', keys))
        return sections

    @staticmethod
    def schema_path(context: str) -> tuple[str, ...]:
        if context == '\\"haylen.json\\"':
            return ("properties",)
        if context.startswith("the plugin "):
            return ("$defs", "pluginEntry", "properties")
        if context.startswith('a rule of \\"content.delivery\\"'):
            return ("properties", "content", "properties", "delivery", "items", "properties")
        section = EngineAppKeysTest.SECTION.match(context)
        assert section, f"The context {context} of AppConfig.cpp names no section that the test knows."
        path: list[str] = []
        for name in section.group(1).split("."):
            path += ["properties", name]
        return (*path, "properties")

    @staticmethod
    def schema() -> dict[str, object]:
        return json.loads(resources.files("haylen.schemas").joinpath("haylen.schema.json").read_text(encoding="utf-8"))

    @staticmethod
    def keys_at(schema: dict[str, object], path: tuple[str, ...]) -> set[str]:
        node: object = schema
        for part in path:
            assert isinstance(node, dict) and part in node, f"The schema has no {'/'.join(path)}."
            node = node[part]
        assert isinstance(node, dict)
        return set(node)

    def test_every_section_of_the_engine_takes_the_keys_of_the_schema(self):
        schema = EngineAppKeysTest.schema()
        sections = EngineAppKeysTest.engine_sections()

        assert ("properties",) in sections
        for path, keys in sections.items():
            assert keys == EngineAppKeysTest.keys_at(schema, path), f"The keys of {'/'.join(path)} differ."

    def test_every_runtime_section_of_the_schema_has_its_keys_in_the_engine(self):
        schema = EngineAppKeysTest.schema()
        sections = EngineAppKeysTest.engine_sections()
        properties = schema["properties"]
        assert isinstance(properties, dict)

        for name, section in properties.items():
            if name in EngineAppKeysTest.TOOLING_SECTIONS or "properties" not in section:
                continue
            assert ("properties", name, "properties") in sections, f'The engine does not check the keys of the section "{name}".'
