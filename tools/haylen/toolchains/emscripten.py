from collections.abc import Mapping, Sequence
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.tool_status import ToolStatus


class Emscripten:
    """The pinned Emscripten SDK in `.tools/emsdk`, which compiles the web builds of the engine and of apps, and whose Node runs the drivers of the pages that the web checks open. Emscripten keeps no ABI between versions, so a build tree that another version configured builds again from nothing."""

    VERSION = "6.0.13"
    ROOT = Repository.TOOLS / "emsdk"
    REPOSITORY = "https://github.com/emscripten-core/emsdk.git"
    # The file in every web build tree that records the version of Emscripten that configured it.
    MARKER = "emscripten-version.txt"

    @staticmethod
    def emcmake() -> Path:
        """The `emcmake` of the SDK, which runs a CMake command with the toolchain of Emscripten."""
        return Emscripten.ROOT / "upstream" / "emscripten" / ("emcmake.bat" if Host.system() == "windows" else "emcmake")

    @staticmethod
    def installed_version() -> str | None:
        version = Emscripten.ROOT / "upstream" / "emscripten" / "emscripten-version.txt"
        if not Emscripten.emcmake().is_file() or not version.is_file():
            return None
        return version.read_text(encoding="utf-8").strip().strip('"')

    @staticmethod
    def status() -> ToolStatus:
        """Tells whether the pinned version is installed, without installing it, for `doctor`."""
        version = Emscripten.installed_version()
        if version == Emscripten.VERSION:
            return ToolStatus("Emscripten", ToolStatus.Role.HOST, version, Emscripten.emcmake(), None)
        found = f"holds Emscripten {version}" if version else "holds no Emscripten"
        problem = (
            f'The folder `{Repository.shown(Emscripten.ROOT)}` {found}. The first web build installs Emscripten {Emscripten.VERSION} there, and "haylen engine tools --emscripten" installs it now.'
        )
        return ToolStatus("Emscripten", ToolStatus.Role.HOST, version, None, problem)

    @staticmethod
    def install(context: ToolingContext) -> Path:
        """Installs the pinned version unless the SDK holds it already, and returns its `emcmake`. An SDK that holds another version takes the newest list of releases first."""
        if Emscripten.installed_version() == Emscripten.VERSION:
            return Emscripten.emcmake()

        context.terminal.step(f"Installing Emscripten {Emscripten.VERSION} into `{Repository.shown(Emscripten.ROOT)}`")
        if Emscripten.ROOT.exists():
            context.process.run(["git", "fetch", "--depth", "1", "origin", "HEAD"], cwd=Emscripten.ROOT)
            context.process.run(["git", "reset", "--hard", "FETCH_HEAD"], cwd=Emscripten.ROOT)
        else:
            context.process.run(["git", "clone", "--depth", "1", Emscripten.REPOSITORY, Emscripten.ROOT])
        script = Emscripten.ROOT / ("emsdk.bat" if Host.system() == "windows" else "emsdk")
        context.process.run([script, "install", Emscripten.VERSION], cwd=Emscripten.ROOT)
        context.process.run([script, "activate", Emscripten.VERSION], cwd=Emscripten.ROOT)
        return Emscripten.emcmake()

    @staticmethod
    def node() -> Path:
        """The Node of the SDK, which runs the drivers of the pages of the web checks."""
        name = "node.exe" if Host.system() == "windows" else "node"
        node = next((Emscripten.ROOT / "node").glob(f"*/bin/{name}"), None)
        if node is None:
            raise MissingToolchain(f'The Emscripten SDK in `{Repository.shown(Emscripten.ROOT)}` holds no Node, which runs the web checks. Install it again with "haylen engine tools --emscripten".')
        return node

    @staticmethod
    def is_stale(folder: Path) -> bool:
        """Tells whether another version of Emscripten configured a web build tree, or one that keeps no record of its version."""
        if not (folder / "CMakeCache.txt").exists():
            return False
        marker = folder / Emscripten.MARKER
        return not marker.is_file() or marker.read_text(encoding="utf-8").strip() != Emscripten.VERSION

    @staticmethod
    def configure(context: ToolingContext, tree: CMakeTree, command: Sequence[object], env: Mapping[str, str] | None = None) -> None:
        """Configures a web build tree through `emcmake` with the pinned version, deleting the tree first when another version configured it, and records the version once the configure succeeded."""
        emcmake = Emscripten.install(context)
        if Emscripten.is_stale(tree.folder):
            Folders.remove(tree.folder)
        tree.configure([emcmake, *command], env)
        (tree.folder / Emscripten.MARKER).write_text(Emscripten.VERSION, encoding="utf-8")
