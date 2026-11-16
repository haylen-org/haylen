import shutil
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository


class Folders:
    """Removes the folders that a step makes again or no longer needs, such as build trees and staging folders, which may not exist yet."""

    @staticmethod
    def remove(folder: Path) -> None:
        """Removes a folder and everything in it. A file that stays, such as one that a running app or an editor holds, stops the command, since the next steps would build on what the folder held before."""
        if not folder.exists() and not folder.is_symlink():
            return
        try:
            shutil.rmtree(folder)
        except OSError as error:
            staying = Path(error.filename) if error.filename else folder
            reason = error.strerror or str(error)
            raise BuildFailed(
                f"The folder `{Repository.shown(folder)}` could not be removed, since `{Repository.shown(staying)}` stays: {reason}. Close what uses it, such as a running app or an editor, and try again."
            ) from error
