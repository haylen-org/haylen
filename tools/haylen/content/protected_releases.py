from pathlib import Path
from types import MappingProxyType

from haylen.assets.app_shaders import AppShaders
from haylen.content.content_profile import ContentProfile
from haylen.content.content_tool import ContentTool
from haylen.content.key_folder import KeyFolder
from haylen.content.protected_release import ProtectedRelease
from haylen.content.release_inspection import ReleaseInspection
from haylen.content.release_package import ReleasePackage
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.missing_release import MissingRelease
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.backend_registry import BackendRegistry
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.product_folders import ProductFolders


class ProtectedReleases:
    """Builds and examines the protected releases of apps with the content tool and the keys of each app. A release lives in `build/apps/<app>-<hash>/release/<profile>/`, which the targets of one content profile share, next to the build cache of the content, and every build follows the release before it, so the chunks that did not change keep their shards. Backends build the release of a resolved product, compile in the bootstrap and inspect the built product, and the content commands verify, inspect, compare and publish it."""

    BOOTSTRAP_FILE = "HaylenBootstrap.cpp"
    # The bootstrap holds the sealed keys of the app, so only its owner reads it.
    BOOTSTRAP_MODE = 0o600

    def __init__(self, context: ToolingContext, tool: ContentTool) -> None:
        self.context = context
        self.tool = tool

    @staticmethod
    def locate(context: ToolingContext, registry: BackendRegistry) -> ProtectedReleases:
        return ProtectedReleases(context, ContentTool.locate(context, registry))

    @staticmethod
    def root(app: AppManifest) -> Path:
        return ProductFolders.app_folder(app.folder) / "release"

    @staticmethod
    def folder(app: AppManifest, profile: str) -> Path:
        """The folder of the last protected release of an app for a content profile."""
        return ProtectedReleases.root(app) / profile

    def keys(self, app: AppManifest) -> Path:
        """Returns the key folder of an app, creating its keys on the machine of the developer the first time."""
        return KeyFolder.of(app.identifier, self.context.environment).ensure(self.tool)

    def build(self, source: ReleasePackage, profile: str, previous: Path | None = None) -> ProtectedRelease:
        """Builds the protected release of a package for a content profile and warns when it is larger than the store of the profile accepts. The release follows the one that `previous` names, such as the release that shipped last, which continuous integration keeps, or else the last release of the build folder."""
        app = source.app
        AppShaders(self.context).update(app.folder)
        keys = self.keys(app)
        root = ProtectedReleases.root(app)
        folder = ProtectedReleases.folder(app, profile)
        staging = root / f"{profile}.new"
        GeneratedTree.remove(staging)
        package = source.package.stage(root / f"{profile}.package")

        command: list[object] = ["build", package, "--keys", keys, "--profile", profile, "--build", app.build, "--output", staging, "--cache", root.parent / "content-cache"]
        if previous is not None:
            command += ["--previous", previous.resolve()]
        elif (folder / ProtectedRelease.MANIFESTS[0]).is_file():
            command += ["--previous", folder]
        content = ContentProfile.of(profile)
        if content.shard_target is not None:
            command += ["--shard-target", content.shard_target]
        self.context.terminal.step(f'Building the protected release of `{Repository.shown(app.folder)}` for the profile "{profile}"')
        self.tool.run(command)
        GeneratedTree.remove(folder)
        staging.rename(folder)

        shipped = root / f"{profile}.shipped"
        packs = root / f"{profile}.packs"
        GeneratedTree.remove(shipped)
        GeneratedTree.remove(packs)
        if not source.content_server and not source.packs:
            self.warn_store_size(folder, content)
            return ProtectedRelease(profile, folder, folder, MappingProxyType({}))

        # The shards of named packs go to the stores that deliver them, and with a server the shards that only later deliveries use stay in the release, which "content publish" puts on the server.
        stage: list[object] = ["stage", folder, "--keys", keys, "--output", shipped, "--app", package, "--packs", packs]
        if not source.content_server:
            stage.append("--ship-deferred")
        self.tool.run(stage)
        self.warn_store_size(shipped, content)
        return ProtectedRelease(profile, folder, shipped, ProtectedReleases.pack_folders(packs))

    @staticmethod
    def pack_folders(packs: Path) -> MappingProxyType[str, Path]:
        found = {path.name: path for path in sorted(packs.iterdir()) if path.is_dir()} if packs.is_dir() else {}
        return MappingProxyType(found)

    def warn_store_size(self, folder: Path, profile: ContentProfile) -> None:
        if profile.store_limit is None:
            return
        size = ProtectedRelease.size(folder)
        if size > profile.store_limit:
            self.context.terminal.warning(
                f"The protected release `{Repository.shown(folder)}` holds {size} bytes, more than {profile.store_limit_words}.",
                "Keep the content of the app within the limit, since the store refuses a larger app.",
            )

    @staticmethod
    def last(app: AppManifest, profile: str, folder: Path | None = None) -> ProtectedRelease:
        """Returns the release in `folder`, or else the last release of the app for a profile, which the content commands examine without building again."""
        release = folder.resolve() if folder is not None else ProtectedReleases.folder(app, profile)
        if not (release / ProtectedRelease.MANIFESTS[0]).is_file():
            raise MissingRelease(f'The folder `{Repository.shown(release)}` holds no protected release. Build one with "haylen content build" and the target of the profile "{profile}".')
        root = ProtectedReleases.root(app)
        shipped = root / f"{profile}.shipped"
        # A release that a command names by its folder is examined alone, without the part of it that shipped.
        if folder is not None or not shipped.is_dir():
            return ProtectedRelease(profile, release, release, MappingProxyType({}))
        return ProtectedRelease(profile, release, shipped, ProtectedReleases.pack_folders(root / f"{profile}.packs"))

    def app_files(self, source: ReleasePackage, release: ProtectedRelease) -> tuple[str, ...]:
        """The names of the files of a release that its app domain needs to start, the manifests and the shards of the code of the app, which the content tool finds in the manifests."""
        boot = ProtectedReleases.root(source.app) / "app-files"
        GeneratedTree.remove(boot)
        self.tool.run(["stage", release.folder, "--keys", self.keys(source.app), "--output", boot, "--app-only"])
        names = tuple(sorted(path.name for path in boot.iterdir()))
        GeneratedTree.remove(boot)
        return names

    def bootstrap_source(self, app: AppManifest, profile: str) -> bytes:
        """Returns the source of the bootstrap that the release build of an app compiles in, with its identity, its build, its public signing key and its content keys sealed. It is the same for the same keys, so a generated tree writes it only when the keys change, with `BOOTSTRAP_MODE`."""
        file = ProtectedReleases.root(app) / f"{profile}.bootstrap" / ProtectedReleases.BOOTSTRAP_FILE
        self.tool.run(["bootstrap", "--keys", self.keys(app), "--profile", profile, "--build", app.build, "--output", file])
        return file.read_bytes()

    def inspection(self, source: ReleasePackage) -> ReleaseInspection:
        """Returns the inspection of the built products of a package, which knows the keys of the app."""
        return ReleaseInspection(source, KeyFolder.of(source.app.identifier, self.context.environment).secrets())

    def verify(self, app: AppManifest, release: Path) -> str:
        """Authenticates every chunk of a release and returns the report of the tool."""
        return self.tool.capture(["verify", release, "--keys", self.keys(app)])

    def inspect(self, app: AppManifest, release: Path, chunks: bool) -> str:
        """Returns the manifests, shards and files of a release, and every chunk with `chunks`."""
        return self.tool.capture(["inspect", release, "--keys", self.keys(app), *(["--chunks"] if chunks else [])])

    def diff(self, app: AppManifest, earlier: Path, later: Path) -> str:
        """Returns what an app that holds the earlier release downloads to reach the later one."""
        return self.tool.capture(["diff", earlier.resolve(), later.resolve(), "--keys", self.keys(app)])

    def publish(self, source: ReleasePackage, profile: str, channel: str, tree: Path) -> str:
        """Publishes the content of a package as the next generation of a channel of a publication tree. The first generation of a channel keeps the shards of the last release that ships with the app, so installed apps download only what changed."""
        app = source.app
        AppShaders(self.context).update(app.folder)
        root = ProtectedReleases.root(app)
        package = source.package.stage(root / f"{profile}.package")
        release = ProtectedReleases.folder(app, profile)
        shipped = ["--release", release] if (release / ProtectedRelease.MANIFESTS[1]).is_file() else []
        command: list[object] = ["publish", package, "--keys", self.keys(app), "--profile", profile, "--build", app.build, "--channel", channel, "--output", tree.resolve()]
        self.context.terminal.step(f'Publishing the content of `{Repository.shown(app.folder)}` to the channel "{channel}" of `{Repository.shown(tree)}`')
        return self.tool.capture([*command, "--cache", root.parent / "content-cache", *shipped])

    def compact(self, app: AppManifest, tree: Path, keep: int) -> str:
        """Removes the manifests and packs of a publication tree that no channel reaches within its last `keep` generations."""
        return self.tool.capture(["compact", tree.resolve(), "--keys", self.keys(app), "--keep", keep])

    def show_keys(self, app: AppManifest, rotate: bool) -> str:
        """Returns the key IDs and the public signing key of an app, after adding a content key for the next builds with `rotate`."""
        return self.tool.capture(["keys", "rotate" if rotate else "show", self.keys(app)])
