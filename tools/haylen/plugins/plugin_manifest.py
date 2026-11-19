import json
import re
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.schema_validator import SchemaValidator
from haylen.core.version import Version
from haylen.plugins.contribution import Contribution
from haylen.plugins.implementation import Implementation
from haylen.plugins.plugin_parameter import PluginParameter


@dataclass(frozen=True)
class PluginManifest:
    """The `plugin.json` of a plugin folder, read and checked once: its identity, the version of the plugin interface it follows, how it works on each platform, the plugins it depends on, the permissions it owns, its parameters and its typed contributions. The engine reads the same file from the package."""

    FILE = "plugin.json"
    # A placeholder of a parameter in a value of a contribution. Build settings such as `$(NAME)` and shell expansions such as `${BUILD_DIR%/Build/*}` are no placeholders.
    PLACEHOLDER = re.compile(r"\$\{([a-z][A-Za-z0-9]*)\}")

    folder: Path
    id: str
    name: str
    version: Version
    description: str
    implementations: Mapping[str, Implementation]
    dependencies: Mapping[str, Version]
    permissions: tuple[str, ...]
    parameters: Mapping[str, PluginParameter]
    contributions: tuple[Contribution, ...]

    @staticmethod
    def load(folder: Path) -> PluginManifest:
        """Reads `plugin.json` of a plugin folder and fails with every problem it has."""
        path = folder / PluginManifest.FILE
        origin = Repository.shown(path)
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise InvalidManifest(f"The file `{origin}` is not JSON: {error.msg} at line {error.lineno}, column {error.colno}.", origin=origin) from None
        SchemaValidator.of("plugin.schema.json").require(document, origin)

        implementations = {platform: Implementation(value) for platform, value in document["implementations"].items()}
        parameters = {name: PluginParameter.parse(name, entry) for name, entry in document.get("parameters", {}).items()}
        contributions = tuple(PluginManifest.contribution(document["id"], index, entry, origin, folder) for index, entry in enumerate(document.get("contributions", [])))
        manifest = PluginManifest(
            folder=folder.resolve(),
            id=document["id"],
            name=document["name"],
            version=Version.parse(document["version"]),
            description=document["description"],
            implementations=Frozen.freeze(implementations),
            dependencies=Frozen.freeze({identifier: Version.parse(version) for identifier, version in document.get("dependencies", {}).items()}),
            permissions=tuple(document.get("permissions", ())),
            parameters=Frozen.freeze(parameters),
            contributions=contributions,
        )
        problems = manifest.problems()
        if problems:
            lines = "\n".join(f"  {problem}" for problem in problems)
            raise InvalidManifest(f"The file `{origin}` has these problems:\n{lines}", origin=origin)
        return manifest

    @staticmethod
    def contribution(owner: str, index: int, entry: Mapping[str, object], origin: str, folder: Path) -> Contribution:
        fields = {key: value for key, value in entry.items() if key not in ("type", "platforms")}
        return Contribution(str(entry["type"]), tuple(entry["platforms"]), Frozen.freeze(fields), owner, f"{origin} at contributions[{index}]", folder.resolve())

    @property
    def origin(self) -> str:
        return Repository.shown(self.folder / PluginManifest.FILE)

    def problems(self) -> list[str]:
        """Checks what the schema cannot: the platforms of parameters and contributions, the defaults, the placeholders and the permissions."""
        problems: list[str] = []
        native = {platform for platform, implementation in self.implementations.items() if implementation is Implementation.NATIVE}
        for parameter in self.parameters.values():
            outside = sorted(set(parameter.platforms or ()) - set(self.implementations))
            if outside:
                problems.append(f'The parameter "{parameter.name}" applies to {SchemaValidator.choices(outside, "and")}, which "implementations" does not list.')
            if parameter.has_default and not parameter.accepts(parameter.default):
                problems.append(f'The default of the parameter "{parameter.name}" must be {parameter.type_words}.')
            if parameter.required and parameter.has_default:
                problems.append(f'The parameter "{parameter.name}" is required, so it has no default.')

        for index, contribution in enumerate(self.contributions):
            outside = sorted(set(contribution.platforms) - native)
            if outside:
                problems.append(
                    f'The contribution "contributions[{index}]" of type "{contribution.type}" names {SchemaValidator.choices(outside, "and")}, where "implementations" does not give the plugin a native part.'
                )
            for name in sorted(PluginManifest.placeholders(contribution.fields)):
                if name not in self.parameters:
                    problems.append(f'The contribution "contributions[{index}]" names the parameter "{name}" in "${{{name}}}", which "parameters" does not declare.')

        contributed = {platform for contribution in self.contributions for platform in contribution.platforms}
        for platform in sorted(native - contributed):
            problems.append(f'The platform "{platform}" has a native part in "implementations", but no contribution brings it. Add its contributions, or make it "lua".')
        if self.id in self.dependencies:
            problems.append('The plugin depends on itself in "dependencies".')
        return problems

    @staticmethod
    def placeholders(value: object) -> set[str]:
        """Returns the parameters that the placeholders of a value name, in its texts and in the keys of its objects."""
        if isinstance(value, str):
            return set(PluginManifest.PLACEHOLDER.findall(value))
        if isinstance(value, Mapping):
            names: set[str] = set()
            for key, item in value.items():
                names |= PluginManifest.placeholders(key) | PluginManifest.placeholders(item)
            return names
        if isinstance(value, tuple):
            names = set()
            for item in value:
                names |= PluginManifest.placeholders(item)
            return names
        return set()

    def implementation(self, platform: str) -> Implementation | None:
        return self.implementations.get(platform)
