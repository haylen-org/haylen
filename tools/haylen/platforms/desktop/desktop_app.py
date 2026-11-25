import re
from dataclasses import dataclass

from haylen.core.app_category import AppCategory
from haylen.core.app_manifest import AppManifest


@dataclass(frozen=True)
class DesktopApp:
    """What the projects and packages of a desktop app take from `haylen.json`, with the defaults that the engine applies: the publisher follows the name of the app until the app gives its own, and the executable and the packages take the name of the app folder in lowercase dash-case, which every package format accepts, or `haylen-app` for a folder name without letters or digits."""

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

    @staticmethod
    def of(app: AppManifest) -> DesktopApp:
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
            executable=DesktopApp.executable_of(app),
        )

    @staticmethod
    def executable_of(app: AppManifest) -> str:
        name = re.sub(r"[^a-z0-9]+", "-", app.folder.name.lower()).strip("-")
        return name or AppManifest.DEFAULT_SLUG
