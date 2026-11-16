import functools
import zipfile
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class ReleaseEntry:
    """A file of a built product that the release inspection examines: its path inside the product, its size and the function that reads its bytes only when the inspection needs them."""

    path: str
    size: int
    read: Callable[[], bytes]

    @staticmethod
    def of_folder(folder: Path) -> tuple[ReleaseEntry, ...]:
        """Lists every file of a built folder or bundle."""
        files = [path for path in sorted(folder.rglob("*")) if path.is_file()]
        return tuple(ReleaseEntry(path.relative_to(folder).as_posix(), path.stat().st_size, path.read_bytes) for path in files)

    @staticmethod
    def of_archive(archive: Path) -> tuple[ReleaseEntry, ...]:
        """Lists every file of a zip archive, such as an APK, with its uncompressed size."""
        with zipfile.ZipFile(archive) as opened:
            members = [member for member in opened.infolist() if not member.is_dir()]
        return tuple(ReleaseEntry(member.filename, member.file_size, functools.partial(ReleaseEntry.read_member, archive, member.filename)) for member in members)

    @staticmethod
    def read_member(archive: Path, name: str) -> bytes:
        with zipfile.ZipFile(archive) as opened:
            return opened.read(name)
