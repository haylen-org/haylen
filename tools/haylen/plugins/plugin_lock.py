import json
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository


@dataclass(frozen=True)
class PluginLock:
    """The `haylen.lock` of an app, which pins every plugin that comes from a git repository to the commit that its branch or tag named when the developer added or updated it, so every build takes the same code and a Release build is reproducible. Each entry is `{"git", "ref", "commit"}` by plugin id."""

    FILE = "haylen.lock"
    VERSION = 1

    folder: Path
    entries: Mapping[str, Mapping[str, str]]

    @staticmethod
    def load(folder: Path) -> PluginLock:
        path = folder / PluginLock.FILE
        if not path.is_file():
            return PluginLock(folder, MappingProxyType({}))
        origin = Repository.shown(path)
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            raise InvalidManifest(PluginLock.damaged(origin), origin=origin) from None

        entries = document.get("plugins") if isinstance(document, dict) and document.get("version") == PluginLock.VERSION else None
        valid = isinstance(entries, dict) and all(
            isinstance(entry, dict) and set(entry) == {"git", "ref", "commit"} and all(isinstance(value, str) for value in entry.values()) for entry in entries.values()
        )
        if not valid:
            raise InvalidManifest(PluginLock.damaged(origin), origin=origin)
        return PluginLock(folder, MappingProxyType({identifier: MappingProxyType(dict(entry)) for identifier, entry in entries.items()}))

    @staticmethod
    def damaged(origin: str) -> str:
        return f'The file `{origin}` is damaged or an older version of Haylen wrote it. Write it again with "haylen plugin update".'

    def commit(self, identifier: str, git: str, ref: str) -> str | None:
        """Returns the pinned commit of a plugin while the lock names the same repository and ref as `haylen.json`."""
        entry = self.entries.get(identifier)
        if entry is None or entry["git"] != git or entry["ref"] != ref:
            return None
        return entry["commit"]

    def with_entry(self, identifier: str, git: str, ref: str, commit: str) -> PluginLock:
        entries = {key: dict(value) for key, value in self.entries.items()}
        entries[identifier] = {"git": git, "ref": ref, "commit": commit}
        return PluginLock(self.folder, MappingProxyType(entries))

    def without(self, identifier: str) -> PluginLock:
        return PluginLock(self.folder, MappingProxyType({key: value for key, value in self.entries.items() if key != identifier}))

    def save(self) -> None:
        path = self.folder / PluginLock.FILE
        if not self.entries:
            path.unlink(missing_ok=True)
            return
        plugins = {identifier: dict(self.entries[identifier]) for identifier in sorted(self.entries)}
        path.write_text(json.dumps({"version": PluginLock.VERSION, "plugins": plugins}, indent=4) + "\n", encoding="utf-8")
