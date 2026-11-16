import json
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.frozen import Frozen
from haylen.core.product_inputs import ProductInputs
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.android.android_contributions import AndroidContributions
from haylen.plugins.implementation import Implementation
from haylen.plugins.merged_contribution import MergedContribution
from haylen.plugins.parameter_substitution import ParameterSubstitution
from haylen.plugins.plugin_manifest import PluginManifest
from haylen.plugins.resolved_plugin import ResolvedPlugin


@dataclass(frozen=True)
class AndroidPluginParts:
    """What the plugins of an app bring to its Android project, from their merged contributions: the library module of each plugin, which the project holds at `plugins/<id>/`, the Maven repositories of their SDKs, the Gradle plugins that the app module applies, the manifest placeholders, the files they place in the project, the packs that one of them delivers from Google Play and `haylen-plugins.json`, which names the plugins whose classes the runtime loads when the process starts. The runtime never reads `haylen.json` or a `plugin.json` for them, which a protected release keeps inside its encrypted app domain."""

    PLUGINS_FILE = "app/src/main/assets/haylen-plugins.json"
    MODULES = "plugins"

    modules: tuple[tuple[str, Path], ...]
    repositories: tuple[str, ...]
    gradle_plugins: tuple[tuple[str, str], ...]
    placeholders: tuple[tuple[str, str], ...]
    project_files: tuple[tuple[str, Path], ...]
    pack_delivery: MergedContribution | None
    plugins_file: bytes

    @staticmethod
    def of(product: ResolvedProduct) -> AndroidPluginParts:
        project = product.primary
        modules = []
        for contribution in project.of_type(AndroidContributions.GRADLE_MODULE):
            owner = contribution.owners[0]
            if any(name == owner for name, _ in modules):
                raise InvalidContribution(
                    f'The plugin "{owner}" gives a second Gradle module at `{contribution.origins[0]}`. A plugin brings one Android library module.', origin=contribution.origins[0]
                )
            module = contribution.folders[0] / str(contribution.field("path"))
            for needed in ("build.gradle.kts", "src/main/AndroidManifest.xml"):
                if not (module / needed).is_file():
                    raise InvalidContribution(
                        f'The Gradle module `{Repository.shown(module)}` of the plugin "{owner}" has no "{needed}". Point "path" of `{contribution.origins[0]}` at an Android library module.',
                        origin=contribution.origins[0],
                    )
            modules.append((owner, module))

        packs = project.of_type(AndroidContributions.ASSET_PACKS)
        listed = [
            {"id": plugin.id, "version": plugin.manifest.version.text, "parameters": Frozen.thaw(plugin.parameters)} for plugin in product.plugins if plugin.implementation is Implementation.NATIVE
        ]
        return AndroidPluginParts(
            modules=tuple(modules),
            repositories=tuple(str(contribution.field("url")) for contribution in project.of_type(AndroidContributions.MAVEN_REPOSITORY)),
            gradle_plugins=tuple((str(contribution.field("id")), str(contribution.field("version"))) for contribution in project.of_type(AndroidContributions.GRADLE_PLUGIN)),
            placeholders=tuple(
                (str(contribution.field("name")), ParameterSubstitution.text(contribution.field("value"))) for contribution in project.of_type(AndroidContributions.MANIFEST_PLACEHOLDER)
            ),
            project_files=AndroidPluginParts.files_of(product),
            pack_delivery=packs[0] if packs else None,
            plugins_file=(json.dumps({"plugins": listed}, indent=4) + "\n").encode(),
        )

    @staticmethod
    def files_of(product: ResolvedProduct) -> tuple[tuple[str, Path], ...]:
        """The files that plugins place in the project by their paths there. A source that names a file parameter of its plugin is a file of the app folder, and any other source is a file of the plugin folder."""
        plugins = {plugin.id: plugin for plugin in product.plugins}
        placed = []
        for contribution in product.primary.of_type(AndroidContributions.PROJECT_FILE):
            if contribution.field("from") is None:
                continue
            owner = plugins[contribution.owners[0]]
            folder = product.app.folder if AndroidPluginParts.names_app_file(owner, contribution) else contribution.folders[0]
            source = folder / str(contribution.field("from"))
            if not source.exists():
                raise InvalidContribution(f"The file `{Repository.shown(source)}` that `{contribution.origins[0]}` places in the Android project does not exist.", origin=contribution.origins[0])
            placed.append((str(contribution.field("to")), source))
        return tuple(placed)

    @staticmethod
    def names_app_file(plugin: ResolvedPlugin, contribution: MergedContribution) -> bool:
        """Tells whether the source of a file of a plugin is a placeholder of one of its file parameters, by the contribution of its `plugin.json` that gives the same file."""
        for raw in plugin.manifest.contributions:
            if raw.type != AndroidContributions.PROJECT_FILE or ParameterSubstitution.apply(raw.fields, plugin.parameters) != contribution.fields:
                continue
            whole = PluginManifest.PLACEHOLDER.fullmatch(str(raw.fields.get("from", "")))
            return whole is not None and plugin.manifest.parameters[whole[1]].type == "file"
        return False

    def files(self) -> dict[str, bytes | Path]:
        """The files of the project that the plugins bring: their modules, the files they place and the list of the plugins of the runtime."""
        files: dict[str, bytes | Path] = {AndroidPluginParts.PLUGINS_FILE: self.plugins_file}
        for identifier, module in self.modules:
            for path in sorted(ProductInputs.folder("", module).values()):
                files[f"{AndroidPluginParts.MODULES}/{identifier}/{path.relative_to(module).as_posix()}"] = path
        for destination, source in self.project_files:
            if source.is_file():
                files[destination] = source
                continue
            for path in sorted(ProductInputs.folder("", source).values()):
                files[f"{destination}/{path.relative_to(source).as_posix()}"] = path
        return files
