import hashlib
import os
import shutil
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.terminal import Terminal


class Download:
    """Downloads the pinned tools of the builds into `.tools`, each checked against its pinned SHA-256 and moved into place only once it is complete, so an interrupted download never leaves a tool that seems installed. It never downloads a proprietary SDK or accepts a license."""

    @staticmethod
    def file(url: str, target: Path, sha256: str, terminal: Terminal) -> None:
        target.parent.mkdir(parents=True, exist_ok=True)
        terminal.step(f"Downloading `{url}`")
        try:
            urllib.request.urlretrieve(url, target)
        except OSError as error:
            target.unlink(missing_ok=True)
            reason = error.reason if isinstance(error, urllib.error.URLError) and not isinstance(error, urllib.error.HTTPError) else error
            raise MissingToolchain(f'The download of `{url}` failed with "{reason}". Check the network of this machine and try again.') from None
        if hashlib.sha256(target.read_bytes()).hexdigest() != sha256:
            target.unlink()
            raise MissingToolchain(f'The download of `{url}` does not match its pinned SHA-256 "{sha256}", so it was deleted. Try again, and if it still differs, the file on the server changed.')

    @staticmethod
    def archive(url: str, sha256: str, folder: Path, executable: Path, root: str, terminal: Terminal) -> None:
        """Downloads the pinned zip of a tool and unpacks it into `folder`, taking the folder `root` of the zip when it names one, with its executable runnable."""
        partial = folder.with_name(f"{folder.name}.{os.getpid()}.part")
        archive = partial.with_name(f"{partial.name}.zip")
        try:
            Download.file(url, archive, sha256, terminal)
            with zipfile.ZipFile(archive) as package:
                package.extractall(partial)
        except BaseException:
            archive.unlink(missing_ok=True)
            shutil.rmtree(partial, ignore_errors=True)
            raise
        archive.unlink()

        unpacked = partial / root if root else partial
        (unpacked / executable.relative_to(folder)).chmod(0o755)
        if executable.exists():
            # Another command installed the tool while this one unpacked it.
            shutil.rmtree(partial)
            return
        # A folder without its executable is what an unpack that stopped halfway left.
        shutil.rmtree(folder, ignore_errors=True)
        unpacked.replace(folder)
        if root:
            partial.rmdir()
