import json
import shutil

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.schema_validator import SchemaValidator
from haylen.core.target_environment import TargetEnvironment
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.android.android_library_check import AndroidLibraryCheck
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.contract import Contract
from haylen.platforms.engine_port import EnginePort
from haylen.platforms.environment_report import EnvironmentReport
from haylen.platforms.port_description import PortDescription
from haylen.platforms.port_target import PortTarget
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_manifest import SdkManifest
from haylen.sdk.sdk_slice import SdkSlice
from haylen.toolchains.android_ndk import AndroidNdk
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.gradle import Gradle
from haylen.toolchains.java import Java
from haylen.toolchains.shader_compiler import ShaderCompiler


class AndroidPort(EnginePort):
    """The engine port of Android, which builds the engine for every ABI it is asked for with the pinned NDK and installs an SDK of one slice per ABI: the CMake package that the native code of apps links against and the player library `libhaylen.so` of each ABI, and the Android libraries of the engine in a local Maven repository, whose `haylen` library carries the player of every ABI of the SDK. The ABIs build one after the other with every job, so the configures never write into the shared sources of the dependencies together and the compilers stay within the jobs."""

    NAME = "android"
    VERSION = Repository.engine_version()
    ABIS_FOLDER = "abis"
    PLAYERS_FOLDER = "jniLibs"
    MAVEN_FOLDER = "maven"
    # The system libraries that the engine lists for Android, which the engine tree writes from `HAYLEN_ANDROID_LIBRARIES` of `engine/cmake/haylen-app.cmake`.
    LIBRARIES_FILE = "haylen-android.json"
    # The libraries that the SDK of an ABI installs into its library folder, by their names.
    SDK_LIBRARIES = {SdkSlice.ENGINE: "libhaylen.a", SdkSlice.RUNTIME: "libhaylen-runtime.a", SdkSlice.LUA: "libhaylen-lua.a", SdkSlice.PLAYER: "haylen-player.o"}

    def describe(self) -> PortDescription:
        target = PortTarget(
            name=AndroidPlatform.TARGET,
            system=AndroidPlatform.SYSTEM,
            environment=TargetEnvironment.DEVICE,
            abis=AndroidPlatform.ABIS,
            toolchain=AndroidPlatform.TOOLCHAIN,
            runtime=AndroidPlatform.RUNTIME,
            minimum_system_version=str(AndroidPlatform.MINIMUM_API),
            graphics_apis=AndroidPlatform.GRAPHICS_APIS,
            shader_languages=AndroidPlatform.SHADER_LANGUAGES,
            capabilities=AndroidPlatform.CAPABILITIES,
        )
        return PortDescription(AndroidPort.NAME, Contract.ENGINE_PORT, AndroidPort.VERSION, (target,), ("Android SDK", "Android NDK", "JDK", "Gradle", "cmake", "ninja"), ())

    def check_environment(self, target: str) -> EnvironmentReport:
        sdk = AndroidSdk(self.context.environment)
        tools = (sdk.status()[0], AndroidNdk(sdk).status(), Java(self.context.environment).status(), Gradle.status(), BuildTools.CMAKE.status(), BuildTools.NINJA.status())
        return EnvironmentReport(tools, ())

    def build_sdk(self, request: SdkRequest) -> SdkManifest:
        abis = request.architectures or AndroidNdk.ABIS
        unknown = [abi for abi in abis if abi not in AndroidPlatform.ABIS]
        if unknown:
            raise UnsupportedTarget(f"The engine port of Android builds for {SchemaValidator.choices(list(AndroidNdk.ABIS))}, not for {SchemaValidator.choices(unknown, 'and')}.")

        kept = self.kept_slices(request)
        trees = [self.build_abi(request, abi) for abi in abis]
        slices = {entry.abi: entry for entry in kept} | {abi: AndroidPort.slice(abi) for abi in abis}
        ordered = tuple(slices[abi] for abi in AndroidNdk.ABIS if abi in slices)
        self.publish_libraries(request, trees[0], ordered)

        manifest = SdkManifest(
            folder=request.folder.resolve(),
            port=AndroidPort.NAME,
            port_version=AndroidPort.VERSION,
            engine_version=Repository.engine_version(),
            revision=request.revision,
            configuration=request.configuration,
            slices=ordered,
            host_tools=(),
            files={AndroidPlatform.MAVEN_FILE: AndroidPort.MAVEN_FOLDER},
        )
        manifest.save()
        return manifest

    def kept_slices(self, request: SdkRequest) -> tuple[SdkSlice, ...]:
        """Returns the slices of the SDK in the folder that were built from the same engine sources in the same configuration, which the new ABIs join. The files of an SDK of other sources or another configuration leave entirely."""
        if (request.folder / SdkManifest.FILE).is_file():
            existing = SdkManifest.load(request.folder)
            if existing.port == AndroidPort.NAME and existing.revision == request.revision and existing.configuration is request.configuration:
                return existing.slices
        for name in (AndroidPort.ABIS_FOLDER, AndroidPort.PLAYERS_FOLDER, AndroidPort.MAVEN_FOLDER, SdkManifest.FILE):
            path = request.folder / name
            if path.is_file():
                path.unlink()
            Folders.remove(path)
        return ()

    @staticmethod
    def slice(abi: str) -> SdkSlice:
        files = {
            AndroidPlatform.CMAKE_FILE: f"{AndroidPort.ABIS_FOLDER}/{abi}/lib/cmake/haylen",
            AndroidPlatform.PLAYER_FILE: f"{AndroidPort.PLAYERS_FOLDER}/{abi}/libhaylen.so",
        }
        return SdkSlice(
            system=AndroidPlatform.SYSTEM,
            environment=TargetEnvironment.DEVICE,
            architectures=(abi,),
            abi=abi,
            toolchain=AndroidPlatform.TOOLCHAIN,
            runtime=AndroidPlatform.RUNTIME,
            minimum_system_version=str(AndroidPlatform.MINIMUM_API),
            graphics_apis=AndroidPlatform.GRAPHICS_APIS,
            capabilities=AndroidPlatform.CAPABILITIES,
            libraries={name: f"{AndroidPort.ABIS_FOLDER}/{abi}/lib/{file}" for name, file in AndroidPort.SDK_LIBRARIES.items()},
            files=files,
        )

    def build_abi(self, request: SdkRequest, abi: str) -> CMakeTree:
        """Builds the player library and the SDK of one ABI in its tree under `build/engine` and installs both into the SDK folder."""
        configuration = request.configuration.value
        tree = CMakeTree(self.context, Repository.ENGINE_BUILDS / f"android-{abi}-{configuration.lower()}")
        compiler = ShaderCompiler.path(self.context)
        if not tree.configured or tree.cache_value("HAYLEN_SHADERC") != compiler.as_posix():
            self.context.terminal.step(f'Configuring the engine for "{abi}" in `{Repository.shown(tree.folder)}`')
            command: list[object] = ["cmake", "-S", Repository.ENGINE, "-B", tree.folder, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={configuration}", f"-DHAYLEN_SHADERC={compiler.as_posix()}"]
            command += ["-DHAYLEN_BUILD_PLAYER=ON", "-DHAYLEN_BUILD_SDK=ON", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF", "-DHAYLEN_BUILD_TOOLS=OFF"]
            command += AndroidNdk(AndroidSdk(self.context.environment)).cmake_options(abi)
            tree.configure(command, env=self.context.environment)

        self.context.terminal.step(f'Building the engine for "{abi}" in the "{configuration}" configuration')
        tree.build(["haylen", "haylen-sdk"], configuration)
        self.check_libraries(tree, abi)
        prefix = request.folder / AndroidPort.ABIS_FOLDER / abi
        Folders.remove(prefix)
        tree.install("haylen-sdk", prefix, configuration)

        player = request.folder / AndroidPort.PLAYERS_FOLDER / abi / "libhaylen.so"
        player.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(tree.folder / "lib" / "libhaylen.so", player)
        return tree

    def check_libraries(self, tree: CMakeTree, abi: str) -> None:
        """Checks that the player library of an ABI needs exactly the system libraries that the engine lists for Android, which the tree writes into `haylen-android.json`."""
        listed = json.loads((tree.folder / AndroidPort.LIBRARIES_FILE).read_text(encoding="utf-8"))["libraries"]
        readelf = AndroidNdk(AndroidSdk(self.context.environment)).tool("llvm-readelf")
        self.context.terminal.step(f'Checking the system libraries of "{abi}"')
        AndroidLibraryCheck(self.context).check(readelf, tree.folder / "lib" / "libhaylen.so", abi, listed)

    def publish_libraries(self, request: SdkRequest, tree: CMakeTree, slices: tuple[SdkSlice, ...]) -> None:
        """Publishes the Android libraries of the engine to the Maven repository of the SDK, the `haylen` library with the player of every ABI of the SDK and the Kotlin transport of Varn from where CPM placed it for the native build."""
        varn = tree.cache_value("varn_SOURCE_DIR")
        if varn is None:
            raise BuildFailed(f'The engine tree `{Repository.shown(tree.folder)}` names no "varn_SOURCE_DIR", so its configure did not finish. Build the SDK again.')
        maven = request.folder / AndroidPort.MAVEN_FOLDER
        Folders.remove(maven)
        players = request.folder / AndroidPort.PLAYERS_FOLDER
        abis = {entry.abi for entry in slices}
        for folder in sorted(players.iterdir()):
            if folder.name not in abis:
                Folders.remove(folder)

        names = ", ".join(f'"{entry.abi}"' for entry in slices)
        self.context.terminal.step(f"Publishing the Android libraries of the engine with the players of {names}")
        builds = Repository.ENGINE_BUILDS / f"android-libraries-{request.configuration.value.lower()}"
        tasks = [f":{module}:publishReleasePublicationToArtifactsRepository" for module in AndroidPlatform.LIBRARIES]
        command: list[object] = [Gradle.path(self.context), "-p", AndroidPlatform.LIBRARY_PROJECT, *tasks, f"--max-workers={self.context.jobs}", "--project-cache-dir", builds / "gradle-cache"]
        command += [f"-PhaylenNativeLibraries={players}", f"-PhaylenVarnSourceDir={varn}", f"-PhaylenMavenDir={maven}", f"-PhaylenBuildDirectory={builds}"]
        self.context.process.run(command, env=self.context.environment)
