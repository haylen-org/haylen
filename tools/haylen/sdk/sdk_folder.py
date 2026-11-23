from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from haylen.core.configuration import Configuration
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.sdk.host_tool import HostTool
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice


@dataclass(frozen=True)
class SdkFolder:
    """The folder that an engine port installs an SDK into, with a folder of its own for each slice. A port keeps the slices of the folder that it built from the same engine sources in the same configuration, so building one slice later adds to the SDK, and an SDK of other sources or another configuration leaves the folder before the first new slice installs."""

    folder: Path
    port: str
    port_version: str
    revision: str
    configuration: Configuration

    def begin(self) -> None:
        """Removes the folder when it holds an SDK of another port, other engine sources or another configuration, whose slices never join the new ones."""
        if (self.folder / SdkManifest.FILE).is_file() and self.kept() is None:
            Folders.remove(self.folder)

    def kept(self) -> SdkManifest | None:
        """Returns the manifest of the slices that stay, or nothing when the folder holds no SDK of the same port, sources and configuration."""
        manifest = SdkManifest.usable(self.folder)
        if manifest is None:
            return None
        if (manifest.port, manifest.revision, manifest.configuration) != (self.port, self.revision, self.configuration):
            return None
        return manifest

    def slice_folder(self, name: str) -> Path:
        """Returns the empty folder of a slice, such as `macos-arm64`, removing what an earlier build installed there."""
        folder = self.folder / name
        Folders.remove(folder)
        folder.mkdir(parents=True)
        return folder

    def save(self, added: SdkSlice, tools: tuple[HostTool, ...]) -> SdkManifest:
        """Writes the manifest with the slices that stay and the slice that the port built now, which replaces a kept slice of the same system, environment and architectures, and the host tools of that slice."""
        kept = self.kept()
        slices = [entry for entry in kept.slices if not SdkFolder.replaces(added, entry)] if kept else []
        names = {(tool.name, tool.system, tool.architecture) for tool in tools}
        host_tools = [tool for tool in kept.host_tools if (tool.name, tool.system, tool.architecture) not in names] if kept else []
        manifest = SdkManifest(
            folder=self.folder.resolve(),
            port=self.port,
            port_version=self.port_version,
            engine_version=Repository.engine_version(),
            revision=self.revision,
            configuration=self.configuration,
            slices=(*slices, added),
            host_tools=(*host_tools, *tools),
            files=MappingProxyType(dict(kept.files) if kept else {}),
        )
        manifest.save()
        return manifest

    @staticmethod
    def replaces(added: SdkSlice, kept: SdkSlice) -> bool:
        return added.system == kept.system and added.environment is kept.environment and bool(set(added.architectures) & set(kept.architectures))
