from collections.abc import Mapping
from dataclasses import dataclass
from types import MappingProxyType

from haylen.core.frozen import Frozen


@dataclass(frozen=True)
class PluginReference:
    """A plugin that `haylen.json` lists, with where it comes from and the values of its parameters. Without `path` and `git` it is the folder `plugins/<id>/` of the app. A `path` alone is absolute or relative to the app folder. A `git` is a repository at `ref`, a branch, tag or commit, and a `path` next to it names the plugin folder inside the repository."""

    id: str
    path: str | None
    git: str | None
    ref: str | None
    parameters: Mapping[str, object]

    @staticmethod
    def parse(identifier: str, entry: Mapping[str, object]) -> PluginReference:
        """Reads an entry that follows the schema of the `plugins` section."""
        parameters = Frozen.freeze(dict(entry.get("parameters", {})))
        path = entry.get("path")
        git = entry.get("git")
        ref = entry.get("ref")
        return PluginReference(identifier, str(path) if path else None, str(git) if git else None, str(ref) if ref else None, parameters or MappingProxyType({}))

    @property
    def is_external(self) -> bool:
        """Tells whether the plugin lives outside the folder `plugins/<id>/` of the app."""
        return self.path is not None or self.git is not None
