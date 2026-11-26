import hashlib
from dataclasses import dataclass
from pathlib import Path

from haylen.core.repository import Repository


@dataclass(frozen=True)
class ProductFolders:
    """The folders of one product of an app under `build/apps/<app>-<hash>/<product>/`, which no other app, target or configuration shares: the generated project, the trees and caches of the native tools and the built products. The app folder holds none of them, so they are disposable and a clean build makes them again."""

    root: Path

    @staticmethod
    def app_folder(app: Path) -> Path:
        """The build folder of an app, named after its folder and a hash of its path, so apps whose folders share a name never share builds."""
        return Repository.APPS / f"{app.name}-{hashlib.sha256(str(app.resolve()).encode()).hexdigest()[:8]}"

    @staticmethod
    def of(app: Path, identity: str) -> ProductFolders:
        return ProductFolders(ProductFolders.app_folder(app) / identity)

    @property
    def project(self) -> Path:
        return self.root / "project"

    @property
    def build(self) -> Path:
        return self.root / "build"

    @property
    def products(self) -> Path:
        return self.root / "products"

    @property
    def symbols(self) -> Path:
        """The symbol files of a Release build, with the line tables of the app and of the engine, which crash reports read and which never ship."""
        return self.root / "symbols"

    @property
    def package(self) -> Path:
        """The staged copy of the package, with every plugin at `plugins/<id>/`."""
        return self.root / "package"
