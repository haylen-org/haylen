import shutil
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.repository import Repository


class SymbolFiles:
    """Places the symbol files of a Release build in `symbols/` of the folders of its product, the one folder where crash reports find them with the line tables of the app and of the engine. They never ship: the products folder holds the stripped binaries."""

    @staticmethod
    def place(folder: Path, files: Mapping[str, Path]) -> Path:
        """Replaces the folder with the files and folders that the build made, each at its path in the folder, and fails when the build made one of them not."""
        missing = sorted(Repository.shown(path) for path in files.values() if not path.exists())
        if missing:
            listed = ", ".join(f"`{path}`" for path in missing)
            raise BuildFailed(f'The build made no symbols {listed}, although it succeeded. Build the app again with "--verbose" to see the output of the build.')

        Folders.remove(folder)
        for name, path in files.items():
            target = folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if path.is_dir():
                shutil.copytree(path, target, symlinks=True)
            else:
                shutil.copy2(path, target)
        return folder
