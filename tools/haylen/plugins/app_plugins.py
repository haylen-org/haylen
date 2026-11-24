import json
import os
import shutil
from dataclasses import dataclass
from pathlib import Path

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.haylen_error import HaylenError
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_manifest import PluginManifest
from haylen.plugins.plugin_reference import PluginReference
from haylen.plugins.plugin_source import PluginSource


class AppPlugins:
    """Adds, updates, removes and lists the plugins of an app: it edits the `plugins` section of `haylen.json`, the copies in `plugins/<id>/` of the app and the pins of `haylen.lock`, and never writes anything else of the app."""

    IGNORED = shutil.ignore_patterns(".DS_Store", ".git", "build", ".gradle", ".cxx", ".kotlin")

    @dataclass(frozen=True)
    class Status:
        """A plugin of an app with where it comes from, the problems that keep it from loading and whether `haylen.json` lists it, since a folder under `plugins/` that it does not list reaches no build."""

        id: str
        version: str | None
        implementations: tuple[str, ...]
        source: str
        problems: tuple[str, ...]
        listed: bool

        def describe(self) -> dict[str, object]:
            return {
                "id": self.id,
                "version": self.version,
                "implementations": list(self.implementations),
                "source": self.source,
                "problems": list(self.problems),
                "listed": self.listed,
            }

    def __init__(self, context: ToolingContext, folder: Path) -> None:
        self.context = context
        self.folder = folder
        self.lock = PluginLock.load(folder)
        self.source = PluginSource(folder, self.lock, context.process, context.terminal)

    @staticmethod
    def is_repository(value: str) -> bool:
        return "://" in value or value.startswith("git@") or value.endswith(".git")

    def relative(self, path: Path) -> str:
        """Writes a path relative to the app folder, so the app and its plugins move together, unless the two share nothing but the root of the file system or lie on different drives."""
        folder = self.folder.resolve()
        target = path.resolve()
        if target.anchor != folder.anchor or os.path.commonpath([folder, target]) == folder.anchor:
            return target.as_posix()
        return Path(os.path.relpath(target, folder)).as_posix()

    def document(self) -> dict:
        AppManifest.load(self.folder)
        return json.loads((self.folder / AppManifest.FILE).read_text(encoding="utf-8"))

    def save(self, document: dict) -> None:
        (self.folder / AppManifest.FILE).write_text(json.dumps(document, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")

    def add(self, plugin: str, ref: str | None, path: str | None, reference: bool) -> PluginManifest:
        """Copies a plugin folder or a folder of a git repository into `plugins/<id>/`, or with `reference` lists where it comes from, pinning a repository in `haylen.lock`. A required parameter gets an empty text for the developer to fill in."""
        repository = AppPlugins.is_repository(plugin)
        if ref and not repository:
            raise InvalidOption('The option "--ref" picks a branch, tag or commit of a git repository, and a plugin folder has none. Leave the option out for a folder.')
        if path and not repository:
            raise InvalidOption('The option "--path" names the plugin folder inside a git repository, and a plugin folder is no repository. Name the plugin folder itself instead.')

        if repository:
            git = plugin if PluginSource.REMOTE.fullmatch(plugin) else self.relative(Path(plugin).expanduser())
            entry = PluginReference.parse("pending", {"git": git, **({"ref": ref} if ref else {}), **({"path": path} if path else {})})
            commit = self.source.resolve_commit(entry)
            source = PluginSource.inside(self.source.checkout(self.source.address(git), commit), path, 'option "--path"')
            origin = {"git": git, **({"ref": ref} if ref else {}), **({"path": path} if path else {})}
        else:
            source = Path(plugin).expanduser().resolve()
            origin = {"path": self.relative(source)}
        if not (source / PluginManifest.FILE).is_file():
            raise InvalidManifest(f'The folder `{Repository.shown(source)}` is not a plugin, because it holds no "{PluginManifest.FILE}". Name the plugin folder inside a repository with "--path".')
        manifest = PluginManifest.load(source)

        document = self.document()
        listed = document.setdefault("plugins", {})
        values = dict(listed.get(manifest.id, {}).get("parameters", {}))
        copy = self.folder / "plugins" / manifest.id
        if reference and copy.exists() and copy.resolve() != source:
            Folders.remove(copy)
            self.context.terminal.info(f"Deleted the copy `{Repository.shown(copy)}`, since the app now references the plugin.")
        if not reference and source != copy.resolve():
            Folders.remove(copy)
            shutil.copytree(source, copy, symlinks=True, ignore=AppPlugins.IGNORED)

        for name, parameter in manifest.parameters.items():
            if name not in values and parameter.required:
                values[name] = ""
        listed[manifest.id] = {**(origin if reference else {}), **({"parameters": values} if values else {})}
        self.save(document)
        lock = self.lock.without(manifest.id)
        if reference and repository:
            lock = lock.with_entry(manifest.id, origin["git"], ref or "HEAD", commit)
        lock.save()
        return manifest

    def update(self, identifier: str) -> str:
        """Pins a git plugin again to the commit that its branch or tag names now, and returns the commit."""
        reference = next((entry for entry in AppManifest.load(self.folder).plugins if entry.id == identifier), None)
        if reference is None or reference.git is None:
            raise InvalidOption(f'The app `{Repository.shown(self.folder)}` has no plugin "{identifier}" from a git repository, which is what "haylen plugin update" pins.')
        commit = self.source.resolve_commit(reference)
        self.lock.with_entry(identifier, reference.git, reference.ref or "HEAD", commit).save()
        return commit

    def remove(self, identifier: str) -> None:
        document = self.document()
        listed = document.get("plugins", {})
        copy = self.folder / "plugins" / identifier
        if identifier not in listed and not copy.exists():
            raise InvalidOption(f'The app `{Repository.shown(self.folder)}` has no plugin "{identifier}". List its plugins with "haylen plugin list".')
        Folders.remove(copy)
        if copy.parent.is_dir() and not any(copy.parent.iterdir()):
            copy.parent.rmdir()
        if identifier in listed:
            del listed[identifier]
            if not listed:
                del document["plugins"]
            self.save(document)
        self.lock.without(identifier).save()

    def statuses(self) -> list[AppPlugins.Status]:
        """The plugins that `haylen.json` lists, in its order, then the folders under `plugins/` of the app that it does not list."""
        statuses = []
        references = AppManifest.load(self.folder).plugins
        for reference in references:
            where = reference.git or reference.path or f"plugins/{reference.id}"
            try:
                manifest = PluginManifest.load(self.source.folder(reference, Configuration.DEBUG))
            except HaylenError as error:
                statuses.append(AppPlugins.Status(reference.id, None, (), where, (error.message,), True))
                continue
            problems = [f'It needs a value for "{name}". {parameter.description}' for name, parameter in manifest.parameters.items() if parameter.required and not reference.parameters.get(name)]
            statuses.append(AppPlugins.Status(reference.id, manifest.version.text, tuple(sorted(manifest.implementations)), where, tuple(problems), True))

        listed = {reference.id for reference in references}
        plugins = self.folder / "plugins"
        folders = sorted(path for path in plugins.iterdir() if path.is_dir() and path.name not in listed) if plugins.is_dir() else []
        for folder in folders:
            where = f"plugins/{folder.name}"
            try:
                manifest = PluginManifest.load(folder)
            except HaylenError as error:
                statuses.append(AppPlugins.Status(folder.name, None, (), where, (error.message,), False))
                continue
            statuses.append(AppPlugins.Status(folder.name, manifest.version.text, tuple(sorted(manifest.implementations)), where, (), False))
        return statuses
