from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.core.frozen import Frozen
from haylen.plugins.implementation import Implementation
from haylen.plugins.plugin_manifest import PluginManifest
from haylen.plugins.plugin_reference import PluginReference


@dataclass(frozen=True)
class ResolvedPlugin:
    """A plugin of an app for one platform: its manifest, how it works there, where it came from and the values of its parameters with the defaults applied. Its contributions for the platform are part of the merged contributions of the project."""

    manifest: PluginManifest
    reference: PluginReference
    implementation: Implementation
    parameters: Mapping[str, object]

    @property
    def id(self) -> str:
        return self.manifest.id

    @property
    def folder(self) -> Path:
        return self.manifest.folder

    def describe(self) -> dict[str, object]:
        return {
            "id": self.id,
            "version": self.manifest.version.text,
            "folder": str(self.folder),
            "implementation": self.implementation.value,
            "parameters": Frozen.thaw(self.parameters),
            "permissions": list(self.manifest.permissions),
        }
