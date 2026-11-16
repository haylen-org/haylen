from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ProtectedRelease:
    """A protected release that the content tool built for a content profile: the folder of the whole release with its two manifests and its shards, the folder of the part that ships inside the app, which is the whole release unless the app has a content server or named packs, and the folder of the shards of each named pack, which a store delivers on its own."""

    MANIFESTS = ("app.hmanifest", "content.hmanifest")
    SHARD_SUFFIX = ".hpak"

    profile: str
    folder: Path
    shipped: Path
    packs: Mapping[str, Path]

    @staticmethod
    def size(folder: Path) -> int:
        """The bytes of the files of a release folder."""
        return sum(path.stat().st_size for path in folder.iterdir() if path.is_file())

    def describe(self) -> dict[str, object]:
        return {"profile": self.profile, "folder": str(self.folder), "shipped": str(self.shipped), "packs": {name: str(folder) for name, folder in self.packs.items()}}
