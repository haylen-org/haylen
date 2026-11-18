from collections.abc import Mapping
from dataclasses import dataclass

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.haylen_error import HaylenError
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.frozen import Frozen
from haylen.core.schema_validator import SchemaValidator
from haylen.platforms.contribution_type import ContributionType
from haylen.plugins.contribution import Contribution
from haylen.plugins.parameter_substitution import ParameterSubstitution
from haylen.plugins.plugin_manifest import PluginManifest
from haylen.plugins.plugin_reference import PluginReference
from haylen.plugins.plugin_source import PluginSource
from haylen.plugins.resolved_plugin import ResolvedPlugin


class PluginResolver:
    """Resolves the plugins of an app once: it loads every plugin that `haylen.json` lists, checks their ids, their dependencies with their versions and their permissions, and orders them so every plugin follows the plugins it depends on and otherwise keeps the order of `haylen.json`. For each platform it then checks that every plugin supports it, applies the parameters and checks every contribution against the type that a backend declares for the platform."""

    @dataclass(frozen=True)
    class Platform:
        """The plugins of an app for one platform, in load order, with their contributions for it and the parameters substituted."""

        plugins: tuple[ResolvedPlugin, ...]
        contributions: tuple[Contribution, ...]

    def __init__(self, app: AppManifest, source: PluginSource) -> None:
        self.app = app
        self.source = source
        self.validators: dict[str, SchemaValidator] = {}

    def load(self, configuration: Configuration) -> tuple[tuple[PluginReference, PluginManifest], ...]:
        """Loads and checks every plugin of the app, and returns them in load order with their references, or fails with every problem at once."""
        problems: list[str] = []
        loaded: dict[str, tuple[PluginReference, PluginManifest]] = {}
        for reference in self.app.plugins:
            try:
                manifest = PluginManifest.load(self.source.folder(reference, configuration))
            except HaylenError as error:
                problems.append(error.message)
                continue
            if manifest.id != reference.id:
                problems.append(f'The key "plugins.{reference.id}" names `{manifest.origin}`, whose id is "{manifest.id}". List the plugin under the id of its "{PluginManifest.FILE}".')
                continue
            loaded[reference.id] = (reference, manifest)

        for _, manifest in loaded.values():
            for dependency, version in manifest.dependencies.items():
                if dependency not in loaded:
                    problems.append(f'The plugin "{manifest.id}" depends on the plugin "{dependency}", which `{self.app.origin}` does not list.')
                elif not version.accepts(loaded[dependency][1].version):
                    problems.append(
                        f'The plugin "{manifest.id}" needs the plugin "{dependency}" {version.text} or a newer version of the same major version, and the app lists {loaded[dependency][1].version.text}.'
                    )
        problems += PluginResolver.permission_conflicts([manifest for _, manifest in loaded.values()])
        if problems:
            lines = "\n".join(f"  {line}" for problem in problems for line in problem.splitlines())
            raise InvalidManifest(f"The plugins of `{self.app.origin}` have these problems:\n{lines}", origin=self.app.origin)
        return PluginResolver.ordered(loaded)

    @staticmethod
    def ordered(loaded: Mapping[str, tuple[PluginReference, PluginManifest]]) -> tuple[tuple[PluginReference, PluginManifest], ...]:
        ordered: list[str] = []
        for identifier in loaded:
            PluginResolver.visit(identifier, loaded, ordered, [])
        return tuple(loaded[identifier] for identifier in ordered)

    @staticmethod
    def visit(identifier: str, loaded: Mapping[str, tuple[PluginReference, PluginManifest]], ordered: list[str], visiting: list[str]) -> None:
        """Appends a plugin to the order after the plugins it depends on, and fails on plugins that depend on each other in a cycle."""
        if identifier in ordered:
            return
        if identifier in visiting:
            cycle = ", ".join(f'"{name}"' for name in visiting[visiting.index(identifier) :])
            raise InvalidManifest(f"The plugins {cycle} depend on each other in a cycle.")

        visiting.append(identifier)
        for dependency in loaded[identifier][1].dependencies:
            PluginResolver.visit(dependency, loaded, ordered, visiting)
        visiting.pop()
        ordered.append(identifier)

    @staticmethod
    def permission_conflicts(manifests: list[PluginManifest]) -> list[str]:
        owners: dict[str, list[str]] = {}
        for manifest in manifests:
            for permission in manifest.permissions:
                owners.setdefault(permission, []).append(manifest.id)
        return [
            f'The plugins {SchemaValidator.choices(names, "and")} declare the permission "{permission}", which one plugin of an app owns.' for permission, names in owners.items() if len(names) > 1
        ]

    def for_platform(self, loaded: tuple[tuple[PluginReference, PluginManifest], ...], platform: str, types: Mapping[str, ContributionType]) -> PluginResolver.Platform:
        """Resolves the plugins for one platform, or fails with every problem: a plugin that does not support it, a parameter value that is missing or of the wrong type, and a contribution of a type that no backend of the platform reads or whose fields its type refuses."""
        problems: list[str] = []
        plugins: list[ResolvedPlugin] = []
        contributions: list[Contribution] = []
        for reference, manifest in loaded:
            implementation = manifest.implementation(platform)
            if implementation is None:
                supported = SchemaValidator.choices(sorted(manifest.implementations), "and")
                problems.append(f'The plugin "{manifest.id}" does not support "{platform}". It supports {supported}, so remove it from the app to build for "{platform}".')
                continue
            values, found = self.parameter_values(reference, manifest, platform)
            problems += found
            plugins.append(ResolvedPlugin(manifest, reference, implementation, Frozen.freeze(values)))
            for contribution in manifest.contributions:
                if not contribution.applies_to(platform):
                    continue
                resolved = self.contribution(contribution, values, manifest, platform, types, problems)
                if resolved is not None:
                    contributions.append(resolved)

        if problems:
            lines = "\n".join(f"  {line}" for problem in problems for line in problem.splitlines())
            raise UnsupportedTarget(f'The plugins of `{self.app.origin}` cannot build for "{platform}":\n{lines}', origin=self.app.origin)
        return PluginResolver.Platform(tuple(plugins), tuple(contributions))

    def parameter_values(self, reference: PluginReference, manifest: PluginManifest, platform: str) -> tuple[dict[str, object], list[str]]:
        """Checks the values that `haylen.json` gives a plugin and returns the ones that apply to a platform with the defaults applied."""
        key = f"plugins.{manifest.id}.parameters"
        problems = [f'The key "{key}.{name}" is no parameter of the plugin "{manifest.id}".' for name in reference.parameters if name not in manifest.parameters]
        values: dict[str, object] = {}
        for name, parameter in manifest.parameters.items():
            if not parameter.applies_to(platform):
                continue
            value = reference.parameters.get(name, parameter.default if parameter.has_default else None)
            if value is None or (value == "" and parameter.required):
                if parameter.required:
                    problems.append(f'The key "{key}.{name}" needs a value to build for "{platform}". {parameter.description}')
                continue
            if not parameter.accepts(value):
                problems.append(f'The key "{key}.{name}" must be {parameter.type_words}.')
            elif parameter.type == "file" and not (self.app.folder / str(value)).is_file():
                problems.append(f'The key "{key}.{name}" names "{value}", which is no file of the app folder.')
            else:
                values[name] = value
        return values, problems

    def contribution(
        self,
        contribution: Contribution,
        values: Mapping[str, object],
        manifest: PluginManifest,
        platform: str,
        types: Mapping[str, ContributionType],
        problems: list[str],
    ) -> Contribution | None:
        kind = types.get(contribution.type)
        if kind is None:
            known = SchemaValidator.choices(sorted(types)) if types else "none"
            problems.append(f'The contribution "{contribution.type}" at `{contribution.origin}` is of a type that no backend of "{platform}" reads. The types of "{platform}" are {known}.')
            return None
        fields = ParameterSubstitution.apply(contribution.fields, values)
        validator = self.validators.setdefault(kind.name, SchemaValidator(kind.schema))
        for problem in validator.problems(Frozen.thaw(fields)):
            problems.append(f"The contribution at `{contribution.origin}` has a problem: {problem}")
        return Contribution(contribution.type, (platform,), fields, manifest.id, contribution.origin, contribution.folder)
