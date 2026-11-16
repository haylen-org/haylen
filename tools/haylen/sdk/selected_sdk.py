from dataclasses import dataclass
from pathlib import Path

from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice


@dataclass(frozen=True)
class SelectedSdk:
    """The SDK and the slice of it that a target consumes, chosen and checked once before the project of the target is generated."""

    manifest: SdkManifest
    slice: SdkSlice

    def file(self, name: str) -> Path:
        """Returns a file of the slice by its name, or a shared file of the SDK when the slice has none of that name."""
        if name in self.slice.files:
            return self.manifest.path(self.slice.files[name])
        return self.manifest.file(name)

    def describe(self) -> dict[str, object]:
        return {
            "folder": str(self.manifest.folder),
            "port": self.manifest.port,
            "revision": self.manifest.revision,
            "configuration": self.manifest.configuration.value,
            "slice": self.slice.describe(),
        }
