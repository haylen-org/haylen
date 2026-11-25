import re
from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from haylen.core.app_category import AppCategory
from haylen.core.app_manifest import AppManifest
from haylen.core.resolved_product import ResolvedProduct


@dataclass(frozen=True)
class DesktopApp:
    """What the projects and packages of a desktop app take from `haylen.json`, with the defaults that the engine applies: the publisher follows the name of the app until the app gives its own, the executable and the packages take the `executableName` of the section of the platform, or else the name of the app folder in lowercase dash-case, which every package format accepts, or `haylen-app` for a folder name without letters or digits, and a Windows app runs in the windows subsystem without elevation unless `console` and `executionLevel` of `platforms.windows` say otherwise."""

    name: str
    identifier: str
    version: str
    build: int
    category: AppCategory | None
    description: str
    company: str
    copyright: str
    website: str
    executable: str
    console: bool
    execution_level: str

    @staticmethod
    def of_product(product: ResolvedProduct) -> DesktopApp:
        """The desktop app of a resolved product, with the settings of the section of its platform."""
        return DesktopApp.of(product.app, product.primary.settings.get(product.platform, {}))

    @staticmethod
    def of(app: AppManifest, section: Mapping[str, object] = MappingProxyType({})) -> DesktopApp:
        """The desktop app of `haylen.json`, with the settings of the section of its platform."""
        document = app.document
        company = str(document.get("company", app.name))
        return DesktopApp(
            name=app.name,
            identifier=app.identifier,
            version=app.version,
            build=app.build,
            category=app.category,
            description=str(document.get("description", app.name)),
            company=company,
            copyright=str(document.get("copyright", f"Copyright © {company}")),
            website=str(document.get("website", "")),
            executable=str(section.get("executableName") or DesktopApp.executable_of(app)),
            console=bool(section.get("console", False)),
            execution_level=str(section.get("executionLevel", "asInvoker")),
        )

    @staticmethod
    def executable_of(app: AppManifest) -> str:
        name = re.sub(r"[^a-z0-9]+", "-", app.folder.name.lower()).strip("-")
        return name or AppManifest.DEFAULT_SLUG
