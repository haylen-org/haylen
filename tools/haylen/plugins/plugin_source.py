import hashlib
import re
from pathlib import Path, PureWindowsPath

from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_manifest import PluginManifest
from haylen.plugins.plugin_reference import PluginReference
from haylen.process import Process
from haylen.terminal import Terminal


class PluginSource:
    """Finds the folder of every plugin that `haylen.json` lists: the folder `plugins/<id>/` of the app, a folder that `path` names, or a git repository at the commit that `haylen.lock` pins, which it checks out once into a cache under `build/` keyed by the address and the commit."""

    COMMIT = re.compile(r"[0-9a-f]{40}")
    REMOTE = re.compile(r"[A-Za-z][A-Za-z0-9+.-]*://.+|[A-Za-z0-9._-]+@[A-Za-z0-9.-]+:.+")

    def __init__(self, app_folder: Path, lock: PluginLock, process: Process, terminal: Terminal) -> None:
        self.app_folder = app_folder
        self.lock = lock
        self.process = process
        self.terminal = terminal

    def folder(self, reference: PluginReference, configuration: Configuration) -> Path:
        """Returns the folder of a plugin and checks that it holds the plugin of that id. A Release build takes a git plugin only at a pinned commit."""
        key = f"plugins.{reference.id}"
        copy = self.app_folder / "plugins" / reference.id
        if reference.git is not None:
            folder = self.repository_folder(reference, configuration)
        elif reference.path is not None:
            folder = (self.app_folder / Path(reference.path).expanduser()).resolve()
        else:
            folder = copy

        if reference.is_external and copy.exists() and folder != copy.resolve():
            raise InvalidManifest(
                f'The plugin "{reference.id}" comes from `{Repository.shown(folder)}`, while the app folder also holds `{Repository.shown(copy)}`. Delete that folder, or leave "path" and "git" out of "{key}" to use it.'
            )
        manifest = folder / PluginManifest.FILE
        if not manifest.is_file():
            raise InvalidManifest(f'The key "{key}" names `{Repository.shown(folder)}`, which holds no "{PluginManifest.FILE}". Add the plugin with "haylen plugin add <folder or repository>".')
        return folder

    def repository_folder(self, reference: PluginReference, configuration: Configuration) -> Path:
        address = self.address(reference.git)
        ref = reference.ref or "HEAD"
        commit = ref if PluginSource.COMMIT.fullmatch(ref) else self.lock.commit(reference.id, reference.git, ref)
        if commit is None and configuration is Configuration.RELEASE:
            raise InvalidManifest(
                f'The plugin "{reference.id}" comes from `{reference.git}` at "{ref}", which "{PluginLock.FILE}" does not pin to a commit, and a Release build takes only reproducible plugins. Pin it with "haylen plugin update {reference.id}".'
            )
        checkout = self.checkout(address, commit or ref)
        return PluginSource.inside(checkout, reference.path, f"plugins.{reference.id}.path")

    def address(self, git: str) -> str:
        """Returns the address of a repository: a remote as it is, or a local repository relative to the app folder."""
        if PluginSource.REMOTE.fullmatch(git):
            return git
        return str((self.app_folder / Path(git).expanduser()).resolve())

    def checkout(self, address: str, ref: str) -> Path:
        """Fetches a repository at a branch, tag or commit into its checkout and returns it. A checkout of a commit is fetched once, since a commit never changes."""
        checkout = Repository.PLUGIN_CHECKOUTS / hashlib.sha256(f"{address}\n{ref}".encode()).hexdigest()[:16]
        if PluginSource.COMMIT.fullmatch(ref) and (checkout / ".git").is_dir() and self.head(checkout) == ref:
            return checkout

        self.terminal.step(f'Fetching the plugin repository `{address}` at "{ref}"')
        try:
            if not (checkout / ".git").is_dir():
                checkout.mkdir(parents=True, exist_ok=True)
                self.process.run(["git", "init", "--quiet", checkout])
            self.process.run(["git", "-C", checkout, "fetch", "--quiet", "--depth", "1", address, ref])
            self.process.run(["git", "-C", checkout, "checkout", "--quiet", "--force", "FETCH_HEAD"])
            self.process.run(["git", "-C", checkout, "clean", "--quiet", "-ffdx"])
        except BuildFailed as error:
            raise BuildFailed(
                f'The plugin repository `{address}` cannot be fetched at "{ref}". Check the address in "git" and the branch, tag or commit in "ref".',
                details=error.details,
            ) from error
        return checkout

    def head(self, checkout: Path) -> str:
        return self.process.capture(["git", "-C", checkout, "rev-parse", "HEAD"]).strip()

    def resolve_commit(self, reference: PluginReference) -> str:
        """Fetches the ref of a git plugin and returns the commit it names now, which `plugin add` and `plugin update` pin."""
        checkout = self.checkout(self.address(reference.git), reference.ref or "HEAD")
        return self.head(checkout)

    @staticmethod
    def inside(checkout: Path, path: str | None, key: str) -> Path:
        """Returns the folder that `path` names inside a checkout, refusing one that leads outside it through an absolute path, `..` or a symbolic link."""
        if path is None:
            return checkout
        folder = (checkout / path).resolve()
        windows = PureWindowsPath(path)
        if windows.anchor or ".." in windows.parts or not folder.is_relative_to(checkout.resolve()):
            raise InvalidManifest(
                f'The key "{key}" names the folder "{path}" of a git repository, which leads outside it. Name a folder inside the repository, relative to its root, such as "camera".'
            )
        return folder
