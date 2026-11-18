import shutil
from types import MappingProxyType

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.repository import Repository
from haylen.core.target_environment import TargetEnvironment
from haylen.native.engine_sdk_tree import EngineSdkTree
from haylen.platforms.contract import Contract
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_folder import SdkFolder
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.emscripten import Emscripten


class WebPort(EnginePort):
    """The engine port of the web, which compiles the engine with the pinned Emscripten in one build tree into the web SDK: the web player, which runs any package that the page hands it and draws through WebGPU or WebGL2, whichever starts first among the ones the page asks for, next to the processor of its audio output, the worker of its content store and the page side and the worker of the Worker mode, and the Emscripten CMake package that release runtimes and C++ apps link against. Emscripten keeps no ABI between its versions, so the toolchain of the target names the pinned version."""

    NAME = "web"
    VERSION = Repository.engine_version()
    SYSTEM = "web"
    ARCHITECTURE = "wasm32"
    ABIS = MappingProxyType({"wasm32": "wasm32-emscripten"})
    TOOLCHAIN = f"emscripten-{Emscripten.VERSION}"
    RUNTIME = "libc++"
    # A page runs in any browser that starts WebGPU or WebGL2, which the loader checks, so the web declares no oldest system.
    MINIMUM = "0"
    GRAPHICS_APIS = ("webGpu", "webGl2")
    SHADER_LANGUAGES = ("wgsl", "glsl300es")
    # The runtime of a page loads the processor of its audio output and the worker of its content store from next to its script, and a page of the Worker mode loads the page side of the runtime and the worker that runs it from there too.
    PLAYER_FILES = ("haylen.js", "haylen.wasm", "haylen-audio-worklet.js", "haylen-content-store.js", "haylen-page.js", "haylen-worker.js")
    SLICE = "web-wasm32"
    TOOLS = ("cmake", "ninja", "Emscripten", "haylen-shaderc")

    def describe(self) -> PortDescription:
        target = PortTarget(
            name="web",
            system=WebPort.SYSTEM,
            environment=TargetEnvironment.DEVICE,
            abis=WebPort.ABIS,
            toolchain=WebPort.TOOLCHAIN,
            runtime=WebPort.RUNTIME,
            minimum_system_version=WebPort.MINIMUM,
            graphics_apis=WebPort.GRAPHICS_APIS,
            shader_languages=WebPort.SHADER_LANGUAGES,
            capabilities=frozenset(),
        )
        return PortDescription(WebPort.NAME, Contract.ENGINE_PORT, WebPort.VERSION, (target,), WebPort.TOOLS)

    def check_environment(self, target: str) -> EnvironmentReport:
        """Needs CMake and Ninja on the path. Emscripten installs on the first build, so its absence is no problem here."""
        return EnvironmentReport((BuildTools.CMAKE.status(), BuildTools.NINJA.status()), ())

    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        architectures = request.architectures or (WebPort.ARCHITECTURE,)
        if tuple(architectures) != (WebPort.ARCHITECTURE,):
            raise UnsupportedTarget(f'The web builds for "{WebPort.ARCHITECTURE}" alone.')

        configuration = request.configuration.value
        tree = EngineSdkTree(self.context, f"web-{configuration.lower()}")
        self.configure(tree, configuration)
        tree.tree.build(["haylen", "haylen-sdk"], configuration)

        folder = SdkFolder(request.folder, WebPort.NAME, WebPort.VERSION, request.revision, request.configuration)
        folder.begin()
        installed = folder.slice_folder(WebPort.SLICE)
        tree.tree.install("haylen-sdk", installed, configuration)
        player = installed / "player"
        player.mkdir()
        for name in WebPort.PLAYER_FILES:
            shutil.copy2(tree.folder / "bin" / "haylen" / name, player / name)

        files = {
            "cmake": EngineSdkTree.package_folder(installed).relative_to(request.folder).as_posix(),
            "player": player.relative_to(request.folder).as_posix(),
            "platform": (installed / "share" / "haylen" / "platform" / "web").relative_to(request.folder).as_posix(),
        }
        added = SdkSlice(
            system=WebPort.SYSTEM,
            environment=TargetEnvironment.DEVICE,
            architectures=(WebPort.ARCHITECTURE,),
            abi=WebPort.ABIS[WebPort.ARCHITECTURE],
            toolchain=WebPort.TOOLCHAIN,
            runtime=WebPort.RUNTIME,
            minimum_system_version=WebPort.MINIMUM,
            graphics_apis=tree.graphics_apis(),
            capabilities=frozenset(),
            files=MappingProxyType(files),
        )
        return folder.save(added, ())

    def configure(self, tree: EngineSdkTree, configuration: str) -> None:
        """Configures the web tree with the pinned Emscripten when it was never configured, its configure failed, or another Emscripten or the shader compiler of another pin configured it."""
        if tree.is_current() and not Emscripten.is_stale(tree.folder):
            return
        command = tree.command(configuration, "-G", "Ninja", "-DHAYLEN_BUILD_TOOLS=OFF")
        self.context.terminal.step(f"Configuring the engine of the web in `{Repository.shown(tree.folder)}`")
        Emscripten.configure(self.context, tree.tree, command)
