from dataclasses import dataclass

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.package_layout import PackageLayout
from haylen.core.product_request import ProductRequest
from haylen.core.resolved_project import ResolvedProject
from haylen.plugins.resolved_plugin import ResolvedPlugin


@dataclass(frozen=True)
class ResolvedProduct:
    """The product that one request builds, such as an APK for several Android ABIs or a universal macOS app: one resolved project per effective target, and an identity of the aggregate that differs from the identity of each target. The targets of one product share the app, its platform and its plugins, and only their architecture and SDK slice differ."""

    request: ProductRequest
    backend: str
    identity: str
    projects: tuple[ResolvedProject, ...]

    @property
    def app(self) -> AppManifest:
        return self.projects[0].app

    @property
    def package(self) -> PackageLayout:
        return self.projects[0].package

    @property
    def platform(self) -> str:
        return self.projects[0].platform

    @property
    def target_name(self) -> str:
        return self.request.target

    @property
    def configuration(self) -> Configuration:
        return self.request.configuration

    @property
    def plugins(self) -> tuple[ResolvedPlugin, ...]:
        return self.projects[0].plugins

    @property
    def primary(self) -> ResolvedProject:
        """The project of the first architecture, whose values every target of the product shares."""
        return self.projects[0]

    def describe(self) -> dict[str, object]:
        return {
            "app": {
                "folder": str(self.app.folder),
                "name": self.app.name,
                "identifier": self.app.identifier,
                "version": self.app.version,
                "build": self.app.build,
                "kind": self.app.kind.value,
            },
            "backend": self.backend,
            "identity": self.identity,
            "targets": [project.describe() for project in self.projects],
        }
