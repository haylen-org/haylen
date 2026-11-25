from dataclasses import dataclass
from pathlib import Path

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.package_layout import PackageLayout
from haylen.core.project_resolver import ProjectResolver
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource


@dataclass(frozen=True)
class ReleasePackage:
    """The app and the package that a protected release is built from, with the plugins of the platform of a target, and what `haylen.json` says about its delivery: the server of its content updates and the named packs that stores deliver on their own. A backend takes it from its resolved product, and the content commands resolve it without an SDK, since the content tool runs on the host."""

    app: AppManifest
    package: PackageLayout

    @staticmethod
    def of(product: ResolvedProduct) -> ReleasePackage:
        return ReleasePackage(product.app, product.package)

    @staticmethod
    def resolve(app_folder: Path, target: BackendRegistry.Target, context: ToolingContext, registry: BackendRegistry) -> ReleasePackage:
        """Resolves the plugins of an app for the platform of a target in the Release configuration, which needs a pinned commit of every plugin of a repository."""
        app = AppManifest.load(app_folder)
        source = PluginSource(app.folder, PluginLock.load(app.folder), context.process, context.terminal)
        plugins = PluginResolver(app, source)
        platform = target.description.platform
        loaded = plugins.load(Configuration.RELEASE)
        resolved = plugins.for_platform(loaded, platform, ProjectResolver.contribution_types(registry, platform))
        return ReleasePackage(app, PackageLayout.of_app(app, {plugin.id: plugin.folder for plugin in resolved.plugins}))

    @property
    def content_server(self) -> str | None:
        """The address of the server of content updates, which delivers the content that the app does not need to start."""
        updates = self.app.section("content").get("updates", {})
        return updates.get("baseUrl")

    @property
    def packs(self) -> tuple[str, ...]:
        """The named packs of the delivery rules, whose shards stores deliver apart from the app."""
        rules = self.app.section("content").get("delivery", ())
        return tuple(sorted({str(rule["pack"]) for rule in rules if "pack" in rule}))

    @property
    def splash_logo(self) -> str | None:
        """The package path of the splash logo, the one file of the package that platforms show on their own before the engine starts."""
        logo = self.app.section("splash").get("logo")
        return f"content/{logo}" if logo else None
