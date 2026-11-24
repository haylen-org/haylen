import json
import re
from importlib import resources

from haylen.core.repository import Repository


class EngineAppKeysTest:
    """The engine reads the runtime keys of `haylen.json`, the Lua layer reads its `lua` section and the tooling validates the whole file, so the keys that `AppConfig.cpp` and `Config.cpp` take in each section are the keys of the schema in the same section, and the top level holds the keys of the engine and the sections of the Lua layer."""

    SOURCES = (Repository.ENGINE / "src" / "core" / "AppConfig.cpp", Repository.ENGINE / "lua" / "src" / "runtime" / "Config.cpp")
    TOP_LEVEL = re.compile(r"AppConfig::kKeys\{([^}]*)\}")
    KNOWN_KEYS = re.compile(r'requireKnownKeys\(\w+, \{([^}]*)\}, "((?:[^"\\]|\\.)*)"')
    SECTION = re.compile(r'^the \\"([a-zA-Z.]+)\\" section of')
    # The sections that the tooling alone reads, which the engine checks only for their types.
    TOOLING_SECTIONS = ("cpp", "platforms")

    @staticmethod
    def engine_sections() -> dict[tuple[str, ...], set[str]]:
        """Returns the keys that each `requireKnownKeys` of `AppConfig.cpp` and `Config.cpp` takes, by the path of its section in the schema, with the top level of `AppConfig::kKeys` and the sections that the Lua layer reads."""
        sections: dict[tuple[str, ...], set[str]] = {}
        layer_sections: set[str] = set()
        for index, source in enumerate(EngineAppKeysTest.SOURCES):
            for keys, context in EngineAppKeysTest.KNOWN_KEYS.findall(source.read_text(encoding="utf-8")):
                path = EngineAppKeysTest.schema_path(context)
                sections[path] = set(re.findall(r'"([A-Za-z]+)"', keys))
                if index > 0:
                    layer_sections.add(path[1])
        top_level = EngineAppKeysTest.TOP_LEVEL.search(EngineAppKeysTest.SOURCES[0].read_text(encoding="utf-8"))
        assert top_level, "The file AppConfig.cpp defines no AppConfig::kKeys."
        sections[("properties",)] = set(re.findall(r'"([A-Za-z]+)"', top_level.group(1))) | layer_sections
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
