import contextlib
import shutil
import zipfile
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from haylen.core.app_manifest import AppManifest


@dataclass(frozen=True)
class PackageLayout:
    """Where the files of the package of an app lie on disk. The package holds `haylen.json` and `content` of the app folder and `plugins/<id>/plugin.json` of every plugin, wherever the plugin folder is, so the runtime never resolves a reference, and the package of an app that runs Lua also holds `source` of the app and `plugins/<id>/source` of every plugin."""

    MANIFEST = "haylen.json"
    CONTENT = "content"
    SOURCE = "source"
    PLUGIN_MANIFEST = "plugin.json"
    IGNORED = frozenset({".DS_Store"})

    folder: Path
    plugins: Mapping[str, Path]
    lua: bool
    manifest: Path

    @staticmethod
    def of(folder: Path, plugins: Mapping[str, Path], lua: bool) -> PackageLayout:
        return PackageLayout(folder, MappingProxyType(dict(plugins)), lua, folder / PackageLayout.MANIFEST)

    @staticmethod
    def of_app(app: AppManifest, plugins: Mapping[str, Path]) -> PackageLayout:
        """The layout of the package of an app, whose `haylen.json` carries the identity that the tooling resolved for it."""
        return PackageLayout(app.folder, MappingProxyType(dict(plugins)), app.runs_lua, app.package_manifest())

    @property
    def roots(self) -> list[tuple[str, Path]]:
        """The package path of every file and folder of the package with where it lies on disk."""
        roots = [(PackageLayout.MANIFEST, self.manifest), (PackageLayout.CONTENT, self.folder / PackageLayout.CONTENT)]
        if self.lua:
            roots.append((PackageLayout.SOURCE, self.folder / PackageLayout.SOURCE))
        for identifier, plugin in self.plugins.items():
            roots.append((f"plugins/{identifier}/{PackageLayout.PLUGIN_MANIFEST}", plugin / PackageLayout.PLUGIN_MANIFEST))
            if self.lua:
                roots.append((f"plugins/{identifier}/{PackageLayout.SOURCE}", plugin / PackageLayout.SOURCE))
        return roots

    def files(self) -> dict[str, Path]:
        """Every file of the package by its package path, in the order of the paths."""
        found: dict[str, Path] = {}
        for prefix, root in self.roots:
            if root.is_file():
                found[prefix] = root
                continue
            for path in root.rglob("*") if root.is_dir() else []:
                if path.is_file() and path.name not in PackageLayout.IGNORED:
                    found[f"{prefix}/{path.relative_to(root).as_posix()}"] = path
        return dict(sorted(found.items()))

    def to_disk(self, path: str) -> Path:
        """Returns where a package path lies on disk. A path outside the plugins names a file of the app folder."""
        for prefix, root in self.roots:
            if path == prefix:
                return root
            if path.startswith(f"{prefix}/"):
                return root / path[len(prefix) + 1 :]
        return self.folder / path

    def to_package(self, file: Path) -> str | None:
        """Returns the package path of a file on disk, or nothing for a file outside the package. The plugins come first, since a plugin folder may lie inside the app folder."""
        resolved = file.resolve()
        for prefix, root in sorted(self.roots, key=lambda entry: len(entry[1].resolve().parts), reverse=True):
            with contextlib.suppress(ValueError):
                relative = resolved.relative_to(root.resolve()).as_posix()
                return prefix if relative == "." else f"{prefix}/{relative}"
        return None

    def stage(self, staged: Path) -> Path:
        """Brings a copy of the package up to date, copying the files that changed and deleting the ones that left, and returns it. Players and the content tool read a package folder, which holds every plugin at `plugins/<id>/` only in such a copy."""
        files = self.files()
        for relative, path in files.items():
            target = staged / relative
            status = path.stat()
            if target.is_file() and target.stat().st_size == status.st_size and target.stat().st_mtime_ns == status.st_mtime_ns:
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)

        for path in sorted(staged.rglob("*"), reverse=True):
            if path.is_dir() and not any(path.iterdir()):
                path.rmdir()
            elif not path.is_dir() and path.relative_to(staged).as_posix() not in files:
                path.unlink()
        return staged

    def zip(self, output: Path) -> None:
        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED) as archive:
            for relative, path in self.files().items():
                archive.write(path, relative)
